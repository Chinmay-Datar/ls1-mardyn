#include "DomainDecomposition.h"

#include "Domain.h"
#include "molecules/Molecule.h"
#include "particleContainer/ParticleContainer.h"
#include "utils/xmlfileUnits.h"
#include "utils/Logger.h"
#include "parallel/NeighbourCommunicationScheme.h"
#include "parallel/HaloRegion.h"
#include "ParticleData.h"

using Log::global_log;
using namespace std;

DomainDecomposition::DomainDecomposition() :
		DomainDecompMPIBase() {
	int period[DIMgeom]; // 1(true) when using periodic boundary conditions in the corresponding dimension
	int reorder; // 1(true) if the ranking may be reordered by MPI_Cart_create

	// We create a torus topology, so all boundary conditions are periodic
	for (int d = 0; d < DIMgeom; d++)
		period[d] = 1;

	// Allow reordering of process ranks
	reorder = 1;

	for (int i = 0; i < DIMgeom; i++) {
		_gridSize[i] = 0;
	}
	MPI_CHECK(MPI_Dims_create( _numProcs, DIMgeom, (int *) &_gridSize ));

	// Create the communicator
	MPI_CHECK(MPI_Cart_create(MPI_COMM_WORLD, DIMgeom, _gridSize, period, reorder, &_comm));
	global_log->info() << "MPI grid dimensions: " << _gridSize[0] << ", " << _gridSize[1] << ", " << _gridSize[2]
			<< endl;

	// introduce coordinates
	MPI_CHECK(MPI_Comm_rank(_comm, &_rank));
	MPI_CHECK(MPI_Cart_coords(_comm, _rank, DIMgeom, _coords));
	global_log->info() << "MPI coordinate of current process: " << _coords[0] << ", " << _coords[1] << ", "
			<< _coords[2] << endl;
}

DomainDecomposition::~DomainDecomposition() {
	MPI_Comm_free(&_comm);
}

void DomainDecomposition::initCommunicationPartners(double cutoffRadius, Domain * domain) {
	for (int d = 0; d < DIMgeom; ++d) {
		_neighbourCommunicationScheme->setCoverWholeDomain(d, _gridSize[d] == 1);
	}
	_neighbourCommunicationScheme->initCommunicationPartners(cutoffRadius, domain, this);
}

void DomainDecomposition::prepareNonBlockingStage(bool /*forceRebalancing*/, ParticleContainer* moleculeContainer,
		Domain* domain, unsigned int stageNumber) {
	if(moleculeContainer->sendLeavingAndHaloTogether()){
		DomainDecompMPIBase::prepareNonBlockingStageImpl(moleculeContainer, domain, stageNumber, LEAVING_AND_HALO_COPIES);
	}
	else {
		DomainDecompMPIBase::prepareNonBlockingStageImpl(moleculeContainer, domain, stageNumber, LEAVING_ONLY);
		DomainDecompMPIBase::prepareNonBlockingStageImpl(moleculeContainer, domain, stageNumber, HALO_COPIES);
	}
}

void DomainDecomposition::finishNonBlockingStage(bool /*forceRebalancing*/, ParticleContainer* moleculeContainer,
		Domain* domain, unsigned int stageNumber) {
	if(moleculeContainer->sendLeavingAndHaloTogether()){
		DomainDecompMPIBase::finishNonBlockingStageImpl(moleculeContainer, domain, stageNumber, LEAVING_AND_HALO_COPIES);
	}else{
		DomainDecompMPIBase::finishNonBlockingStageImpl(moleculeContainer, domain, stageNumber, LEAVING_ONLY);
		DomainDecompMPIBase::finishNonBlockingStageImpl(moleculeContainer, domain, stageNumber, HALO_COPIES);
	}
}

bool DomainDecomposition::queryBalanceAndExchangeNonBlocking(bool /*forceRebalancing*/,
		ParticleContainer* /*moleculeContainer*/, Domain* /*domain*/) {
	return true;
}

void DomainDecomposition::balanceAndExchange(bool /*forceRebalancing*/, ParticleContainer* moleculeContainer,
		Domain* domain) {
	if(moleculeContainer->sendLeavingAndHaloTogether()){
		DomainDecompMPIBase::exchangeMoleculesMPI(moleculeContainer, domain, LEAVING_AND_HALO_COPIES);
	}else{
		DomainDecompMPIBase::exchangeMoleculesMPI(moleculeContainer, domain, LEAVING_ONLY);
		DomainDecompMPIBase::exchangeMoleculesMPI(moleculeContainer, domain, HALO_COPIES);
	}
}

void DomainDecomposition::readXML(XMLfileUnits& xmlconfig) {
	/* TODO: Maybe add decomposition dimensions, default auto. */
	DomainDecompMPIBase::readXML(xmlconfig);
}

