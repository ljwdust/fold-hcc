#include "HUnitScaffold.h"
#include "Numeric.h"
#include "HChainScaffold.h"
#include "CliquerAdapter.h"
#include "IntrRect2Rect2.h"
#include "SuperUnitScaffold.h"
#include "FoldOptionGraph.h"

HUnitScaffold::HUnitScaffold(QString id, QVector<PatchNode*>& ms, QVector<ScaffoldNode*>& ss,
	QVector< QVector<QString> >& mPairs) : UnitScaffold(id)
{
	// clone nodes
	foreach(PatchNode* m, ms)	{
		masters << (PatchNode*)m->clone();
		Structure::Graph::addNode(masters.last());
	}
	foreach(ScaffoldNode* s, ss)
		Structure::Graph::addNode(s->clone());

	// sort masters
	masterTimeStamps = getTimeStampsNormalized(masters, masters.front()->mPatch.Normal, timeScale);
	QMultiMap<double, QString> timeMasterMap;
	foreach(PatchNode* m, masters)
	{
		// base master is the bottom one
		if (masterTimeStamps[m->mID] < ZERO_TOLERANCE_LOW)
			baseMaster = m;

		// time to master
		timeMasterMap.insert(masterTimeStamps[m->mID], m->mID);
	}
	sortedMasters = timeMasterMap.values().toVector();

	// create chains
	createChains(ss, mPairs);

	// compute chain weights
	computeChainWeights();

	// generate all fold options
	genAllFoldOptions();
}

void HUnitScaffold::createChains(QVector<ScaffoldNode*>& ss, QVector< QVector<QString> >& mPairs)
{
	// create chain for each slave with its two masters
	for (int i = 0; i < ss.size(); i++)
	{
		QString mid_low = mPairs[i].front();
		QString mid_high = mPairs[i].last();
		if (masterTimeStamps[mid_low] > masterTimeStamps[mid_high]){
			mid_low = mPairs[i].last();
			mid_high = mPairs[i].front();
		}

		// create chain
		PatchNode* master_low = (PatchNode*)getNode(mid_low);
		PatchNode* master_high = (PatchNode*)getNode(mid_high);
		ChainScaffold* hc = new HChainScaffold(ss[i], master_low, master_high);
		double t0 = 1.0 - masterTimeStamps[mid_high];
		double t1 = 1.0 - masterTimeStamps[mid_low];
		hc->setFoldDuration(t0, t1);
		chains << hc;

		// map from master id to chain idx set
		masterChainsMap[mid_low] << i;
		masterChainsMap[mid_high] << i;

		// map from chain index to top master
		chainTopMasterMap[i] = mid_high;
	}
}

void HUnitScaffold::computeChainWeights()
{
	chainWeights.clear();

	double totalA = 0;
	for(auto c : chains)
	{
		double area = c->getArea();
		chainWeights << area;
		totalA += area;
	}

	for (auto& cw : chainWeights) cw /= totalA;
}


HUnitScaffold::~HUnitScaffold()
{
}

// The keyframe is the configuration of the block at given time \p t
// This is also called regular keyframe to distinguish from super keyframe
Scaffold* HUnitScaffold::getKeyframe(double t, bool useThk)
{
	Scaffold* keyframe = nullptr;

	// the block is not ready to fold
	if (t <= 0)
	{
		keyframe = (Scaffold*)this->clone();
	}
	// chains have been created and ready to fold
	// IOW, the block has been foldabilized
	else
	{
		// keyframe of each chain
		QVector<Scaffold*> chainKeyframes;
		for (int i = 0; i < chains.size(); i++)
		{
			// skip deleted chain
			if (chains[i]->isDeleted)
				chainKeyframes << nullptr;
			else{
				ChainScaffold* cs = chains[i];
				double localT = cs->duration.getLocalTime(t);
				chainKeyframes << chains[i]->getKeyframe(localT, useThk);
			}
		}

		// combine 
		keyframe = combineScaffolds(chainKeyframes, baseMaster->mID, masterChainsMap);

		// thickness of masters
		if (useThk){
			foreach(PatchNode* m, getAllMasters(keyframe))
				m->setThickness(thickness);
		}

		// local garbage collection
		foreach(Scaffold* c, chainKeyframes)
		if (c) delete c;
	}

	// keyframe == nullptr only if this block has not been foldabilized but t > 0
	// should never happen

	// the key frame
	return keyframe;
}

void HUnitScaffold::computeObstacles(ShapeSuperKeyframe* ssKeyframe)
{
	// create super block
	SuperUnitScaffold *superBlock = new SuperUnitScaffold(this, ssKeyframe);

	// request from super block
	obstacles = superBlock->computeObstacles();

	// garbage collection
	delete superBlock;
}

QVector<int> HUnitScaffold::getAvailFoldOptions(ShapeSuperKeyframe* ssKeyframe)
{
	// update obstacles
	computeObstacles(ssKeyframe);
	obstaclePnts << getObstaclePoints(); // store obstacles

	// prune fold options
	QVector<int> afo;
	
	for (int i = 0; i < allFoldOptions.size(); i++)
	{
		// the fold option
		FoldOption* fo = allFoldOptions[i];

		// always accept the delete fold option
		// check acceptance for regular fold option
		bool accepted = true;
		if (fo->scale != 0)
		{
			// the obstacles
			QString top_mid = chainTopMasterMap[fo->chainIdx];
			QVector<Vector2> &obs = obstacles[top_mid];

			// prune
			bool isColliding = fo->regionProj.containsAny(obs, -0.01);
			bool inAABB = aabbConstraint.containsAll(fo->regionProj.getConners(), 0.01);
			accepted = !isColliding && inAABB;
		}

		// store
		if (accepted) afo << i;
	}

	// result
	return afo;
}

