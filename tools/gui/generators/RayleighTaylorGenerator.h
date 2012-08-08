/*
 * RayleighTaylorGenerator.h
 *
 *  Created on: June, 2012
 *      Author: nagashim
 */

class Domain;
using namespace std;

#include "MDGenerator.h"
#include "common/ComponentParameters.h"
#include "parallel/DomainDecompBase.h"
#include "parallel/DomainDecompDummy.h"

#ifndef RAYLEIGHTALORGENERATOR_H_
#define RAYLEIGHTALORGENERATOR_H_

class RayleighTaylorGenerator: public MDGenerator {

private:
	double _temperature;
	int _N;
	double _L1, _L2, _L3, epsilon_A, epsilon_B, sigma_A, sigma_B,
		q_A, q_B, m_A, m_B, _r_cut, _delta_t, _T, _G, _h, _p_max, _skal;
	std::vector<Component> _components;

	//double rho;
	double numSphereSizes;

	//double simBoxLength[3]; // we use L1 and L2 in place of simBoxLength[3]

public:
	/**
	 * Constructor
	 */
	RayleighTaylorGenerator();
	virtual ~RayleighTaylorGenerator();

	vector<ParameterCollection*> getParameters();

	//void generatePreview();

	void setParameter(Parameter* p);

	virtual bool validateParameters();

	//! @brief read the phase space components and header information
	//! @param timestep timestep length
	virtual void readPhaseSpaceHeader(Domain* domain, double timestep);


	//
	//  @brief read the actual phase space information
	//  Returns "the highest molecule ID found in the phase space file";
	//  // todo why? should it be some kind of upper bound for the number of molecules???
	//
	virtual unsigned long readPhaseSpace(ParticleContainer* particleContainer,
			std::list<ChemicalPotential>* lmu, Domain* domain,
			DomainDecompBase* domainDecomp);

private:

	//
	// add a molecule to the container, initializing random velocity, orientation, and so on....
	//
	void addMolecule(double x, double y, double z, unsigned long id, ParticleContainer* particleContainer);
};

#endif // RayleighTaylorGenerator.h

