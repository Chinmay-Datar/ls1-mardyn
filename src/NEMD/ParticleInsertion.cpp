/*
 * ParticleInsertion.cpp
 *
 *  Created on: 12.01.2018
 *      Author: mheinen
 */

#include "ParticleInsertion.h"
#include "utils/Random.h"
#include "utils/Region.h"
#include "parallel/DomainDecompBase.h"
#include "Simulation.h"
#include "Domain.h"
#include "ensemble/EnsembleBase.h"
#include "molecules/Component.h"
#include "NEMD/DensityControl.h"

#include <cstdlib>
#include <iostream>
#include <ctime>
#include <vector>

template<typename T1, typename T2>
void select_rnd_elements(std::list<T1>& mylist, std::vector<T1>& myvec, T2 numSelect)
{
	myvec.clear();
	if(numSelect < 1)
		return;
	uint64_t numElements = mylist.size();
	uint64_t numElementsSub = numElements / numSelect;
	T2 numResidual = numElements % numSelect;

	std::vector<std::vector<T1> > mat;
	mat.resize(numSelect);
	for(T2 ci=0; ci<numSelect; ++ci)
	{
		if(ci<numResidual)
			mat.at(ci).resize(numElementsSub+1);
		else
			mat.at(ci).resize(numElementsSub);

		for(auto&& eli : mat.at(ci))
		{
			eli = mylist.front();
			mylist.pop_front();
		}

		std::srand(std::time(nullptr));
		int rnd = rand()%numElementsSub;
		myvec.push_back(mat.at(ci).at(rnd) );
	}
}

// class ParticleManipDirector
ParticleManipDirector::ParticleManipDirector(dec::ControlRegion* region)
	:
	_region(region),
	_deleter(nullptr),
	_manipulator(nullptr)
{
	uint32_t numComps = global_simulation->getEnsemble()->getComponents()->size()+1;
	_inserter.resize(numComps);
//	_inserter.at(0) = nullptr;
	for(auto&& ins:_inserter)
		ins = new BubbleMethod(this, BMT_INSERTER);

	// deleter
	_deleter = new ParticleDeleter(this);

	// changer
	_changer.resize(numComps);
//	_changer.at(0) = nullptr;
	for(auto&& cit:_changer)
		cit = new BubbleMethod(this, BMT_CHANGER);

	// template methods
	DensityControl* ptrDC = dynamic_cast<DensityControl*>(region->GetParent() );
	_preForceAction = ptrDC->getPreForceAction();
	_postForceAction = ptrDC->getPostForceAction();
}

void ParticleManipDirector::readXML(XMLfileUnits& xmlconfig)
{
	string oldpath = xmlconfig.getcurrentnodepath();

	// set target variables for all components
	if(xmlconfig.changecurrentnode("targets")) {
		uint8_t numNodes = 0;
		XMLfile::Query query = xmlconfig.query("target");
		numNodes = query.card();
		global_log->info() << "Number of targets: " << (uint32_t)numNodes << endl;
		if(numNodes < 1) {
			global_log->error() << "No targets defined in XML-config file. Program exit ..." << endl;
			Simulation::exit(-1);
		}
		XMLfile::Query::const_iterator nodeIter;
		for( nodeIter = query.begin(); nodeIter; nodeIter++ ) {
			xmlconfig.changecurrentnode(nodeIter);
			int32_t cid = -1;
			xmlconfig.getNodeValue("@cid", cid);
			if(-1 == cid) {
				global_log->error() << "Missing attribute 'cid' in target. Program exit ..." << endl;
				Simulation::exit(-1);
			}
			else {
				// inserter
				if(xmlconfig.changecurrentnode("insertion") ) {
					_inserter.at(cid)->readXML(xmlconfig);
					_inserter.at(cid)->setTargetCompID(cid);
					xmlconfig.changecurrentnode("..");
				}

				// changer
				if(xmlconfig.changecurrentnode("change") ) {
					_changer.at(cid)->readXML(xmlconfig);
					_changer.at(cid)->setTargetCompID(cid);
					xmlconfig.changecurrentnode("..");
				}
			}
		}
	}
	xmlconfig.changecurrentnode(oldpath);
}

void ParticleManipDirector::localValuesReseted(Simulation* simulation)
{
	if(nullptr != _manipulator)
		_manipulator->Reset(simulation);
}

void ParticleManipDirector::globalValuesCalculated(Simulation* simulation)
{
	if(nullptr != _manipulator)
	{
		if(_manipulator->getManipState() == PMS_BUSY)
			return;
	}

	// global density > target value
	if(true == _region->globalTargetDensityExeeded(0) )
	{
		_manipulator = _deleter;
		global_log->info() << "DELETER activated" << endl;
	}
	else if(true == _region->globalTargetDensityUndershot(0) )
	{
		bool bValidIDs = this->setNextInsertIDs();
		if(true == bValidIDs)
		{
//			_manipulator = _inserter.at(_nextChangeIDs.from);
			_manipulator = nullptr;
			global_log->info() << "INSERTER activated" << endl;
		}
		else
			_manipulator = nullptr;
	}
	else
	{
		// check if particle identities have to be changed
		if(false == _region->globalCompositionBalanced() )
		{
			bool bValidIDs = this->setNextChangeIDs();
			if(true == bValidIDs)
			{
				_manipulator = _changer.at(_nextChangeIDs.from);
				global_log->info() << "CHANGER activated" << endl;
			}
			else
				_manipulator = nullptr;
		}
		else
			_manipulator = nullptr;
	}
	if(nullptr != _manipulator)
		_manipulator->PrepareParticleManipulation(simulation);
}

