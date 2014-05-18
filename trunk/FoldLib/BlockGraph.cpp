#include "BlockGraph.h"
#include "FdUtility.h"
#include "IntrRectRect.h"
#include "IntrRect2Rect2.h"
#include "Numeric.h"
#include "HChain.h"
#include "CliquerAdapter.h"

BlockGraph::BlockGraph( QVector<PatchNode*>& ms, QVector<FdNode*>& ss, 
	QVector< QVector<QString> >& mPairs, QString id )
	: FdGraph(id)
{
	// selected chain
	selChainIdx = -1;

	// clone nodes
	foreach (PatchNode* m, ms)	{
		masters << (PatchNode*)m->clone();
		Structure::Graph::addNode(masters.last());
	}
	foreach (FdNode* s, ss) 
		Structure::Graph::addNode(s->clone());

	// sort masters
	masterTimeStamps = getTimeStampsNormalized(masters, masters.front()->mPatch.Normal, timeScale);
	foreach (PatchNode* m, masters)
	{
		// base master is the bottom one
		if (masterTimeStamps[m->mID] < ZERO_TOLERANCE_LOW)
			baseMaster = m;

		// height
		masterHeight[m->mID] = masterTimeStamps[m->mID] * timeScale;
	}

	// create chains
	for (int i = 0; i < ss.size(); i++)
	{
		QString mid_low = mPairs[i].front();
		QString mid_high = mPairs[i].last();
		if (masterTimeStamps[mid_low] > masterTimeStamps[mid_high])
		{
			mid_low = mPairs[i].last();
			mid_high = mPairs[i].front();
		}

		// create chain
		HChain* hc = new HChain(ss[i], (PatchNode*)getNode(mid_low), (PatchNode*)getNode(mid_high));
		hc->setFoldDuration(masterTimeStamps[mid_low], masterTimeStamps[mid_high]);
		chains << hc;

		// map from master id to chain idx set
		masterChainsMap[mid_low] << i;
		masterChainsMap[mid_high] << i;

		// map from master id to under chain ids set
		masterUnderChainsMap[mid_high] << i;
	}

	// initial collision graph
	collFogOrig = NULL;
	collFog = new FoldOptionGraph();
}

BlockGraph::~BlockGraph()
{
	foreach(ChainGraph* c, chains)
		delete c;
}

FdGraph* BlockGraph::activeScaffold()
{
	FdGraph* selChain = getSelChain();
	if (selChain)  return selChain;
	else		   return this;
}

HChain* BlockGraph::getSelChain()
{
	if (selChainIdx >= 0 && selChainIdx < chains.size())
		return chains[selChainIdx];
	else
		return NULL;
}

void BlockGraph::selectChain( QString id )
{
	selChainIdx = -1;
	for (int i = 0; i < chains.size(); i++)
	{
		if (chains[i]->mID == id)
		{
			selChainIdx = i;
			break;
		}
	}
}

QStringList BlockGraph::getChainLabels()
{
	QStringList labels;
	foreach(FdGraph* c, chains)
		labels.push_back(c->mID);

	// append string to select none
	labels << "--none--";

	return labels;
}

void BlockGraph::computeMinFoldingRegion()
{
	Geom::Rectangle base_rect = baseMaster->mPatch;
	foreach (PatchNode* top_master, masters)
	{
		// skip base master
		if (top_master == baseMaster) continue;

		// min folding region
		Geom::Rectangle2 min_region = base_rect.get2DRectangle(top_master->mPatch);
		minFoldingRegion[top_master->mID] = min_region;

		//addDebugSegments(base_rect.get3DRectangle(min_region).getEdgeSegments());
	}
}

