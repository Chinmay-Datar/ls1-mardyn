
#include <iostream>
#include <string>
#include <cmath>
#include <sys/stat.h>

#include "Domain.h"

#include "particleContainer/ParticleContainer.h"
#include "parallel/DomainDecompBase.h"
#include "molecules/Molecule.h"
#include "ensemble/GrandCanonical.h"
#include "ensemble/PressureGradient.h"
#include "CutoffCorrections.h"
#include "Simulation.h"
#include "ensemble/EnsembleBase.h"

#include "io/VisitWriter.h"

#include "utils/Logger.h"
using Log::global_log;

using namespace std;


Domain::Domain(int rank, PressureGradient* pg){
	_localRank = rank;
	_localUpot = 0;
	_localVirial = 0;   
	for(int d=0; d < 3; d++) this->_localVirialI[d] = 0.0;
        this->_localVirialIILL = 0.0;
        this->_localVirialIILM = 0.0;
	_globalUpot = 0;
	_globalVirial = 0; 
        for(int d=0; d < 3; d++) this->_globalVirialI[d] = 0.0;
        this->_globalVirialIILL = 0.0;
        this->_globalVirialIILM = 0.0;
	_globalRho = 0;

	this->_universalPG = pg;

	this->_componentToThermostatIdMap = map<int, int>();
	this->_localThermostatN = map<int, unsigned long>();
	this->_localThermostatN[-1] = 0;
	this->_localThermostatN[0] = 0;
	this->_universalThermostatN = map<int, unsigned long>();
	this->_universalThermostatN[-1] = 0;
	this->_universalThermostatN[0] = 0;
	this->_localRotationalDOF = map<int, unsigned long>();
	this->_localRotationalDOF[-1] = 0;
	this->_localRotationalDOF[0] = 0;
	this->_universalRotationalDOF = map<int, unsigned long>();
	this->_universalRotationalDOF[-1] = 0;
	this->_universalRotationalDOF[0] = 0;
	this->_globalLength[0] = 0;
	this->_globalLength[1] = 0;
	this->_globalLength[2] = 0;
	this->_universalCentre[0] = 0.0;
	this->_universalCentre[1] = 0.0;
	this->_universalCentre[2] = 0.0;
	this->_universalBTrans = map<int, double>();
	this->_universalBTrans[0] = 1.0;
	this->_universalThT_heatFlux[0] = 0.0;
	this->_universalThT_heatFlux[1] = 0.0;
	this->_universalThT_heatFlux[2] = 0.0;
	this->_universalThT_heatFlux[3] = 0.0;
	this->_preShearRateConf = 0.0;
	this->_postShearRateConf = 0.0;
	this->_preShearRateDefault = 0.0;
	this->_postShearRateDefault = 0.0;
	this->_shearVelDelta = 0.0;
	this->_universalBRot = map<int, double>();
	this->_universalBRot[0] = 1.0;
	this->_universalTargetTemperature = map<int, double>();
	this->_universalTargetTemperature[0] = 1.0;
	this->_globalTemperatureMap = map<int, double>();
	this->_globalTemperatureMap[0] = 1.0;
	this->_local2KETrans[0] = 0.0;
	this->_local2KERot[0] = 0.0;

	this->_universalNVE = false;
	this->_globalUSteps = 0;
	this->_globalSigmaU = 0.0;
	this->_globalSigmaUU = 0.0;
#ifdef COMPLEX_POTENTIAL_SET
	this->_universalConstantAccelerationTimesteps = 30;
	if(!rank)
		for(int d = 0; d < 3; d++)
			this->_globalVelocitySum[d] = map<unsigned, long double>();
#endif
	// one-dimensional thermostat
	this->_componentwiseThermostat = false;
	this->_dimToThermostat = map<int, int>();
	this->_dimToThermostat[0] = 0;
	this->_scale_v_1Dim = map<int, bool>();
	this->_alphaTransCorrection = map<int, bool>();
	this->_stressCalc = map<int, bool>();
	for(unsigned cid = 0; cid < getNumberOfComponents(); cid++){
	  this->_stressCalc[cid] = false;
	  this->_universalProfiledComponents[cid] = false;
	  this->_universalProfiledComponentsSlab[cid] = false;
	  this->_bulkComponent[cid] = false;
	  this->_barostatComponent[cid] = false;
	  this->_confinementComponent[cid] = false;
	  this->_confinementComponentHardy[cid] = false;
	}
	this->_differentBarostatInterval = false;
	this->_enableThermostatLayers = false;
	this->_enableThermostatWallLayers = false;
	for(int cid = 0; cid < maxThermostat(); cid++){
	  this->_scale_v_1Dim[cid] = false;
	  this->_alphaTransCorrection[cid] = false;
	}
	
	this->_isConfinement = false;
	this->_confinementAreaFixed = false;
	this->_confinementRecordingTimesteps = 0.0;
	this->_stressRecordingTimesteps = 0.0;
	this->_local2KETrans_1Dim[0] = 0.0;
	this->_universalATrans = map<int, double>();
	this->_universalATrans[0] = 1.0;
	
	this->_doFixRegion = false;
	this->_doFixEvery = false;
	this->_isGravity = false;
	
	this->_confinementMidPointID[0] = 0;
	this->_confinementMidPointID[1] = 0;
	
	this->_matlab = "matlab";
	this->_vtk = "vtk";
	this->_all = "all";
	
#ifdef COMPLEX_POTENTIAL_SET
	this->_universalUndirectedThermostat = map<int, bool>();
	for(int d = 0; d < 3; d++)
	{
		this->_universalThermostatDirectedVelocity[d] = map<int, double>();
		this->_localThermostatDirectedVelocity[d] = map<int, double>();
	}
#endif
	this->_universalSelectiveThermostatCounter = 0;
	this->_universalSelectiveThermostatWarning = 0;
	this->_universalSelectiveThermostatError = 0;
}

void Domain::readXML(XMLfileUnits& xmlconfig) {
	string originalpath = xmlconfig.getcurrentnodepath();

	/* volume */
	if ( xmlconfig.changecurrentnode( "volume" )) {
		std::string type;
		xmlconfig.getNodeValue( "@type", type );
		global_log->info() << "Volume type: " << type << endl;
		if( type == "box" ) {
			xmlconfig.getNodeValueReduced( "lx", _globalLength[0] );
			xmlconfig.getNodeValueReduced( "ly", _globalLength[1] );
			xmlconfig.getNodeValueReduced( "lz", _globalLength[2] );
			global_log->info() << "Box size: " << _globalLength[0] << ", "
				<< _globalLength[1] << ", "
				<< _globalLength[2] << endl;
		}
		else {
			global_log->error() << "Unsupported volume type " << type << endl;
		}
	}
	xmlconfig.changecurrentnode(originalpath);

	/* temperature */
	double temperature = 0.;
	xmlconfig.getNodeValueReduced("temperature", temperature);
	setGlobalTemperature(temperature);
	global_log->info() << "Temperature: " << temperature << endl;
	xmlconfig.changecurrentnode(originalpath);

}

void Domain::setLocalUpot(double Upot) {_localUpot = Upot;}

double Domain::getLocalUpot() const {return _localUpot; }

void Domain::setLocalVirial(double Virial)
{
   _localVirial = Virial;
   this->_localVirialI[0] = Virial/3.0;
   this->_localVirialI[1] = Virial/3.0;
   this->_localVirialI[2] = Virial/3.0;
   this->_localVirialIILL = 0.0;
   this->_localVirialIILM = 0.0;
}

void Domain::setLocalVirial(double VIx, double VIy, double VIz, double VIILL, double VIILM)
{
   _localVirial = VIx+VIy+VIz;
   this->_localVirialI[0] = VIx;
   this->_localVirialI[1] = VIy;
   this->_localVirialI[2] = VIz;
   this->_localVirialIILL = VIILL;
   this->_localVirialIILM = VIILM;
}


double Domain::getLocalVirial() const {return _localVirial; }

/* methods accessing thermostat info */
double Domain::getGlobalBetaTrans() { return _universalBTrans[0]; }
double Domain::getGlobalBetaTrans(int thermostat) { return _universalBTrans[thermostat]; }
double Domain::getGlobalBetaRot() { return _universalBRot[0]; }
double Domain::getGlobalBetaRot(int thermostat) { return _universalBRot[thermostat]; }
double Domain::getGlobalAlphaTrans() { return _universalATrans[0]; }
double Domain::getGlobalAlphaTrans(int thermostat) { return _universalATrans[thermostat]; }

void Domain::setLocalSummv2(double summv2, int thermostat)
{
#ifndef NDEBUG
	global_log->debug() << "* local thermostat " << thermostat << ":  mvv = " << summv2 << endl;
#endif
	this->_local2KETrans[thermostat] = summv2;
}

void Domain::setLocalSumIw2(double sumIw2, int thermostat)
{
	_local2KERot[thermostat] = sumIw2;
} 

double Domain::getGlobalPressure()
{
	double globalTemperature = _globalTemperatureMap[0];
	return globalTemperature * _globalRho + _globalRho * getAverageGlobalVirial()/3.;
}

double Domain::getAverageGlobalVirial() const { return _globalVirial/_globalNumMolecules; }

double Domain::getAverageGlobalUpot() const { return _globalUpot/_globalNumMolecules; }

Comp2Param& Domain::getComp2Params(){
	return _comp2params; 
}

void Domain::calculateGlobalValues(
		DomainDecompBase* domainDecomp,
		ParticleContainer* particleContainer,
		bool collectThermostatVelocities,
		double Tfactor
		) {
	double Upot = _localUpot;
	double Virial = _localVirial;
        double VIx = this->_localVirialI[0];
        double VIy = this->_localVirialI[1];
        double VIz = this->_localVirialI[2];
        double VIILL = this->_localVirialIILL;
        double VIILM = this->_localVirialIILM;

	// To calculate Upot, Ukin and Pressure, intermediate values from all      
	// processes are needed. Here the         
	// intermediate values of all processes are summed up so that the root    
	// process can calculate the final values. to be able to calculate all     
	// values at this point, the calculation of the intermediate value sum_v2  
	// had to be moved from Thermostat to upd_postF and the final calculations  
	// of m_Ukin, m_Upot and Pressure had to be moved from Thermostat / upd_F  
	// to this point           
	
	/* FIXME stuff for the ensemble class */
	domainDecomp->collCommInit(7);
	domainDecomp->collCommAppendDouble(Upot);
	domainDecomp->collCommAppendDouble(Virial);
	domainDecomp->collCommAppendDouble(VIx);
        domainDecomp->collCommAppendDouble(VIy);
        domainDecomp->collCommAppendDouble(VIz);
        domainDecomp->collCommAppendDouble(VIILL);
        domainDecomp->collCommAppendDouble(VIILM);
	domainDecomp->collCommAllreduceSum();
	Upot = domainDecomp->collCommGetDouble();
	Virial = domainDecomp->collCommGetDouble();
	VIx = domainDecomp->collCommGetDouble();
        VIy = domainDecomp->collCommGetDouble();
        VIz = domainDecomp->collCommGetDouble();
        VIILL = domainDecomp->collCommGetDouble();
        VIILM = domainDecomp->collCommGetDouble();
	domainDecomp->collCommFinalize();
	/* FIXME: why should process 0 do this alone? 
	 * we should keep symmetry of all proccesses! */
	// Process 0 has to add the dipole correction:
	// m_UpotCorr and m_VirialCorr already contain constant (internal) dipole correction
	_globalUpot = Upot + _UpotCorr;
	_globalVirial = Virial + _VirialCorr;
	_globalVirialI[0] = VIx + _VirialCorr/3.0;
        _globalVirialI[1] = VIy + _VirialCorr/3.0;
        _globalVirialI[2] = VIz + _VirialCorr/3.0;
        _globalVirialIILL = VIILL;
        _globalVirialIILM = VIILM;

	/*
	 * thermostat ID 0 represents the entire system
	 */
	map<int, unsigned long>::iterator thermit;
	if( _componentwiseThermostat || _enableThermostatLayers || _enableThermostatWallLayers)
	{ 
#ifndef NDEBUG
		global_log->debug() << "* applying a componentwise thermostat" << endl;
#endif
		this->_localThermostatN[0] = 0;
		this->_localRotationalDOF[0] = 0;
		this->_local2KETrans[0] = 0;
		this->_local2KERot[0] = 0;
		
		for(thermit = _localThermostatN.begin(); thermit != _localThermostatN.end(); thermit++)
		    if(_scale_v_1Dim[thermit->first]){
			this->_local2KETrans_1Dim[0] = 0;
		    }

		for(thermit = _localThermostatN.begin(); thermit != _localThermostatN.end(); thermit++)
		{
			if(thermit->first == 0) continue;
			this->_localThermostatN[0] += thermit->second;
			this->_localRotationalDOF[0] += this->_localRotationalDOF[thermit->first];
			this->_local2KETrans[0] += this->_local2KETrans[thermit->first];
			this->_local2KERot[0] += this->_local2KERot[thermit->first];
			if(_scale_v_1Dim[thermit->first]){ 
			  this->_local2KETrans_1Dim[0] += this->_local2KETrans_1Dim[thermit->first];
			}
		}
	}

	for(thermit = _universalThermostatN.begin(); thermit != _universalThermostatN.end(); thermit++)
	{
		// number of molecules on the local process. After the reduce operation
		// num_molecules will contain the global number of molecules
		unsigned long numMolecules = _localThermostatN[thermit->first];
		double summv2 = _local2KETrans[thermit->first];
		unsigned long rotDOF = _localRotationalDOF[thermit->first];
		double sumIw2 = (rotDOF > 0)? _local2KERot[thermit->first]: 0.0;
		double summv2_1Dim = 0.0;
	
		if(_scale_v_1Dim[thermit->first]){
		  summv2_1Dim = _local2KETrans_1Dim[thermit->first];
		  domainDecomp->collCommInit(5);
		}
		else{
		  domainDecomp->collCommInit(4);
		}
		domainDecomp->collCommAppendDouble(summv2);
		domainDecomp->collCommAppendDouble(sumIw2);
		if(_scale_v_1Dim[thermit->first])
		  domainDecomp->collCommAppendDouble(summv2_1Dim);
		domainDecomp->collCommAppendUnsLong(numMolecules);
		domainDecomp->collCommAppendUnsLong(rotDOF);
		domainDecomp->collCommAllreduceSum();
		summv2 = domainDecomp->collCommGetDouble();
		sumIw2 = domainDecomp->collCommGetDouble();
		if(_scale_v_1Dim[thermit->first])
		  summv2_1Dim = domainDecomp->collCommGetDouble();
		numMolecules = domainDecomp->collCommGetUnsLong();
		rotDOF = domainDecomp->collCommGetUnsLong();
		domainDecomp->collCommFinalize();
		if(!this->_localRank)
		        global_log->debug() << "[ thermostat ID " << thermit->first << "]\tN = " << numMolecules << "\trotDOF = " << rotDOF 
			<< "\tmv2 = " <<  summv2 << "\tIw2 = " << sumIw2 << endl;
			
		this->_universalThermostatN[thermit->first] = numMolecules;
		this->_universalRotationalDOF[thermit->first] = rotDOF;
		assert((summv2 > 0.0) || (numMolecules == 0));

		/* calculate the temperature of the entire system */
		if(numMolecules > 0){
			_globalTemperatureMap[thermit->first] =
				(summv2 + sumIw2) / (double)(3*numMolecules + rotDOF);
		}
		else
			_globalTemperatureMap[thermit->first] = _universalTargetTemperature[thermit->first];

		double Ti = Tfactor * _universalTargetTemperature[thermit->first];
		if((Ti > 0.0) && (numMolecules > 0) && !_universalNVE && getSimstep()>=_thermostatTimeSlot[0][thermit->first] && getSimstep()<=_thermostatTimeSlot[1][thermit->first])
		{
			_universalBTrans[thermit->first] = pow(3.0*numMolecules*Ti / summv2, 0.4);
			_universalThT_heatFlux[thermit->first] += 0.5*(summv2 - 3.0*numMolecules*Ti);
			if(_scale_v_1Dim[thermit->first]){ 
			  _universalATrans[thermit->first] = pow((3.0*numMolecules*Ti - summv2) / summv2_1Dim + 1, 0.4);
			  this->_alphaTransCorrection[thermit->first] = false;
			  // FIXME: why is that the case?
			  if(((3.0*numMolecules*Ti - summv2) / summv2_1Dim + 1) < 0){
			    this->_alphaTransCorrection[thermit->first] = true;
			  }
			}
			if( sumIw2 == 0.0 ) 
				_universalBRot[thermit->first] = 1.0;
			else 
				_universalBRot[thermit->first] = pow(rotDOF*Ti / sumIw2, 0.4);
		}
		else
		{
			this->_universalBTrans[thermit->first] = 1.0;
			this->_universalBRot[thermit->first] = 1.0;
			if(_scale_v_1Dim[thermit->first]){
			  _universalATrans[thermit->first] = 1.0;
			}
		}

		// heuristic handling of the unfortunate special case of an explosion in the system
		if( ( (_universalBTrans[thermit->first] < MIN_BETA) || (_universalBRot[thermit->first] < MIN_BETA) )
				&& (0 >= _universalSelectiveThermostatError) )
		{	
			global_log->warning() << "Explosion!" << endl;
			global_log->debug() << "Selective thermostat will be applied to set " << thermit->first
				<< " (beta_trans = " << this->_universalBTrans[thermit->first]
				<< ", beta_rot = " << this->_universalBRot[thermit->first] << "!)" << endl;
			int rot_dof;
			double Utrans, Urot;
			double limit_energy =  KINLIMIT_PER_T * Ti;
			double limit_rot_energy;
			double vcorr, Dcorr;
			Molecule* tM;
			for( tM = particleContainer->begin();
					tM != particleContainer->end();
					tM = particleContainer->next() )
			{
				Utrans = tM->U_trans();
				if(Utrans > limit_energy)
				{
					vcorr = sqrt(limit_energy / Utrans);
					//Directed velocity is reduced by to 0.0 to strengthen the impact of the velocity scaling
					for(int d = 0; d < 3; d++){
						tM->setDirectedVelocity(d, 0.0);
						if(_simulation.isRecordingStressProfile())
							tM->setDirectedVelocityStress(d, 0.0);
						if(_simulation.isRecordingSlabProfile())
							tM->setDirectedVelocitySlab(d, 0.0);
						if(_simulation.isRecordingConfinementProfile())
							tM->setDirectedVelocityConfinement(d, 0.0);
					}
					global_log->debug() << ": v(m" << tM->id() << ") *= " << vcorr << endl;
					tM->scale_v(vcorr);
					tM->scale_F(vcorr);
				}

				rot_dof = tM->component()->getRotationalDegreesOfFreedom();
				if(rot_dof > 0)
				{
					limit_rot_energy = 3.0*rot_dof * Ti;
					Urot = tM->U_rot();
					if(Urot > limit_rot_energy)
					{
						Dcorr = sqrt(limit_rot_energy / Urot);
						global_log->debug() << "D(m" << tM->id() << ") *= " << Dcorr << endl;
						tM->scale_D(Dcorr);
						tM->scale_M(Dcorr);
					}
				}
			}

			/* FIXME: Unnamed constant 3960... */
			if(3960 >= _universalSelectiveThermostatCounter)
			{
				if( _universalSelectiveThermostatWarning > 0 )
					_universalSelectiveThermostatError = _universalSelectiveThermostatWarning;
				if( _universalSelectiveThermostatCounter > 0 )
					_universalSelectiveThermostatWarning = _universalSelectiveThermostatCounter;
				_universalSelectiveThermostatCounter = 4000;
			}
			_universalBTrans[thermit->first] = 1.0;
			_universalBRot[thermit->first] = pow(this->_universalBRot[thermit->first], 0.0091);
		}
#ifdef NDEBUG
		if( (_universalSelectiveThermostatCounter > 0) &&
				((_universalSelectiveThermostatCounter % 20) == 10) )
#endif
			/* FIXME: why difference counters? */
			global_log->debug() << "counter " << _universalSelectiveThermostatCounter
				<< ",\t warning " << _universalSelectiveThermostatWarning
				<< ",\t error " << _universalSelectiveThermostatError << endl;
		if(collectThermostatVelocities && _universalUndirectedThermostat[thermit->first])
		{
			double sigv[3];
			for(int d=0; d < 3; d++)
				sigv[d] = _localThermostatDirectedVelocity[d][thermit->first];

			domainDecomp->collCommInit(3);
			for(int d=0; d < 3; d++) domainDecomp->collCommAppendDouble(sigv[d]);
			domainDecomp->collCommAllreduceSum();
			for(int d=0; d < 3; d++) sigv[d] = domainDecomp->collCommGetDouble();
			domainDecomp->collCommFinalize();

			for(int d=0; d < 3; d++)
			{
				_localThermostatDirectedVelocity[d][thermit->first] = 0.0;
				if(numMolecules > 0)
					_universalThermostatDirectedVelocity[d][thermit->first] = sigv[d] / numMolecules;
				else 
					_universalThermostatDirectedVelocity[d][thermit->first] = 0.0;
			}

#ifndef NDEBUG
			global_log->debug() << "* thermostat " << thermit->first
				<< " directed velocity: ("
				<< _universalThermostatDirectedVelocity[0][thermit->first]
				<< " / " << _universalThermostatDirectedVelocity[1][thermit->first]
				<< " / " << _universalThermostatDirectedVelocity[2][thermit->first] 
				<< ")" << endl;
#endif
		}
#ifndef NDEBUG

		global_log->debug() << "* Th" << thermit->first << " N=" << numMolecules
			<< " DOF=" << rotDOF + 3.0*numMolecules
			<< " Tcur=" << _globalTemperatureMap[thermit->first]
			<< " Ttar=" << _universalTargetTemperature[thermit->first]
			<< " Tfactor=" << Tfactor
			<< " bt=" << _universalBTrans[thermit->first]
			<< " br=" << _universalBRot[thermit->first] << "\n";
#endif
			if(!this->_localRank)
		        global_log->debug()/*cout*/ << "* Th" << thermit->first << " N=" << numMolecules
			<< " DOF=" << rotDOF + 3.0*numMolecules
			<< " Tcur=" << _globalTemperatureMap[thermit->first]
			<< " Ttar=" << _universalTargetTemperature[thermit->first]
			<< " Tfactor=" << Tfactor
			<< " bt=" << _universalBTrans[thermit->first]
			<< " br=" << _universalBRot[thermit->first] << "\n";
	}
	if(this->_universalSelectiveThermostatCounter > 0)
		this->_universalSelectiveThermostatCounter--;
	if(this->_universalSelectiveThermostatWarning > 0)
		this->_universalSelectiveThermostatWarning--;
	if(this->_universalSelectiveThermostatError > 0)
		this->_universalSelectiveThermostatError--;
}

void Domain::calculateThermostatDirectedVelocity(ParticleContainer* partCont)
{
	Molecule* tM;
	if(this->_componentwiseThermostat || this->_enableThermostatLayers || this->_enableThermostatWallLayers)
	{		
		for( map<int, bool>::iterator thit = _universalUndirectedThermostat.begin();
				thit != _universalUndirectedThermostat.end();
				thit ++ )
		{
			if(thit->second)
				for(int d=0; d < 3; d++) _localThermostatDirectedVelocity[d][thit->first] = 0.0;
		}
		for(tM = partCont->begin(); tM != partCont->end(); tM = partCont->next() )
		{
			int cid = tM->componentid();
			int thermostat = this->_componentToThermostatIdMap[cid];
			if(isThermostatLayer() == true || isThermostatWallLayer() == true){
			  thermostat = moleculeInLayer(tM->r(0), tM->r(1), tM->r(2), tM->componentid());
			}
			if(this->_universalUndirectedThermostat[thermostat] && thermostat != -10)
			{
				for(int d=0; d < 3; d++)
					_localThermostatDirectedVelocity[d][thermostat] += tM->v(d);
			}
		}
	}
	else if(this->_universalUndirectedThermostat[0])
	{
		for(int d=0; d < 3; d++) _localThermostatDirectedVelocity[d][0] = 0.0;
		for(tM = partCont->begin(); tM != partCont->end(); tM = partCont->next() )
		{
			for(int d=0; d < 3; d++)
				_localThermostatDirectedVelocity[d][0] += tM->v(d);
		}
	}
}

void Domain::calculateVelocitySums(ParticleContainer* partCont)
{
	Molecule* tM;
	if(this->_componentwiseThermostat || this->_enableThermostatLayers || this->_enableThermostatWallLayers)
	{	
		for(tM = partCont->begin(); tM != partCont->end(); tM = partCont->next() )
		{
			int cid = tM->componentid();
			int thermostat = this->_componentToThermostatIdMap[cid];
			if(isThermostatLayer() == true || isThermostatWallLayer() == true)
			  thermostat = moleculeInLayer(tM->r(0), tM->r(1), tM->r(2), tM->componentid());
			this->_localThermostatN[thermostat]++;
			this->_localRotationalDOF[thermostat] += tM->component()->getRotationalDegreesOfFreedom();
			if(this->_universalUndirectedThermostat[thermostat]  && thermostat != -10)
			{
				tM->calculate_mv2_Iw2( this->_local2KETrans[thermostat],
						this->_local2KERot[thermostat],
						this->_universalThermostatDirectedVelocity[0][thermostat],
						this->_universalThermostatDirectedVelocity[1][thermostat],
						this->_universalThermostatDirectedVelocity[2][thermostat]  );
			}
			else
			{
				tM->calculate_mv2_Iw2(_local2KETrans[thermostat], _local2KETrans_1Dim[thermostat], _local2KERot[thermostat], this->_dimToThermostat[thermostat], _simulation.getDomain());
			}
		}
	}
	else
	{
		for(tM = partCont->begin(); tM != partCont->end(); tM = partCont->next() )
		{
			this->_localThermostatN[0]++;
			this->_localRotationalDOF[0] += tM->component()->getRotationalDegreesOfFreedom();
			if(this->_universalUndirectedThermostat[0])
			{
				tM->calculate_mv2_Iw2( this->_local2KETrans[0],
						this->_local2KERot[0],
						this->_universalThermostatDirectedVelocity[0][0],
						this->_universalThermostatDirectedVelocity[1][0],
						this->_universalThermostatDirectedVelocity[2][0]  );
			}
			else
			{
				tM->calculate_mv2_Iw2(_local2KETrans[0], _local2KETrans_1Dim[0],  _local2KERot[0], 5, _simulation.getDomain());
			}
		}
		global_log->debug() << "      * N = " << this->_localThermostatN[0]
			<< "rotDOF = " << this->_localRotationalDOF[0] << "   mv2 = "
			<< _local2KETrans[0] << " Iw2 = " << _local2KERot[0] << endl;
	}
}

