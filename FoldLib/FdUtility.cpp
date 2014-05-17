#include "FdUtility.h"

#include "RodNode.h"
#include "PatchNode.h"
#include "FdGraph.h"

#include "DistSegSeg.h"
#include "DistSegRect.h"
#include "DistRectRect.h"
#include "Numeric.h"
#include "MinOBB.h"
#include "PcaOBB.h"

FdNodeArray2D getConnectedGroups( QVector<FdNode*> nodes, double disThr )
{
	FdNodeArray2D clusters;
	if (nodes.isEmpty()) return clusters;

	// tags used for searching
	QVector<bool> visited(nodes.size(), false);
	int nbVisited = 0;

	// initial current graph
	QVector<FdNode*> curr_g;
	curr_g.push_back(nodes[0]);
	visited[0] = true;
	nbVisited++;

	// search for connected node to current graph
	while(nbVisited < nodes.size())
	{
		int preN = nbVisited;
		for (int i = 0; i < nodes.size(); i++)
		{
			// skip visited node
			if (visited[i]) continue;

			if (getDistance(nodes[i], curr_g) < disThr)
			{
				curr_g.push_back(nodes[i]);
				visited[i] = true;
				nbVisited++;
			}
		}

		// cannot find more nodes
		if (preN == nbVisited)
		{
			// save current graph
			clusters.push_back(curr_g);

			// reset current graph with an unvisited node
			for (int i = 0; i < nodes.size(); i++)
			{
				if (!visited[i])
				{
					curr_g.clear();
					curr_g.push_back(nodes[i]);
					visited[i] = true;
					nbVisited++;
					break;
				}
			}
		}
	}

	// save current graph
	clusters.push_back(curr_g);

	return clusters;
}

Geom::Segment getDistSegment( FdNode* n1, FdNode* n2 )
{
	Geom::Segment ds;

	if (n1->mType == FdNode::ROD)
	{
		RodNode* node1 = (RodNode*)n1;
		Geom::Segment rod1 = node1->mRod;

		// rod-rod
		if (n2->mType == FdNode::ROD)
		{
			RodNode* node2 = (RodNode*)n2;
			Geom::Segment rod2 = node2->mRod;

			Geom::DistSegSeg dss(rod1, rod2);
			ds.set(dss.mClosestPoint0, dss.mClosestPoint1);
		}
		// rod-patch
		else
		{
			PatchNode* node2 = (PatchNode*)n2;
			Geom::Rectangle rect2 = node2->mPatch;

			Geom::DistSegRect dsr(rod1, rect2);
			ds.set(dsr.mClosestPoint0, dsr.mClosestPoint1);
		}
	}

	else 
	{
		PatchNode* node1 = (PatchNode*)n1;
		Geom::Rectangle rect1 = node1->mPatch;

		// patch-rod
		if (n2->mType == FdNode::ROD)
		{
			RodNode* node2 = (RodNode*)n2;
			Geom::Segment rod2 = node2->mRod;

			Geom::DistSegRect dsr(rod2, rect1);
			ds.set(dsr.mClosestPoint0, dsr.mClosestPoint1);
		}
		// patch-patch
		else
		{
			PatchNode* node2 = (PatchNode*)n2;
			Geom::Rectangle rect2 = node2->mPatch;

			Geom::DistRectRect drr(rect1, rect2);
			ds.set(drr.mClosestPoint0, drr.mClosestPoint1);
		}	
	}

	return ds;
}

double getDistance( FdNode* n1, FdNode* n2 )
{
	return getDistSegment(n1, n2).length();
}

double getDistance( FdNode* n, QVector<FdNode*> nset )
{
	double minDist = maxDouble();
	foreach (FdNode* to, nset)
	{
		double dist = getDistance(n, to);
		if (dist < minDist) minDist = dist;
	}

	return minDist;
}

StrArray2D getIds( FdNodeArray2D nodeArray )
{
	StrArray2D idArray;
	QVector<QString> ids;
	foreach (QVector<FdNode*> ns, nodeArray)
	{
		ids.clear();
		foreach(FdNode* n, ns)	ids.push_back(n->mID);
		idArray.push_back(ids);
	}

	return idArray;
}

