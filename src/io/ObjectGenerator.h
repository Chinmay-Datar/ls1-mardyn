#ifndef SRC_IO_OBJECTGENERATOR_H_
#define SRC_IO_OBJECTGENERATOR_H_

#include <memory>

#include "io/InputBase.h"

class GridFiller;
class Object;
class VelocityAssignerBase;
class MoleculeIdPool;

class ObjectGenerator : public InputBase {
public:
	ObjectGenerator() : _filler(nullptr), _object(nullptr), _velocityAssigner(nullptr), _moleculeIdPool(nullptr) {};
	~ObjectGenerator() {}

	/** @brief Read in XML configuration for Generator and all its included objects.
	 *
	 * The following xml object structure is handled by this method:
	 * @note This structure is not fixed yet and may see changes
	 * \code{.xml}
	   <objectgenerator>
	     <filler type=""><!-- ... --></filler>
	     <object type=""><!-- ... --></object>
	     <velocityAssigner type=""><!-- ... --></velocityAssigner>
	   </objectgenerator>
	   \endcode
	 */
	virtual void readXML(XMLfileUnits& xmlconfig);

	void setFiller(std::shared_ptr<GridFiller> filler) { _filler = filler; }
	void setObject(std::shared_ptr<Object> object) { _object = object; }
	void setVelocityAssigner(std::shared_ptr<VelocityAssignerBase> vAssigner) { _velocityAssigner = vAssigner; }
	void setMoleculeIDPool(std::shared_ptr<MoleculeIdPool> moleculeIdPool) { _moleculeIdPool = moleculeIdPool; }

	void readPhaseSpaceHeader(Domain* /*domain*/, double /*timestep*/) {}

	unsigned long readPhaseSpace(ParticleContainer* particleContainer, std::list<ChemicalPotential>* lmu, Domain* domain, DomainDecompBase* domainDecomp);

private:
	std::shared_ptr<GridFiller> _filler;
	std::shared_ptr<Object> _object;
	std::shared_ptr<VelocityAssignerBase> _velocityAssigner;
	std::shared_ptr<MoleculeIdPool> _moleculeIdPool;
};

#endif  // SRC_IO_OBJECTGENERATOR_H_
