#include "UnitScaff.h"
#include "FdUtility.h"
#include "Numeric.h"
#include "ChainScaff.h"
#include "TChainScaff.h"

UnitScaff::UnitScaff(QString id)
	: Scaffold(id)
{
	// selected chain
	selChainIdx = -1;

	// parameters for split and shrink
	maxNbSplits = 1;
	maxNbChunks = 2;

	// thickness
	thickness = 2;
	useThickness = false;

	// cost weight
	weight = 0.05;
	
	// current fold solution
	currSlnIdx = -1;

	// importance 
	importance = 0;
}

UnitScaff::~UnitScaff()
{
	foreach(ChainScaff* c, chains)
		delete c;
}

void UnitScaff::setAabbConstraint(Geom::Box aabb)
{
	Geom::Rectangle base_rect = baseMaster->mPatch;
	int aid = aabb.getAxisId(base_rect.Normal);
	Geom::Rectangle cs_rect = aabb.getCrossSection(aid, 0);

	aabbConstraint = base_rect.get2DRectangle(cs_rect);
}


Scaffold* UnitScaff::activeScaffold()
{
	Scaffold* selChain = getSelChain();
	if (selChain)  return selChain;
	else		   return this;
}

ChainScaff* UnitScaff::getSelChain()
{
	if (selChainIdx >= 0 && selChainIdx < chains.size())
		return chains[selChainIdx];
	else
		return nullptr;
}

void UnitScaff::selectChain( QString id )
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

QStringList UnitScaff::getChainLabels()
{
	QStringList labels;
	foreach(Scaffold* c, chains)
		labels.push_back(c->mID);

	// append string to select none
	labels << "--none--";

	return labels;
}

// The super keyframe is the keyframe + superPatch
// which is an additional patch representing the folded block
Scaffold* UnitScaff::getSuperKeyframe( double t )
{
	// regular key frame w\o thickness
	Scaffold* keyframe = getKeyframe(t, false);

	// do nothing if the block is NOT fully folded
	if (t < 1) return keyframe;

	// create super patch
	PatchNode* superPatch = (PatchNode*)baseMaster->clone();
	superPatch->mID = mID + "_super";
	superPatch->addTag(SUPER_MASTER_TAG); 

	// collect projections of all nodes (including baseMaster) on baseMaster
	Geom::Rectangle base_rect = superPatch->mPatch;
	QVector<Vector2> projPnts2 = base_rect.get2DConners();
	foreach(ScaffNode* n, keyframe->getScaffNodes())
	{
		if (n->mType == ScaffNode::PATCH)
		{
			Geom::Rectangle part_rect = ((PatchNode*)n)->mPatch;
			projPnts2 << base_rect.get2DRectangle(part_rect).getConners();
		}
		else
		{
			Geom::Segment part_rod = ((RodNode*)n)->mRod;
			projPnts2 << base_rect.getProjCoordinates(part_rod.P0);
			projPnts2 << base_rect.getProjCoordinates(part_rod.P1);
		}
	}

	// resize super patch
	Geom::Rectangle2 aabb2 = Geom::Rectangle2::computeAABB(projPnts2);
	superPatch->resize(aabb2);

	// merged parts and masters
	foreach (ScaffNode* n, keyframe->getScaffNodes())
	{
		n->addTag(MERGED_PART_TAG); 
		if (n->hasTag(MASTER_TAG))
			superPatch->appendToSetProperty<QString>(MERGED_MASTERS, n->mID);
	}

	// add super patch to keyframe
	keyframe->Structure::Graph::addNode(superPatch);

	return keyframe;
}

double UnitScaff::getNbTopMasters()
{
	return nbMasters(this) - 1;
}

void UnitScaff::setThickness( double thk )
{
	thickness = thk;
	foreach (ChainScaff* chain, chains)	
	{
		chain->halfThk = thickness / 2;
		chain->baseOffset = thickness / 2;
	}
}

