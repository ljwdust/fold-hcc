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
	masterTimeStamps = getTimeStampsNormalized(masters, masters.front()->mPatch.Normal);

	// base master is the bottom one
	double minT = maxDouble();
	foreach(PatchNode* m, masters)
	{
		if (masterTimeStamps[m->mID] < minT)
		{
			baseMaster = m;
			minT = masterTimeStamps[m->mID];
		}
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

void BlockGraph::computeMinFoldingVolume()
{
	minFoldingVolume.clear();

	Geom::Rectangle base_rect = baseMaster->mPatch;
	foreach (PatchNode* top_master, masters)
	{
		// skip base master
		if (top_master == baseMaster) continue;

		// rect and projection
		Geom::Rectangle top_rect = top_master->mPatch;
		Geom::Rectangle top_rect_proj = base_rect.getProjection(top_rect);

		// folding region and volume
		double height = (top_rect.Center - top_rect_proj.Center).norm();
		minFoldingRegion[top_master->mID] = top_rect_proj;
		minFoldingVolume[top_master->mID] = Geom::Box(top_rect_proj, base_rect.Normal, height);
	}

	// debug
	//addDebugBoxes(minFoldingVolume.values().toVector());
}

void BlockGraph::computeMaxFoldingVolume(Geom::Box shapeAABB)
{
	minFoldingVolume.clear();

	Geom::Rectangle base_rect = baseMaster->mPatch;
	foreach (PatchNode* top_master, masters)
	{
		// skip base master
		if (top_master == baseMaster) continue;

		// conner points of fold regions and projected master rect
		QVector<Vector3> pnts;

		// master rect and projection
		Geom::Rectangle mrect = top_master->mPatch;
		Geom::Rectangle mrect_proj = base_rect.getProjection(mrect);
		pnts << mrect_proj.getConners();

		// chains attaching to master
		foreach (QSet<int> cids, masterUnderChainsMap)
		{
			foreach (int cid, cids)
			{
				pnts << chains[cid]->getMaxFoldRegion(true).getConners();
				pnts << chains[cid]->getMaxFoldRegion(false).getConners();
			}
		}

		// minimal enclosing rectangle on the projection plane
		QVector<Vector2> pnts_proj;
		foreach(Vector3 p, pnts) pnts_proj << base_rect.getProjCoordinates(p);
		Geom::Rectangle2 max_rect2 = getMinEnclosingRectangle2D(pnts_proj);
		Geom::Rectangle max_rect = base_rect.get3DRectangle(max_rect2);

		// folding volume
		double height = (mrect.Center - mrect_proj.Center).norm();
		Geom::Box fv(max_rect, base_rect.Normal, height);
		fv.cropByAxisAlignedBox(shapeAABB);
		maxFoldingVolume[top_master->mID] = fv;
	}

	// debug
	//addDebugBoxes(maxFoldingVolume.values().toVector());
}


QMap<QString, Geom::Box> BlockGraph::computeAvailFoldingVolume( FdGraph* scaffold, 
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

		// samples from max folding volume
		Geom::Box mfv = maxFoldingVolume[top_mid];
		mfv.scale(1 + ZERO_TOLERANCE_LOW);
		samples << mfv.getEdgeSamples(nbs);

		//addDebugPoints(samples);

		// projection on base_rect
		QVector<Vector2> samples_proj;
		foreach (Vector3 s, samples)
			samples_proj << base_rect.getProjCoordinates(s);

		// debug
		QVector<Vector3> samples2D;
		foreach (Vector2 sp, samples_proj)
			samples2D << base_rect.getPosition(sp);

		//addDebugPoints(samples2D);

		// max rect including minFR while excluding all constraint points
		Geom::Rectangle2 minFR = base_rect.get2DRectangle(minFoldingRegion[top_mid]);
		Geom::Rectangle2 avail_rect = RRRRR(minFR, samples_proj);
		Geom::Rectangle avail_rect_3D = base_rect.get3DRectangle(avail_rect);

		//addDebugSegments(avail_rect_3D.getEdgeSegments());

		// folding volume
		// rect and projection
		Geom::Rectangle top_rect = top_master->mPatch;
		Geom::Rectangle top_rect_proj = base_rect.getProjection(top_rect);
		double height = (top_rect.Center- top_rect_proj.Center).norm();
		availFoldingVolume[top_mid] = Geom::Box(avail_rect_3D, base_rect.Normal, height);
	}

	// restore the position of scaffold
	scaffold->translate(-offset, false);

	return availFoldingVolume;
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

		// fold entity
		FoldEntity* cn = new FoldEntity(i, chain->mID);
		collFog->addNode(cn);

		properties.remove("debugSegments");// debug

		// fold options and links
		foreach(FoldOption* fn, chain->generateFoldOptions())
		{
			Geom::Rectangle fArea = fn->properties["fArea"].value<Geom::Rectangle>();

			bool reject = false;

			// reject if stick out of EFV
			foreach (Geom::Box efv, availFoldingVolume.values())
			{
				if (!efv.containsAll(fArea.getConners()))
				{
					reject = true;
					continue;
				}
			}
			if (reject) continue;

			// reject if collide with other masters
			// whose time stamp is within the time interval of fn
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

Geom::Rectangle2 BlockGraph::RRRRR( Geom::Rectangle2 & seed, QVector<Vector2> &pnts )
{
	Geom::Rectangle base_rect = baseMaster->mPatch;

	Geom::Rectangle seed3 = base_rect.get3DRectangle(seed);
	addDebugSegments(seed3.getEdgeSegments());

	QVector<Vector3> pnts3;
	foreach (Vector2 p2, pnts) pnts3 << base_rect.getPosition(p2);
	addDebugPoints(pnts3);



	Geom::Rectangle2 seed_copy = seed;
	// shrink seed rect by epsilon to avoid pnts on edges
	seed_copy.Extent *= 0.5;

	// do nothing if seed rect contains any pnts
	foreach (Vector2 p, pnts) 
		if (seed_copy.contains(p)) 
			return seed_copy;

	// coordinates in the frame of seed rect
	QVector<Vector2> pnts_coord;
	foreach (Vector2 p, pnts) pnts_coord << seed.getCoordinates(p);

	// extend along x
	double left = -maxDouble();
	double right = maxDouble();
	foreach (Vector2 pc, pnts_coord)
	{
		// keep the extent along y
		if (inRange(pc.y(), -1, 1))
		{
			double x = pc.x();
			// tightest bound on left
			if (x < 0 && x > left) left = x;
			// tightest bound on right
			if (x > 0 && x < right) right = x;
		}
	}

	// extend along y
	double bottom = -maxDouble();
	double top = maxDouble();
	foreach (Vector2 pc, pnts_coord)
	{
		// keep the extent along x
		if (inRange(pc.x(), left, right))
		{
			double y = pc.y();
			// tightest bound on left
			if (y < 0 && y > bottom) bottom = y;
			// tightest bound on right
			if (y > 0 && y < top) top = y;
		}
	}

	// set up box
	QVector<Vector2> new_conners;
	new_conners << seed.getPosition(Vector2(left, bottom))
		<< seed.getPosition(Vector2(right, bottom))
		<< seed.getPosition(Vector2(right, top))
		<< seed.getPosition(Vector2(left, top));
	return Geom::Rectangle2(new_conners);

}
