#include "PerCellGenerator.h"

#include <iostream>
#include <random>

#include "Domain.h"
#include "IOHelpers.h"
#include "Simulation.h"
#include "ensemble/EnsembleBase.h"
#include "molecules/Molecule.h"
#include "parallel/DomainDecompBase.h"
#include "utils/Logger.h"
#include "utils/xmlfileUnits.h"

unsigned long PerCellGenerator::readPhaseSpace(ParticleContainer *particleContainer, Domain *domain,
											   DomainDecompBase *domainDecomp) {
	auto &componentVector = *global_simulation->getEnsemble()->getComponents();
	mardyn_assert(not componentVector.empty());
	Component *component = &(componentVector[0]);
	// The halo should not be filled when using readPhaseSpace, as the haloExchange is handled by the domain
	// decomposition!
	bool fillHalo = false;
	fillContainer(particleContainer, component, _numMoleculesPerCell, fillHalo);
	unsigned long totalNumParticles =
		IOHelpers::makeParticleIdsUniqueAndGetTotalNumParticles(particleContainer, domainDecomp);
	IOHelpers::initializeVelocityAccordingToTemperature(particleContainer, domainDecomp, _initTemperature);
	IOHelpers::removeMomentum(particleContainer, componentVector, domainDecomp);
	domain->setglobalNumMolecules(totalNumParticles);
	return totalNumParticles;
}

void PerCellGenerator::readXML(XMLfileUnits &xmlconfig) {
	Log::global_log->info() << "------------------------------------------------------------------------" << std::endl;
	Log::global_log->info() << "PerCellGenerator" << std::endl;

	xmlconfig.getNodeValue("numMoleculesPerCell", _numMoleculesPerCell);
	if (_numMoleculesPerCell > 0) {
		Log::global_log->info() << "numMoleculesPerCell: " << _numMoleculesPerCell << std::endl;
	} else {
		Log::global_log->error() << "Missing required field numMoleculesPerCell. Aborting!" << std::endl;
		Simulation::exit(1949);
	}

	xmlconfig.getNodeValue("initTemperature", _initTemperature);
	if (_numMoleculesPerCell > 0) {
		Log::global_log->info() << "initTemperature: " << _initTemperature << std::endl;
	} else {
		Log::global_log->error() << "Missing required field initTemperature. Aborting!" << std::endl;
		Simulation::exit(1949);
	}
}

void PerCellGenerator::fillContainer(ParticleContainer *particleContainer, Component *component,
									 unsigned int numMoleculesPerCell, bool fillHalo) {
	const auto cellLength = particleContainer->getCellLength();
	const auto boxMin = std::array{particleContainer->getBoundingBoxMin(0), particleContainer->getBoundingBoxMin(1),
								   particleContainer->getBoundingBoxMin(2)};
	const auto boxMax = std::array{particleContainer->getBoundingBoxMax(0), particleContainer->getBoundingBoxMax(1),
								   particleContainer->getBoundingBoxMax(2)};

	std::array<unsigned int, 3> numCells{};
	for (unsigned i = 0; i < 3; ++i) {
		numCells[i] = static_cast<unsigned int>(std::round((boxMax[i] - boxMin[i]) / cellLength[i]));
	}
	auto haloWidthInNumCells = fillHalo ? particleContainer->getHaloWidthNumCells() : 0;
#ifdef _OPENMP
#pragma omp parallel
#endif
	{
		std::random_device r;
		std::default_random_engine randomEngine{r()};
#ifdef _OPENMP
#pragma omp for
#endif
		for (long xind = -haloWidthInNumCells; xind < numCells[0] + haloWidthInNumCells; ++xind) {
			for (long yind = -haloWidthInNumCells; yind < numCells[1] + haloWidthInNumCells; ++yind) {
				for (long zind = -haloWidthInNumCells; zind < numCells[2] + haloWidthInNumCells; ++zind) {
					std::array<long, 3> cellInd3d{xind, yind, zind};
					std::array<double, 3> cellMin{};
					std::array<double, 3> cellMax{};
					long startID =
						xind * (numCells[1] + 2 * haloWidthInNumCells) * (numCells[2] + 2 * haloWidthInNumCells) +
						yind * (numCells[2] + 2 * haloWidthInNumCells) + zind;
					startID *= numMoleculesPerCell;
					bool isHalo = false;
					for (unsigned int i = 0; i < 3; ++i) {
						if (cellInd3d[i] == -1) {
							// lower halo
							cellMin[i] = boxMin[i] + cellInd3d[i] * cellLength[i];
							cellMax[i] = boxMin[i];
							isHalo = true;
						} else if (cellInd3d[i] == numCells[i] - 1) {
							// upper non-halo
							cellMin[i] = boxMin[i] + cellInd3d[i] * cellLength[i];
							cellMax[i] = boxMax[i];
						} else if (cellInd3d[i] == numCells[i]) {
							// upper halo
							cellMin[i] = boxMax[i];
							cellMax[i] = boxMin[i] + cellInd3d[i] * cellLength[i];
							isHalo = true;
						} else {
							// lower non-halo, or normal.
							cellMin[i] = boxMin[i] + cellInd3d[i] * cellLength[i];
							// cell position is calculated based on boxMin!
							cellMax[i] = boxMin[i] + (cellInd3d[i] + 1) * cellLength[i];
						}
					}
					std::array uniform_dists{std::uniform_real_distribution<double>{cellMin[0], cellMax[0]},
											 std::uniform_real_distribution<double>{cellMin[1], cellMax[1]},
											 std::uniform_real_distribution<double>{cellMin[2], cellMax[2]}};
					for (unsigned int particleI = 0; particleI < numMoleculesPerCell; ++particleI) {
						auto id = startID + particleI;
						std::array pos = {uniform_dists[0](randomEngine), uniform_dists[1](randomEngine),
										  uniform_dists[2](randomEngine)};

						Molecule m(id, component, pos[0], pos[1], pos[2], 0., 0., 0.);
						if (isHalo) {
							particleContainer->addHaloParticle(m, true);
						} else {
							particleContainer->addParticle(m, true);
						}
					}
				}
			}
		}
	}
#ifndef NDEBUG
	// statistics[i] contains the number of cell with i particles. (no halo cells are included!)
	auto statistics = particleContainer->getParticleCellStatistics();
	// We check that in every cell there are exactly numMoleculesPerCell particles, i.e., only particles with
	// numMoleculesPerCell particles exist.
	for (size_t i = 0; i < statistics.size(); ++i) {
		if (i != numMoleculesPerCell) {
			mardyn_assert(statistics[i] == 0ul);
		}
	}
#endif
}