void Domain::writeCheckpoint( string filename, 
		ParticleContainer* particleContainer,
		DomainDecompBase* domainDecomp )
{
	domainDecomp->assertDisjunctivity(particleContainer);
	if(!this->_localRank)
	{
		ofstream checkpointfilestream(filename.c_str());
		checkpointfilestream << "mardyn trunk " << CHECKPOINT_FILE_VERSION;
		checkpointfilestream << "\n";
		checkpointfilestream << "currentTime\t" << _simulation.getSimulationTime()-_simulation.getTimeStepLength() << endl;
		checkpointfilestream << "Length\t" << setprecision(9) << _globalLength[0] << " " << _globalLength[1] << " " << _globalLength[2] << "\n";
		if(this->_componentwiseThermostat || this->_enableThermostatLayers || this->_enableThermostatWallLayers)
		{
		    if(!this->_enableThermostatLayers){
		      if(this->_enableThermostatWallLayers){
			for( unsigned compID = 0; compID < getNumberOfComponents(); compID++)
			{
				if(0 > this->_originalThtID[compID]) continue;
				checkpointfilestream << "CT\t" << compID+1 << "\t" << this->_originalThtID[compID] << "\n";
			}      
		      }else{
			for( map<int, int>::iterator thermit = this->_componentToThermostatIdMap.begin();
					thermit != this->_componentToThermostatIdMap.end();
					thermit++ )
			{
				if(0 >= thermit->second) continue;
				checkpointfilestream << "CT\t" << 1+thermit->first
					<< "\t" << thermit->second << "\n";
			}
		      }
		    }
		    if(this->_enableThermostatWallLayers){
			for( unsigned compID = 0; compID < getNumberOfComponents(); compID++)
			{
				if((0 >= this->_originalTargetTemp[compID])) continue;
				checkpointfilestream << "ThT " << this->_originalThtID[compID] << "\t" << this->_originalTargetTemp[compID] << "\n";
			}      
		      }else{
			for( map<int, double>::iterator Tit = this->_universalTargetTemperature.begin();
					Tit != this->_universalTargetTemperature.end();
					Tit++ )
			{
				if((0 >= Tit->first) || (0 >= Tit->second)) continue;
				checkpointfilestream << "ThT " << Tit->first << "\t" << Tit->second << "\n";
			}
		      }
		    if(this->_enableThermostatLayers){
			for( map<int, double>::iterator Tit = this->_universalTargetTemperature.begin();
					Tit != this->_universalTargetTemperature.end();
					Tit++ )
			{
				if((0 >= Tit->first) || (0 >= Tit->second)) continue;
				checkpointfilestream << "ThLayer " << Tit->first << "\t" << _thermostatLayer[0][Tit->first] << "\t" << _thermostatLayer[1][Tit->first] << "\t"
				<< _thermostatLayer[2][Tit->first] << "\t" << _thermostatLayer[3][Tit->first] << "\t" <<  _thermostatLayer[4][Tit->first] << "\t" << _thermostatLayer[5][Tit->first] << "\n";
			}
		    }
		    if(this->_enableThermostatWallLayers){
			for( unsigned compID = 0; compID < getNumberOfComponents(); compID++)
			{
				if((_simulation.getSimulationStep()-1) == this->_originalEndTime[compID])
				    checkpointfilestream << "#";
				checkpointfilestream << "ThTS " << this->_originalThtID[compID] << "\t" << this->_originalStartTime[compID] << " " << this->_originalEndTime[compID] << "\n";
			}      
		    }else{
			for( map<int, int>::iterator ThTime = this->_componentToThermostatIdMap.begin(); 
					ThTime != this->_componentToThermostatIdMap.end();
					ThTime++ )
			{
				if((0 >= ThTime->second)) continue;
				if((_simulation.getSimulationStep()-1) == this->_thermostatTimeSlot[1][ThTime->second])
				    checkpointfilestream << "#";
				checkpointfilestream << "ThTS " << ThTime->second << "\t" << this->_thermostatTimeSlot[0][ThTime->second] << " " << this->_thermostatTimeSlot[1][ThTime->second] << "\n";
			}
		    }
		    if(this->_enableThermostatWallLayers){
			for( unsigned tht = 1; tht <= getNumberOfComponents(); tht++)
			{
				if(isScaling1Dim(tht))
				    checkpointfilestream << "oneDim\t" << tht << " " << this->_dimToThermostat[tht] << "\n";
			}      
		    }else{
			for( map<int, int>::iterator thermit = this->_componentToThermostatIdMap.begin();
					thermit != this->_componentToThermostatIdMap.end();
					thermit++ )
			{
				if(0 >= thermit->second) continue;
				if(isScaling1Dim(thermit->second))
				    checkpointfilestream << "oneDim\t" << thermit->second << " " << this->_dimToThermostat[thermit->second] << "\n";
			}
		    }
		}
		else
		{
			checkpointfilestream << "Temperature\t" << _universalTargetTemperature[0] << endl;
		}
		
		string free ("free");
		string moved ("moved");
		string fixed ("fixed");
		unsigned cid_free =  getCidMovement(free, getNumberOfComponents()) - 1;
		unsigned cid_moved =  getCidMovement(moved, getNumberOfComponents()) - 1;
		unsigned cid_fixed =  getCidMovement(fixed, getNumberOfComponents()) - 1;
		
		for(unsigned i = 0; i < getNumberOfComponents(); i++)
		{
			if(i == cid_free)
			    checkpointfilestream << "State\t" << i+1 << " " << "free\n"; 
			else if(i == cid_moved)
			    checkpointfilestream << "State\t" << i+1 << " " << "moved\n"; 
			else if(i == cid_fixed){
			    checkpointfilestream << "State\t" << i+1 << " " << "fixed "; 
			    if(isFixedEvery() == true)
				    checkpointfilestream << "every\t" << getFixEvery() << "\n"; 
			    else //isFixedRegion
				    checkpointfilestream << "region\t" << getFixedRegion(0) << " " << getFixedRegion(1)<< " "
				    << getFixedRegion(2) << " " << getFixedRegion(3) << " "
				    << getFixedRegion(4) << " " << getFixedRegion(5) << "\n"; 
			}
		}
		
		map<unsigned, unsigned> componentSets = this->_universalPG->getComponentSets();
		for( map<unsigned, unsigned>::const_iterator uCSIDit = componentSets.begin();
				uCSIDit != componentSets.end();
				uCSIDit++ )
		{ 
			if(uCSIDit->first > 100) continue;
			checkpointfilestream << "S\t" << 1+uCSIDit->first << "\t" << uCSIDit->second << "\n";
		}
		map<unsigned, double> tau = this->_universalPG->getTau();
		for( map<unsigned, double>::const_iterator gTit = tau.begin();
				gTit != tau.end();
				gTit++ )
		{
			unsigned cosetid = gTit->first;
			double* ttargetv = this->_universalPG->getTargetVelocity(cosetid);
			double* tacc = this->_universalPG->getAdditionalAcceleration(cosetid);
			checkpointfilestream << "A\t" << cosetid << "\t"
				<< ttargetv[0] << " " << ttargetv[1] << " " << ttargetv[2] << "\t"
				<< gTit->second << "\t"
				<< tacc[0] << " " << tacc[1] << " " << tacc[2] << "\n";
			delete ttargetv;
			delete tacc;
		}
		
		if(isSpringDamped())
		{
			checkpointfilestream << "Spring\t" << getMinSpringID() << " " << getMaxSpringID() << " " 
			<< getAverageY() << " " << getSpringConst() << endl;
		}
		
		if(isGravity())
		{
			checkpointfilestream << "Gravity\t" << getGravityComp()+1 << " " << getGravityDir() << " " 
			<< getGravityForce() << endl;
		}
#ifndef NDEBUG
		checkpointfilestream << "# rho\t" << this->_globalRho << "\n";
		checkpointfilestream << "# rc\t" << global_simulation->getcutoffRadius() << "\n";
		checkpointfilestream << "# rcT\t" << global_simulation->getTersoffCutoff() << "\n";
#endif
		if(this->_globalUSteps > 1)
		{
			checkpointfilestream << "#";
			checkpointfilestream << setprecision(13);
			checkpointfilestream << "I\t" << this->_globalUSteps << " "
				<< this->_globalSigmaU << " " << this->_globalSigmaUU << "\n";
			checkpointfilestream << setprecision(8);
		}
		checkpointfilestream << "#_confinmentMidPointIDs: [0] [1] [x0] [y0] [x1] [y1] " << this->_confinementMidPointID[0] << " " << this->_confinementMidPointID[1] << " " <<  this->_confinementMidPoint[0] << " " <<  this->_confinementMidPoint[1] << " " <<  this->_confinementMidPoint[2] << " " <<  this->_confinementMidPoint[3] << endl;
		vector<Component>* components = _simulation.getEnsemble()->components();
		checkpointfilestream << "NumberOfComponents\t" << components->size() << endl;
		for(vector<Component>::const_iterator pos=components->begin();pos!=components->end();++pos){
			pos->write(checkpointfilestream);
		}
		unsigned int numperline=_simulation.getEnsemble()->components()->size();
		unsigned int iout=0;
		for(vector<double>::const_iterator pos=_mixcoeff.begin();pos!=_mixcoeff.end();++pos){
			checkpointfilestream << *pos;
			iout++;
			// 2 parameters (xi and eta)
			if(iout/2>=numperline) {
				checkpointfilestream << endl;
				iout=0;
				--numperline;
			}
			else if(!(iout%2)) {
				checkpointfilestream << "\t";
			}
			else {
				checkpointfilestream << " ";
			}
		}
		checkpointfilestream << _epsilonRF << endl;  
		
		for( map<int, bool>::iterator uutit = this->_universalUndirectedThermostat.begin();
				uutit != this->_universalUndirectedThermostat.end();
				uutit++ )
		{
			if(0 > uutit->first) continue;
			if(uutit->second) checkpointfilestream << " U\t" << uutit->first << "\n";
		}
		checkpointfilestream << "NumberOfMolecules\t" << _globalNumMolecules << endl;

		checkpointfilestream << "MoleculeFormat\t" << "ICRVQD" << endl << endl;
		checkpointfilestream.close();
	}

	domainDecomp->writeMoleculesToFile(filename, particleContainer); 
}

void Domain::initParameterStreams(double cutoffRadius, double cutoffRadiusLJ){
	_comp2params.initialize(*(_simulation.getEnsemble()->components()), _mixcoeff, _epsilonRF, cutoffRadius, cutoffRadiusLJ); 
}

void Domain::initFarFieldCorr(double cutoffRadius, double cutoffRadiusLJ) {
	double UpotCorrLJ=0.;
	double VirialCorrLJ=0.;
	double MySelbstTerm=0.;
	vector<Component>* components = _simulation.getEnsemble()->components();
	unsigned int numcomp=components->size();
	for(unsigned int i=0;i<numcomp;++i) {
		Component& ci=(*components)[i];
		unsigned int numljcentersi=ci.numLJcenters();
		unsigned int numchargesi = ci.numCharges();
		unsigned int numdipolesi=ci.numDipoles();
		unsigned int numtersoffi = ci.numTersoff();

		// effective dipoles computed from point charge distributions
		double chargeBalance[3];
		for(unsigned d = 0; d < 3; d++) chargeBalance[d] = 0;
		for(unsigned int si = 0; si < numchargesi; si++)
		{
			double tq = ci.charge(si).q();
			for(unsigned d = 0; d < 3; d++) chargeBalance[d] += tq * ci.charge(si).r()[d];
		}
		// point dipoles
		for(unsigned int si=0;si<numdipolesi;++si)
		{
			double tmy = ci.dipole(si).absMy();
			double evect = 0;
			for(unsigned d = 0; d < 3; d++) evect += ci.dipole(si).e()[d] * ci.dipole(si).e()[d];
			double norm = 1.0 / sqrt(evect);
			for(unsigned d = 0; d < 3; d++) chargeBalance[d] += tmy * ci.dipole(si).e()[d] * norm;
		}
		double my2 = 0.0;
		for(unsigned d = 0; d < 3; d++) my2 += chargeBalance[d] * chargeBalance[d];
		MySelbstTerm += my2 * ci.getNumMolecules();

		for(unsigned int j=0;j<numcomp;++j) {
			Component& cj=(*components)[j];
			unsigned numtersoffj = cj.numTersoff();
			// no LJ interaction between Tersoff components
			if(numtersoffi && numtersoffj) continue;
			unsigned int numljcentersj=cj.numLJcenters();
			ParaStrm& params=_comp2params(i,j);
			params.reset_read();
			// LJ centers
			for(unsigned int si=0;si<numljcentersi;++si) {
				double xi=ci.ljcenter(si).rx();
				double yi=ci.ljcenter(si).ry();
				double zi=ci.ljcenter(si).rz();
				double tau1=sqrt(xi*xi+yi*yi+zi*zi);
				for(unsigned int sj=0;sj<numljcentersj;++sj) {
					double xj=cj.ljcenter(sj).rx();
					double yj=cj.ljcenter(sj).ry();
					double zj=cj.ljcenter(sj).rz();
					double tau2=sqrt(xj*xj+yj*yj+zj*zj);
					if(tau1+tau2>=cutoffRadiusLJ){
						global_log->error() << "Error calculating cutoff corrections, rc too small" << endl;
						exit(1);
					}
					double eps24;
					params >> eps24;
					double sig2;
					params >> sig2;
					double uLJshift6;
					params >> uLJshift6;  // 0 unless TRUNCATED_SHIFTED

					if(uLJshift6 == 0.0)
					{
						double fac=double(ci.getNumMolecules())*double(cj.getNumMolecules())*eps24;
						if(tau1==0. && tau2==0.)
						{
							UpotCorrLJ+=fac*(TICCu(-6,cutoffRadiusLJ,sig2)-TICCu(-3,cutoffRadiusLJ,sig2));
							VirialCorrLJ+=fac*(TICCv(-6,cutoffRadiusLJ,sig2)-TICCv(-3,cutoffRadiusLJ,sig2));
						}
						else if(tau1!=0. && tau2!=0.)
						{
							UpotCorrLJ += fac*( TISSu(-6,cutoffRadiusLJ,sig2,tau1,tau2)
									- TISSu(-3,cutoffRadiusLJ,sig2,tau1,tau2) );
							VirialCorrLJ += fac*( TISSv(-6,cutoffRadiusLJ,sig2,tau1,tau2)
									- TISSv(-3,cutoffRadiusLJ,sig2,tau1,tau2) );
						}
						else {
							if(tau2==0.) 
								tau2=tau1;
							UpotCorrLJ+=fac*(TICSu(-6,cutoffRadiusLJ,sig2,tau2)-TICSu(-3,cutoffRadiusLJ,sig2,tau2));
							VirialCorrLJ+=fac*(TICSv(-6,cutoffRadiusLJ,sig2,tau2)-TICSv(-3,cutoffRadiusLJ,sig2,tau2));
						}
					}
				}
			}
		}
	}

	double fac=M_PI*_globalRho/(3.*_globalNumMolecules);
	UpotCorrLJ*=fac;
	VirialCorrLJ*=-fac;

	double epsRFInvrc3=2.*(_epsilonRF-1.)/((cutoffRadius*cutoffRadius*cutoffRadius)*(2.*_epsilonRF+1.));
	MySelbstTerm*=-0.5*epsRFInvrc3;

	_UpotCorr=UpotCorrLJ+MySelbstTerm;
	_VirialCorr=VirialCorrLJ+3.*MySelbstTerm;

	global_log->info() << "Far field terms: U_pot_correction  = " << _UpotCorr << " virial_correction = " << _VirialCorr << endl;
}

void Domain::setupProfile(unsigned xun, unsigned yun, unsigned zun)
{
	this->_universalNProfileUnits[0] = xun;
	this->_universalNProfileUnits[1] = yun;
	this->_universalNProfileUnits[2] = zun;
	for(unsigned d = 0; d < 3; d++)
	{
		_universalInvProfileUnit[d] = (double)_universalNProfileUnits[d] / _globalLength[d];
	}
	this->resetProfile();
	
	// Check if file path already exists; important for independent use of xyz-writer and profile-writer
	FILE *f;
	f=fopen("./Results/Profile","r");
	if(f==0)
	  mkdir("./Results/Profile", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);  
}

void Domain::recordProfile(ParticleContainer* molCont)
{
	int cid;
	unsigned xun, yun, zun, unID;
	double mv2, Iw2;
	string Default ("Default");
	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{
		cid = thismol->componentid();
		if(this->_universalProfiledComponents[cid])
		{
			double distFor_unID = pow((thismol->r(0)- this->_universalCentre[0]),2.0) + pow((thismol->r(1)- this->_universalCentre[1]),2.0);
			if(this->_universalCylindricalGeometry && distFor_unID <= this->_universalR2max && distFor_unID >= this->_universalR2min && thismol->r(1) >= this->_universalCentre[1]){
				unID = this->unID(thismol->r(0), thismol->r(1), thismol->r(2));
				if(unID < 0){
				  global_log->error() << "ERROR: in Domain.cpp/recordProfile: unID < 0!!! Was not calculated in unID() but initialized with -1!\n";
				}
			}
			else if(!this->_universalCylindricalGeometry){
			  xun = (unsigned)floor(thismol->r(0) * this->_universalInvProfileUnit[0]);
			  yun = (unsigned)floor(thismol->r(1) * this->_universalInvProfileUnit[1]);
			  zun = (unsigned)floor(thismol->r(2) * this->_universalInvProfileUnit[2]);
			  unID = xun * this->_universalNProfileUnits[1] * this->_universalNProfileUnits[2]
				+ yun * this->_universalNProfileUnits[2] + zun;
			}
			else{
				continue;
			}
			
			this->_localNProfile[unID] += 1.0;
			for(int d=0; d<3; d++) this->_localvProfile[d][unID] += thismol->v(d);
			this->_localDOFProfile[unID] += 3.0 + (long double)(thismol->component()->getRotationalDegreesOfFreedom());
			
			// record _twice_ the total (ordered + unordered) kinetic energy
			mv2 = 0.0;
			Iw2 = 0.0;
			thismol->calculate_mv2_Iw2(mv2, Iw2, Default);
			this->_localKineticProfile[unID] += mv2+Iw2;
		}
	}
	this->_globalAccumulatedDatasets++;
}

void Domain::collectProfile(DomainDecompBase* dode)
{
	unsigned unIDs = this->_universalNProfileUnits[0] * this->_universalNProfileUnits[1]
		* this->_universalNProfileUnits[2];
	dode->collCommInit(10*unIDs);
	for(unsigned unID = 0; unID < unIDs; unID++)
	{
		dode->collCommAppendLongDouble(this->_localNProfile[unID]);
		for(int d=0; d<3; d++)
			dode->collCommAppendLongDouble(_localvProfile[d][unID]);
		dode->collCommAppendLongDouble(this->_localDOFProfile[unID]);
		dode->collCommAppendLongDouble(_localKineticProfile[unID]);

                dode->collCommAppendLongDouble(this->_localWidomProfile[unID]);
                dode->collCommAppendLongDouble(this->_localWidomInstances[unID]);
                dode->collCommAppendLongDouble(this->_localWidomProfileTloc[unID]);
                dode->collCommAppendLongDouble(this->_localWidomInstancesTloc[unID]);
	}
	dode->collCommAllreduceSum();
	for(unsigned unID = 0; unID < unIDs; unID++)
	{
		_universalNProfile[unID] = (double)dode->collCommGetLongDouble();
		for(int d=0; d<3; d++)
			this->_universalvProfile[d][unID]
				= (double)dode->collCommGetLongDouble();
		this->_universalDOFProfile[unID]
			= (double)dode->collCommGetLongDouble();
		this->_universalKineticProfile[unID]
			= (double)dode->collCommGetLongDouble();

		this->_globalWidomProfile[unID]
			= (double)dode->collCommGetLongDouble();
		this->_globalWidomInstances[unID]
			= (double)dode->collCommGetLongDouble();
		this->_globalWidomProfileTloc[unID]
			= (double)dode->collCommGetLongDouble();
		this->_globalWidomInstancesTloc[unID]
			= (double)dode->collCommGetLongDouble();

		/*
		 * construct the temperature profile
		 */
		double Tun = this->getGlobalCurrentTemperature();
		if((!this->_localRank) && (_universalDOFProfile[unID] >= 1000))
		{
		  double twoEkin = this->_universalKineticProfile[unID];

		  double vvNN = 0.0;
		  for(unsigned d = 0; d < 3; d++)
		     vvNN += _universalvProfile[d][unID] * _universalvProfile[d][unID];
		  double twoEkindir = _universalProfiledComponentMass * vvNN / _universalNProfile[unID];

		  Tun = (twoEkin - twoEkindir) / _universalDOFProfile[unID];
		}
		// domainDecomp->doBroadcast(&Tun); // no longer needed, since MPI_Reduce (branch) was replaced with MPI_Allreduce (trunk)
		this->_universalTProfile[unID] = Tun; 
	}
	dode->collCommFinalize();
}


void Domain::outputProfile(const char* prefix)
{
	if(this->_localRank) return;

	string vxpryname("./Results/Profile/");
	string Tpryname("./Results/Profile/");
	string rhpryname("./Results/Profile/");
        string upryname("./Results/Profile/");
	rhpryname += prefix;
	vxpryname += prefix;
	Tpryname += prefix;
        upryname += prefix;
	rhpryname += ".rhpry";
	vxpryname += ".vxpry";
	Tpryname += ".Tpry";
        upryname += ".upr";
	ofstream rhpry(rhpryname.c_str());
	ofstream vxpry(vxpryname.c_str());
	ofstream Tpry(Tpryname.c_str());
	ofstream upry(upryname.c_str());
	if (!(vxpry && Tpry && rhpry && upry))
	{
		return;
	}
	rhpry.precision(6);
	rhpry << "# y\trho\ttotal DOF\n# \n";
	vxpry.precision(5);
	vxpry << "# y\tvx\tv\n# \n";
	Tpry.precision(6);
        Tpry << "# coord\t2Ekin/#DOF\t2Ekin/3N (dir.)\tT\n# \n";
	upry.precision(5);
        upry << "# coord\t\tmu_conf(loc)  mu_conf(glob) \t\t mu_rho(loc)  mu_rho(glob) \t "
             << "mu_at(loc)  mu_at(glob) \t\t mu_T(loc)  mu_T(glob) \t mu_id(loc)  "
             << "mu_id(glob) \t\t mu_res(loc)  mu_res(glob) \t mu(loc)  mu(glob) \t\t #(loc)  "
             << "#(glob)\n";

	double layerVolume = this->_globalLength[0] * this->_globalLength[1] * this->_globalLength[2]
		/ this->_universalNProfileUnits[1];
	for(unsigned y = 0; y < this->_universalNProfileUnits[1]; y++)
	{
		double yval = (y + 0.5) / this->_universalInvProfileUnit[1];

		long double Ny = 0.0;
		long double DOFy = 0.0;
		long double twoEkiny = 0.0;
                long double twoEkindiry = 0.0;
		long double velocitysumy[3];
                long double widomSigExpy = 0.0;
                long double widomInstancesy = 0.0;
                long double widomSigExpyTloc = 0.0;
                long double widomInstancesyTloc = 0.0;
		for(unsigned d = 0; d < 3; d++) velocitysumy[d] = 0.0;
		for(unsigned x = 0; x < this->_universalNProfileUnits[0]; x++)
		{
			for(unsigned z = 0; z < this->_universalNProfileUnits[2]; z++)
			{
				unsigned unID = x * this->_universalNProfileUnits[1] * this->_universalNProfileUnits[2]
					+ y * this->_universalNProfileUnits[2] + z;
				Ny += this->_universalNProfile[unID];
				DOFy += this->_universalDOFProfile[unID];
				twoEkiny += this->_universalKineticProfile[unID];
				for(unsigned d = 0; d < 3; d++) velocitysumy[d] += this->_universalvProfile[d][unID];
                                widomSigExpy += this->_globalWidomProfile[unID];
                                widomInstancesy += this->_globalWidomInstances[unID];
                                widomSigExpyTloc += this->_globalWidomProfileTloc[unID];
                                widomInstancesyTloc += this->_globalWidomInstancesTloc[unID];
			}
		}
                double rho_loc = Ny / (layerVolume * this->_globalAccumulatedDatasets);

		if(Ny >= 64.0)
		{
		   double vvdir = 0.0;
		   for(unsigned d = 0; d < 3; d++)
		   {
		      double vd = velocitysumy[d] / Ny;
		      vvdir += vd*vd;
		   }
                   twoEkindiry = Ny * _universalProfiledComponentMass * vvdir;

		   rhpry << yval << "\t" << rho_loc << "\t" << DOFy << "\n";
		   vxpry << yval << "\t" << (velocitysumy[0] / Ny) << "\t" << sqrt(vvdir) << "\n";
                   Tpry << yval << "\t" << (twoEkiny / DOFy) << "\t"
                        << (twoEkindiry / (3.0*Ny)) << "\t" << ((twoEkiny - twoEkindiry) / DOFy) << "\n";

                   if(widomInstancesy >= 100.0)
                   {
                      double mu_res_glob = -log(widomSigExpy / widomInstancesy);
                      double mu_conf_glob = mu_res_glob * _globalTemperatureMap[0];
                      double mu_id_glob = _globalTemperatureMap[0] * log(_globalDecisiveDensity);

                      double mu_T_glob = 0.0;
                      double mu_rho_loc = 0.0;
                      double mu_T_loc = 0.0;
                      double mu_res_loc = 0.0;
                      double mu_conf_loc = 0.0;
                      if(Ny >= 10.0)
                      {
                         mu_T_glob = 3.0*_globalTemperatureMap[0] * log(_universalLambda);
                         mu_res_loc = -log(widomSigExpyTloc / widomInstancesyTloc);              

                         if(widomInstancesyTloc >= 100.0)
                         {
                            double Tloc = (twoEkiny - twoEkindiry) / DOFy;
                            // mu_id_loc = Tloc * log(rho_loc * _universalLambda*_universalLambda*_universalLambda);
                            mu_rho_loc = Tloc * log(rho_loc);
                            mu_T_loc = 3.0*Tloc * (log(_universalLambda) + 0.5*log(_globalTemperatureMap[0] / Tloc));
                            mu_conf_loc = Tloc * mu_res_loc;
                         }
                      }
               
                      upry << yval << " \t\t " << mu_conf_loc << "  " << mu_conf_glob << " \t\t "
                           << mu_rho_loc << "  " << mu_id_glob - mu_T_glob << " \t "
                           << mu_conf_loc + mu_rho_loc << "  " << mu_conf_glob + mu_id_glob - mu_T_glob << " \t\t "
                           << mu_T_loc << "  " << mu_T_glob << " \t "
                           << mu_rho_loc + mu_T_loc << "  " << mu_id_glob << " \t\t "
                           << mu_res_loc << "  " << mu_res_glob << " \t "
                           << mu_rho_loc + mu_T_loc + mu_conf_loc << "  " << mu_id_glob + mu_conf_glob << " \t\t "
                           << widomInstancesy << "  " << widomInstancesyTloc << "\n";
                   }
		}
		else
		{
			rhpry << yval << "\t0.000\t" << DOFy << "\n";
		}
	}

	rhpry.close();
	vxpry.close();
	Tpry.close();
	upry.close();
}

