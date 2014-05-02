#include "DcGraph.h"
#include "PatchNode.h"
#include "TBlock.h"
#include "HBlock.h"
#include <QFileInfo>
#include "FdUtility.h"


DcGraph::DcGraph( FdGraph* scaffold, StrArray2D masterGroups, QString id)
	: FdGraph(*scaffold) // clone the FdGraph
{
	path = QFileInfo(path).absolutePath();
	mID = id;

	// merge master groups into patches
	foreach( QVector<QString> masterGroup, masterGroups )
	{
		FdNode* mf = merge(masterGroup);
		mf->addTag(IS_MASTER);

		if (mf->mType != FdNode::PATCH)	changeNodeType(mf);
		masterPatches.push_back((PatchNode*)mf);
	}

	// create blocks
	createSlaves();
	clusterSlaves();
	createBlocks();

	// selection
	selBlockIdx = -1;
}

DcGraph::~DcGraph()
{
	foreach (BlockGraph* l, blocks)	delete l;
}

void DcGraph::computeSlaveMasterRelation()
{
	// slave parts
	slaves.clear();
	foreach (FdNode* n, getFdNodes())
		if (!n->hasTag(IS_MASTER))	slaves << n;

	// to which side of which master a slave's end is attached
	slave2masterSide.clear();
	slave2masterSide.resize(slaves.size());
	double adjacentThr = getConnectivityThr();
	for (int i = 0; i < slaves.size(); i++)
	{
		// find adjacent master(s)
		FdNode* slave = slaves[i];
		for (int j = 0; j < masterPatches.size(); j++)
		{
			// distance to master
			PatchNode* master = masterPatches[j];
			if (getDistance(slave, master) < adjacentThr)
			{
				// each master j has two sides: 2*j (positive) and 2*j+1 (negative)
				Vector3 sc2mc = slave->center() - master->center(); 
				if (dot(sc2mc, master->mPatch.Normal) > 0)
					slave2masterSide[i] << 2*j;
				else
					slave2masterSide[i] << 2*j+1;
			}
		}
	}

}

QVector<FdNode*> DcGraph::mergeConnectedCoplanarParts( QVector<FdNode*> ns )
{
	// classify rods and patches
	QVector<RodNode*>	rootRod;
	QVector<PatchNode*> rootPatch;
	foreach (FdNode* n, ns)
	{
		if (n->mType == FdNode::PATCH) 
			rootPatch << (PatchNode*)n;
		else rootRod << (RodNode*)n;
	}

	// RANSAC-like strategy: use a few samples to produce fitting model, which is a plane
	// a plane is fitted by either one root patch or two coplanar root rods
	QVector<Geom::Plane> fitPlanes;
	foreach(PatchNode* pn, rootPatch) fitPlanes << pn->mPatch.getPlane();
	for (int i = 0; i < rootRod.size(); i++){
		for (int j = i+1; j< rootRod.size(); j++)
		{
			Vector3 v1 = rootRod[i]->mRod.Direction;
			Vector3 v2 = rootRod[j]->mRod.Direction;
			double dotProd = dot(v1, v2);
			if (fabs(dotProd) > 0.9)
			{
				Vector3 v = v1 + v2; 
				if (dotProd < 0) v = v1 - v2;
				Vector3 u = rootRod[i]->mRod.Center - rootRod[j]->mRod.Center;
				v.normalize(); u.normalize();
				Vector3 normal = cross(u, v).normalized();
				fitPlanes << Geom::Plane(rootRod[i]->mRod.Center, normal);
			}
		}
	}

	// search for coplanar parts for each fit plane
	// add group them into connected components
	QVector< QSet<QString> > copConnGroups;
	double distThr = getConnectivityThr();
	foreach(Geom::Plane plane, fitPlanes)
	{
		QVector<FdNode*> copNodes;
		foreach(FdNode* n, ns)
			if (onPlane(n, plane)) copNodes << n;

		foreach(QVector<FdNode*> group, clusterNodes(copNodes, distThr))
		{
			QSet<QString> groupIds;
			foreach(FdNode* n, group) groupIds << n->mID;
			copConnGroups << groupIds;
		}
	}

	// set cover: prefer large subsets
	QVector<bool> deleted(copConnGroups.size(), false);
	int count = copConnGroups.size(); 
	QVector<int> subsetIndices;
	while(count > 0)
	{
		// search for the largest group
		int maxSize = -1;
		int maxIdx = -1;
		for (int i = 0; i < copConnGroups.size(); i++)
		{
			if (deleted[i]) continue;
			if (copConnGroups[i].size() > maxSize)
			{
				maxSize = copConnGroups[i].size();
				maxIdx = i;
			}
		}

		// save selected subset
		subsetIndices << maxIdx;
		deleted[maxIdx] = true;
		count--;

		// delete overlapping subsets
		for (int i = 0; i < copConnGroups.size(); i++)
		{
			if (deleted[i]) continue;

			QSet<QString> maxGroup = copConnGroups[maxIdx];
			maxGroup.intersect(copConnGroups[i]);
			if (!maxGroup.isEmpty())
			{
				deleted[i] = true;
				count--;
			}
		}
	}

	// merge each selected group
	QVector<FdNode*> mnodes;
	foreach(int i, subsetIndices)
		mnodes << merge(copConnGroups[i].toList().toVector());

	return mnodes;
}

