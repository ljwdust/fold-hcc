#include "ShapeScaffold.h"
#include "PatchNode.h"
#include <QFileInfo>
#include "FdUtility.h"
#include "SectorCylinder.h"
#include "ChainScaffold.h"
#include "IntrRect2Rect2.h"
#include "TUnitScaffold.h"
#include "HUnitScaffold.h"
#include "Decomposer.h"

ShapeScaffold::ShapeScaffold(QString id, Scaffold* scaffold, Vector3 v, double connThr)
	: Scaffold(*scaffold), baseMaster(nullptr) // clone the FdGraph
{
	path = QFileInfo(path).absolutePath();
	mID = id;
	sqzV = v;

	// threshold
	connThrRatio = connThr;

	// decompose into units
	createUnits();

	// unit weights
	computeUnitWeights();

	// master order constraints
	// to-do: order constraints among all parts
	computeMasterOrderConstraints();

	// time scale
	double totalDuration = 0;
	foreach (UnitScaffold* b, units) totalDuration += b->getNbTopMasters();
	timeScale = 1.0 / totalDuration;

	// selection
	selBlockIdx = -1;
	keyframeIdx = -1;
	//////////////////////////////////////////////////////////////////////////
	//QVector<Geom::Segment> normals;
	//foreach (PatchNode* m, masters)
	//	normals << Geom::Segment(m->mPatch.Center, m->mPatch.Center + 10 * m->mPatch.Normal);
	//addDebugSegments(normals);
}

ShapeScaffold::~ShapeScaffold()
{
	foreach (UnitScaffold* l, units)	delete l;
}

UnitScaffold* ShapeScaffold::createUnit(QSet<int> sCluster)
{
	QVector<PatchNode*> ms;
	QVector<ScaffoldNode*> ss;
	QVector< QVector<QString> > mPairs;

	QSet<int> mids; // master indices
	foreach(int sidx, sCluster)
	{
		// slaves
		ss << slaves[sidx];

		QVector<QString> mp;
		for (int mid : slave2master[sidx]){
			mids << mid;
			mp << masters[mid]->mID;
		}

		// master pairs
		mPairs << mp;
	}

	// masters
	foreach(int mid, mids)	ms << masters[mid];

	// create
	int bidx = units.size();
	QString id = QString::number(bidx);
	UnitScaffold* b;
	if (ms.size() == 2 && ss.size() == 1 &&
		(ms[0]->hasTag(EDGE_ROD_TAG) || ms[1]->hasTag(EDGE_ROD_TAG)))
		b = new TUnitScaffold("TB_" + id, ms, ss, mPairs);
	else b = new HUnitScaffold("HB_" + id, ms, ss, mPairs);
	b->setAabbConstraint(computeAABB().box());
	units << b;

	// master block map
	for (auto m : ms) masterUnitMap[m->mID] << bidx;

	// set up path
	b->path = path;

	return b;
}

void ShapeScaffold::createUnits()
{
	// clear blocks
	for (auto b : units) delete b;
	units.clear();
	masterUnitMap.clear();

	// decompose
	Decomposer decmp(this);
	decmp.execute();

	// each slave cluster forms a block
	for (auto sc : slaveClusters)
		createUnit(sc);
}