void BlockGraph::computeMaxFoldingRegion(Geom::Box shapeAABB)
{
	Geom::Rectangle base_rect = baseMaster->mPatch;
	foreach (PatchNode* top_master, masters)
	{
		// skip base master
		if (top_master == baseMaster) continue;

		// 2D points from fold region of slaves and master
		QVector<Vector2> pnts_proj;

		// chains under master
		QVector<Vector3> pnts;
		foreach (QSet<int> cids, masterUnderChainsMap) {
			foreach (int cid, cids) {
				pnts << chains[cid]->getMaxFoldRegion(true).getConners();
				pnts << chains[cid]->getMaxFoldRegion(false).getConners();
			}
		}
		foreach(Vector3 p, pnts) pnts_proj << base_rect.getProjCoordinates(p);

		// top master
		pnts_proj << minFoldingRegion[top_master->mID].getConners();

		// 2D AABB
		Geom::Rectangle2 max_region = computeAABB2D(pnts_proj);

		// crop by shape AABB
		int aid = shapeAABB.getAxisId(base_rect.Normal);
		Geom::Rectangle cropper3 = shapeAABB.getPatch(aid, 0);
		Geom::Rectangle2 cropper2 = base_rect.get2DRectangle(cropper3);
		max_region.cropByAxisAlignedRectangle(cropper2);

		// max folding region
		maxFoldingRegion[top_master->mID] = max_region;

		//addDebugSegments(base_rect.get3DRectangle(max_region).getEdgeSegments());
	}
}

void BlockGraph::computeAvailFoldingRegion( FdGraph* scaffold, 
	QMultiMap<QString, QString>& moc_greater, QMultiMap<QString, QString>& moc_less )
{
	// compute the offset
	Vector3 posA = baseMaster->center();
	Vector3 posB = ((FdNode*)scaffold->getNode(baseMaster->mID))->center();
	Vector3 offset = posA - posB;

	// align scaffold with this block
	scaffold->translate(offset, false);

	// extent 
	QString base_mid = baseMaster->mID;
	Geom::Rectangle base_rect = baseMaster->mPatch;
	QVector<QString> allMasterIds = getAllMasterIds(scaffold);
	foreach (PatchNode* top_master, masters)
	{
		// skip base master
		if (top_master == baseMaster) continue;
		
		// constraint parts: in-between and unordered
		QString top_mid = top_master->mID;
		QVector<QString> constraintParts;
		constraintParts << getInbetweenOutsideParts(scaffold, base_mid, top_mid);

		QList<QString> moc_base;
		moc_base << moc_greater.values(base_mid) << moc_less.values(base_mid) << base_mid;
		QList<QString> moc_top;
		moc_top << moc_greater.values(top_mid) << moc_less.values(top_mid) << top_mid;
		foreach (QString mid, allMasterIds) // no order with both
			if (!moc_base.contains(mid) && !moc_top.contains(mid))
				constraintParts << mid;

		// samples from constraint parts
		QVector<Vector3> samples;
		int nbs = 10;
		foreach(QString nid, constraintParts)
		{
			FdNode* n = (FdNode*)scaffold->getNode(nid);
			if (n->mType == FdNode::PATCH)
				samples << ((PatchNode*)n)->mPatch.getEdgeSamples(nbs);
			else
				samples << ((RodNode*)n)->mRod.getUniformSamples(nbs);
		}

		// projection on base_rect
		QVector<Vector2> samples_proj;
		foreach (Vector3 s, samples)
			samples_proj << base_rect.getProjCoordinates(s);

		// max folding region
		samples_proj << maxFoldingRegion[top_mid].getEdgeSamples(nbs);

		// include minFR while exclude all constraint points
		Geom::Rectangle2 avail_region = minFoldingRegion[top_mid];
		extendRectangle2D(avail_region, samples_proj);

		// avail folding region
		availFoldingRegion[top_mid] = avail_region;

		//addDebugSegments(base_rect.get3DRectangle(avail_region).getEdgeSegments());
	}

	// restore the position of scaffold
	scaffold->translate(-offset, false);
}

void BlockGraph::exportCollFOG()
{
	QString filePath = path + "/" + mID + "_collision";
	collFogOrig->saveAsImage(filePath + "_Orig");
	collFog->saveAsImage(filePath);
}

FdGraph* BlockGraph::getKeyframeScaffold( double t )
{
	// scaffolds from folded chains
	QVector<FdGraph*> foldedChains;
	for (int i = 0; i < chains.size(); i++)
	{
		double localT = getLocalTime(t, chains[i]->mFoldDuration);
		foldedChains << chains[i]->getKeyframeScaffold(localT);
	}

	// combine 
	FdGraph* keyframeScaffold = combineDecomposition(foldedChains, baseMaster->mID, masterChainsMap);

	// delete folded chains
	foreach (FdGraph* c, foldedChains) delete c;

	// debug
	//addDebugScaffold(keyframeScaffold);

	return keyframeScaffold;
}

