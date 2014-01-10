#include "FdGraph.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "XmlWriter.h"
#include "MeshMerger.h"
#include "MeshHelper.h"
#include "MeshBoolean.h"

#include "RodNode.h"
#include "PatchNode.h"
#include "BundleNode.h"
#include "FdLink.h"

#include "AABB.h"
#include "MinOBB.h"
#include "CustomDrawObjects.h"


FdGraph::FdGraph(QString id /*= ""*/)
	:Graph(id)
{
	showAABB = false;
	path = "";
}

FdGraph::FdGraph( FdGraph& other )
	:Graph(other)
{
	path = other.path + "_cloned";
	showAABB = false;
}


Structure::Graph* FdGraph::clone()
{
	return new FdGraph(*this);
}

FdGraph * FdGraph::deepClone()
{
	FdGraph *fdGraph = (FdGraph*)clone();

	QVector<FdNode *> nodes = fdGraph->getFdNodes();
	foreach(FdNode *n, nodes){
		n->cloneMesh();
	}

	return fdGraph;
}


FdLink* FdGraph::addLink( FdNode* n1, FdNode* n2 )
{
	FdLink* new_link = new FdLink(n1, n2);
	Graph::addLink(new_link);

	return new_link;
}


QVector<FdNode*> FdGraph::getFdNodes()
{
	QVector<FdNode*> fdns;
	foreach(Structure::Node* n, nodes)
		fdns.push_back((FdNode*)n);
	
	return fdns;
}

void FdGraph::saveToFile(QString fname)
{
	QFile file(fname);
	if (!file.open(QIODevice::WriteOnly)) return;

	// scaffold
	XmlWriter xw(&file);
	xw.setAutoNewLine(true);	
	xw.writeRaw("\<!--?xml Version = \"1.0\"?--\>\n");
	xw.writeOpenTag("document");
	{
		// nodes
		xw.writeTaggedString("cN", QString::number(nbNodes()));
		foreach(FdNode* node, getFdNodes())
		{
			node->write(xw);
		}

		// links
		xw.writeTaggedString("cL", QString::number(nbLinks()));
	}
	xw.writeCloseTag("document");
	file.close();

	// meshes
	QString meshesFolder = "meshes";
	QFileInfo fileInfo(fname);
	QDir graphDir( fileInfo.absolutePath());
	graphDir.mkdir(meshesFolder);
	meshesFolder =  graphDir.path() + "/" + meshesFolder;
	foreach(FdNode* node, getFdNodes())
	{
		MeshHelper::saveOBJ(node->mMesh.data(), meshesFolder + '/' + node->mID + ".obj");
	}
}

void FdGraph::loadFromFile(QString fname)
{
	clear();
	path = fname;

	// open the file
	QFileInfo finfo(fname);
	QString meshFolder = finfo.path() + "/meshes";
	QFile file(fname);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

	// cast using DOM
	QDomDocument doc;
	if (!doc.setContent(&file, false)) {
		file.close();
		return;
	}
	file.close();

	QDomNodeList nodeList = doc.firstChildElement("document").elementsByTagName("node");
	for (int i = 0; i < nodeList.size(); i++)
	{
		QDomNode node = nodeList.at(i);

		// scaffold
		QString nid = node.firstChildElement("ID").text();
		int ntype = node.firstChildElement("type").text().toInt();
		Geom::Box box;
		box.read(node.firstChildElement("box"));

		// mesh
		QString mesh_fname = meshFolder + "/" + nid + ".obj";
		SurfaceMeshModel* mesh(new SurfaceMeshModel(mesh_fname, nid));
		if(mesh->read( qPrintable(mesh_fname) ))
		{
			mesh->update_face_normals();
			mesh->update_vertex_normals();
			mesh->updateBoundingBox();
		}

		// create new node
		FdNode* new_node;
		MeshPtr m(mesh);
		QString id = m->name;
		if(ntype == FdNode::ROD)
			new_node = new RodNode(id, box, m);
		else
			new_node = new PatchNode(id, box, m);

		Graph::addNode(new_node);
	}
}



Geom::AABB FdGraph::computeAABB()
{
	Geom::AABB aabb;
	foreach (FdNode* n, getFdNodes())
	{
		aabb.add(n->computeAABB());
	}

	// in case the graph is empty
	aabb.validate();

	return aabb;
}