void DcGraph::createSlaves()
{
	// split slave parts by master patches
	double adjacentThr = getConnectivityThr();
	foreach (PatchNode* master, masterPatches)
	{
		foreach(FdNode* n, getFdNodes())
		{
			if (n->hasTag(IS_MASTER)) continue;

			// split slave if it intersects the master
			if(hasIntersection(n, master, adjacentThr))
				split(n->mID, master->mPatch.getPlane());
		}
	}

	// current slave-master relation
	computeSlaveMasterRelation();

	// connectivity among slave parts
	for (int i = 0; i < slaves.size(); i++)
	{
		for (int j = i+1; j < slaves.size(); j++)
		{
			// skip slave parts connecting to the same master
			QSet<int> s2m_i, s2m_j;
			foreach (int msidx, slave2masterSide[i]) s2m_i << msidx/2;
			foreach (int msidx, slave2masterSide[j]) s2m_j << msidx/2;
			if (!(s2m_i & s2m_j).isEmpty()) continue;

			// add connectivity
			if (getDistance(slaves[i], slaves[j]) < adjacentThr)
			{
				FdLink* link = FdGraph::addLink(slaves[i], slaves[j]);
			}
		}
	}

	// merge connected and coplanar slave parts
	foreach (QVector<Structure::Node*> connGroup, getNodesOfConnectedSubgraphs())
	{
		// skip single node
		// two cases: master; T-slave that only connects to master
		if (connGroup.size() == 1) continue;

		// convert to fd Nodes
		QVector<FdNode*> fdConnGroup;
		foreach(Structure::Node* n, connGroup) fdConnGroup << (FdNode*)n;

		mergeConnectedCoplanarParts(fdConnGroup);
	}

	// slave-master relation after merging
	computeSlaveMasterRelation();
	for (int i = 0; i < slaves.size();i++)
	{
		if (slave2masterSide[i].isEmpty())
		{
			slaves.remove(i);
			slave2masterSide.remove(i);
			i--;
		}
	}
}

