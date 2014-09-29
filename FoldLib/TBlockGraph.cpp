#include "TBlockGraph.h"
#include "TChainGraph.h"
#include "Numeric.h"

TBlockGraph::TBlockGraph(QString id, QVector<PatchNode*>& ms, QVector<FdNode*>& ss,
	QVector< QVector<QString> >& mPairs, Geom::Box shape_aabb)
	:BlockGraph(id, shape_aabb)
{
	// clone nodes
	foreach(PatchNode* m, ms)	{
		masters << (PatchNode*)m->clone();
		Structure::Graph::addNode(masters.last());
	}
	foreach(FdNode* s, ss)
		Structure::Graph::addNode(s->clone());

	// the base and virtual top masters
	bool isVirtualFirst = masters.front()->hasTag(EDGE_ROD_TAG);
	topMaster = isVirtualFirst? masters.front() : masters.last();
	baseMaster = isVirtualFirst ? masters.last() : masters.front();

	// make sure the normal of base master pointing to the same side as the chain part
	Vector3 chain2base = (ss.front()->center() - baseMaster->center()).normalized();
	if (dot(baseMaster->mPatch.Normal, chain2base) < 0)
		baseMaster->mPatch.flipNormal();

	// the chain
	tChain = new TChainGraph(ss.front(), baseMaster, topMaster);
	tChain->setFoldDuration(0, 1);
	chains << tChain;

	// the chain weights
	chainWeights.clear();
	chainWeights << 1.0;
}

FdGraph* TBlockGraph::getKeyframe(double t, bool useThk)
{
	FdGraph* keyframe = nullptr;

	// chains have been created and ready to fold
	// IOW, the block has been foldabilized
	if (foldabilized)
	{
		keyframe = tChain->getKeyframe(t, useThk);
	}
	// the block is not ready
	// can only answer request on t = 0 and t = 1
	else
	{
		// t = 0 : the entire block
		if (t < 0.5)
			keyframe = (FdGraph*)tChain->clone();
		else
		// t = 1 : just the base master
		{
			keyframe = new FdGraph();
			keyframe->Structure::Graph::addNode(baseMaster->clone());
		}
	}

	return keyframe;
}

QVector<Vector2> TBlockGraph::computeObstacles(ShapeSuperKeyframe* ssKeyframe)
{
	// in-between external parts
	Geom::Rectangle base_rect = baseMaster->mPatch;
	QVector<QString> obstacleParts = getInbetweenExternalParts(baseMaster->center(), topMaster->center(), ssKeyframe);

	// sample obstacle parts
	QVector<Vector3> samples;
	for (auto nid : obstacleParts)
		samples << ssKeyframe->getFdNode(nid)->sampleBoundabyOfScaffold(100);

	// projection
	obstacles.clear();
	for (auto s : samples)
		obstacles << base_rect.getProjCoordinates(s);

	return obstacles;
}


QVector<int> TBlockGraph::getAvailFoldOptions(ShapeSuperKeyframe* ssKeyframe)
{
	QVector<Vector2> obstacles = computeObstacles(ssKeyframe);

	// prune fold options
	QVector<int> afo;
	for (int i = 0; i < allFoldOptions.size(); i++)
	{
		// the fold option
		FoldOption* fo = allFoldOptions[i];

		// prune
		if (!fo->regionProj.containsAny(obstacles, -0.01))
			afo << i;
	}

	// result
	return afo;
}

double TBlockGraph::findOptimalSolution(const QVector<int>& afo)
{
	// choose the one with the lowest cost
	FoldOption* best_fo = nullptr;
	double minCost = maxDouble();
	for(auto fi : afo){
		FoldOption* fo = allFoldOptions[fi];
		double cost = computeCost(fo);
		if (cost < minCost)
		{
			best_fo = fo;
			minCost = cost;
		}
	}

	// store the solution
	testedAvailFoldOptions << afo;
	foldSolutions << (QVector<FoldOption*>() << best_fo);

	// update the current solution
	currSlnIdx = foldSolutions.size();

	// return the cost
	return minCost;
}