#include "HingeDetector.h"
#include "Segment.h"
#include "Box2.h"
#include "Numeric.h"

HingeDetector::HingeDetector()
{
}


QVector<Hinge*> HingeDetector::getHinges( Node *n0, Node *n1 )
{
	QVector<Hinge*> hinges;

	return hinges;
}

void HingeDetector::getPerpAxisAndExtent( Box& box, Vector3 hinge_center, Vector3 hinge_axis, QVector<Vector3>& perpAxis, QVector<double> &perpExtent )
{
	perpAxis.clear();
	perpExtent.clear();

	Vec3d h2n = (box.Center - hinge_center).normalized();

	for(int i = 0; i < 3; i++){
		if (isPerp(box.Axis[i], hinge_axis))
		{
			Vector3 dd = box.Axis[i];
			if (dot(dd, h2n) < 0) dd *= -1;

			perpAxis.push_back(dd);
			perpExtent.push_back(box.Extent[i]);
		}
	}
}

QVector<Hinge*> HingeDetector::getEdgeEdgeHinges( Node *n0, Node *n1 )
{
	QVector<Hinge*> hinges;

	Box&				box0 = n0->mBox;
	Box&				box1 = n1->mBox;
	QVector<Segment>	edges0 = box0.getEdgeSegments();
	QVector<Segment>	edges1 = box1.getEdgeSegments();

	for (int i = 0; i < 12; i++)
	{
		for (int j = 0; j < 12; j++)
		{
			Segment& e0_i = edges0[i];
			Segment& e1_j = edges1[j];

			// create a hinge if two edges overlap
			// to-do: only when one edge contains the other
			if (e0_i.overlaps(e1_j))
			{
				// center and hz
				Vector3 it0 = e0_i.IT0;
				Vector3 it1 = e0_i.IT1;
				Vector3 hcenter = (it0 + it1) / 2;
				Vector3 hz = e0_i.Direction;

				// data used to determine hx and hy
				QVector<Vector3>	perpAxis0, perpAxis1;
				QVector<double>		perpExtent0, perpExtent1;
				getPerpAxisAndExtent(box0, hcenter, hz, perpAxis0, perpExtent0);
				getPerpAxisAndExtent(box1, hcenter, hz, perpAxis1, perpExtent1);
				if (perpAxis0.size() != 2 || perpAxis1.size() != 2)	continue;

				// hx has larger extent
				Vector3 hx = perpAxis0[0], hx_perp = perpAxis0[1];
				if (perpExtent0[0] < perpExtent0[1]) std::swap(hx, hx_perp);

				// hy
				// HxCrossHxPerp and HyCrossHyPerp have opposite directions 
				Vector3 hy = perpAxis1[0], hy_perp = perpAxis1[1];
				Vector3 HxCrossHxPerp = cross(hx, hx_perp);
				Vector3 HyCrossHyPerp = cross(hy, hy_perp);
				if (dot(HxCrossHxPerp, HyCrossHyPerp) > 0) hy = hy_perp;

				// update hz if need
				// HxCrossHxPerp and hz have opposite directions
				if (dot(HxCrossHxPerp, hz) > 0) hz *= -1;

				hinges.push_back(new Hinge(n0, n1, hcenter, hx, hy, hz, M_PI));
			}
		}
	}

	return hinges;
}

QVector<Hinge*> HingeDetector::getEdgeFaceHinges( Node *n0, Node *n1 )
{
	QVector<Hinge*> hinges;

	Box&				box0 = n0->mBox;
	Box&				box1 = n1->mBox;
	QVector<Segment>	edges0 = box0.getEdgeSegments();
	QVector<Segment>	edges1 = box1.getEdgeSegments();
	QVector<Box2>	faces0 = box0.getFaceRectangles();
	QVector<Box2>	faces1 = box1.getFaceRectangles();

	// Edge0-Face1 test
	for (int i = 0; i < 12; i ++)
	{
		for (int j = 0; j < 6; j++)
		{
			Segment& e_i = edges0[i];
			Box2& f_j = faces1[j];

			// create a hinge if f_j contains e_i
			if (f_j.contains(e_i))
			{
			}
		}
	}

	return hinges;
}