void DcGraph::clusterSlaves()
{
	// tags to avoid deleting/copying
	QVector<bool> isHSlave(slaves.size(), true);

	// T-slaves
	for (int i = 0; i < slaves.size(); i++)
	{
		if (slave2masterSide[i].size() == 1)
		{
			// each T-slave is a cluster
			TSlaves.push_back(i);
			isHSlave[i] = false;
		}
	}

	// cluster end props of H-slaves
	for (int i = 0; i < slaves.size(); i++)
	{
		if (isHSlave[i])
		{
			// check whether belong to existed clusters
			QVector<int> clusterIdx;
			for (int j = 0; j < endClusters.size(); j++)
			{
				QSet<int> isct = slave2masterSide[i] & endClusters[j];
				if (!isct.isEmpty()) clusterIdx.push_back(j);
			}

			// belong to none
			switch (clusterIdx.size())
			{
			case 0: 
				{
					// create a new cluster
					endClusters.push_back(slave2masterSide[i]);
				}
				break;
			case 1:
				{
					// merge the slaveSideProp with the cluster
					int idx = clusterIdx[0];
					endClusters[idx] = endClusters[idx] + slave2masterSide[i];
				}
				break;
			case 2:
				{
					// merge these two clusters
					// slaveSideProp is already contained in their union
					int idx0 = clusterIdx[0];
					int idx1 = clusterIdx[1];
					endClusters[idx0] = endClusters[idx0] + endClusters[idx1];
					endClusters.removeAt(idx1);
				}
				break;
			}
		}
	}

	// H-slave clusters
	HSlaveClusters.resize(endClusters.size());
	for (int i = 0; i < slaves.size(); i++)
	{
		if (isHSlave[i])
		{
			for (int j = 0; j < endClusters.size(); j++)
			{
				if (endClusters[j].contains(slave2masterSide[i]))
				{
					HSlaveClusters[j] << i;
				}
			}
		}
	}
}

void DcGraph::createBlocks()
{
	// clear blocks
	foreach (BlockGraph* b, blocks) delete b;
	blocks.clear();

	// T-blocks
	for (int i = 0; i < TSlaves.size(); i++)
	{
		int sidx = TSlaves[i];
		QList<int> endlist = slave2masterSide[sidx].toList();
		int end = endlist.front();
		int midx = end/2;

		PatchNode* m = masterPatches[midx];
		FdNode* s = slaves[sidx];
		QString id = "TB_" + QString::number(i);

		blocks << new TBlock(m, s, id);
	}

	// H-blocks
	for (int i = 0; i < HSlaveClusters.size(); i++)
	{
		// masters
		QVector<PatchNode*> ms;
		QSet<int> mIdxSet;
		foreach (int end, endClusters[i]) mIdxSet << end/2;

		QMap<int, int> masterIdxMap;
		foreach (int midx, mIdxSet)
		{
			masterIdxMap[midx] = ms.size();
			ms << masterPatches[midx];
		}

		// slaves
		QVector<FdNode*> ss;
		foreach (int sidx, HSlaveClusters[i])
			ss << slaves[sidx];

		// master pairs
		QVector< QVector<int> > masterPairs;
		foreach (int sidx, HSlaveClusters[i])
		{
			QVector<int> new_pair;
			foreach (int end, slave2masterSide[sidx])
			{
				int mid = end / 2;
				int new_mid = masterIdxMap[mid];

				new_pair << new_mid;
			}

			masterPairs << new_pair;
		}

		//id
		QString id = "HB_" + QString::number(i);

		// create
		blocks << new HBlock(ms, ss, masterPairs, id);
	}

	// set up path
	foreach (BlockGraph* b, blocks)
		b->path = path;
}

BlockGraph* DcGraph::getSelBlock()
{
	if (selBlockIdx >= 0 && selBlockIdx < blocks.size())
		return blocks[selBlockIdx];
	else
		return NULL;
}

FdGraph* DcGraph::activeScaffold()
{
	BlockGraph* selLayer = getSelBlock();
	if (selLayer) return selLayer->activeScaffold();
	else		  return this;
}

QStringList DcGraph::getBlockLabels()
{
	QStringList labels;
	foreach(BlockGraph* l, blocks)
		labels.append(l->mID);

	// append string to select none
	labels << "--none--";

	return labels;
}

void DcGraph::selectBlock( QString id )
{
	// select layer named id
	selBlockIdx = -1;
	for (int i = 0; i < blocks.size(); i++)
	{
		if (blocks[i]->mID == id)
		{
			selBlockIdx = i;
			break;
		}
	}

	// disable selection on chains
	if (getSelBlock())
	{
		getSelBlock()->selectChain("");
	}
}