void Domain::resetProfile()
{
	unsigned unIDs = this->_universalNProfileUnits[0] * this->_universalNProfileUnits[1]
		* this->_universalNProfileUnits[2];
	for(unsigned unID = 0; unID < unIDs; unID++)
	{
		this->_localNProfile[unID] = 0.0;
		this->_universalNProfile[unID] = 0.0;
		for(int d=0; d<3; d++)
		{
			this->_localvProfile[d][unID] = 0.0;
			this->_universalvProfile[d][unID] = 0.0;
		}
		this->_localDOFProfile[unID] = 0.0;
		this->_universalDOFProfile[unID] = 0.0;
		this->_localKineticProfile[unID] = 0.0;
		this->_universalKineticProfile[unID] = 0.0;

		this->_localWidomProfile[unID] = 0.0;
		this->_globalWidomProfile[unID] = 0.0;
		this->_localWidomInstances[unID] = 0.0;
		this->_globalWidomInstances[unID] = 0.0;
		this->_localWidomProfileTloc[unID] = 0.0;
		this->_globalWidomProfileTloc[unID] = 0.0;
		this->_localWidomInstancesTloc[unID] = 0.0;
		this->_globalWidomInstancesTloc[unID] = 0.0;
	}
	this->_globalAccumulatedDatasets = 0;
}

void Domain::resetSlabProfile()
{
	unsigned long unIDs = this->_universalNProfileUnitsSlab[0] * this->_universalNProfileUnitsSlab[1]
		* this->_universalNProfileUnitsSlab[2];
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		this->_localNProfileSlab[unID] = 0;
		this->_universalNProfileSlab[unID] = 0;
		if(!_simulation.reduceData())
			for(int d=0; d<3; d++)
			{
				this->_localvProfileSlab[d][unID] = 0.0;
				this->_universalvProfileSlab[d][unID] = 0.0;
			}
		this->_localDOFProfileSlab[unID] = 0;
		this->_universalDOFProfileSlab[unID] = 0;
		this->_localKineticProfileSlab[unID] = 0.0;
		this->_universalKineticProfileSlab[unID] = 0.0;

	}
	this->_globalAccumulatedDatasetsSlab = 0;
}


void Domain::confinementDensity(double radius1, double radius2, double xCentre, double yCentre){
	this->_universalCylindricalGeometry = true;

	this->_lowerAsperityRadius = radius1;
	this->_upperAsperityRadius = radius2;
	this->_universalCentre[0] = xCentre;
	this->_universalCentre[1] = yCentre;
	this->_universalCentre[2] = 1.0;
	
	double maxRadius = this->_lowerAsperityRadius + this->_upperAsperityRadius;
	double minRadius = this->_lowerAsperityRadius;
	
	this->_universalR2max = maxRadius * maxRadius;
	this->_universalR2min = minRadius * minRadius;
	
	this->_universalNProfileUnits[2] = 1;
	
	_universalInvProfileUnit[0] = this->_universalNProfileUnits[0]/(M_PI);                   // delta_phi
	_universalInvProfileUnit[1] = this->_universalNProfileUnits[1]/((this->_universalR2max - this->_universalR2min));  // delta_R^2/2
	_universalInvProfileUnit[2] = this->_universalNProfileUnits[2]/(this->_globalLength[2]); // delta_z
	global_log->info() << "\nInv Profile unit for sessile drop: (phi,R^2) = (" << _universalInvProfileUnit[0] <<", " <<_universalInvProfileUnit[1]<<") \n";
	
	this->resetProfile();

	mkdir("./Results/CylindricalProfile", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

//Method called by Simulation::output() in order to decide wheter or not a cylindrical profile is to be written out,
//i.e. wheter the method outputCylProfile() (isCylindrical==true) or the method outputProfile() (isCylindrical==false) is called.
bool Domain::isCylindrical(){
	return this->_universalCylindricalGeometry;
}

long int Domain::unID(double qx, double qy, double qz){
		int xun,yun,zun;// (xun,yun,zun): bin number in a special direction, e.g. yun==5 corresponds to the 5th bin in the radial direction, 
		long int unID;	// as usual
		double xc,yc; // distance of a particle with respect to the origin of the cylindrical coordinate system

		unID = -1; // initialization, causes an error message, if unID is not calculated in this method but used in record profile

	    xc = qx - this->_universalCentre[0];
	    yc = qy - this->_universalCentre[1];
	    
	    // transformation in polar coordinates
	    double R2 = xc*xc + yc*yc;
	    double phi = acos(xc/sqrt(R2));
	    if(phi < 0.0) {phi = phi + 2.0*M_PI;}
	    
	    // just a semi-cylindrical area [0;M_PI] is investigated
	    if(phi < 0.0 || phi > M_PI || R2 < this->_universalR2min || R2 > this->_universalR2max){
		  global_log->error() << "Severe error!! Molecules are not in the semi-cylindrical area [0;M_PI]!\n";
	          global_log->error() << "Coordinates (" << qx << " / " << qy << " / " << qz << ").\n";
		  global_log->error() << "unID = " << unID << "\n";
	    }

	    xun = (int)floor(phi * this->_universalInvProfileUnit[0]);   // bin no. in phi-direction
	    yun = (int)floor((R2 - this->_universalR2min) *  this->_universalInvProfileUnit[1]);   // bin no. in R-direction
	    //zun = (int)floor(yc *  this->_universalInvProfileUnit[2]);   // bin no. in z-direction
	    // for the case that we average over z-direction
	    zun = 0;

	    if((xun >= 0) && (yun >= 0) && (zun >= 0) &&
	          (xun < (int)_universalNProfileUnits[0]) && (yun < (int)_universalNProfileUnits[1]) && (zun < (int)_universalNProfileUnits[2]))
	       {
	          unID = xun * this->_universalNProfileUnits[1] * this->_universalNProfileUnits[2]
	               + yun * this->_universalNProfileUnits[2] + zun;
	       }
	       else 
	       {
	          global_log->error() << "Severe error!! Invalid profile unit (" << xun << " " <<_universalNProfileUnits[0] << " / " << yun << " " << _universalNProfileUnits[1] << " / " << zun << " " << _universalNProfileUnits[2] << ").\n\n";
	          global_log->error() << "Coordinates (" << qx << " / " << qy << " / " << qz << ").\n";
		  global_log->error() << "unID = " << unID << "\n";
	          //exit(707);
	       }
	       return unID;
}

// Method called in the case of a density profile established in cylindrical coordinates. Counterpart of outputProfile(...).
// reason for a sperate method (in addition to "outputProfile(...)"): method neatly matched to the particular needs of the (cylindrical density) profile, otherwise outpuProfile would be inflated, structure became too compilcated.
void Domain::outputCylindricalProfile(const char* prefix){

	if(this->_localRank) return;

	   unsigned IDweight[3];
	   IDweight[0] = this->_universalNProfileUnits[1] * this->_universalNProfileUnits[2];
	   IDweight[1] = this->_universalNProfileUnits[2];
	
	   for( map<unsigned, bool>::iterator pcit = _universalProfiledComponents.begin();    // Loop ueber alle Komponenten
	   	           pcit != _universalProfiledComponents.end();
	   	           pcit++ )
	   	      {
	   	         if(!pcit->second) continue;  // ->second weist auf den key-value von map, d.h. den bool-Wert => falls falsche id, d.h. hiervon ist kein Profil zu erstellen => naechste Schleife

			 // density profile
	   	         string rhoProfName("./Results/CylindricalProfile/");
			 rhoProfName += prefix;
	   	         rhoProfName += ".rhpr";
			 // temperature profile
			 string tmpProfName("./Results/CylindricalProfile/");
			 tmpProfName += prefix;
			 tmpProfName += ".Tpr";
			 // velocity gradient profile
			 string yVelProfname("./Results/CylindricalProfile/");
			 yVelProfname+= prefix;
			 yVelProfname+= ".vxpry";
			 
	   	         if(!this->_universalCylindricalGeometry)
	   	         {
	   	           global_log->error() << "Incorrect call of method \"outputCylindricalProfile()\" !";
	   	         }

	   	         ofstream* rhoProf = new ofstream(rhoProfName.c_str());
			 ofstream* tmpProf = new ofstream(tmpProfName.c_str());
			 ofstream* yVelProf = new ofstream(yVelProfname.c_str());
			 
	   	         if (!(*rhoProf ) || !(*tmpProf)) // geaendert durch M. Horsch, by Stefan Becker: wozu?
	   	         {
	   	            return;
	   	         }
	   	         rhoProf->precision(6);
			 tmpProf->precision(6);
			 yVelProf->precision(6);
			 
	    //##########################################################################################################################################			 
			 // density profile: actual writing procedure 
			 // for constant segmentVolume with varrying d(r2)
	   	         double segmentVolume; // volume of a single bin, in a0^3 (atomic units)
	   	      	 segmentVolume = 0.5/(this->_universalInvProfileUnit[1] * this->_universalInvProfileUnit[2] * this->_universalInvProfileUnit[0]);  // adapted to cylindrical coordinates
			 double radius[this->_universalNProfileUnits[1] + 1];
			  
			 // for constant dr; adaption of segmentVolume -> variation of dV with radius
			 // double segmentVolume[this->_universalNProfileUnits[1]];
			 // for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
			 //	 double r2_low = (this->_universalR2min + n_r2 * 1/this->_universalInvProfileUnit[1]);
			 //	 r2_low *= r2_low; 
			 //      double r2_high = (this->_universalR2min + (n_r2 + 1) * 1/this->_universalInvProfileUnit[1]);
			 //	 r2_high *= r2_high;
			 //      segmentVolume[n_r2] = 1/this->_universalInvProfileUnit[0] * 1/this->_universalInvProfileUnits[2] * 0.5 * (r2_high - r2_low);
			 
			 
			 *rhoProf << "//Local profile of the number density. Output file generated by the \"outputCylindricalProfile\" method, located in Domain.cpp. \n";
	   	         *rhoProf << "//local density profile: Each matrix corresponds to a single value of \"phi_i\", measured in [rad]\n";
	   	         *rhoProf << "//one single matrix of the local number density rho'(phi_i';r_i') \n//      | r_i'\n//---------------------\n//  phi_i'| rho'(r_i',phi_i')\n//      | \n";
			 *rhoProf << "//nDELTA_phi \t nDELTA_r2' \t nDELTA_h'\n";
			 *rhoProf << this->_universalNProfileUnits[0] << "\t\t" << this->_universalNProfileUnits[1] << "\t\t" << this->_universalNProfileUnits[2]<< "\n";
	   	         *rhoProf << "//DELTA_phi \t DELTA_r2' \t DELTA_h' \t quantities in atomic units are denoted by an apostrophe '\n";
	   	         //*rhoProf << this->_universalTargetTemperature[_components.size()] <<"\t"<<_components[0].getSigma(0)<<"\t"<<_components[0].getEps(0)<<"\t";
	   	         *rhoProf << 1/this->_universalInvProfileUnit[0] << "\t\t" << 1/this->_universalInvProfileUnit[1] << "\t\t" << 1/this->_universalInvProfileUnit[2]<< "\n";
	   	         // info: getSigma() und getEps() implementiert in Component.h
	   	         // end of header, start of the data-part of the density file
			 *rhoProf << "//# lines\n";
			 *rhoProf << this->_universalNProfileUnits[0]+1 << "\n";
			 *rhoProf << "//# rows\n";
			 *rhoProf << this->_universalNProfileUnits[1]+1 << "\n";
			 
			 *rhoProf <<"> \n";
			 *rhoProf << 0 <<"  \t";
			 radius[0] = sqrt(this->_universalR2min);
			 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
				radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
	   	        	*rhoProf << radius[n_r2] + 0.5*(radius[n_r2+1] - radius[n_r2]) <<"  \t"; // Eintragen der radialen Koordinaten r_i in Header
	   	         }
			 *rhoProf << "\n";
			 
	   	         for(unsigned n_phi = 0; n_phi < this->_universalNProfileUnits[0]; n_phi++)
	   	         {
	   	        	 *rhoProf << (n_phi+0.5)/this->_universalInvProfileUnit[0] <<"  \t";
	   	        	 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
					 radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
	   	        		 unsigned unID = n_phi * IDweight[0] + n_r2 * IDweight[1];
	   	        			                                   
	   	        	   	 double rho_loc = this->_universalNProfile[unID] / (segmentVolume * this->_globalAccumulatedDatasets);
	   	        	   	 *rhoProf << rho_loc << "\t";
	   	        	 }
	   	        	 *rhoProf << "\n";	   	        	
	   	         }
	   	         rhoProf->close();
	   	         delete rhoProf;
 
	    //##########################################################################################################################################
	   	         // temperature profile: actual writing procedure
			 *tmpProf << "//Local temperature profile generated by the \"outputCylindricalProfile\" method.\n";
			 *tmpProf << "//Temperature expressed by 2Ekin/#DOF\n";
			 *tmpProf << "//one single matrix of the local temperature T(phi_i';r_i') \n//      | r_i\n//---------------------\n//  phi_i'| T(r_i',phi_i')\n//      | \n";
			 *tmpProf << "//nDELTA_phi \t nDELTA_r2' \t nDELTA_h'\n";
			 *tmpProf << this->_universalNProfileUnits[0] << "\t\t" << this->_universalNProfileUnits[1] << "\t\t" << this->_universalNProfileUnits[2]<< "\n";
	   	         *tmpProf << "//DELTA_phi \t DELTA_r2 \t DELTA_h \n";
	   	         //*tmpProf << this->_universalTargetTemperature[_components.size()] <<"\t"<<_components[0].getSigma(0)<<"\t"<<_components[0].getEps(0)<<"\t";
	   	         *tmpProf << 1/this->_universalInvProfileUnit[0] << "\t" << 1/this->_universalInvProfileUnit[1] << "\t" << 1/this->_universalInvProfileUnit[2]<< "\n";
			 *tmpProf << "//# lines\n";
			 *tmpProf << this->_universalNProfileUnits[0]+1 << "\n";
			 *tmpProf << "//# rows\n";
			 *tmpProf << this->_universalNProfileUnits[1]+1 << "\n";
	   	        	
			 
			 *tmpProf <<"> \n";
			 *tmpProf << 0 <<"  \t";
			 radius[0] = sqrt(this->_universalR2min);
			 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
				radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
	   	        	*tmpProf << radius[n_r2] + 0.5*(radius[n_r2+1] - radius[n_r2]) <<"  \t"; // Eintragen der radialen Koordinaten r_i in Header
	   	         }
			 *tmpProf << "\n";			 
			 for(unsigned n_phi = 0; n_phi < this->_universalNProfileUnits[0]; n_phi++)
	   	         {
	   	        	 *tmpProf << (n_phi+0.5)/this->_universalInvProfileUnit[0] <<"  \t";
	   	        	 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
					 radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
	   	        		 unsigned unID = n_phi * IDweight[0] + n_r2 * IDweight[1];
	   	        		   	
					 double DOFc = this->_universalDOFProfile[unID];
					 double twoEkinc = this->_universalKineticProfile[unID];
					
					 if(DOFc == 0.0){
					     *tmpProf << 0 << "\t";
					 }
					 else{
					     *tmpProf << (twoEkinc/DOFc ) << "\t";
					 }
	   	        	 } 
	   	        	 *tmpProf << "\n";
	   	         }
			 
			  tmpProf->close();
			  delete tmpProf;
	   	      
			
			  
			  
			 //##########################################################################################################################################			 
			 // average velocity in phi-direction -> profile: actual writing procedure 
	   	         *yVelProf << "//Local profile of the velocity gradient in phi-direction. Output file generated by the \"outputCylindricalProfile\" method, located in Domain.cpp. \n";
	   	         *yVelProf << "//local velocity gradient profile: Each matrix corresponds to a single value of \"phi_i\", measured in [rad]\n";
	   	         *yVelProf << "//one single matrix of the local velocity gradient phi-velGrad'(phi_i;r_i') \n//      | r_i'\n//---------------------\n//  phi_i'| phi-velGrad'(r_i',phi_i')\n//      | \n";
			 *yVelProf << "//nDELTA_phi \t nDELTA_r2' \t nDELTA_h'\n";
			 *yVelProf << this->_universalNProfileUnits[0] << "\t\t" << this->_universalNProfileUnits[1] << "\t\t" << this->_universalNProfileUnits[2]<< "\n";
	   	         *yVelProf << "//DELTA_phi \t DELTA_r2' \t DELTA_h' \t quantities in atomic units are denoted by an apostrophe '\n";
	   	         //*yVelProf << this->_universalTargetTemperature[_components.size()] <<"\t"<<_components[0].getSigma(0)<<"\t"<<_components[0].getEps(0)<<"\t";
	   	         *yVelProf << 1/this->_universalInvProfileUnit[0] << "\t" << 1/this->_universalInvProfileUnit[1] << "\t" << 1/this->_universalInvProfileUnit[2]<< "\n";
	   	         // info: getSigma() und getEps() implementiert in Component.h
	   	         // end of header, start of the data-part of the density file
			 *yVelProf << "//# lines\n";
			 *yVelProf << this->_universalNProfileUnits[0]+1 << "\n";
			 *yVelProf << "//# rows\n";
			 *yVelProf << this->_universalNProfileUnits[1]+1 << "\n";
			 
						 
			 *yVelProf <<"> \n";
			 *yVelProf << 0 <<"  \t";
			 radius[0] = sqrt(this->_universalR2min);
			 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
				radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
	   	        	*yVelProf << radius[n_r2] + 0.5*(radius[n_r2+1] - radius[n_r2]) <<"  \t"; // Eintragen der radialen Koordinaten r_i in Header
	   	         }
			 *yVelProf << "\n";
			 
			 for(unsigned n_phi = 0; n_phi < this->_universalNProfileUnits[0]; n_phi++)
	   	         {
	   	        	 *yVelProf << (n_phi+0.5)/this->_universalInvProfileUnit[0] <<"  \t";
				 radius[0] = sqrt(this->_universalR2min);
	   	        	 for(unsigned n_r2 = 0; n_r2 < this->_universalNProfileUnits[1]; n_r2++){
					radius[n_r2+1] = sqrt(radius[n_r2]*radius[n_r2] + 1/this->_universalInvProfileUnit[1]); 
					 
					unsigned unID = n_phi * IDweight[0] + n_r2 * IDweight[1];
					// kartesian coordinates
   	        		   	double average_Vx = this->_universalvProfile[0][unID]/(this->_universalNProfile[unID] * this->_globalAccumulatedDatasets);
					double average_Vy = this->_universalvProfile[1][unID]/(this->_universalNProfile[unID] * this->_globalAccumulatedDatasets);
					double absolute_V = sqrt(average_Vx*average_Vx + average_Vy*average_Vy);
					// coordinate transformation to polar coordinates
					double alpha = acos(average_Vx/absolute_V);
					double beta = (n_phi+0.5)/this->_universalInvProfileUnit[0] - M_PI/2 - alpha;
					// final value for average velocity along the phi-direction
					double phiGradient_v = absolute_V * cos(beta);
					// Here it is checked whether phiGradient_v is NaN or not; if it is, it is set to zero;
					/* FIXME: */
					if (phiGradient_v != phiGradient_v){
					    phiGradient_v = 0.0;
					}
					  
					*yVelProf << phiGradient_v << "\t";
					 
	   	        	 }
	   	        	 *yVelProf << "\n";
	   	         }
			 
	   	         yVelProf->close();
	   	         delete yVelProf;
			 
		      }

}

void Domain::setupSlabProfile(unsigned xun, unsigned yun, unsigned zun){
  this->_universalNProfileUnitsSlab[0] = xun;
  this->_universalNProfileUnitsSlab[1] = yun;
  this->_universalNProfileUnitsSlab[2] = zun;
  
  this->_maxSlabDist2 = _globalLength[2]*_globalLength[2]/4.0;
  this->_universalCenterZ = 0.5*_globalLength[2];
  
  // inverse step width of the increments (defining the elemental volumes)
  _universalInvProfileUnitSlab[0] = _universalNProfileUnitsSlab[0] / _globalLength[0];
  _universalInvProfileUnitSlab[1] = _universalNProfileUnitsSlab[1] / _globalLength[1];
  _universalInvProfileUnitSlab[2] = _universalNProfileUnitsSlab[2]/ _globalLength[2];
  
  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
      mkdir("./Results/SlabProfile", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk) 
      mkdir("./Results/SlabProfileVTK", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

void Domain::recordSlabProfile(ParticleContainer* molCont){
  int cid;
  unsigned xun, yun, zun;
  unsigned long unID;
  double mv2, Iw2;
  string Slab ("Slab");
	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{
		cid = thismol->componentid();
		if(this->_universalProfiledComponentsSlab[cid])
		{
			double distToSlab2 = pow(thismol->r(2)- this->_universalCenterZ, 2.0);
			if(distToSlab2 <= this-> _maxSlabDist2){
			    xun = (unsigned)floor(thismol->r(0) * this->_universalInvProfileUnitSlab[0]);
			    yun = (unsigned)floor(thismol->r(1) * this->_universalInvProfileUnitSlab[1]);
			    zun = (unsigned)floor(thismol->r(2) * this->_universalInvProfileUnitSlab[2]);
			    unID = xun * this->_universalNProfileUnitsSlab[1] * this->_universalNProfileUnitsSlab[2]
				    + yun * this->_universalNProfileUnitsSlab[2] + zun;  
			}
			else{
				continue;
			}
			this->_localNProfileSlab[unID] += 1;
			if(!_simulation.reduceData())
				for(int d=0; d<3; d++){
				  this->_localvProfileSlab[d][unID] += thismol->v(d);
				}
			this->_localDOFProfileSlab[unID] += 3 + (thismol->component()->getRotationalDegreesOfFreedom());

			// record _twice_ the total (ordered + unordered) kinetic energy
			mv2 = 0.0;
			Iw2 = 0.0;
			thismol->calculate_mv2_Iw2(mv2, Iw2, Slab);
			this->_localKineticProfileSlab[unID] += mv2+Iw2;
		}
		for(int d=0; d<3; d++)
		  thismol->addAveragedVelocity(d,thismol->v(d));
		mv2 = 0.0;
		Iw2 = 0.0;
		thismol->calculate_mv2_Iw2(mv2, Iw2, Slab);
		thismol->addAveragedTemperature((mv2+Iw2)/(3.0 + (long double)(thismol->component()->getRotationalDegreesOfFreedom())));
		thismol->addAverageCount(1);
	}
	this->_globalAccumulatedDatasetsSlab++;
}

void Domain::collectSlabProfile(DomainDecompBase* dode)
{
	unsigned long unIDs = this->_universalNProfileUnitsSlab[0] * this->_universalNProfileUnitsSlab[1]
		* this->_universalNProfileUnitsSlab[2];
	unsigned long unIDs_Init;
	if(!_simulation.reduceData())
		unIDs_Init = 6*unIDs;
	else
		unIDs_Init = 3*unIDs;	
	dode->collCommInit(unIDs_Init);
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		dode->collCommAppendUnsLong(this->_localNProfileSlab[unID]);
		if(!_simulation.reduceData())
			for(int d=0; d<3; d++)
				dode->collCommAppendDouble(_localvProfileSlab[d][unID]);
		dode->collCommAppendUnsLong(this->_localDOFProfileSlab[unID]);
		dode->collCommAppendDouble(_localKineticProfileSlab[unID]);

	}
	dode->collCommAllreduceSum();
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		_universalNProfileSlab[unID] = (unsigned)dode->collCommGetUnsLong();
		if(!_simulation.reduceData())
			for(int d=0; d<3; d++)
				this->_universalvProfileSlab[d][unID]
					= (double)dode->collCommGetDouble();
		this->_universalDOFProfileSlab[unID]
			= (unsigned)dode->collCommGetUnsLong();
		this->_universalKineticProfileSlab[unID]
			= (double)dode->collCommGetDouble();

	}
	dode->collCommFinalize();
}

