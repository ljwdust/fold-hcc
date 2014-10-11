#pragma once

#include "UnitScaff.h"
#include "HUnitScaff.h"

class ZUnitScaff final: public UnitScaff
{
public:
	ZUnitScaff(QString id, QVector<PatchNode*>& ms, QVector<ScaffNode*>& ss,
		QVector< QVector<QString> >& mPairs);

private:
	// decompose
	void sortMasters();
	void createChains(QVector<ScaffNode*>& ss, QVector< QVector<QString> >& mPairs);

	// two possible fold solutions
	void computeFoldSolution(bool toRight);
	void computeFoldRegionProj(bool toRight);

	// key frame as Z
	Scaffold* getZKeyframe(double t, bool useThk);

	// obstacles
	QVector<Vector2> computeObstacles(SuperShapeKf* ssKeyframe);

	// foldabilize as Z and returns the true if success
	bool foldabilizeZ(SuperShapeKf* ssKeyframe, TimeInterval ti);

public:
	// key frame
	virtual Scaffold* getKeyframe(double t, bool useThk) override;

	// foldabilization
	virtual double foldabilize(SuperShapeKf* ssKeyframe, TimeInterval ti) override;

	// setter
	virtual void setNbSplits(int n) override;
	virtual void setNbChunks(int n) override;
	virtual void setAabbCstr(Geom::Box aabb) override;
	virtual void setCostWeight(double w) override;
	virtual void setImportance(double imp) override;
	virtual void setThickness(double thk) override;

	// get obstacles
	virtual QVector<Vector3> getObstacles() override;
	virtual QVector<Geom::Rectangle> getAFRs() override;

private:
	// the back up HUnit
	HUnitScaff* hUnit;

	// the top master
	PatchNode* topMaster;

	// two possible fold solutions
	// right direction is defined as the rightDirect of the first chain
	bool					fold2Left,		fold2Right;
	QVector<FoldOption*>	optionsLeft,	optionsRight;
	Geom::Rectangle2		regionProjLeft, regionProjRight;

	// obstacles
	QVector<Vector3> obstPnts, obstPntsProj3;
};