bool BlockGraph::fAreasIntersect( Geom::Rectangle& rect1, Geom::Rectangle& rect2 )
{
	Geom::Rectangle base_rect = baseMaster->mPatch;

	Geom::Rectangle2 r1 = base_rect.get2DRectangle(rect1);
	Geom::Rectangle2 r2 = base_rect.get2DRectangle(rect2);

	return Geom::IntrRect2Rect2::test(r1, r2);
}

void BlockGraph::foldabilize()
{
	// collision graph
	buildCollisionGraph();

	// find optimal solution
	findOptimalSolution();

	// apply fold options
	for (int i = 0; i < chains.size(); i++)
	{
		chains[i]->applyFoldOption(foldSolution[i]);
	}
}

void BlockGraph::buildCollisionGraph()
{
	// clear
	collFog->clear();

	// fold entities and options
	for(int i = 0; i < chains.size(); i++)
	{
		HChain* chain = (HChain*)chains[i];

		// available folding space
		PatchNode* top_master = chain->getTopMaster();
		Geom::Box afs = getAvailFoldingSpace(top_master->mID); 

		// fold entity
		FoldEntity* cn = new FoldEntity(i, chain->mID);
		collFog->addNode(cn);

		// debug
		properties.remove("debugSegments");

		// fold options and links
		foreach(FoldOption* fn, chain->generateFoldOptions())
		{
			Geom::Rectangle fArea = fn->properties["fArea"].value<Geom::Rectangle>();

			// reject if stick out of AFS
			if (!afs.containsAll(fArea.getConners()))
				continue;

			// reject if collide with other masters
			// whose time stamp is within the time interval of fn
			bool reject = false;
			foreach (QString mid, masterTimeStamps.keys())
			{
				double mstamp = masterTimeStamps[mid];
				if (!within(mstamp, chain->mFoldDuration)) continue;

				Geom::Rectangle m_rect = ((PatchNode*)getNode(mid))->mPatch;
				if (fAreasIntersect(fArea, m_rect))
				{
					reject = true;
					break;
				}
			}
			if (reject) continue;

			// accept
			collFog->addNode(fn);
			collFog->addFoldLink(cn, fn);

			addDebugSegments(fArea.getEdgeSegments());
		}
	}

	// collision links
	QVector<FoldOption*> fns = collFog->getAllFoldOptions();
	for (int i = 0; i < fns.size(); i++)
	{
		FoldOption* fn = fns[i];
		FoldEntity* cn = collFog->getFoldEntity(fn->mID);
		Geom::Rectangle fArea = fn->properties["fArea"].value<Geom::Rectangle>();
		TimeInterval tInterval = chains[cn->entityIdx]->mFoldDuration;

		// with other fold options
		for (int j = i+1; j < fns.size(); j++)
		{
			FoldOption* other_fn = fns[j];

			// skip siblings
			if (collFog->areSiblings(fn->mID, other_fn->mID)) continue; 

			// skip if time interval don't overlap
			FoldEntity* other_cn = collFog->getFoldEntity(other_fn->mID);
			TimeInterval other_tInterval = chains[other_cn->entityIdx]->mFoldDuration;
			if (!overlap(tInterval, other_tInterval)) continue;

			// collision test using fold region
			Geom::Rectangle other_fArea = other_fn->properties["fArea"].value<Geom::Rectangle>();
			if (fAreasIntersect(fArea, other_fArea))
			{
				collFog->addCollisionLink(fn, other_fn);
			}
		}
	}

	// debug
	std::cout << "# fold options / collision graph size = " 
		<< collFog->getAllFoldOptions().size() << std::endl;
}

void BlockGraph::findOptimalSolution()
{
	// get all folding nodes
	QVector<FoldOption*> fns = collFog->getAllFoldOptions();

	// the dual adjacent matrix
	QVector<bool> dumpy(fns.size(), true);
	QVector< QVector<bool> > conn(fns.size(), dumpy);
	for (int i = 0; i < fns.size(); i++){
		// the i-th node
		FoldOption* fn = fns[i];

		// diagonal entry
		conn[i][i] = false;

		// other fold options
		for (int j = i+1; j < fns.size(); j++){
			// the j-th node
			FoldOption* other_fn = fns[j];

			// connect siblings and colliding folding options
			if (collFog->areSiblings(fn->mID, other_fn->mID) ||
				collFog->verifyLinkType(fn->mID, other_fn->mID, "collision")){
					conn[i][j] = false;	conn[j][i] = false;
			}
		}
	}

	// find minimum cost maximum cliques
	QVector<double> weights;
	foreach(FoldOption* fn, fns) weights.push_back(fn->getCost());
	CliquerAdapter cliquer(conn, weights);
	QVector<QVector<int> > qs = cliquer.getMinWeightMaxCliques();

	// fold solution
	foldSolution.clear();
	foldSolution.resize(chains.size());
	if (collFogOrig) delete collFogOrig;
	collFogOrig = (FoldOptionGraph*)collFog->clone();
	foreach(int idx, qs[0])
	{
		FoldOption* fn = fns[idx];
		fn->addTag(SELECTED_TAG);
		FoldEntity* cn = collFog->getFoldEntity(fn->mID);
		foldSolution[cn->entityIdx] = fn;
	}
}