void Domain::outputSlabProfile(const char* prefix){
      if(this->_localRank) return;
      
	// density profile
	string rhprname("./Results/SlabProfile/");
	rhprname += prefix;
	rhprname += ".rhoprxy";
	// temperature profile
	string Tprname("./Results/SlabProfile/");
	Tprname += prefix;
	Tprname += ".Tprxy";
	// velocity profile
	string vxprname("./Results/SlabProfile/");
	vxprname += prefix;
	vxprname += ".vxprxy";
	
	ofstream rhoProf(rhprname.c_str());
	ofstream TProf(Tprname.c_str());
	ofstream vxProf(vxprname.c_str());
	if ( (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && (!(rhoProf)||!(TProf)||!(vxProf)) )
	{
		return;
	}
	
	rhoProf.precision(6);
	TProf.precision(6);
	vxProf.precision(6);

      // VTK data format
	string vtkname("./Results/SlabProfileVTK/");
	vtkname += prefix;
	vtkname += ".vtk";
	
	//____________________________________________________________________________________________________________________________________________________________________
		
	double segmentVolume; // volume of a single bin, in a0^3 (LJ)
	segmentVolume = 1.0/this->_universalInvProfileUnitSlab[0]/this->_universalInvProfileUnitSlab[1]/this->_universalInvProfileUnitSlab[2];
	
	// VisIT VTK data format
	 int NX = (int)this->_universalNProfileUnitsSlab[0];
	 int NY = (int)this->_universalNProfileUnitsSlab[1];
	 int NZ = (int)this->_universalNProfileUnitsSlab[2];
	 int dims[] = {NX, NY, NZ};
	 int nvars;
	 if(!_simulation.reduceData())
		nvars = 3;
	 else
		nvars = 2;
		
	 int vardims[nvars], centering[nvars];
	 const char *varnames[nvars];
	 float *vars[nvars];
	 float density[NZ][NY][NX];
	 float temperature[NZ][NY][NX];
	 float velocity[NZ][NY][NX][3];
	 
	 if(!_simulation.reduceData()){
		vardims[0] = 1;
		vardims[1] = 1;
		vardims[2] = 3;
		centering[0] = 1;
		centering[1] = 1;
		centering[2] = 1;
		varnames[0] = "density";
		varnames[1] = "temperature";
		varnames[2] = "velocity";
		vars[0] = (float *)density;
		vars[1] = (float *)temperature;
		vars[2] = (float *)velocity;
	 }else{ // reduced data without velocity
		vardims[0] = 1;
		vardims[1] = 1;
		centering[0] = 1;
		centering[1] = 1;
		varnames[0] = "density";
		varnames[1] = "temperature";
		vars[0] = (float *)density;
		vars[1] = (float *)temperature;
	 }
	 	
	if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1){
	  rhoProf << "//Local profile of the number density. Output file generated by the \"outputSlabProfile\" method, located in Domain.cpp. \n";
	  rhoProf << "//Local density profile: The number density is determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	  rhoProf << "//The matrix of the local number density rho(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	  rhoProf << "//DELTA_x \t DELTA_y \t width_z\n";
	  rhoProf << 1/this->_universalInvProfileUnitSlab[0] << "\t" << 1/this->_universalInvProfileUnitSlab[1] << "\t" << 1/this->_universalInvProfileUnitSlab[2] << "\n";
	  rhoProf << "//# lines\n";
	  rhoProf << this->_universalNProfileUnitsSlab[1]+1 << "\n";
	  rhoProf << "//# rows\n";
	  rhoProf << this->_universalNProfileUnitsSlab[0]+1 << "\n";
	  // info: getSigma() und getEps() implementiert in Component.h
	  // end of header, start of the data-part of the density file  
	 
	  // Eintragen des Flags '>' zwecks Kompatibilität
	  rhoProf << "> \n"; 
	  // Eintragen der x-Koordinaten x_i in Header
	  rhoProf << 0 <<"  \t"; 
	  for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsSlab[0]; n_x++){
	    rhoProf << (n_x + 0.5) / this->_universalInvProfileUnitSlab[0] <<"  \t"; 
	  }
	  rhoProf << "\n";
	}
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte rho(y_i,xi)
	for(unsigned n_z = 0; n_z < this->_universalNProfileUnitsSlab[2]; n_z++)
	{
	  for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsSlab[1]; n_y++)
	  {
	    double yval = (n_y + 0.5) / this->_universalInvProfileUnitSlab[1];
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)  
	      rhoProf << yval<< "  \t";
	    for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsSlab[0]; n_x++)
	    {
	      unsigned long unID = n_x * this->_universalNProfileUnitsSlab[1] * this->_universalNProfileUnitsSlab[2] + n_y * this->_universalNProfileUnitsSlab[2] + n_z;
	      double rho_loc = (double)this->_universalNProfileSlab[unID] / (segmentVolume * this->_globalAccumulatedDatasetsSlab);
	      if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
		rhoProf << rho_loc << "\t";
	      if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
		density[n_z][n_y][n_x] = rho_loc;
	    }	
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
	      rhoProf << "\n";
	  }
	}
	if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
	   rhoProf.close();
	 
	//____________________________________________________________________________________________________________________________________________________________________
	
	 if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1){ 
	  TProf << "//Local profile of temperature. Output file generated by the \"outputSlabProfile\" method, located in Domain.cpp. \n";
	  TProf << "//Local temperature profile: The temperature is determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	  TProf << "//The matrix of the local temperature T(y,x) \n//      | y_i\n//---------------------\n//  x_i| T(y_i,x_i)\n//      | \n";
	  TProf << "//DELTA_x \t DELTA_y \t width_z\n";
	  TProf << 1/this->_universalInvProfileUnitSlab[0] << "\t" << 1/this->_universalInvProfileUnitSlab[1] << "\t" << 1/this->_universalInvProfileUnitSlab[2] << "\n";
	  TProf << "//# lines\n";
	  TProf << this->_universalNProfileUnitsSlab[1]+1 << "\n";
	  TProf << "//# rows\n";
	  TProf << this->_universalNProfileUnitsSlab[0]+1 << "\n";
	  // info: getSigma() und getEps() implementiert in Component.h
	  // end of header, start of the data-part of the density file 
	
	
	  // Eintragen des Flags '>' zwecks Kompatibilität
	  TProf << "> \n"; 
	  // Eintragen der x-Koordinaten x_i in Header
	  TProf << 0 <<"  \t"; 
	  for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsSlab[0]; n_x++){
	    TProf << (n_x + 0.5) / this->_universalInvProfileUnitSlab[0] <<"  \t"; 
	  }
	  TProf << "\n";
	}
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte rho(y_i,xi)
	for(unsigned n_z = 0; n_z < this->_universalNProfileUnitsSlab[2]; n_z++)
	{
	  for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsSlab[1]; n_y++)
	  {
	    double yval = (n_y + 0.5) / this->_universalInvProfileUnitSlab[1];
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
	      TProf << yval<< "  \t";
	    for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsSlab[0]; n_x++)
	    {
	      unsigned long unID = n_x * this->_universalNProfileUnitsSlab[1] * this->_universalNProfileUnitsSlab[2] + n_y * this->_universalNProfileUnitsSlab[2] + n_z;
	      double DOFc = (double)this->_universalDOFProfileSlab[unID];
	      double twoEkinc = this->_universalKineticProfileSlab[unID];
	      if(DOFc == 0.0){
		if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)    
		  TProf << 0 << "\t";
		if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)  
		  temperature[n_z][n_y][n_x] = 0.0;
	      }
	      else{
		if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
		  TProf << (twoEkinc/DOFc) << "\t";
		if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)  
		  temperature[n_z][n_y][n_x] = twoEkinc/DOFc;
	      }
	    }
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1) 
	      TProf << "\n";
	  }
	}
	if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)
	   TProf.close(); 
	 
	//____________________________________________________________________________________________________________________________________________________________________
	
	 if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1){   
	  vxProf << "//Local x-velocity. Output file generated by the \"outputSlabProfile\" method, located in Domain.cpp. \n";
	  vxProf << "//Local x-velocity: The x-velocity is determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	  vxProf << "//The matrix of the local x-velocity vx(y,x) \n//      | y_i\n//---------------------\n//  x_i| vx(y_i,x_i)\n//      | \n";
	  vxProf << "//DELTA_x \t DELTA_y \t width_z\n";
	  vxProf << 1/this->_universalInvProfileUnitSlab[0] << "\t" << 1/this->_universalInvProfileUnitSlab[1] << "\t" << 1/this->_universalInvProfileUnitSlab[2] << "\n";
	  vxProf << "//# lines\n";
	  vxProf << this->_universalNProfileUnitsSlab[1]+1 << "\n";
	  vxProf << "//# rows\n";
	  vxProf << this->_universalNProfileUnitsSlab[0]+1 << "\n";
	  // info: getSigma() und getEps() implementiert in Component.h	
	  // end of header, start of the data-part of the density file 
	
	
	  // Eintragen des Flags '>' zwecks Kompatibilität
	  vxProf << "> \n"; 
	  // Eintragen der x-Koordinaten x_i in Header
	  vxProf << 0 <<"  \t"; 
	  for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsSlab[0]; n_x++){
	    vxProf << (n_x + 0.5) / this->_universalInvProfileUnitSlab[0] <<"  \t"; 
	  }
	  vxProf << "\n";
	}
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte vx(y_i,x_i)
	for(unsigned n_z = 0; n_z < this->_universalNProfileUnitsSlab[2]; n_z++)
	{
	  for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsSlab[1]; n_y++)
	  {
	    double yval = (n_y + 0.5) / this->_universalInvProfileUnitSlab[1];
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)    
	      vxProf << yval<< "  \t";	
	    for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsSlab[0]; n_x++)
	    {
	      double average_Vx;
	      double average_Vy;
	      double average_Vz;
	      unsigned long unID;
	     if(!_simulation.reduceData()){
	      unID = n_x * this->_universalNProfileUnitsSlab[1] * this->_universalNProfileUnitsSlab[2] + n_y * this->_universalNProfileUnitsSlab[2] + n_z;
	      average_Vx = this->_universalvProfileSlab[0][unID]/((double)this->_universalNProfileSlab[unID]);
	      average_Vy = this->_universalvProfileSlab[1][unID]/((double)this->_universalNProfileSlab[unID]);
	      average_Vz = this->_universalvProfileSlab[2][unID]/((double)this->_universalNProfileSlab[unID]);
	     }else{
	      unID = 0;
	      average_Vx = 0.0;
	      average_Vy = 0.0;
	      average_Vz = 0.0;
	     }
	      // Here it is checked whether average_Vx is NaN or not; if it is, it is set to zero;
	      /* FIXME: */
	      if (average_Vx != average_Vx)
		average_Vx = 0.0;
	      if (average_Vy != average_Vy)
		average_Vy = 0.0;
	      if (average_Vz != average_Vz)
		average_Vz = 0.0;
	      if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)  
		vxProf << average_Vx << "\t";
	      if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){  
		velocity[n_z][n_y][n_x][0] = average_Vx;
		velocity[n_z][n_y][n_x][1] = average_Vy;
		velocity[n_z][n_y][n_x][2] = average_Vz;
	      }
	    }
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)  
	      vxProf << "\n";
	  }
	}
	if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnitsSlab[2] == 1)  
	   vxProf.close(); 
	 
	/* Use VisitWriter.cpp to write a regular mesh with data. */
	if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)  
	   write_regular_mesh(vtkname.c_str(), 0, dims, nvars, vardims, centering, varnames, vars);	 
}

