#ifndef ENSEMBLE_BASE_H_
#define ENSEMBLE_BASE_H_

#include "molecules/Component.h"

#include <string>
#include <vector>
#include <map>

#include "DomainBase.h"

class ParticleContainer;
class MixingRuleBase;
class ChemicalPotential;
class DomainDecompBase;
class CellProcessor;

//! list of updatable values
enum GlobalVariable {
	NUM_PARTICLES      = 1<<0,
	ENERGY             = 1<<1,
	VOLUME             = 1<<2,
	CHEMICAL_POTENTIAL = 1<<3,
	TEMPERATURE        = 1<<4,
	PRESSURE           = 1<<5
};

class XMLfileUnits;

//! @brief Base class for ensembles
//! @author Christoph Niethammer <niethammer@hlrs.de>
//! 
//! Each ensemble should provide access to extensive (NVE) and intensive 
//! (\mu p t) variables as well as a function to update global variables.
class Ensemble {
public:
	Ensemble() :
			_domain(nullptr) {
	}
	virtual ~Ensemble();
	virtual void readXML(XMLfileUnits& xmlconfig);

	//! @brief Returns the global number of Molecules of the ensemble.
	virtual unsigned long N() = 0;
	//! @brief Returns the global volume of the ensemble
	virtual double V() = 0;
	//! @brief Returns the global energy of the ensemble
	virtual double E() = 0;
	//! @brief Returns the global chemical potential of the ensemble
	virtual double mu() = 0;
	//! @brief Returns the global presure of the ensemble.
	virtual double p() = 0;
	//! @brief Returns the global Temperature of the ensemble.
	virtual double T() = 0;

	//! @brief Calculate global variables
	//! @param variable Variable to be updated.
	virtual void updateGlobalVariable(ParticleContainer *particleContainer, GlobalVariable variable) = 0;

	DomainBase* &domain() { return _domain; }
	Component* getComponent(int cid) {
		mardyn_assert(cid < static_cast<int>(_components.size()));
		return &_components.at(cid);
	}
	Component* getComponent(std::string name) { return getComponent(_componentnamesToIds[name]); }
	std::vector<Component>* getComponents() { return &_components; }
	void addComponent(Component& component) { _components.push_back(component); }

	//! prepare the _compIDs used by the Vectorized*CellProcessors
	void setComponentLookUpIDs();

	/*! get Ensemble Type (NVT or muVT) */
	std::string getType(){return _type;}

    /*! Returns _lmu pointer for processing by external plugins */
    virtual std::list<ChemicalPotential>* getLmu(){return &_lmu;}

    /*! runs steps only needed in GrandCanonicalEnsemble, does nothing for canonical */
    virtual void initConfigXML(ParticleContainer *moleculeContainer) {};
    /*! runs steps only needed in GrandCanonicalEnsemble, does nothing for canonical */
    virtual void prepare_start() {};
    /*! runs simulate step needed in GrandCanonical, nothing for canonical */
    virtual void
    beforeEventNewTimestep(ParticleContainer *moleculeContainer, DomainDecompBase *domainDecomposition,
                           unsigned long simstep) {};
    /*! runs after forces step for GrandCanonical, nothing for canonical */
    virtual void
    afterForces(ParticleContainer *moleculeContainer, DomainDecompBase *domainDecomposition, CellProcessor *cellProcessor,
                    unsigned long simstep) {};

protected:
	std::vector<Component> _components;
	std::map<std::string,int> _componentnamesToIds;
	std::vector<MixingRuleBase*> _mixingrules;
	DomainBase* _domain;
	std::string _type = "Undefined";

    /** List of ChemicalPotential objects needed for GrandCanonical only, each of which describes a
     * particular control volume for the grand canonical ensemble with
     * respect to one of the simulated components.
     *
     * It may at first be unclear why one could want to specify
     * several grand canonical ensembles, which are then stored in a
     * list. However, note that for every component a distinct
     * chemical potential can be specified, and this is of course
     * essential in certain cases. Also, different chemical potentials
     * can be specified for different control volumes to induce a
     * gradient of the chemical potential.
     */
     // This is needed in the EnsembleBase because several plugins dont check for ensemble type and try to iterate through this list
     // Would be nicer to only have it in GrandCanonical, but that would mean implementing ensemble type checks in those other classes.
    std::list<ChemicalPotential> _lmu;
};

#endif /* ENSEMBLE_BASE_H_ */
