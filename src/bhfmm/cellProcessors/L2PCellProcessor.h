/*
 * L2PCellProcessor.h
 *
 *  Created on: Feb 10, 2015
 *      Author: tchipev
 */

#ifndef L2PCELLPROCESSOR_H_
#define L2PCELLPROCESSOR_H_
#include "particleContainer/adapter/CellProcessor.h"
#include "utils/Timer.h"
#include <stdlib.h>

namespace bhfmm {

class PseudoParticleContainer;

class L2PCellProcessor: public CellProcessor {
public:
	L2PCellProcessor(PseudoParticleContainer * pseudoParticleContainer);
	~L2PCellProcessor();

	virtual double processSingleMolecule(Molecule* /*m1*/, ParticleCell& /*cell2*/) {return 0.0;}
        virtual int countNeighbours(Molecule* /*m1*/, ParticleCell& /*cell2*/, double /*RR*/) { exit(0); return 0; }

	void initTraversal();
	void preprocessCell(ParticleCell& /*cell*/) {}
	void processCellPair(ParticleCell& /*cell1*/, ParticleCell& /*cell2*/) {}
	void processCell(ParticleCell& cell);
	void postprocessCell(ParticleCell& /*cell*/) {}
	void endTraversal();

	void printTimers();

private:
	PseudoParticleContainer* const _pseudoParticleContainer;
	Timer _L2PTimer;
};

} /* namespace bhfmm */

#endif /* L2PCELLPROCESSOR_H_ */