void ShapeScaffold::computeMasterOrderConstraints()
{
	// time stamps: bottom-up
	double tScale;
	QMap<QString, double> masterTimeStamps = getTimeStampsNormalized(masters, sqzV, tScale);

	// the base master is the bottom one that is not virtual
	double minT = maxDouble();
	foreach(PatchNode* m, masters){
		if (!m->hasTag(EDGE_ROD_TAG) && masterTimeStamps[m->mID] < minT){
			baseMaster = m;
			minT = masterTimeStamps[m->mID];
		}
	}

	// projected masters on the base master
	QMap<QString, Geom::Rectangle2> proj_rects;
	Geom::Rectangle base_rect = baseMaster->mPatch;
	foreach (PatchNode* m, masters)
		proj_rects[m->mID] = base_rect.get2DRectangle(m->mPatch);

	// check each pair of masters
	for (int i = 0; i < masters.size(); i++){
		for (int j = i+1; j < masters.size(); j++)
		{
			QString mi = masters[i]->mID;
			QString mj = masters[j]->mID;

			// test intersection between projections
			if (Geom::IntrRect2Rect2::test(proj_rects[mi], proj_rects[mj]))
			{
				double ti = masterTimeStamps[mi];
				double tj = masterTimeStamps[mj];

				// <up, down>
				if (ti > tj) 
				{
					masterOrderGreater[mi] << mj;
					masterOrderLess[mj] << mi;
				}
				else 
				{
					masterOrderGreater[mj] << mi;
					masterOrderLess[mi] << mj;
				}	
			}
		}
	}
}

UnitScaffold* ShapeScaffold::getSelUnit()
{
	if (selBlockIdx >= 0 && selBlockIdx < units.size())
		return units[selBlockIdx];
	else
		return NULL;
}

Scaffold* ShapeScaffold::activeScaffold()
{
	UnitScaffold* selLayer = getSelUnit();
	if (selLayer) return selLayer->activeScaffold();
	else		  return this;
}

QStringList ShapeScaffold::getUnitLabels()
{
	QStringList labels;
	foreach(UnitScaffold* l, units)
		labels.append(l->mID);

	// append string to select none
	labels << "--none--";

	return labels;
}

void ShapeScaffold::selectUnit( QString id )
{
	// select layer named id
	selBlockIdx = -1;
	for (int i = 0; i < units.size(); i++){
		if (units[i]->mID == id){
			selBlockIdx = i;
			break;
		}
	}

	// disable selection on chains
	if (getSelUnit())
		getSelUnit()->selectChain("");
}

Scaffold* ShapeScaffold::getKeyframe( double t )
{
	bool showActive = false;	
	Vector3 aOrigPos;
	QString aBaseMID;
	QVector<Geom::Box> aAFS;
	QVector<Vector3> aAFR_CP;
	QVector<Vector3> aMaxFR;
	QVector<Geom::Rectangle> aFR; 
	
	// folded blocks
	QVector<Scaffold*> foldedBlocks;
	for (int i = 0; i < units.size(); i++)
	{
		double lt = getLocalTime(t, units[i]->mFoldDuration);
		Scaffold* fblock = units[i]->getKeyframe(lt, true);
		foldedBlocks << fblock;

		// debug: active block
		if (lt > 0 && lt < 1)
		{
			showActive = true;
			aOrigPos = units[i]->baseMaster->center();
			aBaseMID = units[i]->baseMaster->mID;
			
			// active slaves
			foreach (ScaffoldNode* n, fblock->getScfdNodes())
				n->addTag(ACTIVE_NODE_TAG);
		}
	}

	// shift layers and add nodes into scaffold
	Scaffold *key_graph = combineFdGraphs(foldedBlocks, baseMaster->mID, masterUnitMap);

	// delete folded blocks
	foreach (Scaffold* b, foldedBlocks) delete b;

	// in case the combination fails
	if (key_graph == nullptr) return nullptr;

	// path
	key_graph->path = path;

	return key_graph;
}

ShapeSuperKeyframe* ShapeScaffold::getShapeSuperKeyframe( double t )
{
	// super key frame for each block
	QVector<Scaffold*> foldedBlocks;
	QSet<QString> foldedParts;
	for (int i = 0; i < units.size(); i++)
	{
		double lt = getLocalTime(t, units[i]->mFoldDuration);
		Scaffold* fblock = units[i]->getSuperKeyframe(lt);
		foldedBlocks << fblock;

		// nullptr means fblock is unable to fold at time t
		if (fblock == nullptr) return nullptr;
	}

	// combine
	Scaffold *keyframe = combineFdGraphs(foldedBlocks, baseMaster->mID, masterUnitMap);

	// create shape super key frame
	ShapeSuperKeyframe* ssKeyframe = new ShapeSuperKeyframe(keyframe, masterOrderGreater);

	// garbage collection
	delete keyframe;
	for (int i = 0; i < foldedBlocks.size(); i++) delete foldedBlocks[i];

	// return
	return ssKeyframe;
}

