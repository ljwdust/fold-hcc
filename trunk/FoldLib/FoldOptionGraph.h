#pragma once
#include "Graph.h"
#include "FdUtility.h"

class ChainNode : public Structure::Node
{
public:
	ChainNode(int idx, QString id);
	ChainNode(ChainNode &other);
	~ChainNode();
	Node* clone();

	// chain index
	int chainIdx; 
};

class FoldOption : public Structure::Node
{
public:
	FoldOption( QString id, bool right, double s, double p, int n, double area);
	FoldOption(FoldOption &other);
	~FoldOption();
	Node* clone();

	double getCost(double w);

public:
	// to which side to fold
	bool rightSide;

	// shrinking
	double scale;
	double position;

	// #splits: produce nSplits+1 parts in a chain
	int nSplits;

	// chain Idx
	int chainIdx;

	// duration
	Interval duration;

	// fold region and its projection on the base of block
	Geom::Rectangle region;
	Geom::Rectangle2 regionProj;

	// patch area
	double patchArea;

public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class FoldOptionGraph : public Structure::Graph
{
public:
	FoldOptionGraph(QString id = "");
	FoldOptionGraph(FoldOptionGraph& other);
	~FoldOptionGraph();
	Graph* clone();

	// modifier
	void addNode(ChainNode* cn);
	void addNode(FoldOption* fn);
	void addFoldLink(Structure::Node* n1, Structure::Node* n2);
	void addCollisionLink(Structure::Node* n1, Structure::Node* n2);

	void clearCollisionLinks();

	// verifier
	bool verifyNodeType(QString nid, QString type);
	bool verifyLinkType(QString nid1, QString nid2, QString type);
	bool areSiblings(QString nid1, QString nid2);
	bool hasFreeFoldOptions(QString cnid);

	// getter
	ChainNode*		getChainNode(QString fnid);		// chain node of a folding node
	QVector<ChainNode*>	getAllChainNodes();		
	QVector<FoldOption*>	getAllFoldOptions();	
	QVector<FoldOption*>	getSiblings(QString fnid);		// siblings of folding node
	QVector<FoldOption*>	getFoldOptions(QString cnid);	// fold options of a chain node
	QVector<Structure::Node*> getFamilyNodes(QString nid);

	QVector<Structure::Link*> getFoldinglinks(QString nid);
	QVector<Structure::Link*> getCollisionLinks(QString nid);
	QVector<Structure::Link*> getFamilyCollisionLinks(QString nid);

	// visualize
	QString toGraphvizFormat(QString subcaption, QString caption);
	void saveAsGraphviz(QString fname, QString subcaption = "", QString caption = "");
	void saveAsImage(QString fname);

public:
	static QString dotPath;
};
