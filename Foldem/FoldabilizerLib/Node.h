#pragma once

#include "Box.h"
#include "FoldabilizerLibGlobal.h"

#include "Link.h"

//class Link;

class Node
{
public:
	QString		mID;
	Box			mBox;
	QColor		mColor;

	bool		isFixed;		// tag used for propagation
	Vector3		originalExtent;
	double		scaleFactor;	// scale factor of box

    Node(Box b, QString id);
    ~Node();

public:
	// Geometry
	QVector<Point> getBoxConners();
	QVector< QVector<Point> > getBoxFaces();

	// Frame, coordinates
	Frame getFrame();
	void setFrame(Frame f);
	Vec3d dihedralDirection(Vec3d hinge_pos, Vec3d hinge_axis); 


	// Transformation
	void translate(Vector3 t);
	void changeScaleFactor();

	// Visualize
	void draw();
	void drawBox();
};

// Vertex ID of face corners
static uint cubeIds[6][4] = 
{
	1, 2, 6, 5,
	0, 4, 7, 3,
	4, 5, 6, 7,
	0, 3, 2, 1,
	0, 1, 5, 4,
	2, 3, 7, 6
};