void ShapeScaffold::foldabilize()
{
	// clear tags
	foreach (UnitScaffold* b, units)
		b->foldabilized = false;

	// initial time intervals: t > 1.0 means not be folded
	foreach (UnitScaffold* b, units)
		b->mFoldDuration = INTERVAL(1.0, 2.0);

	// choose best free block
	std::cout << "\n============================="
			  << "\n============START============\n";
	double currTime = 0.0;
	ShapeSuperKeyframe* currKeyframe = getShapeSuperKeyframe(currTime);
	int next_bid = getBestNextUnitIdx(currTime, currKeyframe);  
	std::cout << "Best next = " << next_bid << "\n";

	//return;
	

	while (next_bid >= 0 && next_bid < units.size())
	{
		UnitScaffold* next_block = units[next_bid];
		std::cout << "Foldabilizing block: " << next_block->mID.toStdString() << "\n";

		// time interval
		double timeLength = next_block->getNbTopMasters() * timeScale;
		double nextTime = currTime + timeLength;

		// foldabilize
		next_block->foldabilizeWrt(currKeyframe);
		next_block->applySolution();
		next_block->mFoldDuration = INTERVAL(currTime, nextTime);

		// get best next
		std::cout << "\n============NEXT============\n";
		currTime = nextTime;
		delete currKeyframe;
		currKeyframe = getShapeSuperKeyframe(currTime);
		next_bid = getBestNextUnitIdx(currTime, currKeyframe);
		std::cout << "Best next = " << next_bid << "\n";
	}

	// remaining blocks (if any) are interlocking
	QVector<UnitScaffold*> blocks_copy = units;
	units.clear();
	QVector<int> intlkBlockIndices;
	for (int bid = 0; bid < blocks_copy.size(); bid ++)
	{
		UnitScaffold* b = blocks_copy[bid];
		if (b->foldabilized) units << b;
		else intlkBlockIndices << bid;
	}

	// merge them as single block to foldabilize
	if (!intlkBlockIndices.isEmpty())
	{
		std::cout << "\n=====MERGE INTERLOCKING======\n";
		QSet<int> sCluster;
		foreach (int bidx, intlkBlockIndices)
			sCluster += slaveClusters[bidx];
		
		UnitScaffold* mergedBlock = createUnit(sCluster);
		mergedBlock->foldabilizeWrt(currKeyframe);
		mergedBlock->applySolution();
		mergedBlock->mFoldDuration = INTERVAL(currTime, 1.0);
	}

	delete currKeyframe;
	std::cout << "\n============FINISH============\n";
}

double ShapeScaffold::foldabilizeUnit(int bid, double currTime, ShapeSuperKeyframe* currKf, 
										double& nextTime, ShapeSuperKeyframe*& nextKf)
{
	// foldabilize nextBlock wrt the current super key frame
	UnitScaffold* nextBlock = units[bid];
	double cost = nextBlock->foldabilizeWrt(currKf);
	cost *= unitWeights[bid]; // normalized cost
	nextBlock->applySolution();

	// get the next super key frame 
	double timeLength = nextBlock->getNbTopMasters() * timeScale;
	nextTime = currTime + timeLength;
	nextBlock->mFoldDuration = INTERVAL(currTime, nextTime);
	nextKf = getShapeSuperKeyframe(nextTime);

	// the cost
	return cost;
}