bool DomainDecomposition::procOwnsPos(double x, double y, double z, Domain* domain) {
	if (x < getBoundingBoxMin(0, domain) || x >= getBoundingBoxMax(0, domain))
		return false;
	else if (y < getBoundingBoxMin(1, domain) || y >= getBoundingBoxMax(1, domain))
		return false;
	else if (z < getBoundingBoxMin(2, domain) || z >= getBoundingBoxMax(2, domain))
		return false;
	else
		return true;
}

double DomainDecomposition::getBoundingBoxMin(int dimension, Domain* domain) {
	return _coords[dimension] * domain->getGlobalLength(dimension) / _gridSize[dimension];
}

double DomainDecomposition::getBoundingBoxMax(int dimension, Domain* domain) {
	return (_coords[dimension] + 1) * domain->getGlobalLength(dimension) / _gridSize[dimension];
}

void DomainDecomposition::printDecomp(string filename, Domain* domain) {

	if (_rank == 0) {
		ofstream povcfgstrm(filename.c_str());
		povcfgstrm << "size " << domain->getGlobalLength(0) << " " << domain->getGlobalLength(1) << " "
				<< domain->getGlobalLength(2) << endl;
		povcfgstrm << "cells " << _gridSize[0] << " " << _gridSize[1] << " " << _gridSize[2] << endl;
		povcfgstrm << "procs " << _numProcs << endl;
		povcfgstrm << "data DomainDecomp" << endl;
		povcfgstrm.close();
	}

	for (int process = 0; process < _numProcs; process++) {
		if (_rank == process) {
			ofstream povcfgstrm(filename.c_str(), ios::app);
			povcfgstrm << _coords[2] * _gridSize[0] * _gridSize[1] + _coords[1] * _gridSize[0] + _coords[0] << " "
					<< _rank << endl;
			povcfgstrm.close();
		}
		barrier();
	}
}

std::vector<int> DomainDecomposition::getNeighbourRanks() {
#if defined(ENABLE_MPI)
	std::vector<int> neighbours;
	if (_numProcs == 1) {
		for (int i = 0; i < 6; i++)
			neighbours.push_back(_rank);
	} else {
		neighbours = _neighbourCommunicationScheme->get3StageNeighbourRanks();
	}
	return neighbours;
#else
	return std::vector<int>(0);
#endif
}

/**
 * The key of this function is that opposite sites are always neighbouring each other in the array (i.e. leftAreaIndex = 0, righAreaIndex = 1, ...)
 *
 **/
std::vector<int> DomainDecomposition::getNeighbourRanksFullShell() {
	//order of ranks is important in current version!!!
#if defined(ENABLE_MPI) //evil hack to not destroy the necessary order
    int myRank;
	MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
	int numProcs;
	MPI_Comm_size(MPI_COMM_WORLD,&numProcs);
	std::vector<std::vector<std::vector<int>>> ranks = getAllRanks();
	int myCoords[3];
	MPI_Cart_coords(_comm, myRank, 3, myCoords);
	std::vector<int> neighbours(26,-1);
	if(numProcs == 1){
		for(int i = 0; i<26; i++)
			neighbours[i] = myRank;
	}
	else{
		for(int i = 0; i<26; i++){
			int x,y,z;
			switch(i)
			{
			case 0: //faces
				x=-1;y=0;z=0;break;
			case 1:
				x=1;y=0;z=0;break;
			case 2:
				x=0;y=-1;z=0;break;
			case 3:
				x=0;y=1;z=0;break;
			case 4:
				x=0;y=0;z=-1;break;
			case 5:
				x=0;y=0;z=1;break;
			case 6: //edges
				x=-1;y=-1;z=0;break;
			case 7:
				x=1;y=1;z=0;break;
			case 8:
				x=-1;y=1;z=0;break;
			case 9:
				x=1;y=-1;z=0;break;
			case 10:
				x=-1;y=0;z=-1;break;
			case 11:
				x=1;y=0;z=1; break;
			case 12:
				x=-1;y=0;z=1;break;
			case 13:
				x=1;y=0;z=-1;break;
			case 14:
				x=0;y=-1;z=-1;break;
			case 15:
				x=0;y=1;z=1;break;
			case 16:
				x=0;y=-1;z=1;break;
			case 17:
				x=0;y=1;z=-1;break;
			case 18:
				x=-1;y=-1;z=-1;break;
			case 19: //corners
				x=1;y=1;z=1;break;
			case 20:
				x=-1;y=-1;z=1;break;
			case 21:
				x=1;y=1;z=-1;break;
			case 22:
				x=-1;y=1;z=-1;break;
			case 23:
				x=1;y=-1;z=1;break;
			case 24:
				x=-1;y=1;z=1;break;
			case 25:
				x=1;y=-1;z=-1;break;
			}
			int coordsTemp[3];
			coordsTemp[0] = (myCoords[0] + x + ranks.size()) % ranks.size();
			coordsTemp[1] = (myCoords[1] + y + ranks[0].size()) % ranks[0].size();
			coordsTemp[2] = (myCoords[2] + z + ranks[0][0].size()) % ranks[0][0].size();
			int rank;
			MPI_Cart_rank(_comm, coordsTemp, &rank);
			neighbours[i] = rank;
		}
	}
	/*for(int i=0; i< 26;i++)
		std::cout << neighbours[i];*/
	std::cout << "\n";
	return neighbours;
#else
	return std::vector<int>(0);
#endif
	/* new version that does not work so far
#if defined(ENABLE_MPI)

	std::vector<int> neighbours(26, -1);
	if (_numProcs == 1) {
		for (int i = 0; i < 26; i++)
			neighbours[i] = _rank;
	} else {
		neighbours = _neighbourCommunicationScheme->getFullShellNeighbourRanks();
	}
	return neighbours;
#else
	return std::vector<int>(0);
#endif
*/
}