void Domain::setupStressProfile(unsigned xun, unsigned yun, unsigned zun, bool properties[6]){
  this->_universalNProfileUnits_Stress[0] = xun;
  this->_universalNProfileUnits_Stress[1] = yun;
  this->_universalNProfileUnits_Stress[2] = zun;
  
  // all properties that are written in output are defined
  for(int property = 0; property < 6; property++)
	this->_stressOutputProperty[property] = properties[property];
  
  this->_maxSlabDist2_Stress = _globalLength[2]*_globalLength[2]/4.0;
  this->_universalCenterZ_Stress = 0.5*_globalLength[2];
  
  // inverse step width of the increments (defining the elemental volumes)
  _universalInvProfileUnit_Stress[0] = _universalNProfileUnits_Stress[0] / _globalLength[0];
  _universalInvProfileUnit_Stress[1] = _universalNProfileUnits_Stress[1] / _globalLength[1];
  _universalInvProfileUnit_Stress[2] = _universalNProfileUnits_Stress[2] / _globalLength[2];  
  
  size_t rows = 6, cols = xun*yun*zun;
  this->_localStress = this->allocStressMatrix(rows, cols);
  this->_universalStress = this->allocStressMatrix(rows, cols);
  
  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
    mkdir("./Results/xxStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/yyStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/zzStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/xyStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/xzStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/yzStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/HydrodynamicStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("./Results/vonMisesStress", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)  
    mkdir("./Results/StressVTK", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

void Domain::setupStressProperties(bool properties[6]){
  // all properties that are written in output are defined
  for(int property = 0; property < 6; property++)
	this->_stressOutputProperty[property] = properties[property];
}

double **Domain::allocStressMatrix(size_t rows, size_t cols){
  
  double **matrix;
  matrix = new double*[rows];
    
  for(size_t i = 0; i < rows; i++)
    *(matrix + i) = new double[cols];
  
  int row = static_cast<int>(rows);
  unsigned long col = static_cast<unsigned long>(cols);
  
  for(int n = 0; n < row; n++)
    for(unsigned long m = 0; m < col; m++)
      matrix[n][m] = 0.0;

  return matrix;
}

void Domain::dellocStressMatrix(double **matrix, size_t rows, size_t cols){
  for (size_t i = 0; i < rows; i++)
    delete [] *(matrix + i);
 
  delete [] matrix;
}


void Domain::recordStressProfile(ParticleContainer* molCont, unsigned long simstep, unsigned long initStatistics){
  int cid;
  unsigned xun, yun, zun;
  long int unID;
  int countMol = 0;
  string Stress ("Stress");
  
  if (simstep == initStatistics) 
	  this->resetStressProfile();
		
  	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{
		cid = thismol->componentid();
		if(this->isStressCalculating(cid))
		{	
		    if(simstep == initStatistics){	
			// reset collected virial values of each molecule
			for(int d = 0; d < 3; d++){
				thismol->setConvectivePotHeatfluxStress(d, 0.0);
				for(int e = 0; e < 3; e++){
					thismol->setDiffusiveHeatfluxStress(d, e, 0.0);
				}	
			}
			thismol->clearDiffusiveHeatfluxHardyStress();
		    }
		    
			double mv2 = 0.0;
			double Iw2 = 0.0;
			thismol->calculate_mv2_Iw2(mv2, Iw2, Stress);
			
			double distToSlab2 = pow(thismol->r(2)- this->_universalCenterZ_Stress, 2.0);
			std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > > virialHardy = thismol->getVirialForceHardyStress();
			std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > > heatDiffHardy = thismol->getDiffusiveHeatfluxHardyStress();
			xun = (unsigned)floor(thismol->r(0) * this->_universalInvProfileUnit_Stress[0]);
			yun = (unsigned)floor(thismol->r(1) * this->_universalInvProfileUnit_Stress[1]);
			zun = (unsigned)floor(thismol->r(2) * this->_universalInvProfileUnit_Stress[2]);
			unID = xun * this->_universalNProfileUnits_Stress[1] * this->_universalNProfileUnits_Stress[2]  + yun * this->_universalNProfileUnits_Stress[2] + zun;
			if(distToSlab2 <= this->_maxSlabDist2_Stress){
			 if(this->_stressOutputProperty[5] == true)
			  for(int d = 0; d < 3; d++){
				this->_localDiffusiveMovementStress[d][unID] += thismol->rOld(d) - thismol->r(d) - thismol->getDirectedVelocityStress(d)*getTimestepLength();
				this->_localDiffusiveMovementStress[d][unID] *= this->_localDiffusiveMovementStress[d][unID];
			  }
			  if(thismol->isHardyStress() && this->_universalNProfileUnits_Stress[2] == 1){
			    // for Linear the weightingFrac = 1 as long as a particle is in the control volume
			    double weightingFrac = 1.0;
			    string weightingFunc = thismol->getWeightingFuncStress();
			    string Linear ("Linear");
			    string Pyramide ("Pyramide");
			    if(weightingFunc == Pyramide)
			      weightingFrac = thismol->weightingFunctionPyramide(xun, yun, 1/this->_universalInvProfileUnit_Stress[0], 1/this->_universalInvProfileUnit_Stress[1], 0.0, 0.0);
			    this->_localNProfile_Stress[unID] += 1;
			    for(std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > >::iterator it=virialHardy.begin(); it!=virialHardy.end(); ++it){
			      for(int d = 0; d < 3; d++)
				for(int e = d; e < 3; e++){
				  if(d == e){
				    this->_localStress[d][it->first] += virialHardy[it->first][d][e];
				    if(this->_stressOutputProperty[4] == true){
					for(int f = 0; f < 3; f++)
						this->_localDiffusiveHeatfluxStress[d][it->first] += heatDiffHardy[it->first][d][f]*(thismol->v(f) - thismol->getDirectedVelocityStress(f)); 
				    }
				    // update just once per molecule
				    if(it == virialHardy.begin()){
				      this->_localStress[d][unID] += weightingFrac*(double)thismol->getVirialKin(d, e);
				      if(this->_stressOutputProperty[4] == true){
					this->_localConvectiveKinHeatfluxStress[d][unID] += 0.5 * mv2 * (thismol->v(d) - thismol->getDirectedVelocityStress(d)) * weightingFrac;
					this->_localConvectivePotHeatfluxStress[d][unID] += thismol->getConvectivePotHeatfluxStress(d) * (thismol->v(d) - thismol->getDirectedVelocityStress(d)) * weightingFrac;
				      }
				    }
				  }else{
				    this->_localStress[2+d+e][it->first] += virialHardy[it->first][d][e];
				    // update just once per molecule
				    if(it == virialHardy.begin())
				      this->_localStress[2+d+e][unID] += weightingFrac*(double)thismol->getVirialKin(d, e);
				  }
				}
			    }
			  }else{   
			    this->_localNProfile_Stress[unID] += 1; 
			    for(int d = 0; d < 3; d++)
			      for(int e = d; e < 3; e++){
				if(d == e){
				   this->_localStress[d][unID] += (double)thismol->getVirialForce(d, e);
				   this->_localStress[d][unID] += (double)thismol->getVirialKin(d, e);
				   if(this->_stressOutputProperty[4] == true){
					this->_localConvectivePotHeatfluxStress[d][unID] += thismol->getConvectivePotHeatfluxStress(d) * (thismol->v(d) - thismol->getDirectedVelocityStress(d));
					this->_localConvectiveKinHeatfluxStress[d][unID] += 0.5 * mv2 * (thismol->v(d) - thismol->getDirectedVelocityStress(d));
					for(int f = 0; f < 3; f++)
						this->_localDiffusiveHeatfluxStress[d][unID] += thismol->getDiffusiveHeatfluxStress(d, f)*(thismol->v(f) - thismol->getDirectedVelocityStress(f));
				   }
				}else{
				   this->_localStress[2+d+e][unID] += (double)thismol->getVirialForce(d, e);
				   this->_localStress[2+d+e][unID] += (double)thismol->getVirialKin(d, e);
				}
			      }
			    countMol++;
			  }
			}
			else{
			    continue;
			}
		}
		else{
			 xun = (unsigned)floor(thismol->r(0) * this->_universalInvProfileUnit_Stress[0]);
			 yun = (unsigned)floor(thismol->r(1) * this->_universalInvProfileUnit_Stress[1]);
			 zun = (unsigned)floor(thismol->r(2) * this->_universalInvProfileUnit_Stress[2]);
			 unID = xun * this->_universalNProfileUnits_Stress[1] * this->_universalNProfileUnits_Stress[2]  + yun * this->_universalNProfileUnits_Stress[2] + zun;
		}
	    // reset collected virial values of each molecule
	    for(int d = 0; d < 3; d++){
	      thismol->setConvectivePotHeatfluxStress(d, 0.0);
	      for(int e = 0; e < 3; e++){
		thismol->setVirialForce(d, e, 0.0);
		thismol->setVirialKin(d, e, 0.0);
		thismol->setDiffusiveHeatfluxStress(d, e, 0.0);
	      }	
	    }
	    thismol->clearVirialForceHardyStress();
	    thismol->clearDiffusiveHeatfluxHardyStress();
	}
	this->_globalAccumulatedDatasets_Stress++;
}

void Domain::collectStressProfile(DomainDecompBase* dode)
{
	unsigned long unIDs = this->_universalNProfileUnits_Stress[0] * this->_universalNProfileUnits_Stress[1]
		* this->_universalNProfileUnits_Stress[2];
	unsigned long unID_total = 19*unIDs;
	if(this->_stressOutputProperty[4] == false)
		unID_total = unID_total - 9*unIDs;
	if(this->_stressOutputProperty[5] == false)
		unID_total = unID_total - 3*unIDs;
	dode->collCommInit(unID_total);
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		dode->collCommAppendUnsLong(this->_localNProfile_Stress[unID]);
		for(int d = 0; d < 3; d++){
		 if(this->_stressOutputProperty[5] == true)
		  dode->collCommAppendDouble(this->_localDiffusiveMovementStress[d][unID]);
		 if(this->_stressOutputProperty[4] == true){
		  dode->collCommAppendDouble(this->_localConvectiveKinHeatfluxStress[d][unID]);
		  dode->collCommAppendDouble(this->_localConvectivePotHeatfluxStress[d][unID]);
		  dode->collCommAppendDouble(this->_localDiffusiveHeatfluxStress[d][unID]);
		 }
		}
		for(int d = 0; d < 6; d++)
		  dode->collCommAppendDouble(this->_localStress[d][unID]);
	}
	dode->collCommAllreduceSum();
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		this->_universalNProfile_Stress[unID] = (unsigned)dode->collCommGetUnsLong();
		for(int d = 0; d < 3; d++){
		 if(this->_stressOutputProperty[5] == true)
		  this->_globalDiffusiveMovementStress[d][unID] = (long double)dode->collCommGetDouble();
		 if(this->_stressOutputProperty[4] == true){
		  this->_globalConvectiveKinHeatfluxStress[d][unID] = dode->collCommGetDouble();
		  this->_globalConvectivePotHeatfluxStress[d][unID] = dode->collCommGetDouble();
		  this->_globalDiffusiveHeatfluxStress[d][unID] = dode->collCommGetDouble();
		 }
		}
		// Definition Stress = (-1) * Pressure!!! 
		for(int d = 0; d < 6; d++)
		  this->_universalStress[d][unID] = (double)dode->collCommGetDouble();
	}
		
	dode->collCommFinalize();
	if(this->_stressOutputProperty[5] == true)
	  for(unsigned long unID = 0; unID < unIDs; unID++){
	      for(int d = 0; d < 3; d++){
		    this->_globalDiffusiveMovementStress[d][unID] = this->_globalDiffusiveMovementStress[d][unID]/(2*(long double)this->_universalNProfile_Stress[unID]*this->_globalAccumulatedDatasets_Stress*this->_stressRecordingTimesteps*getTimestepLength());
		    if (this->_globalDiffusiveMovementStress[d][unID] != this->_globalDiffusiveMovementStress[d][unID])
		      this->_globalDiffusiveMovementStress[d][unID] = 0.0;
	      }
	  }
}

void Domain::outputStressProfile(const char* prefix){
      if(this->_localRank) return;
      
	string rhprname_xx("./Results/xxStress/");
	rhprname_xx += prefix;
	rhprname_xx += ".xx_stress";
	ofstream stressProfxx(rhprname_xx.c_str());
	
	string rhprname_yy("./Results/yyStress/");
	rhprname_yy += prefix;
	rhprname_yy += ".yy_stress";
	ofstream stressProfyy(rhprname_yy.c_str());
	
	string rhprname_zz("./Results/zzStress/");
	rhprname_zz += prefix;
	rhprname_zz += ".zz_stress";
	ofstream stressProfzz(rhprname_zz.c_str());
	
	string rhprname_xy("./Results/xyStress/");
	rhprname_xy += prefix;
	rhprname_xy += ".xy_stress";
	ofstream stressProfxy(rhprname_xy.c_str());
	
	string rhprname_xz("./Results/xzStress/");
	rhprname_xz += prefix;
	rhprname_xz += ".xz_stress";
	ofstream stressProfxz(rhprname_xz.c_str());
	
	string rhprname_yz("./Results/yzStress/");
	rhprname_yz += prefix;
	rhprname_yz += ".yz_stress";
	ofstream stressProfyz(rhprname_yz.c_str());
	
	string rhprname_hydr("./Results/HydrodynamicStress/");
	rhprname_hydr += prefix;
	rhprname_hydr += ".hydr_stress";
	ofstream stressProfhydr(rhprname_hydr.c_str());
	
	string rhprname_mises("./Results/vonMisesStress/");
	rhprname_mises += prefix;
	rhprname_mises += ".mises_stress";
	ofstream stressProfmises(rhprname_mises.c_str());
	
	if ( (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && (!stressProfxx || !stressProfyy || !stressProfzz || !stressProfxy || !stressProfxz || !stressProfyz || !stressProfhydr || !stressProfmises) )
	{
		return;
	}
      // VTK data format
	string vtkname("./Results/StressVTK/");
	vtkname += prefix;
	vtkname += ".vtk";
      
      if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	stressProfxx.precision(4);	
	stressProfxx << "//Local profile of the xx stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfxx << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfxx << "//The matrix of the local stresses sigma_xx(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfxx << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfxx << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfxx << "//# lines\n";
	stressProfxx << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfxx << "//# rows\n";
	stressProfxx << this->_universalNProfileUnits_Stress[0]+1 << "\n";

	stressProfyy.precision(4);	
	stressProfyy << "//Local profile of the yy stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfyy << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfyy << "//The matrix of the local stresses sigma_yy(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfyy << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfyy << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfyy << "//# lines\n";
	stressProfyy << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfyy << "//# rows\n";
	stressProfyy << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfzz.precision(4);	
	stressProfzz << "//Local profile of the zz stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfzz << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfzz << "//The matrix of the local stresses sigma_zz(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfzz << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfzz << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfzz << "//# lines\n";
	stressProfzz << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfzz << "//# rows\n";
	stressProfzz << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfxy.precision(4);	
	stressProfxy << "//Local profile of the xy stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfxy << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfxy << "//The matrix of the local stresses sigma_xy(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfxy << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfxy << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfxy << "//# lines\n";
	stressProfxy << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfxy << "//# rows\n";
	stressProfxy << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfxz.precision(4);	
	stressProfxz << "//Local profile of the xz stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfxz << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfxz << "//The matrix of the local stresses sigma_xz(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfxz << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfxz << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfxz << "//# lines\n";
	stressProfxz << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfxz << "//# rows\n";
	stressProfxz << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfyz.precision(4);	
	stressProfyz << "//Local profile of the yz stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfyz << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfyz << "//The matrix of the local stresses sigma_yz(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfyz << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfyz << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfyz << "//# lines\n";
	stressProfyz << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfyz << "//# rows\n";
	stressProfyz << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfhydr.precision(4);	
	stressProfhydr << "//Local profile of the yz stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfhydr << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfhydr << "//The matrix of the local stresses sigma_yz(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfhydr << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfhydr << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfhydr << "//# lines\n";
	stressProfhydr << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfhydr << "//# rows\n";
	stressProfhydr << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	
	stressProfmises.precision(4);	
	stressProfmises << "//Local profile of the yz stress in the solid. Output file generated by the \"outputStressProfile\" method, located in Domain.cpp. \n";
	stressProfmises << "//Local stress profile: The stresses are determined in x,y-direction, in a slab of constant thickness located in the middle of the box at 0.5*Lz";
	stressProfmises << "//The matrix of the local stresses sigma_yz(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	stressProfmises << "//DELTA_x \t DELTA_y \t width_z\n";
	stressProfmises << 1/this->_universalInvProfileUnit_Stress[0] << "\t" << 1/this->_universalInvProfileUnit_Stress[1] << "\t" << 1/this->_universalInvProfileUnit_Stress[2] << "\n";
	stressProfmises << "//# lines\n";
	stressProfmises << this->_universalNProfileUnits_Stress[1]+1 << "\n";
	stressProfmises << "//# rows\n";
	stressProfmises << this->_universalNProfileUnits_Stress[0]+1 << "\n";
	// end of header, start of the data-part of the density file
      }
	
	
	double segmentVolume; // volume of a single bin, in a0^3 (LJ)
	segmentVolume = 1.0/this->_universalInvProfileUnit_Stress[0]/this->_universalInvProfileUnit_Stress[1]/this->_universalInvProfileUnit_Stress[2];
      
	// VisIT VTK data format
	 int NX = (int)this->_universalNProfileUnits_Stress[0];
	 int NY = (int)this->_universalNProfileUnits_Stress[1];
	 int NZ = (int)this->_universalNProfileUnits_Stress[2];
	 int dims[] = {NX, NY, NZ};
	
	 float density[NZ][NY][NX];
	 float HydrodynamicStress[NZ][NY][NX];
	 float NormalStress[NZ][NY][NX][3];
	 float ShearStress[NZ][NY][NX][3];
	 float vanMisesStress[NZ][NY][NX];
	 float Heatflux[NZ][NY][NX][3];
	 float Diffusion[NZ][NY][NX][3];
	 int vardimsAux[] = {1, 1, 3, 3, 1, 3, 3};
	 int centeringAux[] = {1, 1, 1, 1, 1, 1, 1};
	 const char *varnamesAux[] = {"density", "HydrodynamicStress", "NormalStress(xx,yy,zz)", "ShearStress(xy,xz,yz)", "vanMisesStress", "Heatflux", "Diffusion"};
	 float *varsAux[] = {(float *)density, (float *)HydrodynamicStress, (float *)NormalStress, (float *)ShearStress, (float *)vanMisesStress, (float *)Heatflux, (float *)Diffusion};
	 
	 int nvars = 1;
	 int nline = 1;
	 for(int property = 0; property < 6; property++)
		if (this->_stressOutputProperty[property] == true){
			nvars++;
			vardimsAux[nline] = vardimsAux[1+property];
			centeringAux[nline] = centeringAux[1+property];
			varnamesAux[nline] = varnamesAux[1+property];
			varsAux[nline] = varsAux[1+property];
			nline++;
		}
		
	int vardims[nvars];
	int centering[nvars];
	const char *varnames[nvars];
	float *vars[nvars];
	
	for (int auxCount = 0; auxCount < nvars; auxCount++){
		vardims[auxCount] = vardimsAux[auxCount];
		centering[auxCount] = centeringAux[auxCount];
		varnames[auxCount] = varnamesAux[auxCount];
		vars[auxCount] = varsAux[auxCount];
	}
     
      if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	// Eintragen des Flags '>' zwecks Kompatibilität
	stressProfxx << "> \n"; 
	stressProfyy << "> \n"; 
	stressProfzz << "> \n"; 
	stressProfxy << "> \n"; 
	stressProfxz << "> \n"; 
	stressProfyz << "> \n"; 
	stressProfhydr << "> \n";
	stressProfmises << "> \n";
	// Eintragen der x-Koordinaten x_i in Header
	stressProfxx << 0 <<"  \t"; 
	stressProfyy << 0 <<"  \t";
	stressProfzz << 0 <<"  \t";
	stressProfxy << 0 <<"  \t";
	stressProfxz << 0 <<"  \t";
	stressProfyz << 0 <<"  \t";
	stressProfhydr << 0 <<"  \t";
	stressProfmises << 0 <<"  \t";

	
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnits_Stress[0]; n_x++){
	  stressProfxx << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfyy << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfzz << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfxy << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfxz << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfyz << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfhydr << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	  stressProfmises << (n_x + 0.5) / this->_universalInvProfileUnit_Stress[0] <<"  \t"; 
	}
	stressProfxx << "\n";
	stressProfyy << "\n";
	stressProfzz << "\n";
	stressProfxy << "\n";
	stressProfxz << "\n";
	stressProfyz << "\n";
	stressProfhydr << "\n";
	stressProfmises << "\n";
      }
	
      //! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte rho(y_i,xi)
      for(unsigned n_z = 0; n_z < this->_universalNProfileUnits_Stress[2]; n_z++)
      {	  
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnits_Stress[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnit_Stress[1];
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	    stressProfxx << yval<< "  \t";
	    stressProfyy << yval<< "  \t";
	    stressProfzz << yval<< "  \t";
	    stressProfxy << yval<< "  \t";
	    stressProfxz << yval<< "  \t";
	    stressProfyz << yval<< "  \t";	
	    stressProfhydr << yval<< "  \t";
	    stressProfmises << yval<< "  \t";
	  }
		  
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnits_Stress[0]; n_x++)
	  {
	    unsigned long unID = n_x * this->_universalNProfileUnits_Stress[1] * this->_universalNProfileUnits_Stress[2] + n_y * this->_universalNProfileUnits_Stress[2] + n_z;
	    double stress_locxx = 0., stress_locyy = 0., stress_loczz = 0., stress_locxy = 0., stress_locxz = 0., stress_locyz = 0., stress_lochydr = 0., stress_locmises = 0., local_density = 0.;
	    double q_x_convective_kin = 0.0, q_y_convective_kin = 0.0, q_z_convective_kin = 0.0, q_x_convective_pot = 0.0, q_y_convective_pot = 0.0, q_z_convective_pot = 0.0, q_x_diffusive = 0.0, q_y_diffusive = 0.0, q_z_diffusive = 0.0, q_x_total = 0.0, q_y_total = 0.0, q_z_total = 0.0;
	    double aux_mis1 = 0., aux_mis2 = 0., aux_mis3, aux_mis4 = 0.;
	    if(this->_universalNProfile_Stress[unID] > 0){
		local_density = (double)this->_universalNProfile_Stress[unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_locxx = this->_universalStress[0][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_locyy = this->_universalStress[1][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_loczz = this->_universalStress[2][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_locxy = this->_universalStress[3][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_locxz = this->_universalStress[4][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_locyz = this->_universalStress[5][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		stress_lochydr = (this->_universalStress[0][unID]+this->_universalStress[1][unID]+this->_universalStress[2][unID])/(3 * (segmentVolume * this->_globalAccumulatedDatasets_Stress));
		aux_mis1 = (this->_universalStress[0][unID] - this->_universalStress[1][unID])*(this->_universalStress[0][unID] - this->_universalStress[1][unID]);
		aux_mis2 = (this->_universalStress[1][unID] - this->_universalStress[2][unID])*(this->_universalStress[1][unID] - this->_universalStress[2][unID]);
		aux_mis3 = (this->_universalStress[2][unID] - this->_universalStress[0][unID])*(this->_universalStress[2][unID] - this->_universalStress[0][unID]);
		aux_mis4 = 6 * (this->_universalStress[3][unID] * this->_universalStress[3][unID] + this->_universalStress[4][unID] * this->_universalStress[4][unID] + this->_universalStress[5][unID] * this->_universalStress[5][unID]);
		stress_locmises = sqrt(0.5 * (aux_mis1 + aux_mis2 + aux_mis3 + aux_mis4))/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
		if(this->_stressOutputProperty[5] == true){
			Diffusion[n_z][n_y][n_x][0] = this->_globalDiffusiveMovementStress[0][unID];
			Diffusion[n_z][n_y][n_x][1] = this->_globalDiffusiveMovementStress[1][unID];
			Diffusion[n_z][n_y][n_x][2] = this->_globalDiffusiveMovementStress[2][unID];
		}else{
			Diffusion[n_z][n_y][n_x][0] = 0.0;
			Diffusion[n_z][n_y][n_x][1] = 0.0;
			Diffusion[n_z][n_y][n_x][2] = 0.0;
		}
		// Heat Flux
		if(this->_stressOutputProperty[4] == true){
			q_x_convective_kin = (this->_globalConvectiveKinHeatfluxStress[0][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_y_convective_kin = (this->_globalConvectiveKinHeatfluxStress[1][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_z_convective_kin = (this->_globalConvectiveKinHeatfluxStress[2][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_x_convective_pot = (this->_globalConvectivePotHeatfluxStress[0][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_y_convective_pot = (this->_globalConvectivePotHeatfluxStress[1][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_z_convective_pot = (this->_globalConvectivePotHeatfluxStress[2][unID])/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_x_diffusive = this->_globalDiffusiveHeatfluxStress[0][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_y_diffusive = this->_globalDiffusiveHeatfluxStress[1][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_z_diffusive = this->_globalDiffusiveHeatfluxStress[2][unID]/(segmentVolume * this->_globalAccumulatedDatasets_Stress);
			q_x_total = q_x_diffusive + q_x_convective_kin + q_x_convective_pot;
			q_y_total = q_y_diffusive + q_y_convective_kin + q_y_convective_pot;
			q_z_total = q_z_diffusive + q_z_convective_kin + q_z_convective_pot;
		}else{
			q_x_convective_kin = 0.0;
			q_y_convective_kin = 0.0;
			q_z_convective_kin = 0.0;
			q_x_convective_pot = 0.0;
			q_y_convective_pot = 0.0;
			q_z_convective_pot = 0.0;
			q_x_diffusive = 0.0;
			q_y_diffusive = 0.0;
			q_z_diffusive = 0.0;
			q_x_total = 0.0;
			q_y_total = 0.0;
			q_z_total = 0.0;
		}
	    }
	    if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	      stressProfxx << stress_locxx << "\t";
	      stressProfyy << stress_locyy << "\t";
	      stressProfzz << stress_loczz << "\t";
	      stressProfxy << stress_locxy << "\t";
	      stressProfxz << stress_locxz << "\t";
	      stressProfyz << stress_locyz << "\t";
	      stressProfhydr << stress_lochydr << "\t";
	      stressProfmises << stress_locmises << "\t";
	    }
	    
	    
	    setStressXX(stress_locxx);
	    setStressYY(stress_locyy);
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){
	      density[n_z][n_y][n_x] = local_density;
	      HydrodynamicStress[n_z][n_y][n_x] = stress_lochydr;
	      vanMisesStress[n_z][n_y][n_x] = stress_locmises;
	      NormalStress[n_z][n_y][n_x][0] = stress_locxx;
	      NormalStress[n_z][n_y][n_x][1] = stress_locyy;
	      NormalStress[n_z][n_y][n_x][2] = stress_loczz;
	      ShearStress[n_z][n_y][n_x][0] = stress_locxy;
	      ShearStress[n_z][n_y][n_x][1] = stress_locxz;
	      ShearStress[n_z][n_y][n_x][2] = stress_locyz;
	      Heatflux[n_z][n_y][n_x][0] = q_x_total; 
	      Heatflux[n_z][n_y][n_x][1] = q_y_total;
	      Heatflux[n_z][n_y][n_x][2] = q_z_total;
	    }
	  }
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	    stressProfxx << "\n";
	    stressProfyy << "\n";
	    stressProfzz << "\n";
	    stressProfxy << "\n";
	    stressProfxz << "\n";
	    stressProfyz << "\n";
	    stressProfhydr << "\n";
	    stressProfmises << "\n";
	  }
	 }
      }
	 if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && this->_universalNProfileUnits_Stress[2] == 1){
	  stressProfxx.close();
	  stressProfyy.close();
	  stressProfzz.close();
	  stressProfxy.close();
	  stressProfxz.close();
	  stressProfyz.close();
	  stressProfhydr.close();
	  stressProfmises.close();
	 }
	 
	 /* Use VisitWriter.cpp to write a regular mesh with data. */
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
	 write_regular_mesh(vtkname.c_str(), 0, dims, nvars, vardims, centering, varnames, vars);

}

void Domain::resetStressProfile()
{
	unsigned long unIDs = this->_universalNProfileUnits_Stress[0] * this->_universalNProfileUnits_Stress[1]
		* this->_universalNProfileUnits_Stress[2];
	for(unsigned long unID = 0; unID < unIDs; unID++)
	{
		this->_localNProfile_Stress[unID] = 0;
		this->_universalNProfile_Stress[unID] = 0;	
		for(int d = 0; d < 3; d++){
		   if(this->_stressOutputProperty[4] == true){
		    this->_localConvectiveKinHeatfluxStress[d][unID] = 0.0;
		    this->_globalConvectiveKinHeatfluxStress[d][unID] = 0.0;
		    this->_localConvectivePotHeatfluxStress[d][unID] = 0.0;
		    this->_globalConvectivePotHeatfluxStress[d][unID] = 0.0;
		    this->_localDiffusiveHeatfluxStress[d][unID] = 0.0;
		    this->_globalDiffusiveHeatfluxStress[d][unID] = 0.0;
		   }
		   if(this->_stressOutputProperty[5] == true){
		    this->_localDiffusiveMovementStress[d][unID] = 0.0;
		    this->_globalDiffusiveMovementStress[d][unID] = 0.0;
		   }
		}
	}
	
	this->_globalAccumulatedDatasets_Stress = 0;
	
	size_t rows = 6, cols = this->_universalNProfileUnits_Stress[0]*this->_universalNProfileUnits_Stress[1]*this->_universalNProfileUnits_Stress[2];

	 for(unsigned i = 0; i < rows; i++){
	      for(unsigned long j = 0; j < cols; j++){
		  this->_localStress[i][j] = 0.0;
		  this->_universalStress[i][j] = 0.0;
	      }
	 }
}

// Calculates the bulk properties in a box "far away" from the confinement
void Domain::setupBulkPressure(double xmin, double xmax, double ymin, double ymax, unsigned cid)
{
	this->_bulkCorner[0] = xmin;
	this->_bulkCorner[1] = xmax;
	this->_bulkCorner[2] = ymin;
	this->_bulkCorner[3] = ymax;
	
	this->_bulkVolume = (xmax - xmin) * (ymax - ymin) * this->_globalLength[2];
	
	this->_bulkComponent[cid] = true;
	
	this->resetBulkPressure();
}

void Domain::recordBulkPressure(ParticleContainer* molCont)
{
	unsigned cid;
	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{
		cid = thismol->componentid();
		if(this->_bulkComponent[cid] && thismol->r(0) >= this->getBulkBoundary(0) && thismol->r(0) <= this->getBulkBoundary(1) && thismol->r(1) >= this->getBulkBoundary(2) && thismol->r(1) <= this->getBulkBoundary(3))
		{
			this->_localNBulk += 1.0;
			for(int d = 0; d < 3; d++){
			    this->_localPressureVirial += thismol->getPressureVirial(d);
			    this->_localPressureKin += thismol->getPressureKin(d);
			}
		}
		
				
	// reset collected virial values of each molecule
	    for(int d = 0; d < 3; d++){
		thismol->setPressureVirial(d, 0.0);
		thismol->setPressureKin(d, 0.0);
	    }
	}
	this->_globalAccumulatedDatasets_BulkPressure++;
}

void Domain::collectBulkPressure(DomainDecompBase* dode)
{
	dode->collCommInit(3);
	dode->collCommAppendLongDouble(this->_localNBulk);
	dode->collCommAppendLongDouble(this->_localPressureVirial);
	dode->collCommAppendLongDouble(this->_localPressureKin);
	dode->collCommAllreduceSum();
	this->_universalNBulk = (double)dode->collCommGetLongDouble();
	this->_universalPressureVirial = (double)dode->collCommGetLongDouble();
	this->_universalPressureKin = (double)dode->collCommGetLongDouble();
	dode->collCommFinalize();
}

void Domain::resetBulkPressure()
{
	
	this->_localNBulk = 0.0;
	this->_localPressureVirial = 0.0;
	this->_localPressureKin = 0.0;
	this->_globalAccumulatedDatasets_BulkPressure = 0;
	this->_universalNBulk = 0.0;
	this->_universalPressureVirial = 0.0;
	this->_universalPressureKin = 0.0;
}

void Domain::setupBarostat(double xmin, double xmax, double ymin, double ymax, double zmin, double zmax, unsigned cid)
{
	if(this->_isConfinement == true){
	  this->_control_top[0] = this->_confinementEdge[1];
	  this->_control_top[1] = this->_confinementMidPoint[1];
	  this->_control_top[2] = this->_confinementEdge[2];
	  this->_control_bottom[0] = this->_confinementEdge[0];
	  this->_control_bottom[1] = this->_confinementMidPoint[3];
	  this->_control_bottom[2] = 0.0;
	}else{
	  this->_control_top[0] = xmax;
	  this->_control_top[1] = ymax;
	  this->_control_top[2] = zmax;
	  this->_control_bottom[0] = xmin;
	  this->_control_bottom[1] = ymin;
	  this->_control_bottom[2] = zmin;
	}
	
	this->_barostatComponent[cid] = true;
	
	this->resetBarostat();
}

void Domain::recordBarostat(ParticleContainer* molCont)
{
	unsigned cid;
	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{
		cid = thismol->componentid();
		if(this->_barostatComponent[cid] && thismol->r(0) >= this->_control_bottom[0] && thismol->r(0) <= this->_control_top[0] && thismol->r(1) >= this->_control_bottom[1] && thismol->r(1) <= this->_control_top[1] 
		  && thismol->r(2) >= this->_control_bottom[2] && thismol->r(2) <= this->_control_top[2]){
			this->_localN_barostat += 1.0;
			for(int d = 0; d < 3; d++){
			    this->_localPressureVirial_barostat += thismol->getPressureVirial_barostat(d);
			    this->_localPressureKin_barostat += thismol->getPressureKin_barostat(d);
			}
		}
				
	  // reset collected virial values of each molecule
	    for(int d = 0; d < 3; d++){
		thismol->setPressureVirial_barostat(d, 0.0);
		thismol->setPressureKin_barostat(d, 0.0);
	    }
	}
	this->_globalAccumulatedDatasets_barostat++;
}

void Domain::collectBarostat(DomainDecompBase* dode)
{
	dode->collCommInit(3);
	dode->collCommAppendLongDouble(this->_localN_barostat);
	dode->collCommAppendLongDouble(this->_localPressureVirial_barostat);
	dode->collCommAppendLongDouble(this->_localPressureKin_barostat);
	dode->collCommAllreduceSum();
	this->_universalN_barostat = (double)dode->collCommGetLongDouble();
	this->_universalPressureVirial_barostat = (double)dode->collCommGetLongDouble();
	this->_universalPressureKin_barostat = (double)dode->collCommGetLongDouble();
	dode->collCommFinalize();
	
	this->_universalN_barostat = this->_universalN_barostat/this->_globalAccumulatedDatasets_barostat;
	this->_universalPressureVirial_barostat = this->_universalPressureVirial_barostat/this->_globalAccumulatedDatasets_barostat;
	this->_universalPressureKin_barostat = this->_universalPressureKin_barostat/this->_globalAccumulatedDatasets_barostat;
}

void Domain::resetBarostat()
{
	this->_localN_barostat = 0.0;
	this->_localPressureVirial_barostat = 0.0;
	this->_localPressureKin_barostat = 0.0;
	this->_globalAccumulatedDatasets_barostat = 0;
	this->_universalN_barostat = 0.0;
	this->_universalPressureVirial_barostat = 0.0;
	this->_universalPressureKin_barostat = 0.0;
}


// Calculates the fluid properties in the confinement
// if (radius 2 == 0) horDist = half length of the lower plate
void Domain::setupConfinementProperties(double wallThickness, double horDist, double vertDist, double radius2, int cid, double xmax, double ymax, double zmax, unsigned long upperID, unsigned long lowerID)
{
	double midPoint_x1, midPoint_x2, midPoint_y1, midPoint_y2;
		
	// calculate coordinates of the midpoints of the two asperities
	midPoint_x2 = xmax/2;
	midPoint_x1 = midPoint_x2 - horDist;
	midPoint_y2 = wallThickness;
	midPoint_y1 = wallThickness + vertDist;
	
	//this->_isConfinement = true;
	this->_confinementAreaFixed = false;
	
	this->_confinementMidPointID[0] = upperID;
	this->_confinementMidPointID[1] = lowerID;
	this->_confinementEdge[0] = xmax/2 - horDist - radius2;
	this->_confinementEdge[1] = xmax/2 + horDist + radius2;
	this->_confinementEdge[2] = zmax;
	this->_confinementEdge[3] = radius2;
	this->_confinementEdge[4] = vertDist;
	this->_confinementEdge[5] = 0.0;
	this->_confinementMidPoint[0] = midPoint_x1;
	this->_confinementMidPoint[1] = midPoint_y1;
	this->_confinementMidPoint[2] = midPoint_x2;
	this->_confinementMidPoint[3] = midPoint_y2;
	
	if (cid < 0){
	  // if (cid < 0) confinement properties of all components are recorded and the area of recording is enlarged including both walls
	  this->_confinementEdge[5] = wallThickness;
	  for (unsigned compID = 0; compID < getNumberOfComponents(); compID++)
	    this->_confinementComponent[compID] = true;
	}else{
	    this->_confinementComponent[cid] = true;
	}
	//FIXME Hardy is always just applied to component 1 (cid = 0)
	this->_confinementComponentHardy[0] = true;
}

void Domain::setupConfinementPropertiesFixed(double xmin, double xmax, double ymin, double ymax, double zmax, int cid)
{
	this->_confinementAreaFixed = true;
	this->_confinementMidPointID[0] = 0;
	this->_confinementMidPointID[1] = 0;
	this->_confinementEdge[0] = xmin;
	this->_confinementEdge[1] = xmax;
	this->_confinementEdge[2] = zmax;
	this->_confinementEdge[3] = 0.0;
	this->_confinementEdge[4] = ymax-ymin;
	this->_confinementEdge[5] = 0.0;
	this->_confinementMidPoint[0] = xmax;
	this->_confinementMidPoint[1] = ymax;
	this->_confinementMidPoint[2] = xmin;
	this->_confinementMidPoint[3] = ymin;
	
	if (cid < 0){
	  // if (cid < 0) confinement properties of all components are recorded and the area of recording is enlarged including both walls
	  this->_confinementEdge[5] = 0.0;
	  for (unsigned compID = 0; compID < getNumberOfComponents(); compID++)
	    this->_confinementComponent[compID] = true;
	}else{
	    this->_confinementComponent[cid] = true;
	}
	//FIXME Hardy is always just applied to component 1 (cid = 0)
	this->_confinementComponentHardy[0] = true;
}

void Domain::setupConfinementProfile(unsigned xun, unsigned yun, double correlationLength)
{
    // choice different stress calculation methods
    string hardy ("Hardy");
  
    this->_universalNProfileUnitsConfinement[0] = xun;
    this->_universalNProfileUnitsConfinement[1] = yun;
    this->_universalNProfileUnitsConfinement[2] = 1;
    
    correlationLength = this->_confinementEdge[4]/ceil(this->_confinementEdge[4]/correlationLength);
      
    // inverse step width of the increments (defining the elemental volumes)
    _universalInvProfileUnitConfinement[0] = _universalNProfileUnitsConfinement[0] / (this->_confinementEdge[1] - this->_confinementEdge[0]);
    _universalInvProfileUnitConfinement[1] = _universalNProfileUnitsConfinement[1] / (this->_confinementEdge[4] + this->_confinementEdge[3] + 2*this->_confinementEdge[5]);
    _universalInvProfileUnitConfinement[2] = _universalNProfileUnitsConfinement[2] / _globalLength[2];
    
    // Hardy stress calculation is just valid for control volumes with certain correlation length
    // Ulz, Manfred H., Kranthi K. Mandadapu, and Panayiotis Papadopoulos. "On the estimation of spatial averaging volume for determining stress using atomistic methods." 
    // Model. Simul. Mater. Sci. Eng 21 (2013): 15010-15015.
    // choice different stress calculation methods
    if(this->_stressCalcMethodConfinement == hardy){
      this->_universalNProfileUnitsStressConfinement[0] = ceil((this->_confinementEdge[1] - this->_confinementEdge[0])/correlationLength);
      this->_universalNProfileUnitsStressConfinement[1] = ceil((this->_confinementEdge[4])/correlationLength);
      this->_universalNProfileUnitsStressConfinement[2] = 1;
    
      _universalInvProfileUnitStressConfinement[0] = 1/correlationLength;
      _universalInvProfileUnitStressConfinement[1] = 1/correlationLength;
      _universalInvProfileUnitStressConfinement[2] = _universalNProfileUnitsStressConfinement[2] / _globalLength[2];
    }else{ // for Virial Stress
      for(unsigned cid = 0; cid < getNumberOfComponents(); cid++)
	this->_confinementComponentHardy[cid] = false;
      
      this->_universalNProfileUnitsStressConfinement[0] = this->_universalNProfileUnitsConfinement[0];
      this->_universalNProfileUnitsStressConfinement[1] = this->_universalNProfileUnitsConfinement[1];
      this->_universalNProfileUnitsStressConfinement[2] = this->_universalNProfileUnitsConfinement[2];
      
      _universalInvProfileUnitStressConfinement[0] = _universalInvProfileUnitConfinement[0];
      _universalInvProfileUnitStressConfinement[1] = _universalInvProfileUnitConfinement[1];
      _universalInvProfileUnitStressConfinement[2] = _universalInvProfileUnitConfinement[2];
    }
    
     size_t rows = 6, cols = this->_universalNProfileUnitsStressConfinement[0]*this->_universalNProfileUnitsStressConfinement[1];
     this->_localStressConfinement = this->allocStressMatrix(rows, cols);
     this->_globalStressConfinement = this->allocStressMatrix(rows, cols);
     
    
    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
      mkdir("./Results/Confinement", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
      mkdir("./Results/ConfinementVTK", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    this->resetConfinementProperties();
}

void Domain::recordConfinementProperties(DomainDecompBase* dode, ParticleContainer* molCont, unsigned long simstep, unsigned long initStatistics)
{
	unsigned cid;
	unsigned xun, yun, unID, binID, stressCalc_xun, stressCalc_yun, stressCalc_unID;
	double origLowUp, origHiLow;
	string hardy ("Hardy");
	string Confinement ("Confinement");
		
	// needed for the communication of the current coordinates of the midpoints
	double confinementMidPointAux[4];
	confinementMidPointAux[0] = 0.0;
	confinementMidPointAux[1] = 0.0;
	confinementMidPointAux[2] = 0.0;
	confinementMidPointAux[3] = 0.0;
	
	unsigned unIDs_dist = this->_universalNProfileUnitsConfinement[0];
	// initialization for distance measurement between upper and lower plate in each bin
	for(binID = 0; binID < unIDs_dist; binID++)
	{
	  _lowUpper[binID] = _globalLength[1];
	  _highLower[binID] = 0.0;
	}
	 origLowUp = _lowUpper[0];
	 origHiLow = _highLower[0];
	
	string moved ("moved");
	string fixed ("fixed");
	string free ("free");
	unsigned cid_moved =  getCidMovement(moved, getNumberOfComponents()) - 1;
	unsigned cid_fixed =  getCidMovement(fixed, getNumberOfComponents()) - 1;
	unsigned cid_free =  getCidMovement(free, getNumberOfComponents()) - 1;
	
	// finds once the IDs of the midpoint molecules of the asperities
	if(simstep == initStatistics && (this->_confinementMidPointID[0] == 0 || this->_confinementMidPointID[1] == 0) && this->_confinementAreaFixed == false){
	    int count1 = 0;
	    int count2 = 0;
	    double auxFactor = 1.0;
	    this->_confinementMidPointID[0] = 0;
	    this->_confinementMidPointID[1] = 0;
	    LOOP:for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	    { 
	      // find molecule-id of the midpoint of the upper asperity; 0.775 is half the lattice constant
	      if(thismol->componentid() == cid_moved && count1 == 0 && thismol->r(0) >= this->_confinementMidPoint[0]-auxFactor*0.775 && thismol->r(0) <= this->_confinementMidPoint[0]+auxFactor*0.775 && thismol->r(1) >= this->_confinementMidPoint[1]-auxFactor*0.775/2 && thismol->r(1) <= this->_confinementMidPoint[1]+auxFactor*0.775/2 && thismol->r(2) <= auxFactor*1.55){
		this->_confinementMidPointID[0] = thismol->id();
	      }
	      // find molecule-id of the midpoint of the lower asperity; 0.775 is half the lattice constant
	      if(thismol->componentid() == cid_fixed && count2 == 0 && thismol->r(0) >= this->_confinementMidPoint[2]-auxFactor*0.775 && thismol->r(0) <= this->_confinementMidPoint[2]+auxFactor*0.775 && thismol->r(1) >= this->_confinementMidPoint[3]-auxFactor*0.775/2 && thismol->r(1) <= this->_confinementMidPoint[3]+auxFactor*0.775/2 && thismol->r(2) <= auxFactor*1.55){
		this->_confinementMidPointID[1] = thismol->id();
	      }
	      // reset collected virial values of each molecule
	      for(int d = 0; d < 3; d++){
		thismol->setPressureVirialConfinement(d, 0.0);
		thismol->setPressureKinConfinement(d, 0.0);
		thismol->setConvectivePotHeatflux(d, 0.0);
		for(int e = 0; e < 3; e++){
		  thismol->setVirialForceConfinement(d, e, 0.0);
		  thismol->setVirialKinConfinement(d, e, 0.0);
		  thismol->setDiffusiveHeatflux(d, e, 0.0);
		}
	      }
	      thismol->clearVirialForceHardyConfinement();
	      thismol->clearDiffusiveHeatfluxHardyConfinement();
	    }
	    // this molecule-ID will not be taken into account for the search of midpoints; helps to find the midpoints by function collCommAllreduceMin()
	    if (this->_confinementMidPointID[0] == 0)
		this->_confinementMidPointID[0] = _globalNumMolecules + 1;
	    if (this->_confinementMidPointID[1] == 0)
		this->_confinementMidPointID[1] = _globalNumMolecules + 1;
	    dode->collCommInit(2);
	    dode->collCommAppendDouble(this->_confinementMidPointID[0]);
	    dode->collCommAppendDouble(this->_confinementMidPointID[1]);
	    dode->collCommAllreduceMin();
	    this->_confinementMidPointID[0] = (double)dode->collCommGetDouble();
	    this->_confinementMidPointID[1] = (double)dode->collCommGetDouble();
	    if((this->_confinementMidPointID[0] == _globalNumMolecules + 1 || this->_confinementMidPointID[0] == _globalNumMolecules + 1) && (this->isShearRate() == false && this->isShearForce() == false)){
		auxFactor += 0.1;
		goto LOOP;
	    }

	}else{

	  for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	  {
		cid = thismol->componentid();
		if(this->_confinementComponent[cid] && thismol->r(0) >= this->_confinementEdge[0] && thismol->r(0) <= this->_confinementEdge[1] && thismol->r(1) >= this->_confinementMidPoint[3] - this->_confinementEdge[5] && thismol->r(1) <= this->_confinementMidPoint[1] + this->_confinementEdge[5])
		{
			std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > > virialHardy = thismol->getVirialForceHardyConfinement();
			std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > > heatDiffHardy = thismol->getDiffusiveHeatfluxHardyConfinement();
			xun = (unsigned)floor((thismol->r(0)-this->_confinementEdge[0]) * this->_universalInvProfileUnitConfinement[0]);
			yun = (unsigned)floor((thismol->r(1)-(this->_confinementMidPoint[3]-this->_confinementEdge[5])) * this->_universalInvProfileUnitConfinement[1]); 
			unID = xun * this->_universalNProfileUnitsConfinement[1] + yun;
			stressCalc_xun = (unsigned)floor((thismol->r(0)-this->_confinementEdge[0]) * this->_universalInvProfileUnitStressConfinement[0]);
			if(this->_stressCalcMethodConfinement == hardy)
			  stressCalc_yun = (unsigned)floor((thismol->r(1)-this->_confinementMidPoint[3]) * this->_universalInvProfileUnitStressConfinement[1]); 
			else
			  stressCalc_yun = (unsigned)floor((thismol->r(1)-(this->_confinementMidPoint[3]-this->_confinementEdge[5])) * this->_universalInvProfileUnitStressConfinement[1]);
			stressCalc_unID = stressCalc_xun * this->_universalNProfileUnitsStressConfinement[1] + stressCalc_yun;

			this->_localNConfinement[unID] += 1.0;

			for(int d = 0; d < 3; d++){
			    this->_localPressureVirial_Confinement[unID] += thismol->getPressureVirialConfinement(d);
			    this->_localPressureKin_Confinement[unID] += thismol->getPressureKinConfinement(d);
			    this->_localvProfile_Confinement[d][unID] += thismol->v(d);
			    this->_localFluidForce_Confinement[d][unID] += thismol->F(d);
			    this->_localDiffusiveMovement[d][unID] += thismol->rOld(d) - thismol->r(d) - thismol->getDirectedVelocityConfinement(d)*getTimestepLength();
			    this->_localDiffusiveMovement[d][unID] *= this->_localDiffusiveMovement[d][unID];
			}
			
			// barostat current temperature and pressure calculation
			if(this->_barostatComponent[cid] && getSimstep() >= getBarostatTimeInit() && getSimstep() <= getBarostatTimeEnd()){
			  this->_localN_barostat += 1.0;
			  for(int d = 0; d < 3; d++){
			    this->_localPressureVirial_barostat += thismol->getPressureVirialConfinement(d);
			    this->_localPressureKin_barostat += thismol->getPressureKinConfinement(d);
			  }
			}
			
			// kinetic energy
			this->_localDOFProfile_Confinement[unID] += 3.0 + (long double)(thismol->component()->getRotationalDegreesOfFreedom());
			// record _twice_ the total (ordered + unordered) kinetic energy
			double mv2 = 0.0;
			double Iw2 = 0.0;
			
			thismol->calculate_mv2_Iw2(mv2, Iw2, Confinement);
			this->_localKineticProfile_Confinement[unID] += mv2+Iw2;
			
			// stress calculation just in fluid
			if(cid == cid_free){
			  if(thismol->isHardyConfinement() && thismol->r(1) >= this->_confinementMidPoint[3] && thismol->r(1) <= this->_confinementMidPoint[1]){
			    // for Linear the weightingFrac = 1 as long as a particle is in the control volume
			    double weightingFrac = 1.0;
			    string weightingFunc = thismol->getWeightingFuncConfinement();
			    string Linear ("Linear");
			    string Pyramide ("Pyramide");
			    if(weightingFunc == Pyramide)
			      weightingFrac = thismol->weightingFunctionPyramide(stressCalc_xun, stressCalc_yun, 1/this->_universalInvProfileUnitStressConfinement[0], 1/this->_universalInvProfileUnitStressConfinement[1], this->_confinementEdge[0], this->_confinementMidPoint[3]);
			    for(std::map<unsigned long, std::map<unsigned, std::map<unsigned, double> > >::iterator it=virialHardy.begin(); it!=virialHardy.end(); ++it){
			      for(int d = 0; d < 3; d++)
				for(int e = d; e < 3; e++){
				  if(d == e){
				    this->_localStressConfinement[d][it->first] += virialHardy[it->first][d][e];
                                    for(int f = 0; f < 3; f++)
                                      this->_localDiffusiveHeatflux[d][it->first] += heatDiffHardy[it->first][d][f]*(thismol->v(f) - thismol->getDirectedVelocityConfinement(f)); 
				    // update just once per molecule
				    if(it == virialHardy.begin()){
				      this->_localStressConfinement[d][stressCalc_unID] += weightingFrac*(double)thismol->getVirialKinConfinement(d, e);
				      this->_localConvectiveKinHeatflux[d][stressCalc_unID] += 0.5 * mv2 * (thismol->v(d) - thismol->getDirectedVelocityConfinement(d)) * weightingFrac;
				      this->_localConvectivePotHeatflux[d][stressCalc_unID] += thismol->getConvectivePotHeatflux(d) * (thismol->v(d) - thismol->getDirectedVelocityConfinement(d)) * weightingFrac;
				    }
				  }else{
				    this->_localStressConfinement[2+d+e][it->first] += virialHardy[it->first][d][e];
				    // update just once per molecule
				    if(it == virialHardy.begin()){
				      this->_localStressConfinement[2+d+e][stressCalc_unID] += weightingFrac*(double)thismol->getVirialKinConfinement(d, e);
				    }
				  }
				}
			    }
			  }else{
			    for(int d = 0; d < 3; d++)
			      for(int e = d; e < 3; e++){
				if(d == e){
				   this->_localStressConfinement[d][stressCalc_unID] += (double)thismol->getVirialForceConfinement(d, e);
				   this->_localStressConfinement[d][stressCalc_unID] += (double)thismol->getVirialKinConfinement(d, e);
				   this->_localConvectivePotHeatflux[d][stressCalc_unID] += thismol->getConvectivePotHeatflux(d) * (thismol->v(d) - thismol->getDirectedVelocityConfinement(d));
				   this->_localConvectiveKinHeatflux[d][stressCalc_unID] += 0.5 * mv2 * (thismol->v(d) - thismol->getDirectedVelocityConfinement(d));
                                   for(int f = 0; f < 3; f++)
                                       this->_localDiffusiveHeatflux[d][stressCalc_unID] += thismol->getDiffusiveHeatflux(d, f)*(thismol->v(f) - thismol->getDirectedVelocityConfinement(f));
				}else{
				   this->_localStressConfinement[2+d+e][stressCalc_unID] += (double)thismol->getVirialForceConfinement(d, e);
				   this->_localStressConfinement[2+d+e][stressCalc_unID] += (double)thismol->getVirialKinConfinement(d, e);
				}
			      }
			  }
			}
		}
		// positions of the asperity-midpoints are located to calculate the distance between the two plates and the confined volume
		if(cid == cid_moved || cid == cid_fixed || this->_confinementAreaFixed == true)
		{
			xun = (unsigned)floor((thismol->r(0)-this->_confinementEdge[0]) * this->_universalInvProfileUnitConfinement[0]);
			yun = (unsigned)floor((thismol->r(1)-(this->_confinementMidPoint[3]-this->_confinementEdge[5])) * this->_universalInvProfileUnitConfinement[1]); 
			unID = xun * this->_universalNProfileUnitsConfinement[1] + yun; 
			this->_localWallNConfinement[unID] += 1.0;
			
			if(thismol->id() == this->_confinementMidPointID[0]){
			    confinementMidPointAux[0] = thismol->r(0);
			    confinementMidPointAux[1] = thismol->r(1);
			}
			if(thismol->id() == this->_confinementMidPointID[1]){
			    confinementMidPointAux[2] = thismol->r(0);
			    confinementMidPointAux[3] = thismol->r(1);
			}
			
			if(thismol->r(0) >= this->_confinementEdge[0] && thismol->r(0) <= this->_confinementEdge[1]){
			  // calculation of distance between upper and lower plate in each incremental bin
			  xun = (unsigned)floor((thismol->r(0)-this->_confinementEdge[0]) * this->_universalInvProfileUnitConfinement[0]);
			  binID = xun;

			  // lowest molecule in upper plate bin
			  if(cid == cid_moved && _lowUpper[binID] > thismol->r(1) && thismol->r(1) >= this->_confinementMidPoint[3])
			      _lowUpper[binID] = thismol->r(1);
			
			  // highest molecule in lower plate bin
			  if(cid == cid_fixed && _highLower[binID] < thismol->r(1) && thismol->r(1) <= this->_confinementMidPoint[1])
			      _highLower[binID] = thismol->r(1);	
			  
			  if(this->_confinementAreaFixed == true){
			    _lowUpper[binID] = this->_confinementMidPoint[1];
			    _lowUpper[binID] = this->_confinementMidPoint[3];
			  }
			}
		}
				
	  // reset collected virial values of each molecule
	    for(int d = 0; d < 3; d++){
		thismol->setPressureVirialConfinement(d, 0.0);
		thismol->setPressureKinConfinement(d, 0.0);
		thismol->setConvectivePotHeatflux(d, 0.0);
		for(int e = 0; e < 3; e++){
		  thismol->setVirialForceConfinement(d, e, 0.0);
		  thismol->setVirialKinConfinement(d, e, 0.0);
		  thismol->setDiffusiveHeatflux(d, e, 0.0);
		}
	    }
	    thismol->clearVirialForceHardyConfinement();
	    thismol->clearDiffusiveHeatfluxHardyConfinement();
	  }
	 
	  // in the upper loop, just one to two CPUs are responsible to the finding of the midpoint coordinates; here this current value is communicated to all CPUs
	  if(this->isShearRate() == false  && this->isShearForce() == false && this->_confinementAreaFixed == false){
	    dode->collCommInit(4);
	    for(int i = 0; i < 4; i++)
	      dode->collCommAppendDouble(confinementMidPointAux[i]);
	    dode->collCommAllreduceSum();
	    for(int i = 0; i < 4; i++){
	      this->_confinementMidPoint[i] = (double)dode->collCommGetDouble();
	    }
	    dode->collCommFinalize();
	  }
	  this->_globalAccumulatedDatasets_ConfinementProperties++;
	  
	  // barostat current temperature and pressure calculation
	  if(getSimstep() >= getBarostatTimeInit() && getSimstep() <= getBarostatTimeEnd())
		this->_globalAccumulatedDatasets_barostat++;
	  
	  
	    // calculate the distance between upper and lower plate in each bin	
	    dode->collCommInit(1*unIDs_dist);
	    for(binID = 0; binID < unIDs_dist; binID++)
	    {
		dode->collCommAppendDouble(this->_lowUpper[binID]);
	    }
	    dode->collCommAllreduceMin();
	    for(binID = 0; binID < unIDs_dist; binID++)
	    {
		this->_lowUpper[binID] = (double)dode->collCommGetDouble();
		// prevents outliers for averaging
		if(this->_lowUpper[binID] == origLowUp && this->_confinementAreaFixed == false){
		  if(binID == 0)
		    this->_lowUpper[binID] = 0.0;
		  else
		    this->_lowUpper[binID] = this->_lowUpper[binID-1];
		}
	    }
	    dode->collCommFinalize();
	    dode->collCommInit(1*unIDs_dist);
	    for(binID = 0; binID < unIDs_dist; binID++)
	    {
		dode->collCommAppendDouble(this->_highLower[binID]);
	    }
	    dode->collCommAllreduceMax();
	    for(binID = 0; binID < unIDs_dist; binID++)
	    {
		this->_highLower[binID] = (double)dode->collCommGetDouble();
		// prevents outliers for averaging
		if(this->_highLower[binID] == origHiLow && this->_confinementAreaFixed == false){
		  if(binID == 0)
		    this->_highLower[binID] = 0.0;
		  else
		    this->_highLower[binID] = this->_highLower[binID-1];
		}
	    }
	    dode->collCommFinalize();
	    for(binID = 0; binID < unIDs_dist; binID++){
		if(this->_lowUpper[binID] == 0.0 || this->_highLower[binID] == 0.0){
		  this->_lowUpper[binID] = 0.0;
		  this->_highLower[binID] = 0.0;
		  this->_dBinFailCount[binID]++;
		}
		this->_dBin[binID] += (long double)(this->_lowUpper[binID] - this->_highLower[binID]);
		
		if(this->_lowUpper[binID] != 0.0 || this->_highLower[binID] != 0.0){
		  // Calculation of a local matrix which (like a boolean) defines areas in between the confining walls
		  // with 0.5*sigma-correction for the volume that is accessible for the fluid molecules
		  unsigned xun, yun_lower, yun_upper, unID_lower, unID_upper;
		  xun = binID;
		  yun_lower = (unsigned)floor(((this->_highLower[binID] + 0.5*getSigma(cid_free,0))-(this->_confinementMidPoint[3]-this->_confinementEdge[5])) * this->_universalInvProfileUnitConfinement[1]);
		  yun_upper = (unsigned)floor(((this->_lowUpper[binID] - 0.5*getSigma(cid_free,0))-(this->_confinementMidPoint[3]-this->_confinementEdge[5])) * this->_universalInvProfileUnitConfinement[1]); 
		  unID_lower = xun * this->_universalNProfileUnitsConfinement[1] + yun_lower; 
		  unID_upper = xun * this->_universalNProfileUnitsConfinement[1] + yun_upper;
		
		  for(unsigned boolID = unID_lower; boolID <= unID_upper; boolID++)
		    this->_localFluidicArea[boolID] += 1;
		}		  
	    }
	  
	    this->_dMax += this->_confinementMidPoint[1]-this->_confinementMidPoint[3];
	}
}

void Domain::collectForcesOnComponentConfinement(ParticleContainer* molCont)
{
	unsigned xun, yun, unID;
	string moved ("moved");
	unsigned cid_moved =  getCidMovement(moved, getNumberOfComponents()) - 1;
	  
	for(Molecule* thismol = molCont->begin(); thismol != molCont->end(); thismol = molCont->next())
	{	  
	  if(thismol->componentid() == cid_moved && thismol->r(0) >= this->_confinementEdge[0] && thismol->r(0) <= this->_confinementEdge[1]){
		
	      xun = (unsigned)floor((thismol->r(0)-this->_confinementEdge[0]) * this->_universalInvProfileUnitConfinement[0]);
	      for (yun = 0; yun < this->_universalNProfileUnitsConfinement[1]; yun++){
		unID = xun * this->_universalNProfileUnitsConfinement[1] + yun;
		for(unsigned short int d = 0; d < 3; d++)
			this->_localForceConfinement[d][unID] += thismol->F(d);
	      }
	  }
	}
}

void Domain::collectConfinementProperties(DomainDecompBase* dode)
{  
	unsigned unIDs = this->_universalNProfileUnitsConfinement[0] * this->_universalNProfileUnitsConfinement[1];
	unsigned stressCalc_unIDs = this->_universalNProfileUnitsStressConfinement[0] * this->_universalNProfileUnitsStressConfinement[1];
	unsigned unIDs_dist = this->_universalNProfileUnitsConfinement[0]; 
	dode->collCommInit(16*unIDs+15*stressCalc_unIDs+3*unIDs);
	for(unsigned unID = 0; unID < unIDs; unID++)
	{	  
		dode->collCommAppendLongDouble(this->_localNConfinement[unID]);
		dode->collCommAppendLongDouble(this->_localWallNConfinement[unID]);
		dode->collCommAppendLongDouble(this->_localPressureVirial_Confinement[unID]);
		dode->collCommAppendLongDouble(this->_localPressureKin_Confinement[unID]);
		dode->collCommAppendLongDouble(this->_localDOFProfile_Confinement[unID]);
		dode->collCommAppendLongDouble(this->_localKineticProfile_Confinement[unID]);
		dode->collCommAppendUnsLong(this->_localFluidicArea[unID]);
		for(int d = 0; d < 3; d++){
		    dode->collCommAppendLongDouble(this->_localForceConfinement[d][unID]);
		    dode->collCommAppendLongDouble(this->_localvProfile_Confinement[d][unID]);
		    dode->collCommAppendLongDouble(this->_localFluidForce_Confinement[d][unID]);
		    dode->collCommAppendLongDouble(this->_localDiffusiveMovement[d][unID]);
		}
	}
	for(unsigned unID = 0; unID < stressCalc_unIDs; unID++)
		for(int d = 0; d < 6; d++){
		    dode->collCommAppendDouble(this->_localStressConfinement[d][unID]);
		    if(d < 3){
		      dode->collCommAppendDouble(this->_localConvectiveKinHeatflux[d][unID]);
		      dode->collCommAppendDouble(this->_localConvectivePotHeatflux[d][unID]);
		      dode->collCommAppendDouble(this->_localDiffusiveHeatflux[d][unID]);
		    }
		}
	dode->collCommAllreduceSum();
	for(unsigned unID = 0; unID < unIDs; unID++)
	{
		this->_globalNConfinement[unID] = dode->collCommGetLongDouble();
		this->_globalWallNConfinement[unID] = dode->collCommGetLongDouble();
		this->_globalPressureVirial_Confinement[unID] = dode->collCommGetLongDouble();
		this->_globalPressureKin_Confinement[unID] = dode->collCommGetLongDouble();
		this->_globalDOFProfile_Confinement[unID] = dode->collCommGetLongDouble();
		this->_globalKineticProfile_Confinement[unID] = dode->collCommGetLongDouble();
		this->_globalFluidicArea[unID] = dode->collCommGetUnsLong();
		for(int d = 0; d < 3; d++){
		    this->_globalForceConfinement[d][unID] = dode->collCommGetLongDouble();
		    this->_globalvProfile_Confinement[d][unID] = dode->collCommGetLongDouble();
		    this->_globalFluidForce_Confinement[d][unID] = dode->collCommGetLongDouble();
		    this->_globalDiffusiveMovement[d][unID] = dode->collCommGetLongDouble();
		}
	}
	for(unsigned unID = 0; unID < unIDs; unID++)
	{	
		// if there is a fluidic particle in more than 90% of the observed cases, the bin is part of the fluidic area
		if(this->_globalFluidicArea[unID]/this->_globalAccumulatedDatasets_ConfinementProperties >= 0.9)
		  this->_globalFluidicArea[unID] = 1;
		else
		  this->_globalFluidicArea[unID] = 0;
		
		// averaging results; just of the fluidic part
		if((this->_globalFluidicArea[unID] == 1 && this->_confinementAreaFixed == false) || this->_confinementAreaFixed == true)
		{
		    this->_universalNConfinement += this->_globalNConfinement[unID];
		    this->_universalPressureVirial_Confinement += this->_globalPressureVirial_Confinement[unID];
		    this->_universalPressureKin_Confinement += this->_globalPressureKin_Confinement[unID];
		    this->_universalDOFProfile_Confinement += this->_globalDOFProfile_Confinement[unID];
		    this->_universalKineticProfile_Confinement += this->_globalKineticProfile_Confinement[unID];
		    for(int d = 0; d < 3; d++)
			this->_globalForce_Confinement[d] += this->_globalForceConfinement[d][unID];
		}
	}
	for(unsigned unID = 0; unID < stressCalc_unIDs; unID++)
		for(int d = 0; d < 6; d++){
		    this->_globalStressConfinement[d][unID] = (double)dode->collCommGetDouble();
		    if(d < 3){
		      this->_globalConvectiveKinHeatflux[d][unID] = dode->collCommGetDouble();
		      this->_globalConvectivePotHeatflux[d][unID] = dode->collCommGetDouble();
		      this->_globalDiffusiveHeatflux[d][unID] = dode->collCommGetDouble();
		    }
		}
	dode->collCommFinalize();
	
	// collect shear energy for Heatflux.data
	dode->collCommInit(5);
	dode->collCommAppendLongDouble(this->_preShearRateConf);
	dode->collCommAppendLongDouble(this->_postShearRateConf);
	dode->collCommAppendLongDouble(this->_preShearRateDefault);
	dode->collCommAppendLongDouble(this->_postShearRateDefault);
	dode->collCommAppendLongDouble(this->_shearVelDelta);
	dode->collCommAllreduceSum();
	this->_preShearRateConf = dode->collCommGetLongDouble();
	this->_postShearRateConf = dode->collCommGetLongDouble();
	this->_preShearRateDefault = dode->collCommGetLongDouble();
	this->_postShearRateDefault = dode->collCommGetLongDouble();
	this->_shearVelDelta = dode->collCommGetLongDouble();
	dode->collCommFinalize();
	
	string free ("free");
	unsigned cid_free =  getCidMovement(free, getNumberOfComponents()) - 1;
	
	// averaging results
	_dBinFailureCount = 0;
	for(unsigned binID = 0; binID < unIDs_dist; binID++){
	  if((this->_globalAccumulatedDatasets_ConfinementProperties - _dBinFailCount[binID]) != 0)
	      this->_dBin[binID] = this->_dBin[binID]/(this->_globalAccumulatedDatasets_ConfinementProperties - _dBinFailCount[binID]);
	  else
	      this->_dBin[binID] = 0.0;
	  
	  // mean distance between upper and lower plate; hemispheres are averaged out
	  if(this->_dBin[binID] == 0.0)
	    _dBinFailureCount++;
	  
	  //
	  if(_dBin[binID] <= getSigma(cid_free,0))
	    _dBin[binID] = 0.0;
	  else
	    _dBin[binID] -=  getSigma(cid_free,0); 
	  
	  this->_dAveraged += _dBin[binID];
	}
	this->_dAveraged = this->_dAveraged/(unIDs_dist - _dBinFailureCount);
	if ((this->isShearRate() == true || this->isShearForce() == true) || this->_confinementAreaFixed == true)
	  this->_dAveraged = this->_confinementMidPoint[1] - this->_confinementMidPoint[3];
	
	for(unsigned unID = 0; unID < unIDs; unID++){
	      for(int d = 0; d < 3; d++){
		    this->_globalForceConfinement[d][unID] = this->_globalForceConfinement[d][unID]/this->_globalAccumulatedDatasets_ConfinementProperties;
		    this->_globalDiffusiveMovement[d][unID] = this->_globalDiffusiveMovement[d][unID]/(2*this->_globalNConfinement[unID]*this->_globalAccumulatedDatasets_ConfinementProperties*this->_confinementRecordingTimesteps*getTimestepLength());
		    if (this->_globalDiffusiveMovement[d][unID] != this->_globalDiffusiveMovement[d][unID])
		      this->_globalDiffusiveMovement[d][unID] = 0.0;
	      }
	}
	
	this->_universalNConfinement = round(this->_universalNConfinement/this->_globalAccumulatedDatasets_ConfinementProperties);
	this->_universalPressureVirial_Confinement = this->_universalPressureVirial_Confinement/this->_globalAccumulatedDatasets_ConfinementProperties;
	this->_universalPressureKin_Confinement = this->_universalPressureKin_Confinement/this->_globalAccumulatedDatasets_ConfinementProperties;
	this->_universalDOFProfile_Confinement = this->_universalDOFProfile_Confinement/this->_globalAccumulatedDatasets_ConfinementProperties;
	this->_universalKineticProfile_Confinement = this->_universalKineticProfile_Confinement/this->_globalAccumulatedDatasets_ConfinementProperties;
	for(int d = 0; d < 3; d++)
	    this->_globalForce_Confinement[d] = this->_globalForce_Confinement[d]/this->_globalAccumulatedDatasets_ConfinementProperties;
	// distance between upper and lower plane
	this->_dMax = this->_dMax/this->_globalAccumulatedDatasets_ConfinementProperties;
	this->_dMax -= getSigma(cid_free,0);
	// volume of the confinement
	//this->_confinedVolume = (this->_confinementEdge[1]-this->_confinementEdge[0]) * this->_dMax * this->_confinementEdge[2] - M_PI*this->_confinementEdge[3]*this->_confinementEdge[3]*this->_confinementEdge[2];
	//if(this->_confinementEdge[3] == 0.0)
	this->_confinedVolume = (this->_confinementEdge[1]-this->_confinementEdge[0]) * this->_dAveraged * this->_confinementEdge[2];
	// area of the upper plate's surface as a projection of the confinement
	this->_confinedSolidSurface = this->_confinementEdge[2] * (this->_confinementEdge[1]-this->_confinementEdge[0]);
	
}

void Domain::outputConfinementProperties(const char* prefix, PressureGradient* pg){
	if(this->_localRank) return;
	
	  // viscosity profile
	  string viscname("./Results/Confinement/");
	  viscname += prefix;
	  viscname += ".viscxy";
	  // temperature profile
	  string tempname("./Results/Confinement/");
	  tempname += prefix;
	  tempname += ".Tempxy";
	  // density profile
	  string rhoname("./Results/Confinement/");
	  rhoname += prefix;
	  rhoname += ".rhoxy";
	  // velocity profile
	  string velname("./Results/Confinement/");
	  velname += prefix;
	  velname += ".velx_xy";
	  // force profiles
	  string forcexname("./Results/Confinement/");
	  string forceyname("./Results/Confinement/");
	  forcexname += prefix;
	  forceyname += prefix;
	  forcexname += ".f_x";
	  forceyname += ".f_y";
			
	  ofstream viscProf(viscname.c_str());
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(viscProf)) return;
	  viscProf.precision(6);
	
	  ofstream TProf(tempname.c_str());
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(TProf)) return;
	  TProf.precision(6);
	
	  ofstream rhoProf(rhoname.c_str());
	  if ( (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(rhoProf)) return;
	  rhoProf.precision(6);
	
	  ofstream vxProf(velname.c_str());
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(vxProf)) return;
	  vxProf.precision(6);
	
	  ofstream fxProf(forcexname.c_str());
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(fxProf)) return;
	  fxProf.precision(6);
	
	  ofstream fyProf(forceyname.c_str());
	  if ((this->_outputFormat == this->_all || this->_outputFormat == this->_matlab) && !(fyProf)) return;
	  fyProf.precision(6);
	
	// VTK data format
	  string vtkname("./Results/ConfinementVTK/");
	  string vtknameStress("./Results/ConfinementVTK/");
	  string hardy ("Hardy");
	  vtkname += prefix;
	  vtkname += ".vtk";
	  if (this->_stressCalcMethodConfinement == hardy){
	    vtknameStress += prefix;
	    vtknameStress += "_stress.vtk";
	  }
	
	 int NX = (int)this->_universalNProfileUnitsConfinement[0];
	 int NY = (int)this->_universalNProfileUnitsConfinement[1];
	 int NZ = (int)this->_universalNProfileUnitsConfinement[2];
	 int NX_Stress = (int)this->_universalNProfileUnitsStressConfinement[0];
	 int NY_Stress = (int)this->_universalNProfileUnitsStressConfinement[1];
	 int NZ_Stress = (int)this->_universalNProfileUnitsStressConfinement[2];
	 
	 int nvars, nvars_Stress;
	 
	 if (this->_stressCalcMethodConfinement == hardy){
	    nvars = 6;
	    nvars_Stress = 8;
	 }else{
	    nvars = 14;
	    nvars_Stress = 1;
	 }
	 
	 int dims[] = {NX, NY, NZ};
	 int dims_Stress[] = {NX_Stress, NY_Stress, NZ_Stress};
	 
	 float temperature[NZ][NY][NX];
	 float density[NZ][NY][NX];
	 float velocity[NZ][NY][NX][3];
	 float fluidForce[NZ][NY][NX][3];
	 float fluidicComponent[NZ][NY][NX];
	 float diffusion[NZ][NY][NX][3];
	 float HydrodynamicStress[NZ_Stress][NY_Stress][NX_Stress];
	 float vanMisesStress[NZ_Stress][NY_Stress][NX_Stress];
	 float NormalStress[NZ_Stress][NY_Stress][NX_Stress][3];
	 float ShearStress[NZ_Stress][NY_Stress][NX_Stress][3];
	 float q_convective_kin[NZ_Stress][NY_Stress][NX_Stress][3];
	 float q_convective_pot[NZ_Stress][NY_Stress][NX_Stress][3];
	 float q_diffusive[NZ_Stress][NY_Stress][NX_Stress][3];
	 float q_total[NZ_Stress][NY_Stress][NX_Stress][3];
	 float *vars[nvars], *vars_Stress[nvars_Stress];
	 int vardims[nvars], vardims_Stress[nvars_Stress], centering[nvars], centering_Stress[nvars_Stress];
	 const char *varnames[nvars];
	 const char *varnames_Stress[nvars_Stress];
	 	 
	 if (this->_stressCalcMethodConfinement == hardy){
	    vardims[0] = 1;
	    vardims[1] = 1;
	    vardims[2] = 3;
	    vardims[3] = 3;
	    vardims[4] = 1;
	    vardims[5] = 3;
	    vardims_Stress[0] = 1;
	    vardims_Stress[1] = 1;
	    vardims_Stress[2] = 3;
	    vardims_Stress[3] = 3;
	    vardims_Stress[4] = 3;
	    vardims_Stress[5] = 3;
	    vardims_Stress[6] = 3;
	    vardims_Stress[7] = 3;
	    for(int d = 0; d < nvars; d++)
	      centering[d] = 1;
	    for(int d = 0; d < nvars_Stress; d++)
	      centering_Stress[d] = 1;
	    varnames[0] = "density";
	    varnames[1] = "temperature";
	    varnames[2] = "velocity";
	    varnames[3] = "fluidForce";
	    varnames[4] = "fluidicComponent";
	    varnames[5] = "diffusion";
	    varnames_Stress[0] = "HydrodynamicStress";
	    varnames_Stress[1] = "vanMisesStress";
	    varnames_Stress[2] = "NormalStress(xx,yy,zz)";
	    varnames_Stress[3] = "ShearStress(xy,xz,yz)";
	    varnames_Stress[4] = "q_convective_kin";
	    varnames_Stress[5] = "q_convective_pot";
	    varnames_Stress[6] = "q_diffusive";
	    varnames_Stress[7] = "q_total";
	    vars[0] = (float *)density;
	    vars[1] = (float *)temperature;
	    vars[2] = (float *)velocity;
	    vars[3] = (float *)fluidForce;
	    vars[4] = (float *)fluidicComponent;
	    vars[5] = (float *)diffusion;
	    vars_Stress[0] = (float *)HydrodynamicStress;
	    vars_Stress[1] = (float *)vanMisesStress;
	    vars_Stress[2] = (float *)NormalStress;
	    vars_Stress[3] = (float *)ShearStress;
	    vars_Stress[4] = (float *)q_convective_kin;
	    vars_Stress[5] = (float *)q_convective_pot;
	    vars_Stress[6] = (float *)q_diffusive;
	    vars_Stress[7] = (float *)q_total;
	 }else{
	    vardims[0] = 1;
	    vardims[1] = 1;
	    vardims[2] = 1;
	    vardims[3] = 1;
	    vardims[4] = 3;
	    vardims[5] = 3;
	    vardims[6] = 3;
	    vardims[7] = 3;
	    vardims[8] = 1;
	    vardims[9] = 3;
	    vardims[10] = 3;
	    vardims[11] = 3;
	    vardims[12] = 3;
	    vardims[13] = 3;
	    vardims_Stress[0] = 3;
	    for(int d = 0; d < nvars; d++)
	      centering[d] = 1;
	    for(int d = 0; d < nvars_Stress; d++)
	      centering_Stress[d] = 1;
	    varnames[0] = "density";
	    varnames[1] = "temperature";
	    varnames[2] = "HydrodynamicStress";
	    varnames[3] = "vanMisesStress";
	    varnames[4] = "velocity";
	    varnames[5] = "fluidForce";
	    varnames[6] = "NormalStress(xx,yy,zz)";
	    varnames[7] = "ShearStress(xy,xz,yz)";
	    varnames[8] = "fluidicComponent";
	    varnames[9] = "q_convective_kin";
	    varnames[10] = "q_convective_pot";
	    varnames[11] = "q_diffusive";
	    varnames[12] = "q_total";
	    varnames[13] = "diffusion";
	    varnames_Stress[0] = "HydrodynamicStress";
	    vars[0] = (float *)density;
	    vars[1] = (float *)temperature;
	    vars[2] = (float *)HydrodynamicStress;
	    vars[3] = (float *)vanMisesStress;
	    vars[4] = (float *)velocity;
	    vars[5] = (float *)fluidForce;
	    vars[6] = (float *)NormalStress;
	    vars[7] = (float *)ShearStress;
	    vars[8] = (float *)fluidicComponent;
	    vars[9] = (float *)q_convective_kin;
	    vars[10] = (float *)q_convective_pot;
	    vars[11] = (float *)q_diffusive;
	    vars[12] = (float *)q_total;
	    vars[13] = (float *)diffusion;
	    vars_Stress[0] = (float *)HydrodynamicStress;
	 }
	 
		
	//____________________________________________________________________________________________________________________________________________________________________
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	viscProf << "//Local profile of effective viscosity in each bin. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	viscProf << "//Local viscosity profile: The viscosity is determined in bins in x,y-direction";
	viscProf << "//The vector of the local viscosity is eta(x) [d = distance between upper and lower plate] \n//      | d(x_i), eta(x_i)\n//---------------------\n//  x_i\n//      | \n";
	viscProf << "//DELTA_x \t DELTA_y \t width_z\n";
	viscProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	viscProf << "//# lines\n";
	viscProf << 3 << "\n";
	viscProf << "//# rows\n";
	viscProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
		
	double segmentSurface; // volume of a single bin, in a0^3 (LJ)
	segmentSurface = 1.0/this->_universalInvProfileUnitConfinement[0]/this->_universalInvProfileUnitConfinement[2];
	
	// Eintragen des Flags '>' zwecks Kompatibilität
	viscProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	viscProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  viscProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	viscProf << "\n";
	
	//! Eintragen von "d(x_i), eta(x_i)" in erste Spalte und in jede weitere Spalte die Werte d, eta(x_i)
          viscProf << "d(x_i)  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1];
	    double d_loc = (double)this->_dBin[unID];
	    viscProf << d_loc << "\t";
	  }	
	
	  viscProf << "\neta(x_i)  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1];
	    double eta_loc = (this->_globalForceConfinement[0][unID]/segmentSurface) / (pg->getGlobalTargetVelocity(0,1)/(double)this->_dBin[unID]);
	    viscProf << eta_loc << "\t";
	  }
	  
	  viscProf << "\n";
	  
	 viscProf.close();
	 //____________________________________________________________________________________________________________________________________________________________________
	
	rhoProf << "//Local profile of the number density. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	rhoProf << "//Local density profile: The number density is determined in bins in x,y-direction";
	rhoProf << "//The matrix of the local number density rho(y,x) \n//      | y_i\n//---------------------\n//  x_i| rho(y_i,x_i)\n//      | \n";
	rhoProf << "//DELTA_x \t DELTA_y \t width_z\n";
	rhoProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	rhoProf << "//# lines\n";
	rhoProf << this->_universalNProfileUnitsConfinement[1]+1 << "\n";
	rhoProf << "//# rows\n";
	rhoProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
	// info: getSigma() und getEps() implementiert in Component.h
	// end of header, start of the data-part of the density file
      }
	
	
	double segmentVolume; // volume of a single bin, in a0^3 (LJ)
	segmentVolume = 1.0/this->_universalInvProfileUnitConfinement[0]/this->_universalInvProfileUnitConfinement[1]/this->_universalInvProfileUnitConfinement[2];
	
	double segmentVolumeStress; // volume of a single bin, in a0^3 (LJ)
	segmentVolumeStress = 1.0/this->_universalInvProfileUnitStressConfinement[0]/this->_universalInvProfileUnitStressConfinement[1]/this->_universalInvProfileUnitStressConfinement[2];
	
	// Eintragen des Flags '>' zwecks Kompatibilität
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	rhoProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	rhoProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  rhoProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	rhoProf << "\n";
      }
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte rho(y_i,xi)
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnitConfinement[1];
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    rhoProf << yval<< "  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	    double rho_loc = this->_globalNConfinement[unID] / (segmentVolume * this->_globalAccumulatedDatasets_ConfinementProperties);
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	      rhoProf << rho_loc << "\t";
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){
	      density[0][n_y][n_x] = rho_loc;
	      fluidicComponent[0][n_y][n_x] = (double)this->_globalFluidicArea[unID];
	    }
	  }
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    rhoProf << "\n";
	 }
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	   rhoProf.close();
	 
	//____________________________________________________________________________________________________________________________________________________________________
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	TProf << "//Local profile of temperature. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	TProf << "//Local temperature profile: The temperature is determined in x,y-direction";
	TProf << "//The matrix of the local temperature T(y,x) \n//      | y_i\n//---------------------\n//  x_i| T(y_i,x_i)\n//      | \n";
	TProf << "//DELTA_x \t DELTA_y \t width_z\n";
	TProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	TProf << "//# lines\n";
	TProf << this->_universalNProfileUnitsConfinement[1]+1 << "\n";
	TProf << "//# rows\n";
	TProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
	// info: getSigma() und getEps() implementiert in Component.h
	// end of header, start of the data-part of the density file 
	
	
	// Eintragen des Flags '>' zwecks Kompatibilität
	TProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	TProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  TProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	TProf << "\n";
      }
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte rho(y_i,xi)
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnitConfinement[1];
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    TProf << yval<< "  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	    double DOFc = this->_globalDOFProfile_Confinement[unID];
	    double twoEkinc = this->_globalKineticProfile_Confinement[unID];
	    if(DOFc == 0.0){
		if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)  
		  TProf << 0 << "\t";
		if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
		  temperature[0][n_y][n_x] = 0.0;
	    }
	    else{
	        if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
		  TProf << (twoEkinc/DOFc ) << "\t";
		if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
		  temperature[0][n_y][n_x] = (twoEkinc/DOFc);
	    }
	  }
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    TProf << "\n";
	 }
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	   TProf.close(); 
	 
	//____________________________________________________________________________________________________________________________________________________________________
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	vxProf << "//Local x-velocity. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	vxProf << "//Local x-velocity: The x-velocity is determined in x,y-direction";
	vxProf << "//The matrix of the local x-velocity vx(y,x) \n//      | y_i\n//---------------------\n//  x_i| vx(y_i,x_i)\n//      | \n";
	vxProf << "//DELTA_x \t DELTA_y \t width_z\n";
	vxProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	vxProf << "//# lines\n";
	vxProf << this->_universalNProfileUnitsConfinement[1]+1 << "\n";
	vxProf << "//# rows\n";
	vxProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
	// info: getSigma() und getEps() implementiert in Component.h
	// end of header, start of the data-part of the density file 
	
	
	// Eintragen des Flags '>' zwecks Kompatibilität
	vxProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	vxProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  vxProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	vxProf << "\n";
      }
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte vx(y_i,x_i)
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnitConfinement[1];
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    vxProf << yval<< "  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	    double average_Vx = this->_globalvProfile_Confinement[0][unID]/this->_globalNConfinement[unID];
	    double average_Vy = this->_globalvProfile_Confinement[1][unID]/this->_globalNConfinement[unID];
	    double average_Vz = this->_globalvProfile_Confinement[2][unID]/this->_globalNConfinement[unID];
	    // Here it is checked whether average_Vx is NaN or not; if it is, it is set to zero;
	    /* FIXME: */
	    if (average_Vx != average_Vx)
		average_Vx = 0.0;
	    if (average_Vy != average_Vy)
		average_Vy = 0.0;
	    if (average_Vz != average_Vz)
		average_Vz = 0.0;
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	      vxProf << average_Vx << "\t";
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){
	      velocity[0][n_y][n_x][0] = average_Vx;
	      velocity[0][n_y][n_x][1] = average_Vy;
	      velocity[0][n_y][n_x][2] = average_Vz;
	    }
	  }
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    vxProf << "\n";
	 }
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	   vxProf.close();
	 
	 //____________________________________________________________________________________________________________________________________________________________________
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	fxProf << "//Local x-force. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	fxProf << "//Local x-force: The x-force is determined in x,y-direction";
	fxProf << "//The matrix of the local x-force fx(y,x) \n//      | y_i\n//---------------------\n//  x_i| fx(y_i,x_i)\n//      | \n";
	fxProf << "//For the calculation of stresses (F/A) the following area have to be used\n";
	fxProf << "//A_xy\tA_xz\tA_yz\n";
	fxProf << 1/this->_universalInvProfileUnitConfinement[0]*1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[0]*1/this->_universalInvProfileUnitConfinement[2] << "\t" << 1/this->_universalInvProfileUnitConfinement[1]*1/this->_universalInvProfileUnitConfinement[2] << "\n";
	fxProf << "//DELTA_x \t DELTA_y \t width_z\n";
	fxProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	fxProf << "//# lines\n";
	fxProf << this->_universalNProfileUnitsConfinement[1]+1 << "\n";
	fxProf << "//# rows\n";
	fxProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
	
	// Eintragen des Flags '>' zwecks Kompatibilität
	fxProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	fxProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  fxProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	fxProf << "\n";
      }
	
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte fx(y_i,x_i)
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnitConfinement[1];
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    fxProf << yval<< "  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	    double sum_Fx = this->_globalFluidForce_Confinement[0][unID]/this->_globalAccumulatedDatasets_ConfinementProperties;
	    double sum_Fz = this->_globalFluidForce_Confinement[2][unID]/this->_globalAccumulatedDatasets_ConfinementProperties;
	    // Here it is checked whether sum_Fx is NaN or not; if it is, it is set to zero;
	    /* FIXME: */
	    if (sum_Fx != sum_Fx)
		sum_Fx = 0.0;
	    if (sum_Fz != sum_Fz)
		sum_Fz = 0.0;
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)  
	      fxProf << sum_Fx << "\t";
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){
	      fluidForce[0][n_y][n_x][0] = sum_Fx;
	      fluidForce[0][n_y][n_x][2] = sum_Fz;
	    }
	  }
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    fxProf << "\n";	
	 }
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	   fxProf.close(); 	
	 
	 //____________________________________________________________________________________________________________________________________________________________________
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab){
	fyProf << "//Local y-force. Output file generated by the \"outputConfinementProperties\" method, located in Domain.cpp. \n";
	fyProf << "//Local y-force: The y-force is determined in x,y-direction";
	fyProf << "//The matrix of the local y-force fy(y,x) \n//      | y_i\n//---------------------\n//  x_i| fy(y_i,x_i)\n//      | \n";
	fyProf << "//For the calculation of stresses (F/A) the following area have to be used\n";
	fyProf << "//A_xy\tA_xz\tA_yz\n";
	fyProf << 1/this->_universalInvProfileUnitConfinement[0]*1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[0]*1/this->_universalInvProfileUnitConfinement[2] << "\t" << 1/this->_universalInvProfileUnitConfinement[1]*1/this->_universalInvProfileUnitConfinement[2] << "\n";
	fyProf << "//DELTA_x \t DELTA_y \t width_z\n";
	fyProf << 1/this->_universalInvProfileUnitConfinement[0] << "\t" << 1/this->_universalInvProfileUnitConfinement[1] << "\t" << 1/this->_universalInvProfileUnitConfinement[2] << "\n";
	fyProf << "//# lines\n";
	fyProf << this->_universalNProfileUnitsConfinement[1]+1 << "\n";
	fyProf << "//# rows\n";
	fyProf << this->_universalNProfileUnitsConfinement[0]+1 << "\n";
	
	// Eintragen des Flags '>' zwecks Kompatibilität
	fyProf << "> \n"; 
	// Eintragen der x-Koordinaten x_i in Header
	fyProf << 0 <<"  \t"; 
	for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsConfinement[0]; n_x++){
	  fyProf << (n_x + 0.5) / this->_universalInvProfileUnitConfinement[0] <<"  \t"; 
	}
	fyProf << "\n";
      }
      
	//! Eintragen der y_i in erste Spalte und in jede weitere Spalte die Werte fy(y_i,x_i)
	for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++)
	{
	  double yval = (n_y + 0.5) / this->_universalInvProfileUnitConfinement[1];
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    fyProf << yval<< "  \t";
	  for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++)
	  {
	    unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	    double sum_Fy = this->_globalFluidForce_Confinement[1][unID]/this->_globalAccumulatedDatasets_ConfinementProperties;
	    // Here it is checked whether sum_Fy is NaN or not; if it is, it is set to zero;
	    /* FIXME: */
	    if (sum_Fy != sum_Fy)
		sum_Fy = 0.0;
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	      fyProf << sum_Fy << "\t";	
	    if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk)
	      fluidForce[0][n_y][n_x][1] = sum_Fy;
	  }
	  if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	    fyProf << "\n";
	 }
	 if (this->_outputFormat == this->_all || this->_outputFormat == this->_matlab)
	   fyProf.close();  
	 
	  //____________________________________________________________________________________________________________________________________________________________________
	 
      if (this->_outputFormat == this->_all || this->_outputFormat == this->_vtk){	 
	 for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsStressConfinement[1]; n_y++)
	  for(unsigned n_x = 0; n_x < this->_universalNProfileUnitsStressConfinement[0]; n_x++)
	  {
	    double stress_locxx = 0.0, stress_locyy = 0.0, stress_loczz = 0.0, stress_locxy = 0.0, stress_locxz = 0.0, stress_locyz = 0.0, stress_lochydr = 0.0, stress_locmises = 0.0;
	    double aux_mis1 = 0.0, aux_mis2 = 0.0, aux_mis3 = 0.0, aux_mis4 = 0.0;
	    double q_x_convective_kin = 0.0, q_y_convective_kin = 0.0, q_z_convective_kin = 0.0, q_x_convective_pot = 0.0, q_y_convective_pot = 0.0, q_z_convective_pot = 0.0, q_x_diffusive = 0.0, q_y_diffusive = 0.0, q_z_diffusive = 0.0, q_x_total = 0.0, q_y_total = 0.0, q_z_total = 0.0;
	    unsigned unID = n_x * this->_universalNProfileUnitsStressConfinement[1]  + n_y;
	    
	    // Stresses
		stress_locxx = this->_globalStressConfinement[0][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_locyy = this->_globalStressConfinement[1][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_loczz = this->_globalStressConfinement[2][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_locxy = this->_globalStressConfinement[3][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_locxz = this->_globalStressConfinement[4][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_locyz = this->_globalStressConfinement[5][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		stress_lochydr = (this->_globalStressConfinement[0][unID]+this->_globalStressConfinement[1][unID]+this->_globalStressConfinement[2][unID])/(3 * (segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties));
		aux_mis1 = (this->_globalStressConfinement[0][unID] - this->_globalStressConfinement[1][unID])*(this->_globalStressConfinement[0][unID] - this->_globalStressConfinement[1][unID]);
		aux_mis2 = (this->_globalStressConfinement[1][unID] - this->_globalStressConfinement[2][unID])*(this->_globalStressConfinement[1][unID] - this->_globalStressConfinement[2][unID]);
		aux_mis3 = (this->_globalStressConfinement[2][unID] - this->_globalStressConfinement[0][unID])*(this->_globalStressConfinement[2][unID] - this->_globalStressConfinement[0][unID]);
		aux_mis4 = 6 * (this->_globalStressConfinement[3][unID] * this->_globalStressConfinement[3][unID] + this->_globalStressConfinement[4][unID] * this->_globalStressConfinement[4][unID] + this->_globalStressConfinement[5][unID] * this->_globalStressConfinement[5][unID]);
		stress_locmises = sqrt(0.5 * (aux_mis1 + aux_mis2 + aux_mis3 + aux_mis4))/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		
	    HydrodynamicStress[0][n_y][n_x] = stress_lochydr;
	    vanMisesStress[0][n_y][n_x] = stress_locmises;
	    NormalStress[0][n_y][n_x][0] = stress_locxx;
	    NormalStress[0][n_y][n_x][1] = stress_locyy;
	    NormalStress[0][n_y][n_x][2] = stress_loczz;
	    ShearStress[0][n_y][n_x][0] = stress_locxy;
	    ShearStress[0][n_y][n_x][1] = stress_locxz;
	    ShearStress[0][n_y][n_x][2] = stress_locyz;
	    
	    
	    // Heat Flux
		q_x_convective_kin = (this->_globalConvectiveKinHeatflux[0][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_y_convective_kin = (this->_globalConvectiveKinHeatflux[1][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_z_convective_kin = (this->_globalConvectiveKinHeatflux[2][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_x_convective_pot = (this->_globalConvectivePotHeatflux[0][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_y_convective_pot = (this->_globalConvectivePotHeatflux[1][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_z_convective_pot = (this->_globalConvectivePotHeatflux[2][unID])/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_x_diffusive = this->_globalDiffusiveHeatflux[0][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_y_diffusive = this->_globalDiffusiveHeatflux[1][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_z_diffusive = this->_globalDiffusiveHeatflux[2][unID]/(segmentVolumeStress * this->_globalAccumulatedDatasets_ConfinementProperties);
		q_x_total = q_x_diffusive + q_x_convective_kin + q_x_convective_pot;
		q_y_total = q_y_diffusive + q_y_convective_kin + q_y_convective_pot;
		q_z_total = q_z_diffusive + q_z_convective_kin + q_z_convective_pot;
	    
	    q_convective_kin[0][n_y][n_x][0] = q_x_convective_kin;
	    q_convective_kin[0][n_y][n_x][1] = q_y_convective_kin;
	    q_convective_kin[0][n_y][n_x][2] = q_z_convective_kin;
	    q_convective_pot[0][n_y][n_x][0] = q_x_convective_pot;
	    q_convective_pot[0][n_y][n_x][1] = q_y_convective_pot;
	    q_convective_pot[0][n_y][n_x][2] = q_z_convective_pot;
	    q_diffusive[0][n_y][n_x][0] = q_x_diffusive;
	    q_diffusive[0][n_y][n_x][1] = q_y_diffusive;
	    q_diffusive[0][n_y][n_x][2] = q_z_diffusive;
	    q_total[0][n_y][n_x][0] = q_x_total;
	    q_total[0][n_y][n_x][1] = q_y_total;
	    q_total[0][n_y][n_x][2] = q_z_total;
	  }
	  
	  for(unsigned n_y = 0; n_y < this->_universalNProfileUnitsConfinement[1]; n_y++){
	    for(unsigned n_x = 0; n_x< this->_universalNProfileUnitsConfinement[0]; n_x++){
	      unsigned unID = n_x * this->_universalNProfileUnitsConfinement[1]  + n_y;
	      diffusion[0][n_y][n_x][0] = this->_globalDiffusiveMovement[0][unID];
	      diffusion[0][n_y][n_x][1] = this->_globalDiffusiveMovement[1][unID];
	      diffusion[0][n_y][n_x][2] = this->_globalDiffusiveMovement[2][unID];
	    }
	  }
	 /* Use VisitWriter.cpp to write a regular mesh with data. */
	 if (this->_stressCalcMethodConfinement == hardy)
	   write_regular_mesh(vtknameStress.c_str(), 0, dims_Stress, nvars_Stress, vardims_Stress, centering_Stress, varnames_Stress, vars_Stress);
	 
	 write_regular_mesh(vtkname.c_str(), 0, dims, nvars, vardims, centering, varnames, vars);
      }
}


void Domain::resetConfinementProperties()
{
	unsigned unIDs = this->_universalNProfileUnitsConfinement[0] * this->_universalNProfileUnitsConfinement[1];
	unsigned stressCalc_unIDs = this->_universalNProfileUnitsStressConfinement[0] * this->_universalNProfileUnitsStressConfinement[1];

	for(unsigned unID = 0; unID < unIDs; unID++)
	{
		this->_localNConfinement[unID] = 0.0;
		this->_localWallNConfinement[unID] = 0.0;
		this->_localPressureVirial_Confinement[unID] = 0.0;
		this->_localPressureKin_Confinement[unID] = 0.0;
		this->_localDOFProfile_Confinement[unID] = 0.0;
		this->_localKineticProfile_Confinement[unID] = 0.0;
		this->_localFluidicArea[unID] = 0;
		this->_globalNConfinement[unID] = 0.0;
		this->_globalWallNConfinement[unID] = 0.0;
		this->_globalPressureVirial_Confinement[unID] = 0.0;
		this->_globalPressureKin_Confinement[unID] = 0.0;
		this->_globalDOFProfile_Confinement[unID] = 0.0;
		this->_globalKineticProfile_Confinement[unID] = 0.0;
		this->_dBinFailCount[unID] = 0;
		this->_globalFluidicArea[unID] = 0;
		for(int d = 0; d < 3; d++){
		    this->_localForceConfinement[d][unID] = 0.0;
		    this->_globalForceConfinement[d][unID] = 0.0;
		    this->_localvProfile_Confinement[d][unID] = 0.0;
		    this->_globalvProfile_Confinement[d][unID] = 0.0;
		    this->_localFluidForce_Confinement[d][unID] = 0.0;
		    this->_globalFluidForce_Confinement[d][unID] = 0.0;
		    this->_localDiffusiveMovement[d][unID] = 0.0;
		    this->_globalDiffusiveMovement[d][unID] = 0.0;
		}
		if (unID < this->_universalNProfileUnitsConfinement[0])
		    this->_dBin[unID] = 0.0;
	}
	
	for(unsigned unID = 0; unID < stressCalc_unIDs; unID++)
	{
	  for(int d = 0; d < 3; d++){
		    this->_localConvectiveKinHeatflux[d][unID] = 0.0;
		    this->_globalConvectiveKinHeatflux[d][unID] = 0.0;
		    this->_localConvectivePotHeatflux[d][unID] = 0.0;
		    this->_globalConvectivePotHeatflux[d][unID] = 0.0;
		    this->_localDiffusiveHeatflux[d][unID] = 0.0;
		    this->_globalDiffusiveHeatflux[d][unID] = 0.0;
	  }
	}
	
	this->_universalNConfinement = 0.0;
	this->_universalPressureVirial_Confinement = 0.0;
	this->_universalPressureKin_Confinement = 0.0;
	this->_universalDOFProfile_Confinement = 0.0;
	this->_universalKineticProfile_Confinement = 0.0;
	for(int d = 0; d < 3; d++)
	    this->_globalForce_Confinement[d] = 0.0;
	this->_dAveraged = 0.0;
	this->_dMax = 0.0;
	
	this->_globalAccumulatedDatasets_ConfinementProperties = 0;
	
	size_t rows = 6, cols = stressCalc_unIDs;

	for(unsigned i = 0; i < rows; i++){
	      for(unsigned j = 0; j < cols; j++){
		  this->_localStressConfinement[i][j] = 0.0;
		  this->_globalStressConfinement[i][j] = 0.0;
	      }
	 }
}

void Domain::Nadd(unsigned cid, int N, int localN)
{
	Ensemble* ensemble = _simulation.getEnsemble();
	Component* component = ensemble->component(cid);
	component->incNumMolecules(N);
	unsigned int rotationDegreesOfFreeedom = component->getRotationalDegreesOfFreedom();
	
	this->_globalNumMolecules += N;
	this->_localRotationalDOF[0] += localN * rotationDegreesOfFreeedom;
	this->_universalRotationalDOF[0] += N * rotationDegreesOfFreeedom;
	if( (this->_componentwiseThermostat || this->_enableThermostatLayers || this->_enableThermostatWallLayers)
			&& (this->_componentToThermostatIdMap[cid] > 0) )
	{
		int thid = this->_componentToThermostatIdMap[cid];
		if(isThermostatLayer() == true){
			cout << "Check thermostating; layerwise thermostating could be wrong in that case!\n";
		}
		this->_localThermostatN[thid] += localN;
		this->_universalThermostatN[thid] += N;
		this->_localRotationalDOF[thid] += localN * rotationDegreesOfFreeedom;
		this->_universalRotationalDOF[thid] += N * rotationDegreesOfFreeedom;
	}
	this->_localThermostatN[0] += localN;
	this->_universalThermostatN[0] += N;
	this->_localRotationalDOF[0] += localN * rotationDegreesOfFreeedom;
	this->_universalRotationalDOF[0] += N * rotationDegreesOfFreeedom;
}

void Domain::evaluateRho(
		unsigned long localN, DomainDecompBase* domainDecomp
		) {
	domainDecomp->collCommInit(1);
	domainDecomp->collCommAppendUnsLong(localN);
	domainDecomp->collCommAllreduceSum();
	this->_globalNumMolecules = domainDecomp->collCommGetUnsLong();
	domainDecomp->collCommFinalize();

	this->_globalRho = this->_globalNumMolecules /
		(this->_globalLength[0] * this->_globalLength[1] * this->_globalLength[2]);
}

void Domain::setTargetTemperature(int thermostat, double targetT)
{
	if(thermostat < 0)
	{
		global_log->warning() << "Warning: thermostat \'" << thermostat << "\' (T = "
			<< targetT << ") will be ignored." << endl;
		return;
	}

	this->_universalTargetTemperature[thermostat] = targetT;
	if(!(this->_universalUndirectedThermostat[thermostat] == true))
		this->_universalUndirectedThermostat[thermostat] = false;

	/* FIXME: Substantial change in program behaviour! */
	if(thermostat == 0) {
		global_log->warning() << "Disabling the component wise thermostat!" << endl;
		disableComponentwiseThermostat();
	}
	if(thermostat >= 1 && !this->_enableThermostatWallLayers) {
		if( ! _componentwiseThermostat ) {
			/* FIXME: Substantial change in program behaviour! */
			global_log->warning() << "Enabling the component wise thermostat!" << endl;
			_componentwiseThermostat = true;
			_universalTargetTemperature.erase(0);
			_universalUndirectedThermostat.erase(0);
			for(int d=0; d < 3; d++) this->_universalThermostatDirectedVelocity[d].erase(0);
			vector<Component>* components = _simulation.getEnsemble()->components();
			for( vector<Component>::iterator tc = components->begin(); tc != components->end(); tc ++ ) {
				if(!(this->_componentToThermostatIdMap[ tc->ID() ] > 0)) {
					this->_componentToThermostatIdMap[ tc->ID() ] = -1;
				}
			}
		}
		// initialize the time slot of each thermostat
		this->_thermostatTimeSlot[0][thermostat] = 0;
		this->_thermostatTimeSlot[1][thermostat] = _simulation.getNumTimesteps();
	}
}

void Domain::set1DimThermostat(int thermostat, int dimension)
{
	if(thermostat < 0)
	{
		global_log->warning() << "Warning: thermostat \'" << thermostat << "will be ignored." << endl;
		return;
	}
	
	if(thermostat == 0) {
		global_log->warning() << "Disabling the component wise thermostat!" << endl;
	}
	if(thermostat >= 1) {
		this->_dimToThermostat[thermostat] = dimension;
	}
}

// geometrical setup for layerwise thermostats
void Domain::setThermostatLayer(int thermostat, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax)
{
	if(thermostat < 0)
	{
		global_log->warning() << "Warning: thermostat layer \'" << thermostat << "\' will be ignored." << endl;
		return;
	}
	setComponentThermostat(thermostat, thermostat); 
	
	this->_thermostatLayer[0][thermostat] = xmin;
	this->_thermostatLayer[1][thermostat] = xmax;
	this->_thermostatLayer[2][thermostat] = ymin;
	this->_thermostatLayer[3][thermostat] = ymax;
	this->_thermostatLayer[4][thermostat] = zmin;
	this->_thermostatLayer[5][thermostat] = zmax;
	
	this->_enableThermostatLayers = true;
}

void Domain::setThermostatWallLayer()
{
	int numberOfLayersPerWall = ceil(this->_confinementMidPoint[3]/0.775)+1;
	double lowEdgeUpperWall = this->_confinementMidPoint[1];
	//double highEdgeLowerWall = this->_confinementMidPoint[3];
	
	for(unsigned id = 0; id < getNumberOfComponents(); id++){
		this->_originalThtID[id] = _componentToThermostatIdMap[id];
		this->_originalStartTime[id] = this->_thermostatTimeSlot[0][this->_originalThtID[id]];
		this->_originalEndTime[id] = this->_thermostatTimeSlot[1][this->_originalThtID[id]];
		this->_originalTargetTemp[id] = this->_universalTargetTemperature[this->_originalThtID[id]];
	}
	
	int thermostat = 1;
	
	// new setting of the thermostats
	for (unsigned compCount = 0; compCount < getNumberOfComponents(); compCount++){
		if(compCount == 0){
			for (int layerCount = 0; layerCount < numberOfLayersPerWall; layerCount++){
				this->_componentToThermostatIdMap[thermostat] = thermostat;
				this->_universalThermostatN[thermostat] = 0;
				this->_thermostatLayer[0][thermostat] = 0.0;
				this->_thermostatLayer[1][thermostat] = 10000;
				if(layerCount == 0)
					this->_thermostatLayer[2][thermostat] = lowEdgeUpperWall - 0.350;
				else
					this->_thermostatLayer[2][thermostat] = this->_thermostatLayer[2][thermostat-1] + 0.776;
				this->_thermostatLayer[3][thermostat] = this->_thermostatLayer[2][thermostat] + 0.775;
				this->_thermostatLayer[4][thermostat] = 0.0;
				this->_thermostatLayer[5][thermostat] = 10000;
				this->_thermostatLayerComp[thermostat] = 1;		// upper wall
				setTargetTemperature(thermostat, this->_originalTargetTemp[1]);
				setThermostatTimeSlot(thermostat, this->_originalStartTime[1], this->_originalEndTime[1]);
				if(_localRank == 0)
					cout << " upThT " << thermostat << " ymin " << this->_thermostatLayer[2][thermostat] << " ymax " << this->_thermostatLayer[3][thermostat] << " CID " << this->_thermostatLayerComp[thermostat] << " T_tar " << this->_originalTargetTemp[1] << " tmin " << this->_originalStartTime[1] << " tmax " << this->_originalEndTime[1] << endl;
				thermostat++;
			}
		}else if(compCount == 1){
			for (int layerCount = 0; layerCount < numberOfLayersPerWall; layerCount++){
				this->_componentToThermostatIdMap[thermostat] = thermostat;
				this->_universalThermostatN[thermostat] = 0;
				this->_thermostatLayer[0][thermostat] = 0.0;
				this->_thermostatLayer[1][thermostat] = 10000;
				if(layerCount == 0)
					this->_thermostatLayer[2][thermostat] = 0.0 - 0.350;
				else
					this->_thermostatLayer[2][thermostat] = this->_thermostatLayer[2][thermostat-1] + 0.776;
				this->_thermostatLayer[3][thermostat] = this->_thermostatLayer[2][thermostat] + 0.775;
				this->_thermostatLayer[4][thermostat] = 0.0;
				this->_thermostatLayer[5][thermostat] = 10000;
				this->_thermostatLayerComp[thermostat] = 2;	// lower wall
				setTargetTemperature(thermostat, this->_originalTargetTemp[2]);
				setThermostatTimeSlot(thermostat, this->_originalStartTime[2], this->_originalEndTime[2]);
				if(_localRank == 0)
					cout << " lowThT " << thermostat << " ymin " << this->_thermostatLayer[2][thermostat] << " ymax " << this->_thermostatLayer[3][thermostat] << " CID " << this->_thermostatLayerComp[thermostat] << " T_tar " << this->_originalTargetTemp[2] << " tmin " << this->_originalStartTime[2] << " tmax " << this->_originalEndTime[2] << endl;
				thermostat++;
			}
		}else{
			this->_componentToThermostatIdMap[thermostat] = thermostat;
			this->_universalThermostatN[thermostat] = 0;
			this->_thermostatLayer[0][thermostat] = 0.0;
			this->_thermostatLayer[1][thermostat] = 10000;
			this->_thermostatLayer[2][thermostat] = 0.0;
			this->_thermostatLayer[3][thermostat] = 10000;
			this->_thermostatLayer[4][thermostat] = 0.0;
			this->_thermostatLayer[5][thermostat] = 10000;
			this->_thermostatLayerComp[thermostat] = 0;	// fluid
			setTargetTemperature(thermostat, this->_originalTargetTemp[0]);
			setThermostatTimeSlot(thermostat, this->_originalStartTime[0], this->_originalEndTime[0]);
			if(_localRank == 0)
					cout << " fluidThT " << thermostat << " ymin " << this->_thermostatLayer[2][thermostat] << " ymax " << this->_thermostatLayer[3][thermostat] << " CID " << this->_thermostatLayerComp[thermostat] << " T_tar " << this->_originalTargetTemp[0] << " tmin " << this->_originalStartTime[0] << " tmax " << this->_originalEndTime[0] << endl;
			thermostat++;
		}
	}
	this->_enableThermostatWallLayers = true;
}

// checks whether there is an overlap between the thermostats layers; if there is an overlap that leads to an error   
void Domain::checkThermostatLayerDisjunct()
{
   if(maxThermostat() > 1)  
     for(int layer1 = 1; layer1 <= maxThermostat() - 1; layer1++){
       double layer1_dx = this->_thermostatLayer[1][layer1] - this->_thermostatLayer[0][layer1];
       double layer1_dy = this->_thermostatLayer[3][layer1] - this->_thermostatLayer[2][layer1];
       double layer1_dz = this->_thermostatLayer[5][layer1] - this->_thermostatLayer[4][layer1];
       
       for(int layer2 = layer1 + 1; layer2 <= maxThermostat(); layer2++){
	 double layer2_dx = this->_thermostatLayer[1][layer2] - this->_thermostatLayer[0][layer2];
	 double layer2_dy = this->_thermostatLayer[3][layer2] - this->_thermostatLayer[2][layer2];
	 double layer2_dz = this->_thermostatLayer[5][layer2] - this->_thermostatLayer[4][layer2];
	 double dx = this->_thermostatLayer[0][layer1] - this->_thermostatLayer[0][layer2];
	 double dy = this->_thermostatLayer[2][layer1] - this->_thermostatLayer[2][layer2];
	 double dz = this->_thermostatLayer[4][layer1] - this->_thermostatLayer[4][layer2];
	 int disjunct = 0;
	 
	 if(dx > layer2_dx || (-1)*dx > layer1_dx){}
	 else disjunct++;
	 if(dy > layer2_dy || (-1)*dy > layer1_dy){}
	 else disjunct++;
	 if(dz > layer2_dz || (-1)*dz > layer1_dz){}
	 else disjunct++;
	   
	 if(disjunct >= 3){
	   global_log->info() << "Thermostat layer number " << layer1 << " is disjunct with layer number " << layer2 << " (total number of thermostats: " << maxThermostat() << ").\n";
	   exit(345);
	 }
       }
     }
}

// assigns each molecule to its current thermostat layer
int Domain::moleculeInLayer(double molX, double molY, double molZ, unsigned int cid)
{	
    int layer = -10, inside = 0;
    for(int layer1 = 1; layer1 <= maxThermostat(); layer1++){
      inside = 0;
      if(this->_thermostatLayer[1][layer1] >= molX && this->_thermostatLayer[0][layer1] < molX) {inside++;}
      if(this->_thermostatLayer[3][layer1] >= molY && this->_thermostatLayer[2][layer1] < molY) {inside++;}
      if(this->_thermostatLayer[5][layer1] >= molZ && this->_thermostatLayer[4][layer1] < molZ) {inside++;}
      
      if((inside == 3 && !this->_enableThermostatWallLayers) 
      || (inside == 3 && this->_enableThermostatWallLayers && cid == (unsigned int)this->_thermostatLayerComp[layer1])){
	layer = layer1;
	goto LAYER;
      }
    }
     LAYER:if(inside == 3)
	return layer;
      else
	return -10;
}

// determines the time span of the thermostating for each thermostat
void Domain::setThermostatTimeSlot(int thermostat, unsigned long startTime, unsigned long endTime){
  
	if(thermostat < 0)
	{
		global_log->warning() << "Warning: thermostat \'" << thermostat << "\' will be turned on for the whole simulation time." << endl;
		return;
	}
	this->_thermostatTimeSlot[0][thermostat] = startTime;
	this->_thermostatTimeSlot[1][thermostat] = endTime;
	global_log->info() << "Thermostat number " << thermostat << " is turned on at simstep = " << startTime << " and turned off at simstep = " << endTime << ".\n";
}

void Domain::enableComponentwiseThermostat()
{
	if(this->_componentwiseThermostat) return;

	this->_componentwiseThermostat = true;
	this->_universalTargetTemperature.erase(0);
	vector<Component>* components = _simulation.getEnsemble()->components();
	for( vector<Component>::iterator tc = components->begin(); tc != components->end(); tc ++ ) {
		if(!(this->_componentToThermostatIdMap[ tc->ID() ] > 0)) {
			this->_componentToThermostatIdMap[ tc->ID() ] = -1;
		}
	}
}

void Domain::enableLayerwiseThermostat()
{
	if(this->_enableThermostatLayers) return;

	this->_enableThermostatLayers = true;
	this->_universalTargetTemperature.erase(0);
	for( int tc = 1; tc <= maxThermostat(); tc ++ ) {
		if(!(this->_componentToThermostatIdMap[ tc ] > 0)) {
			this->_componentToThermostatIdMap[ tc ] = -1;
		}
	}
}

void Domain::enableLayerwiseWallThermostat()
{
	if(this->_enableThermostatWallLayers) return;

	this->_enableThermostatWallLayers = true;
	this->_universalTargetTemperature.erase(0);
	for( int tc = 1; tc <= maxThermostat(); tc ++ ) {
		if(!(this->_componentToThermostatIdMap[ tc ] > 0)) {
			this->_componentToThermostatIdMap[ tc ] = -1;
		}
	}
	
}

void Domain::enableUndirectedThermostat(int tst)
{
	this->_universalUndirectedThermostat[tst] = true;
	for(int d=0; d < 3; d++)
	{
		this->_universalThermostatDirectedVelocity[d][tst] = 0.0;
		this->_localThermostatDirectedVelocity[d][tst] = 0.0;
	}
}

void Domain::setGlobalTemperature(double temp)
{
	this->disableComponentwiseThermostat();
	this->_universalTargetTemperature[0] = temp;
}

vector<double> & Domain::getmixcoeff() { return _mixcoeff; }

double Domain::getepsilonRF() const { return _epsilonRF; }

void Domain::setepsilonRF(double erf) { _epsilonRF = erf; }

unsigned long Domain::getglobalNumMolecules() const { return _globalNumMolecules; }

void Domain::setglobalNumMolecules(unsigned long glnummol) { _globalNumMolecules = glnummol; }

double Domain::getglobalRho(){ return _globalRho;}

void Domain::setglobalRho(double grho){ _globalRho = grho;}

unsigned long Domain::getglobalRotDOF()
{
	return this->_universalRotationalDOF[0]; 
}

void Domain::setglobalRotDOF(unsigned long grotdof)
{
	this->_universalRotationalDOF[0] = grotdof;
}

void Domain::setGlobalLength(int index, double length) {
	_globalLength[index] = length;
}

void Domain::record_cv()
{
	if(_localRank != 0) return;

	this->_globalUSteps ++;
	this->_globalSigmaU += this->_globalUpot;
	this->_globalSigmaUU += this->_globalUpot*_globalUpot;
}

double Domain::cv()
{
	if((_localRank != 0) || (_globalUSteps == 0)) return 0.0;

	double id = 1.5 + 0.5*_universalRotationalDOF[0]/_globalNumMolecules;
	double conf = (_globalSigmaUU - _globalSigmaU*_globalSigmaU/_globalUSteps)
		/ (_globalUSteps * _globalNumMolecules * _globalTemperatureMap[0] * _globalTemperatureMap[0]);

	return id + conf;
}

//! methods implemented by Stefan Becker <stefan.becker@mv.uni-kl.de>
// the following two methods are used by the MmspdWriter (writing the output file in a format used by MegaMol)
double Domain::getSigma(unsigned cid, unsigned nthSigma){
  return _simulation.getEnsemble()->component(cid)->getSigma(nthSigma);
}
unsigned Domain::getNumberOfComponents(){
  return _simulation.getEnsemble()->components()->size();
}

void Domain::submitDU(unsigned cid, double DU, double* r)
{
   unsigned xun, yun, zun;
   xun = (unsigned)floor(r[0] * this->_universalInvProfileUnit[0]);
   yun = (unsigned)floor(r[1] * this->_universalInvProfileUnit[1]);
   zun = (unsigned)floor(r[2] * this->_universalInvProfileUnit[2]);
   int unID = xun * this->_universalNProfileUnits[1] * this->_universalNProfileUnits[2]
                  + yun * this->_universalNProfileUnits[2] + zun;
   if(unID < 0) return;

   _localWidomProfile[unID] += exp(-DU / _globalTemperatureMap[0]);
   _localWidomInstances[unID] += 1.0;

   double Tloc = _universalTProfile[unID];
   if(Tloc != 0.0)
   {
      _localWidomProfileTloc[unID] += exp(-DU/Tloc);
      _localWidomInstancesTloc[unID] += 1.0;
   }
}

// cancels the momentum of the complete system taking into accout whether there is a shear rate applied
void Domain::cancelMomentum(DomainDecompBase* domainDecomp, ParticleContainer* molCont)
{
   unsigned numComp = getNumberOfComponents();
  
   unsigned long numberOfMoleculesPerComp[numComp];
   double localMomentum[numComp][3];
   for(unsigned num = 0; num < numComp; num++){
    numberOfMoleculesPerComp[num] = 0; 
    for(unsigned d = 0; d < 3; d++) localMomentum[num][d] = 0.0;
   }
   for(Molecule* tm = molCont->begin(); tm != molCont->end(); tm = molCont->next())
   {
      double tmass = tm->mass();
      unsigned comp = tm->componentid();
      numberOfMoleculesPerComp[comp]++;
      for(unsigned d = 0; d < 3; d++)
         localMomentum[comp][d] += tm->v(d) * tmass;
   }
   unsigned long globalNumberOfMoleculesPerComp[numComp];
   double globalMomentum[numComp][3];
   for(unsigned num = 0; num < numComp; num++){
    globalNumberOfMoleculesPerComp[num] = numberOfMoleculesPerComp[num];
    for(unsigned d = 0; d < 3; d++) globalMomentum[num][d] = localMomentum[num][d];
   }
    
   domainDecomp->collCommInit(4*numComp);
   for(unsigned num = 0; num < numComp; num++){
    domainDecomp->collCommAppendUnsLong(globalNumberOfMoleculesPerComp[num]); 
    for(unsigned short d = 0; d<3; d++)domainDecomp->collCommAppendDouble(globalMomentum[num][d]);
   }
    
   domainDecomp->collCommAllreduceSum();
   for(unsigned num = 0; num < numComp; num++){
    globalNumberOfMoleculesPerComp[num] = domainDecomp->collCommGetUnsLong();
    for(unsigned short d = 0; d < 3; d++) globalMomentum[num][d]  = domainDecomp->collCommGetDouble();
   }
   domainDecomp->collCommFinalize();
    
   for(unsigned num = 0; num < numComp; num++){
    for(unsigned short d = 0; d < 3; d++) globalMomentum[num][d] /= globalNumberOfMoleculesPerComp[num];
    
    if((_simulation.isShearRate() || _simulation.isShearForce()) || getPG()->isAcceleratingInstantaneously(getNumberOfComponents()))
	    globalMomentum[num][0] = 0.0;
   }
	    
   for(Molecule* tm = molCont->begin(); tm != molCont->end(); tm = molCont->next())
   {
      double tmass = tm->mass();
      unsigned comp = tm->componentid();
      tm->vsub(globalMomentum[comp][0]/tmass, globalMomentum[comp][1]/tmass, globalMomentum[comp][2]/tmass);
      if(_simulation.getBoolDirVel() == true)
	tm->vDirSub(globalMomentum[comp][0]/tmass, globalMomentum[comp][1]/tmass, globalMomentum[comp][2]/tmass, _simulation.isRecordingSlabProfile(), _simulation.isRecordingStressProfile(), _simulation.isRecordingConfinementProfile());
   }
}

// sets for type 'fixed' that it is fixed by not integrating every xth molecule
void Domain::specifyFixed(unsigned xth_mol) {
	this->_doFixEvery = true;
	this->_fixEvery = xth_mol;
}

// sets for type 'fixed' that it is fixed by not integrating a region spanned by the lower left and upper right corner
void Domain::specifyFixed(double x_min, double x_max, double y_min, double y_max, double z_min, double z_max) {
	this->_doFixRegion = true;
	this->_fixRegion[0] = x_min;
	this->_fixRegion[1] = x_max;
	this->_fixRegion[2] = y_min;
	this->_fixRegion[3] = y_max;
	this->_fixRegion[4] = z_min;
	this->_fixRegion[5] = z_max;
}

void Domain::specifySpringInfluence(unsigned long minSpringID, unsigned long maxSpringID, double averageYPos, double springConst)
{
	this->_minSpringID = minSpringID;
	this->_maxSpringID = maxSpringID;
	this->_averageYPos = averageYPos;
	this->_springConst = springConst;
	
	if(!this->_localRank)
	    cout << "Spring Force Effect is prepared for MolIDs " << this->_minSpringID << " to " << this->_maxSpringID << " with minimum y-Position " << this->_averageYPos << endl;
}

void  Domain::specifyGravity(unsigned int cid, unsigned int direction, double force)
{
	this->_gravitationalComp = cid;
	this->_gravitationalDir = direction;
	this->_gravitationalForce = force;
	this->_isGravity = true;
	
	if(!this->_localRank)
	    cout << "Gravitation is prepared for component " << this->_gravitationalComp << " in direction " << this->_gravitationalDir << " (0 = x; 1 = y; 2 = z) with the force " << this->_gravitationalForce << endl;
}

unsigned long Domain::getSimstep(){
    return _simulation.getSimulationStep(); 
}
double Domain::getTimestepLength(){
    return _simulation.getTimeStepLength();
}
unsigned long Domain::getInitStatistics(){
  return _simulation.getInitStatistics(); 
}
unsigned Domain::getStressRecordTimeStep(){
    return _simulation.getStressRecordTimestep();
}
unsigned Domain::getBarostatTimeInit(){
    return _simulation.getBarostatTimeInit();
}
unsigned Domain::getBarostatTimeEnd(){
    return _simulation.getBarostatTimeEnd();
}
unsigned Domain::getConfinementRecordTimeStep(){
    return _simulation.getConfinementRecordTimestep();
}
double Domain::getCutoffRadius(){
    return _simulation.getLJCutoff(); 
}
bool Domain::isShearRate(){
    return _simulation.isShearRate();
}
bool Domain::isShearForce(){
    return _simulation.isShearForce();
}