QVector<QString> UnitScaff::getInbetweenExternalParts(Vector3 base_center, Vector3 top_center, ShapeSuperKeyframe* ssKeyframe)
{
	// time line
	Vector3 sqzV = baseMaster->mPatch.Normal;
	Geom::Line timeLine(Vector3(0, 0, 0), sqzV);

	// position on time line
	double t0 = timeLine.getProjTime(base_center);
	double t1 = timeLine.getProjTime(top_center);
	double epsilon = 0.05 * (t1 - t0);
	TimeInterval m1m2(t0 + epsilon, t1 - epsilon);

	// find parts in between m1 and m2
	QVector<QString> inbetweens;
	foreach(ScaffNode* n, ssKeyframe->getScaffNodes())
	{
		// skip parts that has been folded
		if (n->hasTag(MERGED_PART_TAG)) continue;

		// skip parts in this block
		if (containsNode(n->mID)) continue;

		// master
		if (n->hasTag(MASTER_TAG))
		{
			double t = timeLine.getProjTime(n->center());

			if (m1m2.contains(t)) 	inbetweens << n->mID;
		}
		else
			// slave		
		{
			int aid = n->mBox.getAxisId(sqzV);
			Geom::Segment sklt = n->mBox.getSkeleton(aid);
			double t0 = timeLine.getProjTime(sklt.P0);
			double t1 = timeLine.getProjTime(sklt.P1);
			if (t0 > t1) std::swap(t0, t1);
			TimeInterval ti(t0, t1);

			if (ti.overlaps(m1m2))	inbetweens << n->mID;
		}
	}

	return inbetweens;
}

void UnitScaff::genAllFoldOptions()
{
	// clear
	allFoldOptions.clear();

	// collect all
	Geom::Rectangle base_rect = baseMaster->mPatch;
	for (int i = 0; i < chains.size(); i++)
	{
		// regular option
		for (FoldOption* fo : chains[i]->genRegularFoldOptions(maxNbSplits, maxNbChunks))
		{
			// chain index
			fo->chainIdx = i;

			// region projection
			fo->regionProj = base_rect.get2DRectangle(fo->region);

			// store
			allFoldOptions << fo;

			// index
			fo->index = allFoldOptions.size() - 1;
		}

		// delete option
		FoldOption* dfo = chains[i]->genDeleteFoldOption(maxNbSplits);
		dfo->chainIdx = i;
		allFoldOptions << dfo;
		dfo->index = allFoldOptions.size() - 1;
	}
}

int UnitScaff::searchForExistedSolution(const QVector<int>& afo)
{
	for (int i = 0; i < testedAvailFoldOptions.size(); i++)
		if (testedAvailFoldOptions[i] == afo) return i;

	return -1;
}

double UnitScaff::foldabilizeWrt(ShapeSuperKeyframe* ssKeyframe)
{
	QVector<int> afo = getAvailFoldOptions(ssKeyframe);
	
	// search for existed solutions
	currSlnIdx = searchForExistedSolution(afo);
	if (currSlnIdx >= 0 && currSlnIdx < testedAvailFoldOptions.size())
		return foldCost[currSlnIdx];

	// find the optimal solution
	double cost = findOptimalSolution(afo);

	// return the cost (\in [0, 1])
	return cost;
}

void UnitScaff::applySolution()
{
	// assert idx
	if (currSlnIdx < 0 || currSlnIdx >= foldSolutions.size())
		return;

	// apply selected fold option to each chain
	for (int i = 0; i < chains.size(); i++)
	{
		FoldOption* fn = foldSolutions[currSlnIdx][i];
		chains[i]->applyFoldOption(fn);
	}
}

double UnitScaff::computeCost(FoldOption* fo)
{
	// split
	double a = fo->nSplits / (double)maxNbSplits;

	// shrinking
	double b = 1 - fo->scale;

	// blended cost : c \in [0, 1]
	double c = weight * a + (1 - weight) * b;

	// normalized
	double cost = chainWeights[fo->chainIdx] * c;

	// return
	return cost;
}

double UnitScaff::getFoldablePatchArea()
{
	double a = 0;
	for (auto c : chains)a += c->getArea();
	return a;
}

void UnitScaff::showObstaclesAndFoldOptions()
{
	// clear
	properties.remove(DEBUG_POINTS);
	properties.remove(DEBUG_RECTS);

	// assert idx
	if (currSlnIdx < 0 || currSlnIdx >= foldSolutions.size())
		return;

	// obstacles
	appendToVectorProperty(DEBUG_POINTS, obstaclePnts[currSlnIdx]);

	// available fold options
	QVector<Geom::Rectangle> rects;
	Geom::Rectangle base_rect = baseMaster->mPatch;
	for (int foi : testedAvailFoldOptions[currSlnIdx])
		rects << base_rect.get3DRectangle(allFoldOptions[foi]->regionProj);
	appendToVectorProperty(DEBUG_RECTS, rects);
}

void UnitScaff::resetAllFoldOptions()
{
	// regenerate all fold options
	genAllFoldOptions();

	// clear saved solutions
	testedAvailFoldOptions.clear();
	foldSolutions.clear();
	foldCost.clear();
	obstaclePnts.clear();
}

bool UnitScaff::hasFoldabilized()
{
	return mFoldDuration.hasPassed(1.0);
}

