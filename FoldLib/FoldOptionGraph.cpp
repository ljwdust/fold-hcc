
#include "FoldOptionGraph.h"
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QColor>
#include <QDebug>
#include <QFileInfo>
#include <QFileDialog>
#include "UtilityGlobal.h"
#include <QQueue>
#include "Numeric.h"
#include "FdUtility.h"


ChainNode::ChainNode( int idx, QString id )
	: Node(id), chainIdx(idx)
{
}

ChainNode::ChainNode( ChainNode &other )
	: Node(other)
{
	chainIdx = other.chainIdx;
}

Structure::Node* ChainNode::clone()
{
	return new ChainNode(*this);
}

//////////////////////////////////////////////////////////////////////////

FoldOption::FoldOption( QString id, bool right, double s, double p, int n, double area )
	: Node(id), rightSide(right), scale(s), position(p), nSplits(n), patchArea(area)
{
}

FoldOption::FoldOption( FoldOption &other )
	: Node(other)
{
	rightSide = other.rightSide;
	scale = other.scale;
	position = other.position;
	nSplits = other.nSplits;
	patchArea = other.patchArea;
	
}

FoldOption::FoldOption(QString id)
	:Node(id)
{
	// dumpy fold option for H-block
	scale = 1;
	nSplits = 0;
}

Structure::Node* FoldOption::clone()
{
	return new FoldOption(*this);
}


//////////////////////////////////////////////////////////////////////////

QString FoldOptionGraph::dotPath = "\"" + getcwd() + "/FoldLib/Graphviz/dot.exe" + "\"";


FoldOptionGraph::FoldOptionGraph( QString id )
	:Graph(id)
{
}

FoldOptionGraph::FoldOptionGraph( FoldOptionGraph& other )
	:Graph(other)
{
}

FoldOptionGraph::~FoldOptionGraph()
{
}

Structure::Graph* FoldOptionGraph::clone()
{
	return new FoldOptionGraph(*this);
}


void FoldOptionGraph::addNode( ChainNode* cn )
{
	Graph::addNode(cn);
	cn->properties["type"] = "entity";
}

void FoldOptionGraph::addNode( FoldOption* fn )
{
	Graph::addNode(fn);
	fn->properties["type"] = "option";
}

void FoldOptionGraph::addFoldLink( Structure::Node* n1, Structure::Node* n2 )
{
	Structure::Link* link =  Graph::addLink(n1, n2);
	link->properties["type"] = "option";
}

void FoldOptionGraph::addCollisionLink( Structure::Node* n1, Structure::Node* n2 )
{
	Structure::Link* link =  Graph::addLink(n1, n2);
	link->properties["type"] = "collision";
}


bool FoldOptionGraph::verifyNodeType( QString nid, QString type )
{
	Structure::Node* node = getNode(nid);
	return (node && node->properties["type"] == type);
}

bool FoldOptionGraph::verifyLinkType( QString nid1, QString nid2, QString type )
{
	Structure::Link* link = getLink(nid1, nid2);
	return (link && link->properties["type"] == type);
}

bool FoldOptionGraph::areSiblings( QString nid1, QString nid2 )
{
	// both are folding nodes
	if (!verifyNodeType(nid1, "option") || !verifyNodeType(nid2, "option"))
		return false;

	// share the same chain node
	ChainNode* cn1 = getChainNode(nid1);
	ChainNode* cn2 = getChainNode(nid2);
	return cn1->mID == cn2->mID;
}


ChainNode* FoldOptionGraph::getChainNode( QString fnid )
{
	if (!verifyNodeType(fnid, "option")) return NULL;

	Structure::Link* l = getFoldinglinks(fnid)[0];
	return (ChainNode*)l->getNodeOther(fnid);
}

QVector<FoldOption*> FoldOptionGraph::getFoldOptions( QString cnid )
{
	QVector<FoldOption*> fns;
	if (!verifyNodeType(cnid, "entity")) return fns;

	foreach(Structure::Link* l, getFoldinglinks(cnid))
	{
		fns.push_back((FoldOption*) l->getNodeOther(cnid));
	}

	return fns;
}

QVector<FoldOption*> FoldOptionGraph::getSiblings( QString fnid )
{
	if (verifyNodeType(fnid, "entity")) 
		return getFoldOptions(getChainNode(fnid)->mID);
	else
		return QVector<FoldOption*>();
}

QVector<FoldOption*> FoldOptionGraph::getAllFoldOptions()
{
	QVector<FoldOption*> fns;
	foreach(Structure::Node* n, nodes)
	{
		if (n->properties["type"].toString() == "option")
			fns << (FoldOption*)n;
	}

	return fns;
}

QVector<ChainNode*> FoldOptionGraph::getAllChainNodes()
{
	QVector<ChainNode*> cns;
	foreach(Structure::Node* n, nodes)
	{
		if (n->properties["type"].toString() == "entity")
			cns << (ChainNode*)n;
	}

	return cns;
}

