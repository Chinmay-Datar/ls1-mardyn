/**
 * @file NeighborAcquirer.h
 * @author seckler
 * @date 06.05.19
 */

#pragma once
#include <vector>
#include "CommunicationPartner.h"

class Domain;
class HaloRegion;

class NeighborAcquirer {
public:
	/**
	 * Acquire the needed neighbors defined through the specific desired HaloRegions.
	 *
	 * @param domain The domain object.
	 * @param ownRegion The region of the own process.
	 * @param desiredRegions This is a vector of the desired regions. Partners are generated if at least parts of the
	 * desiredRegions lie outside of ownRegion.
	 * @param partners01 Vector of communication partners that contain domains outside of ownRegion.
	 * @param partners02 Vector of communication partners that contain domains inside of ownRegion.
	 * @return A tuple of 2 vectors: The first vector represents the partners NOT owning the haloDomain, while the
	 * second vector will own the particles.
	 */
	static std::tuple<std::vector<CommunicationPartner>, std::vector<CommunicationPartner>> acquireNeighbors(
		Domain* domain, HaloRegion* ownRegion, std::vector<HaloRegion>& desiredRegions, double skin);
	static std::vector<CommunicationPartner> squeezePartners(const std::vector<CommunicationPartner>& partners);

private:
	static bool isIncluded(HaloRegion* myRegion, HaloRegion* inQuestion);
	static void overlap(HaloRegion* myRegion, HaloRegion* inQuestion);
	static HaloRegion getPotentiallyShiftedRegion(const double* domainLength, const HaloRegion& region,
												  double* shiftArray, double skin);

	friend class NeighbourCommunicationSchemeTest;
};
