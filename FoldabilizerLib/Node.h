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
	Vector3		scaleFactor;	// scale factor of box

    Node(Box b, QString id);
    ~Node();

public:
	// Geometry
	double	getVolume();
	Frame	getFrame();
	void	setFrame(Frame f);

	// Hinge property
	Vec3d dihedralDirection(Vec3d hinge_pos, Vec3d hinge_axis); 

	// Transformation
	void translate(Vector3 t);
	void fix();

	// Visualize
	bool isHighlight;
	void draw();
};                                 