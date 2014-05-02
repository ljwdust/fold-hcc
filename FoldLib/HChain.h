#pragma once

#include "ChainGraph.h"
#include "Rectangle2.h"

class HChain : public ChainGraph
{
public:
    HChain(FdNode* slave, PatchNode* master1, PatchNode* master2);

	// fold options
	QVector<FoldOption*> generateFoldOptions();
	void modify(FoldOption* fn);

	// fold region on master patch
	Geom::Rectangle2 getFoldRegion(FoldOption* fn);


	Geom::Segment2 getFoldingAxis2D(FoldOption* fn);

	Geom::Segment getJointSegment(FoldOption* fn);
};