QVector<Structure::Link*> FoldOptionGraph::getFoldinglinks( QString nid )
{
	QVector<Structure::Link*> flinks;
	foreach (Structure::Link* l, getLinks(nid))
	{
		if (l->properties["type"].toString() == "option")
		{
			flinks.push_back(l);
		}
	}

	return flinks;
}

QVector<Structure::Link*> FoldOptionGraph::getCollisionLinks( QString nid )
{
	QVector<Structure::Link*> clinks;
	foreach (Structure::Link* l, getLinks(nid))
	{
		if (l->properties["type"].toString() == "collision")
		{
			clinks.push_back(l);
		}
	}

	return clinks;
}

QVector<Structure::Node*> FoldOptionGraph::getFamilyNodes( QString nid )
{
	QVector<Structure::Node*> family;

	Structure::Node* node = getNode(nid);
	ChainNode* cnode = (node->properties["type"] == "entity") ? 
		(ChainNode*)node : getChainNode(nid);

	family << cnode;
	foreach(FoldOption* fn, getFoldOptions(cnode->mID))
		family << fn;

	return family;
}

QVector<Structure::Link*> FoldOptionGraph::getFamilyCollisionLinks( QString nid )
{
	QVector<Structure::Link*> clinks;
	foreach(Structure::Node* node, getFamilyNodes(nid))
	{
		clinks += getCollisionLinks(node->mID);
	}

	return clinks;
}

QString FoldOptionGraph::toGraphvizFormat( QString subcaption, QString caption )
{
	QStringList out;
	out << "graph G{\n";
	out << "\t" << "node [fontcolor = black, color = white];" << "\n";

	// Place on a grid
	double size = 50;
	double spacing = size / 10.0;
	double x = 0, y = 0;
	double dx = size + spacing;
	double dy = dx;
	int length = sqrt((double)nodes.size());

	// Write nodes
	for(int i = 0; i < nodes.size(); i++)
	{
		Structure::Node* node = nodes[i];
		QString type = node->properties["type"].toString();

		// label
		QString label = node->mID;

		// shape
		QString shape = "rectangle";
		if (type == "option") shape = "ellipse";

		// color
		QString colorHex; 
		QColor color = Qt::blue;
		if (type == "option") color = Qt::green;
		if (node->properties.contains("selected")) color = color.lighter();
		colorHex.sprintf("#%02X%02X%02X", color.red(), color.green(), color.blue());

		// highlight by filling colors
		QString other;
		if (node->hasTag(SELECTED_TAG)) other = "style = filled";

		out << "\t" << QString("%1 [label = \"%2\", color = \"%3\", shape = %4, %5];").arg(i).arg(label).arg(colorHex).arg(shape).arg(other) << "\n";

		// Move virtual cursor
		x += dx;
		if(i % length == 0){
			x = 0;
			y += dy;
		}
	}

	// Write links
	for(int i = 0; i < links.size(); i++)
	{
		Structure::Link* link = links[i];
		QString type = link->properties["type"].toString();

		int n1idx = getNodeIndex(link->nid1);
		int n2idx = getNodeIndex(link->nid2);

		QString color = "black";
		if (type == "collision") color = "red";

		out << "\t\"" << QString::number(n1idx) << "\" -- \"" << QString::number(n2idx) << "\"" 
			<< QString(" [color=\"%1\"]").arg(color) << ";\n";
	}

	// Labels
	out << "label = \"\\n\\n" << caption << "\\n" << subcaption << "\"\n";
	out << "fontsize = 20;\n";

	out << "}\n";

	return out.join("");
}

void FoldOptionGraph::saveAsGraphviz( QString fname, QString subcaption /*= ""*/, QString caption /*= ""*/ )
{
	QFile file(fname + ".gv");
	if (!file.open(QFile::WriteOnly | QFile::Text))	return;
	QTextStream out(&file);

	out << toGraphvizFormat(subcaption, caption); 

	file.flush();
	file.close();
}

void FoldOptionGraph::saveAsImage( QString fname )
{
	// save graphvis
	saveAsGraphviz(fname);

	// convert into png
	QString command = dotPath + QString(" -Tpng %1.gv > %2.png").arg(fname).arg(fname);
	qDebug() << "Executing: "  << command;
	system(qPrintable(command));
}

void FoldOptionGraph::clearCollisionLinks()
{
	foreach (Structure::Link* link, links)
	{
		if (link->properties["type"] == "collision")
		{
			Structure::Graph::removeLink(link);
		}
	}
}

bool FoldOptionGraph::hasFreeFoldOptions( QString cnid )
{
	bool isFree = false;
	foreach (FoldOption* fn, getFoldOptions(cnid))
	{
		if (getCollisionLinks(fn->mID).isEmpty())
		{
			isFree = true;
			break;
		}
	}

	return isFree;
}
