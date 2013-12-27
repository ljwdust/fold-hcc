#pragma once

#include "UtilityGlobal.h"

namespace Geom{

enum {SEG_NEGATIVE, SEG_ON, SEG_POSITIVE, SEG_OFF};

class Segment
{
public:
	// constructor
	Segment(Vector3 p0 = Vector3(0,0,0), Vector3 p1 = Vector3(1,0,0));
	Segment(Vector3 c, Vector3 d, double e);

	// setter
	void set(Vector3 p0, Vector3 p1);
	void set(Vector3 c, Vector3 d, double e);
	void computeEndPoints();
	void computeCenterDirectionExtent();

	// modifier
	void flip();

	// coordinates
	double	getProjCoordinates(Vector3 p);
	Vector3 getPosition(double coord); 
	int		whichSide(Vector3 p);

	// projection
	Vector3 getProjection(Vector3 p);

	// relation with other
	bool isCollinearWith(Vector3 p);
	bool isCollinearWith(const Segment& other);
	bool overlaps(const Segment& other);
	bool contains(Vector3 p);
	bool contains(const Segment& other);

	// visualization
	void draw(double width = 3.0, QColor color = Qt::blue);

	// geometry
	double length();

public:
	// end points 
	// this is a open set (P0, P1)
	// which is useful for intersection
	Vector3 P0, P1;

	// center-extent representation
	Vector3 Center, Direction;
	double	Extent;

	// intersection info that can be queried
	// point intersection: IT0
	// segment intersection: IT0, IT1
	Vector3 IT0, IT1;
};

}