void FdGraph::draw()
{
	Structure::Graph::draw();

	// draw aabb
	if (showAABB)
	{

		if (properties.contains("pushAId"))
		{
			int aid =  properties["pushAId"].toInt();
			Geom::Box box = computeAABB().box();
			Geom::Rectangle patch1 = box.getPatch(aid, 1);
			Geom::Rectangle patch2 = box.getPatch(aid, -1);

			patch1.drawEdges(2.0, Qt::yellow);
			patch2.drawEdges(2.0, Qt::yellow);

			LineSegments ls(2.0);
			foreach (Geom::Segment seg, box.getEdgeSegments(aid))
				ls.addLine(seg.P0, seg.P1, Qt::cyan);
			ls.draw();
		}
		else
		{
			Geom::AABB aabb = computeAABB();
			aabb.box().drawWireframe(2.0, Qt::cyan);
		}
	}

	// debug
	drawDebug();
}

FdNode* FdGraph::merge( QVector<QString> nids )
{
	// retrieve all nodes
	QVector<FdNode*> ns;
	foreach(QString nid, nids)
	{
		Structure::Node* n = getNode(nid);
		if (n) ns.push_back((FdNode*)n);
	}

	// trivial cases
	if (ns.isEmpty()) return NULL;
	if (ns.size() == 1) return ns[0];

	// merge into a bundle node
	QVector<FdNode*> plainNodes;
	foreach (FdNode* n, ns)	plainNodes += n->getPlainNodes();
	QString bid = getBundleName(plainNodes);
	Geom::Box box = getBundleBox(plainNodes);
	BundleNode* mergedNode = new BundleNode(bid, box, plainNodes); 
	Structure::Graph::addNode(mergedNode);

	// remove original nodes
	foreach (FdNode* n, ns)
		Structure::Graph::removeNode(n->mID);

	return mergedNode;
}


FdNode* FdGraph::addNode( MeshPtr mesh, BOX_FIT_METHOD method )
{
	// fit box
	QVector<Vector3> points = MeshHelper::getMeshVertices(mesh.data());
	Geom::Box box = fitBox(points, method);

	return addNode(mesh, box);
}

FdNode* FdGraph::addNode(MeshPtr mesh, Geom::Box& box)
{
	// create node depends on box type
	int box_type = box.getType(5);

	FdNode* node;
	QString id = mesh->name;
	if (box_type == Geom::Box::ROD)	
		node = new RodNode(id, box, mesh);
	else node = new PatchNode(id, box, mesh);

	// add to graph and set id
	Graph::addNode(node);
	node->mID = mesh->name;

	return node;
}

QVector<FdNode*> FdGraph::split( QString nid, Geom::Plane& plane)
{
	return split(nid, QVector<Geom::Plane>() << plane);

	//QVector<FdNode*> chopped;

	//// get node
	//FdNode* fn = (FdNode*) getNode(nid);
	//if (!fn) return chopped;

	//// split only when the plane cuts the node
	//if (relationWithPlane(fn, plane, 0.1) != ISCT_PLANE)
	//	return chopped;

	//// positive side
	//FdNode* node1 = fn->cloneChopped(plane);
	//node1->mID = nid + "_" + QString::number(0);
	//Structure::Graph::addNode(node1);
	//chopped.push_back(node1);

	//// negative side
	//FdNode* node2 = fn->cloneChopped(plane.opposite());
	//node2->mID = nid + "_" + QString::number(1);
	//Structure::Graph::addNode(node2);
	//chopped.push_back(node2);

	//// remove the original node
	//if (!chopped.isEmpty()) removeNode(fn->mID);

	//return chopped;
}