/**   currT                 nextT                 next2T
	    |------nextBlock------|------next2Block------|
   currKeyframe          nextKeyframe           next2Keyframe
**/  
// currKeyframe is a super keyframe providing context information
int ShapeScaffold::getBestNextUnitIdx(double currTime, ShapeSuperKeyframe* currKeyframe)
{
	double min_cost = maxDouble();
	int best_next_bid = -1;
	for (int next_bid = 0; next_bid < units.size(); next_bid++)
	{
		UnitScaffold* nextBlock = units[next_bid];
		if (passed(currTime, nextBlock->mFoldDuration)) continue; // skip foldabilized blocks

		// foldabilize nextBlock
		double nextTime = -1.0;
		ShapeSuperKeyframe* nextKeyframe = nullptr;
		Interval origNextTi = nextBlock->mFoldDuration; // back up
		double nextCost = foldabilizeUnit(next_bid, currTime, currKeyframe, nextTime, nextKeyframe);

		//nextBlock->showObstaclesAndFoldOptions();
		//return -1;
		
		// the folding of nextBlock must be valid, otherwise skip further evaluation
		if (nextKeyframe->isValid(sqzV))
		{
			// cost of folding remaining open blocks
			for (int next2_bid = 0; next2_bid < units.size(); next2_bid++)
			{
				UnitScaffold* next2Block = units[next2_bid];
				if (passed(nextTime, next2Block->mFoldDuration)) continue; // skip foldabilized block

				// foldabilize next2Block
				double next2Time;
				ShapeSuperKeyframe* next2Keyframe;
				Interval origNext2Ti = next2Block->mFoldDuration;
				double next2Cost = foldabilizeUnit(next2_bid, nextTime, nextKeyframe, next2Time, next2Keyframe);

				// invalid block folding generates large cost as being deleted
				if (!next2Keyframe->isValid(sqzV)) next2Cost = 1.0; // deletion cost = 1.0

				// update cost : normalized cost
				nextCost += next2Cost * unitWeights[next2_bid];

				delete next2Keyframe; // garbage collection
				next2Block->mFoldDuration = origNext2Ti; // restore time interval
			}

			// nextBlock with lowest cost wins
			if (nextCost < min_cost - ZERO_TOLERANCE_LOW){
				min_cost = nextCost;
				best_next_bid = next_bid;
			}
		}else
		{
			// invalid info for debug
			nextCost = -1;
		}

		delete nextKeyframe; // garbage collection
		nextBlock->mFoldDuration = origNextTi;// restore time interval

		// debug info
		std::cout << units[next_bid]->mID.toStdString() << " : " << nextCost << std::endl;
	}

	// found the best next block in terms of introducing most free space for others
	return best_next_bid;
}

void ShapeScaffold::generateKeyframes( int N )
{
	keyframes.clear();

	double step = 1.0 / (N-1);
	for (int i = 0; i < N; i++)
	{
		Scaffold* kf = getKeyframe(i * step);
		if(!kf) return;

		keyframes << kf;

		kf->unwrapBundleNodes();
		kf->hideEdgeRods();

		// color
		foreach (ScaffoldNode* n, kf->getScfdNodes())
		{
			double grey = 240;
			QColor c = (n->hasTag(ACTIVE_NODE_TAG) && !n->hasTag(MASTER_TAG)) ? 
				QColor::fromRgb(255, 110, 80) : QColor::fromRgb(grey, grey, grey);
			c.setAlphaF(0.78);
			n->mColor = c;
		}
	}
}

Scaffold* ShapeScaffold::getSelKeyframe()
{
	if (keyframeIdx >= 0 && keyframeIdx < keyframes.size())
		return keyframes[keyframeIdx];
	else return NULL;
}

void ShapeScaffold::selectKeyframe( int idx )
{
	if (idx >= 0 && idx < keyframes.size())
		keyframeIdx = idx;
}

double ShapeScaffold::getConnectivityThr()
{
	return connThrRatio * computeAABB().radius();
}

void ShapeScaffold::computeUnitWeights()
{
	unitWeights.clear();

	double totalA = 0;
	for (auto b : units)
	{
		double area = b->getChainArea();
		unitWeights << area;
		totalA += area;
	}

	for (auto& bw : unitWeights) bw /= totalA;
}
