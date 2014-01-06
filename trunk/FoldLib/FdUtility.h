#pragma once

#include "UtilityGlobal.h"
#include "FdNode.h"
#include "PatchNode.h"
#include "Segment.h"
#include "Box.h"

Geom::Segment getDistSegment( FdNode* n1, FdNode* n2 );
double getDistance( FdNode* n1, FdNode* n2 );
double getDistance( FdNode* n, QVector<FdNode*> nset);

FdNodeArray2D clusterNodes( QVector<FdNode*> nodes, double disThr );

StrArray2D getIds(FdNodeArray2D nodeArray);

QVector<Geom::Segment> detectJointSegments(FdNode* part, PatchNode* panel);

double getLocalTime(double globalT, double localStart, double localEnd);
QVector<double> getEvenDivision(int n, double start = 0, double end = 1);

bool onPlane( FdNode* n, Geom::Plane& plane );

enum BOX_FIT_METHOD{FIT_AABB, FIT_MIN, FIT_PCA};
Geom::Box fitBox(QVector<Vector3>& pnts, BOX_FIT_METHOD method = FIT_PCA);