double BlockGraph::getTimeLength()
{
	return nbMasters(this) - 1;
}

QVector<QString> BlockGraph::getInbetweenOutsideParts( FdGraph* scaffold, QString mid1, QString mid2 )
{
	// time line
	Vector3 sqzV = baseMaster->mPatch.Normal;
	Geom::Line timeLine(Vector3(0, 0, 0), sqzV);

	// position on time line
	FdNode* master1 = (FdNode*)getNode(mid1);
	FdNode* master2 = (FdNode*)getNode(mid2);
	double t0 = timeLine.getProjTime(master1->center());
	double t1 = timeLine.getProjTime(master2->center());
	if (t0 > t1) std::swap(t0, t1);
	TimeInterval m1m2 = TIME_INTERVAL(t0, t1);

	// find parts in between m1 and m2
	QVector<QString> inbetweens;
	foreach (FdNode* n, scaffold->getFdNodes())
	{
		// skip parts in this block
		if (containsNode(n->mID)) continue;

		// master
		if (n->hasTag(IS_MASTER))
		{
			double t = timeLine.getProjTime(n->center());
			
			if (within(t, m1m2)) 
				inbetweens << n->mID;
		}
		// slave
		else
		{
			int aid = n->mBox.getAxisId(sqzV);
			Geom::Segment sklt = n->mBox.getSkeleton(aid);
			double t0 = timeLine.getProjTime(sklt.P0);
			double t1 = timeLine.getProjTime(sklt.P1);
			if (t0 > t1) std::swap(t0, t1);
			TimeInterval ti = TIME_INTERVAL(t0+ZERO_TOLERANCE_LOW, t1-ZERO_TOLERANCE_LOW);
			
			if (overlap(ti, m1m2))
				inbetweens << n->mID;
		}
	}

	return inbetweens;
}

double BlockGraph::getAvailFoldingVolume()
{
	Geom::Rectangle base_rect = baseMaster->mPatch;
	QList<QString> mids = availFoldingRegion.keys();

	// trivial case
	if (availFoldingRegion.size() == 1)
	{
		QString mid = mids.front();
		Geom::Rectangle afr3 = base_rect.get3DRectangle(availFoldingRegion[mid]);
		return afr3.area() * masterHeight[mid];
	}

	// AABB of avail folding regions
	QVector<Vector2> conners;
	foreach (QString key, availFoldingRegion.keys())
		conners << availFoldingRegion[key].getConners();
	Geom::Rectangle2 aabb2 = computeAABB2D(conners);

	// pixelize folding region
	Geom::Rectangle aabb3 = base_rect.get3DRectangle(aabb2);
	double pixel_size = aabb3.Extent[0] / 100;
	double pixel_area = pixel_size * pixel_size;
	double AFV = 0;
	foreach (Vector3 gp3, aabb3.getGridSamples(pixel_size))
	{
		Vector2 gp2 = base_rect.getProjCoordinates(gp3);

		double best_height = 0;
		foreach (QString mid, mids)
			if (availFoldingRegion[mid].contains(gp2) && masterHeight[mid] > best_height)
				best_height = masterHeight[mid];

		AFV += best_height;
	}
	AFV *= pixel_area;

	return AFV;
}

Geom::Box BlockGraph::getAvailFoldingSpace( QString mid )
{
	Geom::Rectangle base_rect = baseMaster->mPatch;
	Geom::Rectangle afr3 = base_rect.get3DRectangle(availFoldingRegion[mid]);
	return Geom::Box(afr3, afr3.Normal, masterHeight[mid]);
}