QVector<Geom::Segment> detectJointSegments( FdNode* part, PatchNode* panel )
{
	QVector<Geom::Segment> jointSegs;

	if (part->mType == FdNode::PATCH)
	{
		PatchNode* partPatch = (PatchNode*)part;
		Vector3 panelNormal = panel->mPatch.Normal;
		QVector<Geom::Segment> perpEdges = partPatch->mPatch.getPerpEdges(panelNormal);

		Geom::DistSegRect dsr1(perpEdges[0], panel->mPatch);
		Geom::DistSegRect dsr2(perpEdges[1], panel->mPatch);
		if (dsr1.get() < dsr2.get())	jointSegs << perpEdges[0];
		else							jointSegs << perpEdges[1];
	}
	else if (part->mType == FdNode::ROD)
	{
		Geom::Segment distSeg = getDistSegment(part, panel);
		Vector3 p = distSeg.P0;

		Vector3 v1 = panel->mPatch.Axis[0];
		int aid1 = part->mBox.getAxisId(v1);
		double ext1 = part->mBox.getExtent(aid1);

		Vector3 v2 = panel->mPatch.Axis[1];
		int aid2 = part->mBox.getAxisId(v2);
		double ext2 = part->mBox.getExtent(aid2);

		jointSegs << Geom::Segment(p, v1, ext1) << Geom::Segment(p, v2, ext2);
	}

	return jointSegs;
}

double getLocalTime( double globalT, TimeInterval itv )
{
	if (globalT <= itv.first) return 0;
	else if(globalT > itv.first && globalT < itv.second)
		return (globalT - itv.first) / (itv.second - itv.first);
	else return 1;
}


Geom::Box fitBox( QVector<Vector3>& pnts, BOX_FIT_METHOD method )
{
	Geom::Box box;

	if (method == FIT_AABB)
	{
		Geom::AABB aabb(pnts);
		box = aabb.box();
	}
	else if (method == FIT_MIN)
	{
		Geom::MinOBB obb(pnts, false);
		box = obb.mMinBox;
	}
	else
	{
		Geom::PcaOBB pcaOBB(pnts);
		box = pcaOBB.minBox;
	}

	return box;
}

PLANE_RELATION relationWithPlane( FdNode* n, Geom::Plane plane, double thr )
{
	double minDist = maxDouble();
	double maxDist = minDouble();
	foreach (Vector3 p, n->mBox.getConnerPoints())
	{
		double dist = plane.signedDistanceTo(p);
		if (dist < minDist) minDist = dist;
		if (dist > maxDist) maxDist = dist;
	}

	double distThr = n->mBox.radius() * thr;

	int minSide;
	if (minDist < -distThr) minSide = -1;
	else if (minDist > distThr) minSide = 1;
	else minSide = 0;

	int maxSide;
	if (maxDist < -distThr) maxSide = -1;
	else if (maxDist > distThr) maxSide = 1;
	else maxSide = 0;

	int sum = minSide + maxSide;
	if (sum >= 1) return POS_PLANE;
	if (sum <= -1) return NEG_PLANE;
	if (minSide == 0 && maxSide == 0) return ON_PLANE;
	else return ISCT_PLANE;

}

bool onPlane( FdNode* n, Geom::Plane& plane )
{
	return (relationWithPlane(n, plane, 0.2) == ON_PLANE);
}

QString getBundleName( const QVector<FdNode*>& nodes )
{
	QString bname;
	foreach (FdNode* n, nodes)
	{
		bname += "+" + n->mID;
	}

	return "(" + bname + ")";
}

Geom::Box getBundleBox( const QVector<FdNode*>& nodes )
{
	QVector<Vector3> points;
	foreach (FdNode* n, nodes)
	{
		points += n->mBox.getConnerPoints();
	}

	return fitBox(points);
}

bool hasIntersection( FdNode* slave, PatchNode* master, double thr )
{
	if (getDistance(slave, master) > thr) return false;

	// check whether slave locates on both sides of master
	double minDist = maxDouble();
	double maxDist = -maxDouble();
	foreach (Vector3 p, slave->mBox.getConnerPoints())
	{
		double dist = master->mPatch.getPlane().signedDistanceTo(p);
		if (dist < minDist) minDist = dist;
		if (dist > maxDist) maxDist = dist;
	}

	// one side
	if (minDist * maxDist >= 0) return false;

	// two sides
	double a = fabs(minDist);
	double b = fabs(maxDist);
	double ratio = a / (a + b);
	if (ratio > 0.5) ratio  = 1 - ratio;
	return ratio > 0.05; // this is ugly but can avoid some cuts
}

bool overlap( TimeInterval itv1, TimeInterval itv2 )
{
	return within(itv1.first, itv2) || within(itv1.second, itv2) ||
		within(itv2.first, itv1) || within(itv2.second, itv1);
}

bool within( double t, TimeInterval itv )
{
	return inRange(t, itv.first, itv.second);
}

QVector<PatchNode*> getAllMasters( FdGraph* scaffold )
{
	QVector<PatchNode*> masters;
	foreach(Structure::Node* n, scaffold->getNodesWithTag(IS_MASTER))
		masters << (PatchNode*)n;

	return masters;
}