FoldOptionGraph* HUnitScaffold::createCollisionGraph(const QVector<int>& afo)
{
	FoldOptionGraph* collFog = new FoldOptionGraph();

	// chain nodes
	QVector<ChainNode*> chainNodes;
	for (int ci = 0; ci < chains.size(); ci++)
	{
		ChainNode* cn = new ChainNode(ci, chains[ci]->mID);
		collFog->addNode(cn);
		chainNodes << cn;
	}

	// fold option nodes and fold edges
	QVector<FoldOption*> fos;
	for (auto fi: afo)
	{
		FoldOption* fo = (FoldOption*)allFoldOptions[fi]->clone();
		fos << fo;
		collFog->addNode(fo);

		ChainNode* cn = chainNodes[fo->chainIdx];
		collFog->addFoldLink(cn, fo);
	}
	
	// collision edges
	for (int i = 0; i < fos.size(); i++)
	{
		// skip delete fold option
		FoldOption* foi = fos[i];
		if (foi->scale == 0) continue;

		// collision with others
		for (int j = i + 1; j < afo.size(); j++)
		{
			// skip delete fold option
			FoldOption* foj = fos[j];
			if (foj->scale == 0) continue;

			// skip siblings
			if (foi->chainIdx == foj->chainIdx) continue;

			// skip if time interval don't overlap
			if (!foi->duration.overlaps(foj->duration)) continue;

			// collision test using fold region
			if (Geom::IntrRect2Rect2::test(foi->regionProj, foj->regionProj))
				collFog->addCollisionLink(foi, foj);
		}
	}

	return collFog;
}

double HUnitScaffold::findOptimalSolution(const QVector<int>& afo)
{
	// total cost
	double totalCost = 0;

	// solution
	QVector<FoldOption*> solution;
	for (int i = 0; i < chains.size(); i++) solution << nullptr;

	// optimal solution on each component
	FoldOptionGraph* collFog = createCollisionGraph(afo);
	for (auto component : collFog->getComponents()) {
		for (auto fo: findOptimalSolution(collFog, component))
		{
			solution[fo->chainIdx] = fo;
			totalCost += computeCost(fo);
		}
	}

	// store the solution
	testedAvailFoldOptions << afo;
	foldSolutions << solution;
	foldCost << totalCost;

	// update the current solution
	currSlnIdx = foldSolutions.size() - 1;

	// garbage collection
	delete collFog;

	// returns the cost
	return totalCost;
}

QVector<FoldOption*> HUnitScaffold::findOptimalSolution(FoldOptionGraph* collFog, const QVector<Structure::Node*>& component)
{
	QVector<FoldOption*> partialSln;

	// all fold options in the component
	// these fold options are from allFoldOptions
	QVector<FoldOption*> fns;
	int nbCn = 0;
	for (auto n : component) 
	{
		if (n->properties["type"].toString() == "option")
		{
			int idx = ((FoldOption*)n)->index;
			fns << allFoldOptions[idx];
		}
		else nbCn++;
	}

	// ***single chain: choose the one with lowest cost
	if (nbCn == 1)
	{
		FoldOption* best_fo = nullptr;
		double minCost = maxDouble();
		for (auto fo : fns)	{
			double cost = computeCost(fo);
			if (cost < minCost){
				minCost = cost;	best_fo = fo;
			}
		}
		partialSln << best_fo;
	}else
	// ***multiple chains: MIS
	{
		// the dual adjacent matrix and weights for each fold option
		QVector< QVector<bool> > conn = genDualAdjMatrix(collFog, fns);
		QVector<double> weights = genReversedWeights(fns);

		// find maximum weighted clique
		CliquerAdapter cliquer(conn, weights);
		QVector<QVector<int> > qs = cliquer.getMaxWeightedCliques();

		// there might be multiple solution, we simply use the first one
		foreach(int idx, qs.front())
			partialSln << fns[idx];
	}

	return partialSln;
}

QVector< QVector<bool> > HUnitScaffold::genDualAdjMatrix(FoldOptionGraph* collFog, const QVector<FoldOption*>& fns)
{
	QVector<bool> dumpy(fns.size(), true);
	QVector< QVector<bool> > conn(fns.size(), dumpy);
	for (int i = 0; i < fns.size(); i++)
	{
		// the i-th node
		FoldOption* fn = fns[i];

		// diagonal entry
		conn[i][i] = false;

		// other fold options
		for (int j = i + 1; j < fns.size(); j++)
		{
			// the j-th node
			FoldOption* other_fn = fns[j];

			// connect siblings and colliding folding options
			if ((fn->chainIdx == other_fn->chainIdx) ||
				collFog->verifyLinkType(fn->mID, other_fn->mID, "collision")){
				conn[i][j] = false;	conn[j][i] = false;
			}
		}
	}

	return conn;
}

// max weights means lowest cost: weights = maxCost - cost
QVector<double> HUnitScaffold::genReversedWeights(const QVector<FoldOption*>& fns)
{
	QVector<double> weights;

	double maxCost = 0;
	QVector<double> costs;
	foreach(FoldOption* fn, fns)
	{
		double cost = computeCost(fn);
		costs << cost;
		if (cost > maxCost) maxCost = cost;
	}
	maxCost += 1;

	foreach(double cost, costs) 
		weights.push_back(maxCost - cost);

	return weights;
}

QVector<Vector3> HUnitScaffold::getObstaclePoints()
{
	QVector<Vector3> pnts;
	Geom::Rectangle base_rect = baseMaster->mPatch;
	for (QString mid : obstacles.keys())
	for (Vector2 p : obstacles[mid])
		pnts << base_rect.getPosition(p);

	return pnts;
}