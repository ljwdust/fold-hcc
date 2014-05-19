#pragma once

#include "ChainGraph.h"
#include "SectorCylinder.h"

class TChain : public ChainGraph
{
public:
    TChain(PatchNode* master, FdNode* slave);

	// foldem
	QVector<FoldOption*> generateFoldOptions(int nSplits, int nUsedChunks, int nChunks);

	// fold info
	Geom::SectorCylinder getFoldingVolume(FoldOption* fn);

	// modifier
	QVector<Geom::Plane> generateCutPlanes( int nbSplit );
};