std::vector<std::vector<std::vector<int>>> DomainDecomposition::getAllRanks(){
#ifdef ENABLE_MPI
	std::vector<std::vector<std::vector<int>>> ranks;
	int myRank;
	MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
	int numProcessors;
	MPI_Comm_size(MPI_COMM_WORLD,&numProcessors);

	ranks.resize(_gridSize[0]);
	for(int i = 0; i < _gridSize[0]; i++){
		ranks[i].resize(_gridSize[1]);
		for(int j = 0; j < _gridSize[1]; j++){
			ranks[i][j].resize(_gridSize[2]);
		}
	}
	int coords[3];
	for(int i = 0; i < numProcessors; i++){
		MPI_Cart_coords(_comm, i, 3, coords);
		ranks[coords[0]][coords[1]][coords[2]] = i;
//		if(myRank == 0)
//		std:: cout << i << coords[0] << coords[1] << coords[2] << "\n";
	}
//	if(myRank == 0){
//		int previous, next;
//		MPI_CHECK( MPI_Cart_shift(_comm, 0, 1, &previous, &next ) );
//		if(next != ranks[1][0][0]){
//			std::cout << "Error!!!!!!! \n\n\n\n\n\n";
//		}
//	}

	return ranks;
#else
	return std::vector<std::vector<std::vector<int>>>(0);
#endif
}


std::vector<CommunicationPartner> DomainDecomposition::getNeighboursFromHaloRegion(Domain* domain, const HaloRegion& haloRegion,
		double cutoff) {
//TODO: change this method for support of midpoint rule, half shell, eighth shell, Neutral Territory
// currently only one process per region is possible.
	int rank;
	int regionCoords[DIMgeom];
	for (unsigned int d = 0; d < DIMgeom; d++) {
		regionCoords[d] = _coords[d] + haloRegion.offset[d];
	}
	//TODO: only full shell! (otherwise more neighbours possible)
	MPI_CHECK(MPI_Cart_rank(getCommunicator(), regionCoords, &rank)); //does automatic shift for periodic boundaries
	double haloLow[3];
	double haloHigh[3];
	double boundaryLow[3];
	double boundaryHigh[3];
	double shift[3];
	bool enlarged[3][2];

	for (unsigned int d = 0; d < DIMgeom; d++) {
		haloLow[d] = haloRegion.rmin[d];
		haloHigh[d] = haloRegion.rmax[d];
		//TODO: ONLY FULL SHELL!!!
		boundaryLow[d] = haloRegion.rmin[d] - haloRegion.offset[d] * cutoff; //rmin[d] if offset[d]==0
		boundaryHigh[d] = haloRegion.rmax[d] - haloRegion.offset[d] * cutoff; //if offset[d]!=0 : shift by cutoff in negative offset direction
		if (_coords[d] == 0 and haloRegion.offset[d] == -1) {
			shift[d] = domain->getGlobalLength(d);
		} else if (_coords[d] == _gridSize[d] - 1 and haloRegion.offset[d] == 1) {
			shift[d] = -domain->getGlobalLength(d);
		} else{
			shift[d] = 0.;
		}
		enlarged[d][0] = false;
		enlarged[d][1] = false;
	}
	// initialize using initializer list - here a vector with one element is created
	std::vector<CommunicationPartner> temp;
	temp.push_back(CommunicationPartner(rank, haloLow, haloHigh, boundaryLow, boundaryHigh, shift, haloRegion.offset, enlarged));
	return temp;
}

