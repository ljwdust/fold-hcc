#pragma once
#include "LayerGraph.h"

class SandwichLayer : public LayerGraph
{
public:
    SandwichLayer(QVector<FdNode*> nodes, PatchNode* panel1, PatchNode* panel2, QString id);

public:
	PatchNode *mPanel1, *mPanel2;
};
