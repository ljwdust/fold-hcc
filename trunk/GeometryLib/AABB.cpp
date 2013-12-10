#include "AABB.h"
#include "Numeric.h"
#include "UtilityGlobal.h"

Geom::AABB::AABB()
{
}

Geom::AABB::AABB( QVector<Vector3>& pnts )
{
	buildFromPoints(pnts);       
}

Geom::AABB::AABB( SurfaceMeshModel* mesh)
{
	buildFromMesh(mesh);
}

SurfaceMesh::Vector3 Geom::AABB::center()
{
	return (bbmin + bbmax) * 0.5f;
}

double Geom::AABB::radius()
{
	return (bbmax - bbmin).norm() * 0.5f;
}

Geom::Box Geom::AABB::box()
{
	Box b;
	b.Center = center();
	b.Axis = XYZ();
	b.Extent = (bbmax - bbmin) * 0.5f;

	return b;
}

void Geom::AABB::buildFromPoints( QVector<Vector3>& pnts )
{
	bbmin = Point( FLT_MAX,  FLT_MAX,  FLT_MAX);
	bbmax = Point(-FLT_MAX, -FLT_MAX, -FLT_MAX);    


	foreach(Point p, pnts) {
		bbmin = minimize(bbmin, p);
		bbmax = maximize(bbmax, p);
	} 
}

void Geom::AABB::buildFromMesh( SurfaceMeshModel* mesh )
{
	buildFromPoints(getMeshVertices(mesh));
}