QVector<FdNode*> FdGraph::split( QString nid, QVector<Geom::Plane>& planes )
{
	QVector<FdNode*> chopped;

	if (planes.isEmpty()) return chopped;

	FdNode* fn = (FdNode*) getNode(nid);
	if (!fn) return chopped;

	// sort cut planes 
	int aid = fn->mBox.getClosestAxisId(planes.front().Normal);
	Geom::Segment sklt = fn->mBox.getSkeleton(aid);
	QMap<double, Geom::Plane> distPlaneMap;
	foreach (Geom::Plane plane, planes)
	{
		// skip a plane if it doesn't intersect the node
		if (relationWithPlane(fn, plane, 0.1) != ISCT_PLANE) continue;

		Vector3 cutPoint = plane.getIntersection(sklt);
		double dist = (cutPoint - sklt.P0).norm();
		distPlaneMap[dist] = plane;
	}
	QVector<Geom::Plane> sortedPlanes = distPlaneMap.values().toVector();
	if (sortedPlanes.isEmpty()) return chopped;

	// the front end
	Geom::Plane frontPlane = sortedPlanes.front();
	if (frontPlane.whichSide(sklt.P0) < 0) frontPlane.flip();
	chopped << fn->cloneChopped(frontPlane);

	// the middle section bounded by two planes
	for (int i = 0; i < sortedPlanes.size()-1; i++)
	{
		chopped << fn->cloneChopped(sortedPlanes[i], sortedPlanes[i+1]);
	}

	// the back end
	Geom::Plane backPlane = sortedPlanes.back();
	if (backPlane.whichSide(sklt.P1) < 0) backPlane.flip();
	chopped << fn->cloneChopped(backPlane);

	// change node id and add to graph
	for (int i = 0; i < chopped.size(); i++)
	{
		chopped[i]->mID = nid + "_" + QString::number(i);
		Structure::Graph::addNode(chopped[i]);
	}

	// remove the original node
	if (!chopped.isEmpty()) removeNode(nid);

	return chopped;
}

void FdGraph::showCuboids( bool show )
{
	foreach(FdNode* n, getFdNodes())
		n->showCuboids = show;
}

void FdGraph::showScaffold( bool show )
{
	foreach(FdNode* n, getFdNodes())
		n->showScaffold = show;
}

void FdGraph::showMeshes( bool show )
{
	foreach(FdNode* n, getFdNodes())
		n->showMesh = show;
}

void FdGraph::changeNodeType( FdNode* n )
{
	// create new node
	Structure::Node* new_node;
	if (n->mType == FdNode::PATCH)
		new_node =new RodNode(n->mID, n->mBox, n->mMesh);
	else
		new_node = new PatchNode(n->mID, n->mBox, n->mMesh);

	// replace
	replaceNode(n, new_node);
}


void FdGraph::restoreConfiguration()
{
	QQueue<FdNode*> activeNodes;
	foreach(FdNode* fn, getFdNodes())
	{
		if (fn->properties["fixed"].toBool())
		{
			activeNodes.enqueue(fn);
		}
	}

	while (!activeNodes.isEmpty())
	{
		FdNode* anode = activeNodes.dequeue();
		foreach(Structure::Link* l, getLinks(anode->mID))
		{
			FdLink* fl = (FdLink*)l;
			if (!fl->properties["active"].toBool()) continue;

			if (fl->fix()) 
			{
				Structure::Node* other_node = l->getNodeOther(anode->mID);
				activeNodes.enqueue((FdNode*)other_node);
			}
		}
	}
}

void FdGraph::normalize(double f)
{
	Geom::AABB aabb = computeAABB();
	Vector3 offset = aabb.center();

	foreach(FdNode* n, getFdNodes())
	{
		n->mBox.scaleAsPart(f, offset);
		n->deformMesh();
		n->createScaffold();
	}
}

void FdGraph::translate(Vector3 v)
{
	foreach(FdNode* n, getFdNodes())
	{
		n->mBox.translate(v);
		n->deformMesh();
		n->createScaffold();
	}
}

void FdGraph::drawDebug()
{
	// debug segments
	if (properties.contains("debugSegs"))
	{
		QVector<Geom::Segment> debugSegs = properties["debugSegs"].value< QVector<Geom::Segment> >();
		foreach (Geom::Segment seg, debugSegs)
		{
			seg.draw();
		}
	}
}

void FdGraph::addDebugSegment( Geom::Segment seg )
{
	QVector<Geom::Segment> debugSegs;
	if (properties.contains("debugSegs"))
		debugSegs = properties["debugSegs"].value< QVector<Geom::Segment> >();

	debugSegs << seg;

	properties["debugSegs"].setValue(debugSegs);
}
