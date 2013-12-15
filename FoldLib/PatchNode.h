#pragma once

#include "FdNode.h"

class PatchNode : public FdNode
{
public:
    PatchNode(MeshPtr m, Geom::Box &b);
	~PatchNode();

public:
	void draw();
	void refit(int method);
	void createPatch();

public:
	Geom::Rectangle mPatch;

public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

