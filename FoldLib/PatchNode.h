#pragma once

#include "ScaffNode.h"

class RodNode;

class PatchNode : public ScaffNode
{
public:
    PatchNode(QString id, Geom::Box &b, MeshPtr m, Vector3 v = Vector3(0, 0, 0));
	PatchNode(PatchNode &other);
	PatchNode(RodNode* rodNode, Vector3 v);
	virtual ~PatchNode();
	virtual Node* clone() override;

public:
	// create patch
	virtual void createScaffold(bool useAid) override;

	// visual
	virtual void drawScaffold() override;

	// geometry
	double getThickness();
	Geom::Plane getSurfacePlane(bool positive);
	Geom::Rectangle getSurfaceRect(bool positive);
	void resize(Geom::Rectangle2& newPatch);
	virtual bool isPerpTo(Vector3 v, double dotThreshold) override;

	// edges
	QVector<RodNode*> getEdgeRodNodes();

	// samples
	virtual QVector<Vector3> sampleBoundabyOfScaffold(int n) override;

public:
	Geom::Rectangle mPatch;

public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class SuperPatchNode : public PatchNode
{
public:
	SuperPatchNode(QString id, PatchNode* other)
		: PatchNode(*other){	
		mID = id; 
	}
	SuperPatchNode(SuperPatchNode& other) 
		: PatchNode(other){
		enclosedPatches = other.enclosedPatches;
	}
	virtual ~SuperPatchNode(){}
	Node* clone() {	
		return new SuperPatchNode(*this); 
	}

public:
	QSet<QString> enclosedPatches;
};