void ParticleManipDirector::ManipulateParticles(Simulation* simulation, Molecule* mol)
{
	if(nullptr != _manipulator)
		_manipulator->ManipulateParticles(simulation, mol);
}

void ParticleManipDirector::ManipulateParticleForces(Simulation* simulation, Molecule* mol)
{
	if(nullptr != _manipulator)
		_manipulator->ManipulateParticleForces(simulation, mol);
}

void ParticleManipDirector::FinalizeParticleManipulation(Simulation* simulation, MainLoopAction* action)
{
	if(nullptr != _manipulator)
	{
		if(action == _postForceAction)
			_manipulator->FinalizeParticleManipulation(simulation);
		else if(action == _preForceAction)
			_manipulator->FinalizeParticleManipulation_preForce(simulation);
	}
}

std::vector<dec::CompVarsStruct> ParticleManipDirector::getCompVars()
{
	return _region->getCompVars();
}

double ParticleManipDirector::GetLowerCorner(uint32_t nDim)
{
	return _region->GetLowerCorner(nDim);
}

double ParticleManipDirector::GetWidth(uint32_t nDim)
{
	return _region->GetWidth(nDim);
}

std::list<uint64_t> ParticleManipDirector::GetLocalParticleIDs(const uint32_t& nCompID)
{
	return _region->GetLocalParticleIDs(nCompID);
}

int64_t ParticleManipDirector::getLocalNumMoleculesSpread(uint32_t nCompID)
{
	return _region->getLocalNumMoleculesSpread(nCompID);
}

bool ParticleManipDirector::setNextChangeIDs()
{
	bool bRet = true;
	uint32_t numComps = global_simulation->getEnsemble()->getComponents()->size()+1;
	uint32_t cidMinSpread, cidMaxSpread;
	_region->getGlobalMinMaxNumMoleculesSpreadCompIDs(cidMinSpread, cidMaxSpread);
	_nextChangeIDs.from = cidMaxSpread;
	_nextChangeIDs.to   = cidMinSpread;
	bRet = bRet && _nextChangeIDs.from > 0 && _nextChangeIDs.from < numComps;
	bRet = bRet && _nextChangeIDs.to   > 0 && _nextChangeIDs.to   < numComps;
	return bRet;
}

bool ParticleManipDirector::setNextInsertIDs()
{
	bool bRet = true;
	uint32_t numComps = global_simulation->getEnsemble()->getComponents()->size()+1;
	uint32_t cidMinSpread, cidMaxSpread;
	_region->getGlobalMinMaxNumMoleculesSpreadCompIDs(cidMinSpread, cidMaxSpread);
	_nextChangeIDs.from = _nextChangeIDs.to = cidMinSpread;
	bRet = bRet && _nextChangeIDs.from > 0 && _nextChangeIDs.from < numComps;
	bRet = bRet && _nextChangeIDs.to   > 0 && _nextChangeIDs.to   < numComps;
	return bRet;
}

// class ParticleDeleter
ParticleDeleter::ParticleDeleter(ParticleManipDirector* director)
	:
	ParticleManipulator(director)
{
	uint32_t numComps = global_simulation->getEnsemble()->getComponents()->size()+1;
	_deletionLists.resize(numComps);
}

void ParticleDeleter::PrepareParticleManipulation(Simulation* simulation)
{
	_nManipState = PMS_BUSY;
	this->CreateDeletionLists(_director->getCompVars() );
}

void ParticleDeleter::ManipulateParticles(Simulation* simulation, Molecule* mol)
{
	ParticleContainer* particleCont = simulation->getMolecules();

	uint64_t mid = mol->id();
	uint64_t cid = mol->componentid()+1;
	bool bDeleteMolecule = false;
	for(auto did:_deletionLists.at(cid) )
	{
		if(did == mid)
			bDeleteMolecule = true;
	}
	if(true == bDeleteMolecule)
		particleCont->deleteMolecule(*mol, true);
}