FdGraph* combineDecomposition( QVector<FdGraph*> decmps, QString baseMid, 
								QMap<QString, QSet<int> >& masterDecmpMap )
{
	// trivial combination
	if (decmps.size() == 1)
		return (FdGraph*)decmps.front()->clone();

	// create scaffold for combination
	FdGraph* keyframeScaffold = new FdGraph();

	// combined tags
	QMap<QString, bool> masterCombined;
	foreach (QString mid, masterDecmpMap.keys()) 
		masterCombined[mid] = false;
	QVector<bool> decmpCombined(decmps.size(), false);

	// start from base master
	int base_decmp_id = masterDecmpMap[baseMid].toList().front();
	FdGraph* base_decmp = decmps[base_decmp_id];
	PatchNode* baseMaster = (PatchNode*)base_decmp->getNode(baseMid);
	keyframeScaffold->Structure::Graph::addNode(baseMaster);
	masterCombined[baseMid] = true;

	// prorogation
	QQueue<QString> activeMids;
	activeMids.enqueue( baseMid );
	while (!activeMids.isEmpty())
	{
		// set current to an active master
		QString curr_mid = activeMids.dequeue();
		PatchNode* currMaster = (PatchNode*)keyframeScaffold->getNode(curr_mid);
		Vector3 currPos = currMaster->center();

		// combine decompositions containing current master
		foreach (int decmp_id, masterDecmpMap[curr_mid])
		{
			// skip combined decmp
			if (decmpCombined[decmp_id]) continue;

			// translate
			FdGraph* decmp = decmps[decmp_id];
			PatchNode* oldMaster = (PatchNode*)decmp->getNode(curr_mid);
			Vector3 delta = currPos - oldMaster->center();
			decmp->translate(delta);

			// combine parts from decomposition
			foreach (Structure::Node* n, decmp->nodes)
			{
				// master nodes
				if (n->hasTag(IS_MASTER))
				{
					if (!masterCombined[n->mID])
					{
						// combine unvisited masters
						keyframeScaffold->Structure::Graph::addNode(n->clone());
						masterCombined[n->mID] = true;

						// store as active
						activeMids.enqueue(n->mID);
					}
				}
				// clone slave nodes
				else 
				{
					keyframeScaffold->Structure::Graph::addNode(n->clone());
				}
			}

			// mark current decmp
			decmpCombined[decmp_id] = true;
		}
	}

	return keyframeScaffold;
}

int nbMasters( FdGraph* scaffold )
{
	return getAllMasters(scaffold).size();
}

QVector<QString> getAllMasterIds( FdGraph* scaffold )
{
	QVector<QString> mids;
	foreach (PatchNode* m, getAllMasters(scaffold))
		mids << m->mID;

	return mids;
}

Geom::Rectangle2 getMinEnclosingRectangle2D( QVector<Vector2> &pnts )
{
	// compute extent along x and y
	double minX = maxDouble();
	double maxX = -maxDouble();
	double minY = maxDouble();
	double maxY = -maxDouble();
	foreach (Vector2 p, pnts)
	{
		if (p.x() < minX) minX = p.x();
		if (p.x() > maxX) maxX = p.x();

		if (p.y() < minY) minY = p.y();
		if (p.y() > maxY) maxY = p.y();
	}

	// create rect
	QVector<Vector2> conners;
	conners << Vector2(minX, minY) << Vector2(maxX, minY) << Vector2(maxX, maxY) << Vector2(minX, maxY);
	return Geom::Rectangle2(conners);
}

bool passed( double t, TimeInterval itv )
{
	return t >= itv.second;
}

QMap<QString, double> getTimeStampsNormalized( QVector<FdNode*> nodes, Vector3 v )
{
	QMap<QString, double> timeStamps;

	// time line
	Geom::Line timeLine(Vector3(0, 0, 0), v);

	// position on time line
	double minT = maxDouble();
	double maxT = -maxDouble();
	foreach (FdNode* n, nodes)
	{
		double t = timeLine.getProjTime(n->center());
		timeStamps[n->mID] = t;

		if (t < minT) minT = t;
		if (t > maxT) maxT = t;
	}

	// normalize time stamps
	double timeRange = maxT - minT;
	foreach (QString mid, timeStamps.keys())
	{
		timeStamps[mid] = ( timeStamps[mid] - minT ) / timeRange;
	}

	return timeStamps;
}

QMap<QString, double> getTimeStampsNormalized( QVector<PatchNode*> pnodes, Vector3 v )
{
	QVector<FdNode*> nodes;
	foreach (PatchNode* pn, pnodes) nodes << pn;
	return getTimeStampsNormalized(nodes, v);
}

Geom::Rectangle2 extendRectangle2D( Geom::Rectangle2& seed, QVector<Vector2> &pnts )
{
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