FdGraph* DcGraph::getKeyFrame( double t )
{
	//// evenly distribute the time among layers
	//// if the end layer is empty, assign zero time interval to it
	//bool empty_front = (blocks.front()->nbNodes() == 1);
	//bool empty_back = (blocks.back()->nbNodes() == 1);
	//QVector<double> layerStarts;
	//if (empty_front && empty_back)
	//{
	//	layerStarts = getEvenDivision(blocks.size() - 2);
	//	layerStarts.insert(layerStarts.begin(), 0);
	//	layerStarts.append(1);
	//}
	//else if (empty_front)
	//{
	//	layerStarts = getEvenDivision(blocks.size() - 1);
	//	layerStarts.insert(layerStarts.begin(), 0);
	//}
	//else if (empty_back)
	//{
	//	layerStarts = getEvenDivision(blocks.size() - 1);
	//	layerStarts.append(1);
	//}
	//else
	//{
	//	layerStarts = getEvenDivision(blocks.size());
	//}

	//// get folded nodes of each layer
	//// the folding base is the first control panel
	//QVector< QVector<Structure::Node*> > knodes;
	//for (int i = 0; i < blocks.size(); i++)
	//{
	//	double lt = getLocalTime(t, layerStarts[i], layerStarts[i+1]);
	//	knodes << blocks[i]->getKeyFrameNodes(lt);
	//}

	//// compute offset of each control panel
	//// and remove redundant control panels
	//QVector<Vector3> panelDeltas;
	//for (int i = 0; i < masterPatches.size(); i++)
	//{
	//	QString panel_id = masterPatches[i]->mID;
	//	
	//	// copy1 in layer[i]
	//	FdNode* panelCopy1 = NULL;
	//	int idx1 = -1;
	//	QVector<Structure::Node*> &lnodes1 = knodes[i];
	//	for(int j = 0; j < lnodes1.size(); j++)
	//	{
	//		if (lnodes1[j]->hasId(panel_id))
	//		{
	//			panelCopy1 = (FdNode*)lnodes1[j];
	//			idx1 = j;
	//		}
	//	}

	//	// copy2 in layer[i+1]
	//	FdNode* panelCopy2 = NULL;
	//	int idx2 = -1;
	//	QVector<Structure::Node*> &lnodes2 = knodes[i+1];
	//	for(int j = 0; j < lnodes2.size(); j++)
	//	{
	//		if (lnodes2[j]->hasId(panel_id))
	//		{
	//			panelCopy2 = (FdNode*)lnodes2[j];
	//			idx2 = j;
	//		}
	//	}
	//	 
	//	// delta
	//	Vector3 delta(0, 0, 0);
	//	if (panelCopy1 && panelCopy2) 
	//		delta = panelCopy1->center() - panelCopy2->center();
	//	panelDeltas << delta;

	//	// remove copy2
	//	if (idx2 >= 0 && idx2 < lnodes2.size())
	//		lnodes2.remove(idx2);
	//}

	//// shift layers and add nodes into scaffold
	FdGraph *key_graph = new FdGraph();
	//Vector3 offset(0, 0, 0);
	//for (int i = 0; i < knodes.size(); i++)
	//{
	//	// keep the first layer but shift others
	//	if (i > 0) offset += panelDeltas[i-1];

	//	QVector<Structure::Node*> &lnodes = knodes[i];
	//	for (int j = 0; j < lnodes.size(); j++)
	//	{
	//		FdNode* n = (FdNode*)lnodes[j];
	//		n->mBox.translate(offset);
	//		n->createScaffold();

	//		key_graph->Structure::Graph::addNode(n);
	//	}
	//}

	//key_graph->properties["pushAId"] = pushAId;

	return key_graph;
}

void DcGraph::foldabilize()
{
	foreach(BlockGraph* layer, blocks)
		layer->foldabilize();
}