void ParticleDeleter::CreateDeletionLists(std::vector<dec::CompVarsStruct> compVars)
{
	DomainDecompBase domainDecomp = global_simulation->domainDecomposition();
	uint8_t numComps = compVars.size();
	CommVar<uint64_t> numDel[numComps];
	if(compVars.at(0).numMolecules.spread.global > 0)
		numDel[0].global = compVars.at(0).numMolecules.spread.global;
	else
	{
		for(uint8_t cid=0; cid<numComps; ++cid)
			_deletionLists.at(cid).clear();
		return;
	}
	CommVar<int64_t> positiveSpread[numComps];
	CommVar<int64_t> positiveSpreadSumOverComp;
	positiveSpreadSumOverComp.local  = 0;
	positiveSpreadSumOverComp.global = 0;

	for(uint8_t cid=1; cid<numComps; ++cid)
	{
		CommVar<int64_t> spread;
		// sum over components
		spread.global = compVars.at(cid).numMolecules.spread.global;
		if(spread.global > 0)
			positiveSpreadSumOverComp.global += spread.global;
		// sum over processes
		spread.local = compVars.at(cid).numMolecules.spread.local;
		if(spread.local > 0)
			positiveSpread[cid].local = spread.local;
		else
			positiveSpread[cid].local = 0;

	#ifdef ENABLE_MPI
		MPI_Allreduce( &positiveSpread[cid].local, &positiveSpread[cid].global, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
	#else
		positiveSpread[cid].global = positiveSpread[cid].local;
	#endif
	}

	CommVar<double> dInvPositiveSpread[numComps];
	CommVar<double> dInvPositiveSpreadSumOverComp;
	dInvPositiveSpreadSumOverComp.global = 1./( (double) (positiveSpreadSumOverComp.global) );

	for(uint8_t cid=1; cid<numComps; ++cid)
	{
		CommVar<int64_t> spread;
		// global
		spread.global = compVars.at(cid).numMolecules.spread.global;
		if(spread.global > 0)
			numDel[cid].global = round(spread.global * dInvPositiveSpreadSumOverComp.global * numDel[0].global);
		else
			numDel[cid].global = 0;
		// local
		dInvPositiveSpread[cid].global = 1./( (double) (positiveSpread[cid].global) );
		spread.local = compVars.at(cid).numMolecules.spread.local;
		cout << "Rank[" << domainDecomp.getRank() << "]: spread.local["<<(uint32_t)(cid)<<"] = " << spread.local << endl;
		if(spread.local > 0)
		{
			numDel[cid].local = round(spread.local * dInvPositiveSpread[cid].global * numDel[cid].global);
			cout << "Rank[" << domainDecomp.getRank() << "]: numDel["<<(uint32_t)(cid)<<"].local = " << numDel[cid].local << endl;
			select_rnd_elements(compVars.at(cid).particleIDs, _deletionLists.at(cid), numDel[cid].local);
		}
		else
			numDel[cid].local = 0;

		//DEBUG
		cout << "Rank[" << domainDecomp.getRank() << "]compVars.at("<<(uint32_t)(cid)<<").deletionList:";
		for(auto mid:_deletionLists.at(cid) )
		{
			cout << " " << mid;
		}
		cout << endl;
		//DEBUG
	}
}

ParticleInsertion::ParticleInsertion(ParticleManipDirector* director, uint32_t state)
	:
	ParticleManipulator(director),
	_nState(state),
	_nTargetCompID(0)
{
}

BubbleMethod::BubbleMethod(ParticleManipDirector* director, uint32_t nType)
	:
	ParticleInsertion(director, BMS_IDLE),
	_selector(nullptr),
	_insRank(0),
	_maxID(100000000),
	_selectedMoleculeID(0),
	_nType(nType)
{
	_selector = new CompDependSelector(this);

	// init pbc vector
	Domain* domain = global_simulation->getDomain();
	double box[3];
	for(int16_t d=0; d<3; ++d)
		box[d] = domain->getGlobalLength(d);
	_pbc.initial.resize(27);
	int index = 0;
	for(int16_t xi=-1; xi<2; ++xi)
		for(int16_t yi=-1; yi<2; ++yi)
			for(int16_t zi=-1; zi<2; ++zi)
			{
				_pbc.initial.at(index).at(0) = xi*box[0];
				_pbc.initial.at(index).at(1) = yi*box[1];
				_pbc.initial.at(index).at(2) = zi*box[2];
				index++;
				cout << index << ":("<<xi*box[0]<<","<<yi*box[1]<<","<<zi*box[2]<<")" << endl;
			}
}

BubbleMethod::~BubbleMethod()
{
}

void BubbleMethod::readXML(XMLfileUnits& xmlconfig)
{
	xmlconfig.getNodeValue("maxforce", _bubble.force.maxVal);
	xmlconfig.getNodeValue("bubbleradius", _bubble.radius.target.global);
	_bubble.radius.target.local = _bubble.radius.actual.local = _bubble.radius.target.global;
	_bubble.radiusSquared.target.global = _bubble.radius.target.global*_bubble.radius.target.global;
	xmlconfig.getNodeValue("velocity", _bubble.velocity);

	if(this->checkBubbleRadius() == false)
	{
		global_log->error() << "BubbleMethod::readXML(): Bubble radius to large! Must be smaller than (0.5*boxLength - maxOuterMoleculeRadius)! Program exit ..." << endl;
		Simulation::exit(-1);
	}

	DomainDecompBase domainDecomp = global_simulation->domainDecomposition();
	int ownRank = domainDecomp.getRank();
	cout << "rank[" << ownRank << "]: _bubble.radius.actual.global=" << _bubble.radius.actual.global << endl;
	cout << "rank[" << ownRank << "]: _bubble.radius.target.global=" << _bubble.radius.target.global << endl;
	cout << "rank[" << ownRank << "]: _bubble.radius.actual.local=" << _bubble.radius.actual.local << endl;
	cout << "rank[" << ownRank << "]: _bubble.radius.target.local=" << _bubble.radius.target.local << endl;

	// insertion molecules
	{
		string oldpath = xmlconfig.getcurrentnodepath();
		if(xmlconfig.changecurrentnode("molecules")) {
			uint8_t numNodes = 0;
			XMLfile::Query query = xmlconfig.query("molecule");
			numNodes = query.card();
			global_log->info() << "Number of insertion molecules: " << (uint32_t)numNodes << endl;
			if(numNodes < 1) {
				global_log->error() << "BubbleMethod::readXML(): No insertion molecules defined in XML-config file. Program exit ..." << endl;
				Simulation::exit(-1);
			}
			XMLfile::Query::const_iterator nodeIter;
			for( nodeIter = query.begin(); nodeIter; nodeIter++ ) {
				xmlconfig.changecurrentnode(nodeIter);
				double x, y, z, vx, vy, vz, q0, q1, q2, q3, Dx, Dy, Dz;
				x, y, z, vx, vy, vz, q0, q1, q2, q3, Dx, Dy, Dz = 0.0;
				xmlconfig.getNodeValue("x", x);
				xmlconfig.getNodeValue("y", y);
				xmlconfig.getNodeValue("z", z);
				xmlconfig.getNodeValue("vx", vx);
				xmlconfig.getNodeValue("vy", vy);
				xmlconfig.getNodeValue("vz", vz);
				xmlconfig.getNodeValue("q0", q0);
				xmlconfig.getNodeValue("q1", q1);
				xmlconfig.getNodeValue("q2", q2);
				xmlconfig.getNodeValue("q3", q3);
				xmlconfig.getNodeValue("Dx", Dx);
				xmlconfig.getNodeValue("Dy", Dy);
				xmlconfig.getNodeValue("Dz", Dz);
				Molecule mol(0, nullptr, x, y, z, vx, vy, vz, q0, q1, q2, q3, Dx, Dy, Dz);
				_insertMolecules.initial.push_back(mol);
				_insertMolecules.actual.push_back(mol);
			}
		}
		xmlconfig.changecurrentnode(oldpath);
	}  // insertion molecules
}

bool BubbleMethod::checkBubbleRadius()
{
	std::vector<Component>* ptrComps = global_simulation->getEnsemble()->getComponents();
	double maxOuterMoleculeRadius = 0.0;
	for(auto cit:*ptrComps)
	{
		double tmp = cit.getOuterMoleculeRadiusLJ();
		if(tmp > maxOuterMoleculeRadius)
			maxOuterMoleculeRadius = tmp;
	}
	Domain* domain = global_simulation->getDomain();
	bool bValidRadius = true;
	for(uint8_t d=0; d<3; ++d)
		bValidRadius = (bValidRadius && (_bubble.radius.target.global < (0.5*domain->getGlobalLength(d)-maxOuterMoleculeRadius) ) );
	return bValidRadius;
}

void BubbleMethod::Reset(Simulation* simulation)
{
	// reset local values
	_numManipulatedParticles.local = 0;
	_bubble.radius.actual.local = _bubble.radius.target.global;

	ParticleContainer* particleCont = simulation->getMolecules();
	ParticleIterator pit;
	for( pit  = particleCont->iteratorBegin();
			pit != particleCont->iteratorEnd();
		 ++pit )
	{
		this->resetBubbleMoleculeComponent(&(*pit) );
	}
	// reset bubble molecules vector
	_bubbleMolecules.clear();
}

void BubbleMethod::PrepareParticleManipulation(Simulation* simulation)
{
	_nManipState = PMS_BUSY;

	// reset local values
	this->Reset(simulation);

	ChangeVar<uint32_t> nextChangeIDs = _director->getNextChangeIDs();
	_nextChangeIDs.from = nextChangeIDs.from;
	_nextChangeIDs.to = nextChangeIDs.to;
	this->selectParticle(simulation);
	if(BMT_INSERTER == _nType)
		this->initInsertionMolecules(simulation);
}

void BubbleMethod::ManipulateParticles(Simulation* simulation, Molecule* mol)
{
	this->FreezeSelectedMolecule(mol);
}

void BubbleMethod::ManipulateParticleForces(Simulation* simulation, Molecule* mol)
{
	this->GrowBubble(simulation, mol);
}

void BubbleMethod::FreezeSelectedMolecule(Molecule* mol)
{
	if(_nState != BMS_GROWING_BUBBLE)
		return;

	if(mol->id() == _selectedMoleculeID)
	{
		for(uint8_t d=0; d<3; ++d)
			mol->setr(d, _selectedMoleculeInitPos.at(d) );
	}
}

void BubbleMethod::resetBubbleMoleculeComponent(Molecule* mol)
{
	if(_nState != BMS_GROWING_BUBBLE)
		return;

	for(auto mit:_bubbleMolecules)
	{
		if(mol->id() == mit.mid)
		{
			Component* comp = global_simulation->getEnsemble()->getComponent(mit.cid);
			mol->setComponent(comp);
		}
	}
}

double BubbleMethod::calcMinSquaredDistance(Molecule* mol)
{
	double dOuterMoleculeRadiusLJ = mol->component()->getOuterMoleculeRadiusLJ();
	double dist2_min = (_bubble.radius.target.global+dOuterMoleculeRadiusLJ)*(_bubble.radius.target.global+dOuterMoleculeRadiusLJ);
	return dist2_min;
}

bool BubbleMethod::outerMoleculeRadiusCutsBubbleRadius(Simulation* simulation, Molecule* mol, const double& dist2_min, double& dist2, double* distVec)
{
	Domain* domain = simulation->getDomain();
	double box[3];
	for(uint8_t d=0; d<3; ++d)
		box[d] = domain->getGlobalLength(d);

//	cout << "box: " << box[0] << ", " << box[1] << ", " << box[2] << endl;

	bool bCutsBubbleRadius = false;
	for(auto pbc:_pbc.initial)
	{
		dist2 = 0.0;
		for(uint8_t d=0; d<3; ++d)
		{
			distVec[d] = mol->r(d) - (_selectedMoleculeInitPos.at(d)+pbc.at(d) );
			dist2 += distVec[d]*distVec[d];
		}
		if(dist2 < dist2_min)
		{
			bCutsBubbleRadius = true;
			break;
		}
	}
	return bCutsBubbleRadius;
}

void BubbleMethod::updateActualBubbleRadiusLocal(Molecule* mol, const double& dist)
{
	double tmp = dist - mol->component()->getOuterMoleculeRadiusLJ();
	if(tmp < _bubble.radius.actual.local)
		_bubble.radius.actual.local = tmp;
}

void BubbleMethod::updateForceOnBubbleCuttingMolecule(Molecule* mol, const double& dist2_min, const double& dist2, const double& dist, double* distVec)
{
	double invDist = 1./dist;
	double Fadd[3];
	for(uint8_t d=0; d<3; ++d)
		Fadd[d] = distVec[d] * _bubble.force.maxVal * (1-dist2/dist2_min);
	mol->Fadd(Fadd);
}

void BubbleMethod::GrowBubble(Simulation* simulation, Molecule* mol)
{
	DomainDecompBase domainDecomp = simulation->domainDecomposition();
	int ownRank = domainDecomp.getRank();
//	cout << "rank[" << ownRank << "]: GrowBubble(), state:" << _nState << endl;

	if(_nState != BMS_GROWING_BUBBLE)
	{
//		cout << "postEventNewTimestepAction(), state:" << _nState << endl;
		return;
	}

	{
		// skip selected molecule, but update its position
		if(mol->id() == _selectedMoleculeID)
		{
			cout << "rank[" << ownRank << "] BubbleMethod::GrowBubble: _selectedMoleculeID="<<_selectedMoleculeID<< endl;
			Component* comp4 = global_simulation->getEnsemble()->getComponent(3);  // selected
			ID_pair ids;
			ids.mid = mol->id();
			ids.cid = mol->componentid();
			_bubbleMolecules.push_back(ids);
			mol->setComponent(comp4);

			return;
		}
	//	else
	//		global_log->error() << "Selected molecule NOT found!" << endl;

		/*
		 * Change components for visualization
		 *
		 */
//		Component* comp2 = global_simulation->getEnsemble()->getComponent(1);  // H2
//		if(mol->componentid()+1 == 5)
//			mol->setComponent(comp2);


		double distVec[3] = {0.0, 0.0, 0.0};
		double dist2 = 0.0;
		double dist2_min = calcMinSquaredDistance(mol);
		bool bCutsBubbleRadius = this->outerMoleculeRadiusCutsBubbleRadius(simulation, mol, dist2_min, dist2, distVec);
		if(false == bCutsBubbleRadius)
			return;
		else
		{
			Component* comp5 = global_simulation->getEnsemble()->getComponent(4);  // inside_bubble
			ID_pair ids;
			ids.mid = mol->id();
			ids.cid = mol->componentid();
			_bubbleMolecules.push_back(ids);
			mol->setComponent(comp5);
		}

		double dist = sqrt(dist2);
		this->updateActualBubbleRadiusLocal(mol, dist);
		this->updateForceOnBubbleCuttingMolecule(mol, dist2_min, dist2, dist, distVec);

//		// DEBUG
//		dist2 = 0.0;
//		for(uint8_t d=0; d<3; ++d)
//		{
//			distVec[d] = pit->r(d) - _selectedMoleculePos.at(d);
//			dist2 += distVec[d]*distVec[d];
//		}
//		dist = sqrt(dist2);
//		cout << "GrowBubble(), id: " << pit->id() << ", after replacement: " << _dReplacement << ", dist:" << dist << endl;
//		// DEBUG

		_numManipulatedParticles.local++;
	}
}

void BubbleMethod::ChangeIdentity(Simulation* simulation, Molecule* mol)
{
	// --> CHANGE_IDENTITY
	{
		std::vector<Component>* ptrComps = simulation->getEnsemble()->getComponents();
		Component* compOld = mol->component();
		uint32_t newID = _nextChangeIDs.to-1;
		if(newID > ptrComps->size() )
		{
			global_log->error() << "Component ID (1st Component: ID=1): " << newID+1 << " does NOT exist! Program exit ..." << endl;
			Simulation::exit(-1);
		}
		Component* compNew = &(ptrComps->at(newID) );

		uint8_t numRotDOF_old = compOld->getRotationalDegreesOfFreedom();
		uint8_t numRotDOF_new = compNew->getRotationalDegreesOfFreedom();
		double dUkinOld = mol->U_kin();
		double dUkinPerDOF = dUkinOld / (3 + numRotDOF_old);

		// rotation
		double U_rot = mol->U_rot();
		global_log->info() << "U_rot_old = " << U_rot << endl;

		double L[3];
		double Ipa[3];

		Ipa[0] = compNew->I11();
		Ipa[1] = compNew->I22();
		Ipa[2] = compNew->I33();

		for(uint8_t dim=0; dim<3; ++dim)
		{
			L[dim] = sqrt(dUkinPerDOF * 2. * Ipa[dim] );
			mol->setD(dim, L[dim] );
		}

#ifndef NDEBUG
		cout << "L[0] = " << L[0] << endl;
		cout << "L[1] = " << L[1] << endl;
		cout << "L[2] = " << L[2] << endl;
#endif
		Quaternion q(1., 0., 0., 0.);
		mol->setq(q);

		mol->setComponent(compNew);
//		mol->clearFM();  // <-- necessary?
#ifndef NDEBUG
		cout << "Changed cid of molecule " << mol->id() << " from: " << _nextChangeIDs.from << " to: " << mol->componentid()+1 << endl;
#endif

		// update transl. kin. energy
		double dScaleFactorTrans = sqrt(6*dUkinPerDOF/compNew->m()/mol->v2() );
		mol->scale_v(dScaleFactorTrans);

		U_rot = mol->U_rot();
		global_log->info() << "U_rot_new = " << U_rot << endl;

		/*
		//connection to MettDeamon
		if(NULL != _mettDeamon)
			_mettDeamon->IncrementChangedMoleculesLocal();
		*/
	}
	// <-- CHANGE_IDENTITY
}

void BubbleMethod::FinalizeParticleManipulation(Simulation* simulation)
{
	DomainDecompBase domainDecomp = simulation->domainDecomposition();
	int ownRank = domainDecomp.getRank();
	ParticleContainer* particleCont = global_simulation->getMolecules();

#ifdef ENABLE_MPI
	MPI_Allreduce( &_numManipulatedParticles.local, &_numManipulatedParticles.global, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce( &_bubble.radius.actual.local, &_bubble.radius.actual.global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
#else
	_numManipulatedParticles.global = _numManipulatedParticles.local;
	_bubble.radius.actual.global = _bubble.radius.actual.local;
#endif

	cout << "rank[" << ownRank << "]: _numManipulatedParticles.global=" << _numManipulatedParticles.global << endl;
	cout << "rank[" << ownRank << "]: _bubble.radius.actual.global=" << _bubble.radius.actual.global << endl;
	cout << "rank[" << ownRank << "]: _bubble.radius.target.global=" << _bubble.radius.target.global << endl;
	cout << "rank[" << ownRank << "]: BubbleMethod::FinalizeParticleManipulation(): _selectedMoleculeID="<<_selectedMoleculeID<< endl;
	cout << "rank[" << ownRank << "]: BubbleMethod::FinalizeParticleManipulation(): _nState="<<_nState<< endl;

//	if( !(BMS_GROWING_BUBBLE == _nState && _numManipulatedParticles.global == 0) )

	bool bBubbleSizeReached = (_bubble.radius.actual.global >= _bubble.radius.target.global) && BMS_GROWING_BUBBLE == _nState;
	cout << "rank[" << ownRank << "]: BubbleMethod::FinalizeParticleManipulation(): bBubbleSizeReached="<<bBubbleSizeReached<< ", targetID=" << this->getTargetCompID() << endl;
	if(false == bBubbleSizeReached)
		return;

	if(ownRank == _insRank)
	{
		if(BMT_CHANGER == this->getType() )
		{
			ParticleContainer* particles = global_simulation->getMolecules();
			ParticleIterator pit;
			for( pit  = particles->iteratorBegin();
					pit != particles->iteratorEnd();
				 ++pit )
			{
				if(pit->id() == _selectedMoleculeID) //componentid()+1 == 3)
				{
					this->ChangeIdentity(simulation, &(*pit) );
				}
			}
		}
		else if(BMT_INSERTER == this->getType() )
		{
			ParticleContainer* particles = global_simulation->getMolecules();
			ParticleIterator pit;
			for( pit  = particles->iteratorBegin();
					pit != particles->iteratorEnd();
				 ++pit )
			{
				if(pit->id() == _selectedMoleculeID) //componentid()+1 == 3)
				{
					particleCont->deleteMolecule(*pit, false);
					break;
				}
			}
	//		particleCont->addParticles(_insertMolecules, false);
			for(auto&& mol:_insertMolecules.actual)
			{
				cout << "Adding particle..." << endl;
				cout << mol;
				particleCont->addParticle(mol, true, true, true);
				cout << "... added!" << endl;
			}
		}
	}
	// update maxID
#ifdef ENABLE_MPI
	MPI_Bcast( &_maxID, 1, MPI_UNSIGNED_LONG, _insRank, MPI_COMM_WORLD);
#endif
	_nState = BMS_IDLE;
	_nManipState = PMS_IDLE;
	return;

	/*
	 * change component for visualization
	 *
	 */
	Component* comp2 = global_simulation->getEnsemble()->getComponent(1);  // H2
	Component* comp6 = global_simulation->getEnsemble()->getComponent(5);  // H2 insert

	ParticleContainer* particles = global_simulation->getMolecules();
	ParticleIterator pit;
	double dBubbleRadius = _bubble.radius.target.global;
	double vm = _bubble.velocity;
	for( pit  = particles->iteratorBegin();
			pit != particles->iteratorEnd();
		 ++pit )
	{
		if(pit->id() == _selectedMoleculeID) //componentid()+1 == 3)
		{
//			pit->setComponent(comp6);
			pit->setr(0, pit->r(0)-dBubbleRadius*0.5);
			pit->setv(0, -vm);
		}
//		else if(pit->componentid()+1 == 4)
//			pit->setComponent(comp2);
	}

	if(ownRank == _insRank)
	{
		double rx, ry, rz;
		rx = _selectedMoleculeInitPos.at(0);
		ry = _selectedMoleculeInitPos.at(1);
		rz = _selectedMoleculeInitPos.at(2);
		Molecule mol1(++_maxID, comp6, rx+dBubbleRadius*0.5, ry, rz, vm, 0, 0);
		Molecule mol2(++_maxID, comp6, rx, ry+dBubbleRadius*0.5, rz, 0,  vm, 0);
		Molecule mol3(++_maxID, comp6, rx, ry-dBubbleRadius*0.5, rz, 0, -vm, 0);
		Molecule mol4(++_maxID, comp6, rx, ry, rz+dBubbleRadius*0.5, 0, 0,  vm);
		Molecule mol5(++_maxID, comp6, rx, ry, rz-dBubbleRadius*0.5, 0, 0, -vm);

		cout << "rank[" << ownRank << "]: postLoopAction() adding particle at: " << rx << ", " << ry << ", " << rz << endl;
		particleCont->addParticle(mol1, true, true, true);
		particleCont->addParticle(mol2, true, true, true);
		particleCont->addParticle(mol3, true, true, true);
		particleCont->addParticle(mol4, true, true, true);
		particleCont->addParticle(mol5, true, true, true);
	}

	// update maxID
#ifdef ENABLE_MPI
	MPI_Bcast( &_maxID, 1, MPI_UNSIGNED_LONG, _insRank, MPI_COMM_WORLD);
#endif

	_nState = BMS_IDLE;
}

void BubbleMethod::FinalizeParticleManipulation_preForce(Simulation* simulation)
{
}

void BubbleMethod::selectParticle(Simulation* simulation)
{
	if(_nState != BMS_IDLE)
		return;

	DomainDecompBase* domainDecomp = &(simulation->domainDecomposition() );
	Molecule selectedMolecule;
	_insRank = _selector->selectParticle(simulation, selectedMolecule);
//	Component* comp4 = global_simulation->getEnsemble()->getComponent(3);  // selected
//	selectedMolecule.setComponent(comp4);
	uint64_t selectedMoleculeID = 0;
	double dPos[3] = {0.0, 0.0, 0.0};
	cout << "BubbleMethod::selectParticle: selectedMolecule.id()=" << selectedMolecule.id() << endl;
	if(domainDecomp->getRank() == _insRank)
	{
//		if(nullptr == selectedMolecule)
//		{
//			global_log->error() << "BubbleMethod::selectParticle: Condition (nullptr == selectedMolecule) failed! Programm exit ..." << endl;
//			Simulation::exit(-1);
//		}
		selectedMoleculeID = selectedMolecule.id();
		for(uint8_t d=0; d<3; ++d)
			dPos[d] = selectedMolecule.r(d);
	}
#ifdef ENABLE_MPI
	MPI_Bcast( &selectedMoleculeID, 1, MPI_UNSIGNED_LONG, _insRank, MPI_COMM_WORLD);
	MPI_Bcast( dPos, 3, MPI_DOUBLE, _insRank, MPI_COMM_WORLD);
#endif
	// store selected molecule values
	_selectedMoleculeID = selectedMoleculeID;
	for(uint8_t d=0; d<3; ++d)
		_selectedMoleculeInitPos.at(d) = selectedMolecule.r(d);

	// inform director about selected particle
//	_director->
	_nState = BMS_GROWING_BUBBLE;
}

void BubbleMethod::initInsertionMolecules(Simulation* simulation)
{
	// velocity
	Random rnd;
	float vi[3];
	for(uint8_t d=0; d<3; ++d)
		vi[d] = rnd.rnd();

	for(size_t mi=0; mi<_insertMolecules.actual.size(); ++mi)
	{
		for(uint8_t d=0; d<3; ++d)
			_insertMolecules.actual.at(mi).setr(d, _insertMolecules.initial.at(mi).r(d) );
	}

	std::vector<Component>* ptrComps = simulation->getEnsemble()->getComponents();
	Component* compIns = &(ptrComps->at(_nTargetCompID-1) );

	int16_t flip = -1;  // TODO: approach only for two insert molecules
	for(auto&& mol:_insertMolecules.actual)
	{
		flip *= flip;
		mol.setid(++_maxID);
		mol.setComponent(compIns);
		for(uint8_t d=0; d<3; ++d)
		{
			mol.setr(d, mol.r(d) + _selectedMoleculeInitPos.at(d) );
			mol.setv(d, vi[d]*_bubble.velocity*flip);
		}
	}
}

// Selector info
double BubbleMethod::GetLowerCorner(uint32_t nDim)
{
	return _director->GetLowerCorner(nDim);
}

double BubbleMethod::GetWidth(uint32_t nDim)
{
	return _director->GetLowerCorner(nDim);
}

std::list<uint64_t> BubbleMethod::GetLocalParticleIDs(const uint32_t& nCompID)
{
	return _director->GetLocalParticleIDs(nCompID);
}

int64_t BubbleMethod::getLocalNumMoleculesSpread(uint32_t nCompID)
{
	return _director->getLocalNumMoleculesSpread(nCompID);
}

// class RandomSelector : public ParticleSelector
RandomSelector::RandomSelector(BubbleMethod* parent)
	:
	ParticleSelector(parent),
	_random(nullptr)
{
	_random = new Random;
}

void RandomSelector::generateRandomInsPos()
{
	for(uint8_t d=0; d<3; ++d) {
		float rnd = _random->rnd();
		_daInsertionPosition.at(d) = _insRegLowerCorner.at(d) + rnd * _insRegWidth.at(d);
	}
}

int RandomSelector::selectParticle(Simulation* simulation, Molecule& selectedMolecule)
{
	this->collectInfo();

	DomainDecompBase domainDecomp = simulation->domainDecomposition();
	int ownRank = domainDecomp.getRank();
	cout << "rank[" << ownRank << "]: selectParticle()" << endl;

	Domain* domain = global_simulation->getDomain();
	double bbMin[3];
	double bbMax[3];
	double insPos[3];

	domainDecomp.getBoundingBoxMinMax(domain, bbMin, bbMax);

	this->generateRandomInsPos();

	ParticleContainer* particles = simulation->getMolecules();
	uint64_t numMols = particles->getNumberOfParticles();

	// find molecule with min distance to insertion position
	cout << "rank[" << ownRank << "]: find molecule with min distance to insertion position" << endl;
	double dist2_min = 1000;

	ParticleIterator pit;
	for( pit  = particles->iteratorBegin();
			pit != particles->iteratorEnd();
		 ++pit )
	{
		double dist2 = 0.0;
		double distVec[3];
		for(uint8_t d=0; d<3; ++d)
		{
			distVec[d] = pit->r(d) - insPos[d];
			dist2 += distVec[d]*distVec[d];
		}
		if(dist2 < dist2_min)
		{
			dist2_min = dist2;
			selectedMolecule = *pit;
		}
	}

	struct {
		double val;
		int   rank;
	} in, out;

	in.val = dist2_min;
	in.rank = ownRank;

#ifdef ENABLE_MPI
	MPI_Allreduce( &in, &out, 1, MPI_DOUBLE_INT, MPI_MINLOC, MPI_COMM_WORLD);
#else
	out.rank = ownRank;
#endif
	cout << "rank["<<ownRank<<"]: dist2_min="<<dist2_min<<endl;

	return out.rank;
}

void RandomSelector::collectInfo()
{
	for(uint8_t d=0; d<3; ++d)
	{
		_insRegLowerCorner.at(d) = _parent->GetLowerCorner(d);
		_insRegWidth.at(d) = _parent->GetWidth(d);
	}
}


// class CompDependSelector : public ParticleSelector
CompDependSelector::CompDependSelector(BubbleMethod* parent)
	:
	ParticleSelector(parent)
{
}

int CompDependSelector::selectParticle(Simulation* simulation, Molecule& selectedMolecule)
{
	DomainDecompBase domainDecomp = global_simulation->domainDecomposition();
	int ownRank = domainDecomp.getRank();
	cout << "ownRank=" << ownRank << endl;
	ChangeVar<uint32_t> nextChangeIDs = _parent->getNextChangeIDs();
	std::list<uint64_t> particleIDsFrom = _parent->GetLocalParticleIDs(nextChangeIDs.from);
	std::list<uint64_t> particleIDsTo   = _parent->GetLocalParticleIDs(nextChangeIDs.to);
	int64_t spreadDiff = 0;
	if(particleIDsFrom.size() > 0)
	{
		int64_t spreadFrom = _parent->getLocalNumMoleculesSpread(nextChangeIDs.from);
		int64_t spreadTo   = _parent->getLocalNumMoleculesSpread(nextChangeIDs.to);
		if(nextChangeIDs.from != nextChangeIDs.to)  // change
			spreadDiff = spreadFrom - spreadTo;
		else
			spreadDiff = spreadFrom*-1;  // insert
	}
	cout << "spreadDiff=" << spreadDiff << endl;

	struct {
		int64_t val;
		int    rank;
	} in, out;

	in.val = spreadDiff;
	in.rank = ownRank;

#ifdef ENABLE_MPI
	MPI_Allreduce( &in, &out, 1, MPI_LONG_INT, MPI_MAXLOC, MPI_COMM_WORLD);
#else
	out.rank = ownRank;
#endif

	uint64_t selectedID = 0;
	if(ownRank == out.rank)
	{
		std::vector<uint64_t> selectedIDs;
		if(particleIDsFrom.size() < 1)
		{
			global_log->error() << "CompDependSelector::selectParticle: Condition (particleIDsFrom.size() < 1) failed! Program exit ..." << endl;
			Simulation::exit(-1);
		}
		select_rnd_elements(particleIDsFrom, selectedIDs, 1);
		if(selectedIDs.size() != 1)
		{
			global_log->error() << "CompDependSelector::selectParticle: Condition (selectedIDs.size() != 1) failed! Program exit ..." << endl;
			Simulation::exit(-1);
		}
		selectedID = selectedIDs.at(0);
	}

//	selectedID = 23;  // DEBUG
	cout << "selectedID=" << selectedID << endl;

	ParticleContainer* particles = simulation->getMolecules();
	ParticleIterator pit;
	for( pit  = particles->iteratorBegin();
			pit != particles->iteratorEnd();
		 ++pit )
	{
		uint64_t id = pit->id();
		if(selectedID == id)
		{
			selectedMolecule = *pit;
//			cout << "selectedMolecule=" << selectedMolecule << endl;
			break;
		}
	}
//	cout << "out.rank=" << out.rank << endl;
//	cout << "selectedMolecule.id()=" << selectedMolecule.id() << endl;
	return out.rank;
}

void CompDependSelector::collectInfo()
{

}
