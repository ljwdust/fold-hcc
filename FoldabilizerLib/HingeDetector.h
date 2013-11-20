#pragma once

#include "Node.h"
#include "Hinge.h"

class HingeDetector
{
public:
    HingeDetector();

	static QVector<Hinge*> getHinges(Node *n0, Node *n1);
	static QVector<Hinge*> getEdgeEdgeHinges(Node *n0, Node *n1);
	static QVector<Hinge*> getEdgeFaceHinges(Node *n0, Node *n1);
	static void getPerpAxisAndExtent(Box& box, Vector3 hinge_center, Vector3 hinge_axis, QVector<Vector3>& perpAxis, QVector<double> &perpExtent);
};