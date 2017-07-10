/*
 * MidpointTraversal.h
 *
 *  Created on: 06.07.2017
 *      Author: sascha
 */

#ifndef SRC_PARTICLECONTAINER_LINKEDCELLTRAVERSALS_MIDPOINTTRAVERSAL_H_
#define SRC_PARTICLECONTAINER_LINKEDCELLTRAVERSALS_MIDPOINTTRAVERSAL_H_

#include "particleContainer/LinkedCellTraversals/CellPairTraversals.h"
#include <tuple>

struct MidpointTraversalData : CellPairTraversalData {
};


template <class CellTemplate>
class MidpointTraversal : public CellPairTraversals<CellTemplate>{
public:
	MidpointTraversal(std::vector<CellTemplate>& cells,	const std::array<unsigned long, 3>& dims):
	CellPairTraversals<CellTemplate>(cells, dims) {
		fillArrays();
		computeOffsets();
	}
	virtual ~MidpointTraversal() {}

	/**
     * Reset all necessary data without reallocation.
     */
	virtual void rebuild(std::vector<CellTemplate> &cells,
		const std::array<unsigned long, 3> &dims, CellPairTraversalData *data) override;

	virtual void traverseCellPairs(CellProcessor& cellProcessor);
	virtual void traverseCellPairsOuter(CellProcessor& cellProcessor);
	virtual void traverseCellPairsInner(CellProcessor& cellProcessor, unsigned stage, unsigned stageCount);

	// Midpoint traversal requires force exchange
	virtual bool requiresForceExchange() const {return true;}

protected:
	virtual void processBaseCell(CellProcessor& cellProcessor, unsigned long cellIndex) const;

	// All pairs that have to be processed when calculating the forces
	std::array<std::pair<unsigned long, unsigned long>, 62> _cellPairOffsets;

private:
	void computeOffsets();
	void fillArrays();

	void pairCells(std::tuple<long, long, long>& a,
			std::tuple<long, long, long>& b, int& index, std::array<long, 3>& dims);
	void pairOriginWithForewardNeighbors(int& index, std::array<long, 3>& dims);
	void pairCellsWithPlane(std::tuple<long, long, long>& cc,
			std::tuple<long, long, long>& oc, int& index, std::array<long, 3>& dims);
	void pairCellsWithAdjacentCorners(std::tuple<long, long, long>& ce,
			std::tuple<long, long, long>& oe, int& index, std::array<long, 3>& dims);

	// Offsets of all centers of the surrounding cube. Opposite sides are (i+3)%6
	std::array<std::tuple<long, long, long>, 6> _centers;

	// Offsets of all edges of the surrounding cube. Opposite sides are (i+6)%12
	std::array<std::tuple<long, long, long>, 12> _edges;

	// Offsets of all corners of the surrounding cube. Opposite sides are (i+4)%8
	std::array<std::tuple<long, long, long>, 8> _corners;
};

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::fillArrays() {
	// Centers
	_centers[0] = std::make_tuple( 1, 0, 0);
	_centers[1] = std::make_tuple( 0, 1, 0);
	_centers[2] = std::make_tuple( 0, 0, 1);
	// Mirrored at origin
	_centers[3] = std::make_tuple(-1, 0, 0);
	_centers[4] = std::make_tuple( 0,-1, 0);
	_centers[5] = std::make_tuple( 0, 0,-1);

	// Edges
	_edges[0] = std::make_tuple( 1, 1, 0);
	_edges[1] = std::make_tuple( 1, 0, 1);
	_edges[2] = std::make_tuple( 0, 1, 1);
	_edges[3] = std::make_tuple(-1, 1, 0);
	_edges[4] = std::make_tuple(-1, 0, 1);
	_edges[5] = std::make_tuple( 0,-1, 1);
	// Mirrored at origin
	_edges[6] = std::make_tuple(-1,-1, 0);
	_edges[7] = std::make_tuple(-1, 0,-1);
	_edges[8] = std::make_tuple( 0,-1,-1);
	_edges[9] = std::make_tuple( 1,-1, 0);
	_edges[10] = std::make_tuple( 1, 0,-1);
	_edges[11] = std::make_tuple( 0, 1,-1);

	// Corners
	_corners[0] = std::make_tuple( 1, 1, 1);
	_corners[1] = std::make_tuple( 1, 1,-1);
	_corners[2] = std::make_tuple( 1,-1, 1);
	_corners[3] = std::make_tuple( 1,-1,-1);
	// Mirrored at origin
	_corners[4] = std::make_tuple(-1,-1,-1);
	_corners[5] = std::make_tuple(-1,-1, 1);
	_corners[6] = std::make_tuple(-1, 1,-1);
	_corners[7] = std::make_tuple(-1, 1, 1);
}


template<class CellTemplate>
void MidpointTraversal<CellTemplate>::computeOffsets() {

	using threeDimensionalMapping::threeToOneD;
	using std::make_pair;

	// Dim array is int but we need it as long for some reason (copied from C08BasedTraversal)
	std::array<long, 3> dims;
	for (int d = 0; d < 3; ++d) {
		dims[d] = static_cast<long>(this->_dims[d]);
	}

	// Cell index of origin
	long int o = threeToOneD(0l, 0l, 0l, dims);

	// Next free index of the cellPairOffsets array
	int index = 0;

	// ----------------------------------------------------
	// Opposite cells with midpoint in origin
	// Opposite cells are set up to have index ((i+(n/2)) % n)
	// ----------------------------------------------------

	// process only half of the centers to get no pair twice
	for(int i=0; i<3; ++i){ // centers
		int j = (i+3)%6;
		pairCells(_centers[i], _centers[j], index, dims);
	}
	// process only half of the edges to get no pair twice
	for(int i=0; i<6; ++i){ // edges
		int j = (i+6)%12;
		pairCells(_edges[i], _edges[j], index, dims);
	}
	// process only half of the corners to get no pair twice
	for(int i=0; i<4; ++i){ // corners
		int j = (i+4)%8;
		pairCells(_corners[i], _corners[j], index, dims);
	}

	// ----------------------------------------------------
	// Forward neighbors of origin (similar to half shell)
	// ----------------------------------------------------

	pairOriginWithForewardNeighbors(index, dims);


	// ----------------------------------------------------
	// 'Cone' for half the centers
	// ----------------------------------------------------
	for(int i=0; i<3; ++i){
		auto oc = _centers[(i+3)%6]; // opposite center
		auto cc = _centers[i]; // current center

		// Create pairs for each pair cc <--> oc + offset where offset is not in the direction of [CC Origin OC]
		// Therefore with every cell (except OC) in the plane with normal vector [CC Origin OC] that contains OC.
		pairCellsWithPlane(cc, oc, index, dims);
	}

	// ----------------------------------------------------
	// 'Cone' for half the edges
	// ----------------------------------------------------
	for(int i=0; i<6; ++i){
		auto oe = _edges[(i+6)%12]; // opposite edge
		auto ce = _edges[i]; // current edge

		// Create pairs for each pair ce <--> (corners adjacent to oe)
		pairCellsWithAdjacentCorners(ce, oe, index, dims);

	}

	// We need exactly 62 cell offset pairs
	mardyn_assert(index == 62);

}


template<class CellTemplate>
void MidpointTraversal<CellTemplate>::pairCells(std::tuple<long, long, long>& a,
		std::tuple<long, long, long>& b, int& index, std::array<long, 3>& dims){

	using threeDimensionalMapping::threeToOneD;

	auto ax = std::get<0>(a);
	auto ay = std::get<1>(a);
	auto az = std::get<2>(a);

	auto bx = std::get<0>(b);
	auto by = std::get<1>(b);
	auto bz = std::get<2>(b);

	mardyn_assert((abs(ax) <= 1) && (abs(ay) <= 1) && (abs(az) <= 1));
	mardyn_assert((abs(bx) <= 1) && (abs(by) <= 1) && (abs(bz) <= 1));

	// convert 3d index to 1d
	auto aIndex = threeToOneD(ax, ay, az, dims);
	auto bIndex = threeToOneD(bx, by, bz, dims);

	// store offset pair
	_cellPairOffsets[index] = std::make_pair(aIndex, bIndex);
	// increment index for the next pair
	++index;
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::pairOriginWithForewardNeighbors(int& index, std::array<long, 3>& dims){
	// TODO: ____Implement
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::pairCellsWithPlane(std::tuple<long, long, long>& cc,
		std::tuple<long, long, long>& oc, int& index, std::array<long, 3>& dims){
	// Pairs the current cell (cc) with every cell next to oc except the origin.
	for(int i=-1; i<=1; ++i){
		for(int j=-1; j<=1; ++j){

			// Skip pair cc <--> oc
			if(i==0 && j==0) continue;

			std::tuple<long,long,long> cell;

			if(std::get<0>(oc) == 0){
				if(std::get<1>(oc) == 0){ // x and y are 0
					// Center in +-z direction -> ring around oc in xy plane
					cell = std::make_tuple(i, j, std::get<2>(oc));
				} else { // x and z are 0
					// Center in +-y direction -> ring around oc in xz plane
					cell = std::make_tuple(i, std::get<1>(oc), j);
				}
			} else { // y and z are 0
				// Center in +-x direction -> ring around oc in yz plane
				cell = std::make_tuple(std::get<0>(oc), i, j);
			}

			pairCells(cc, cell, index, dims);
		}
	}
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::pairCellsWithAdjacentCorners(std::tuple<long, long, long>& ce,
		std::tuple<long, long, long>& oe, int& index, std::array<long, 3>& dims){
	// Pairs the current edge cell (ce) with both adjacent corners to oe
	for(int i=-1; i<=1; i+=2){ // i=-1 and i=1

			std::tuple<long,long,long> cell;

			if(std::get<0>(oe) == 0) { // x = 0
				// Corners are oe with x=+-1
				cell = std::make_tuple(i, std::get<1>(oe), std::get<2>(oe));
			} else if(std::get<1>(oe) == 0) { // y = 0
				// Corners are oe with y=+-1
				cell = std::make_tuple(std::get<0>(oe), i, std::get<2>(oe));
			} else { // z = 0
				// Corners are oe with z=+-1
				cell = std::make_tuple(std::get<0>(oe), std::get<1>(oe), i);
			}

			pairCells(ce, cell, index, dims);
	}
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::rebuild(std::vector<CellTemplate> &cells,
		const std::array<unsigned long, 3> &dims, CellPairTraversalData *data){
	//TODO ____ Implement
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::traverseCellPairs(CellProcessor& cellProcessor){
	//TODO ____ Implement
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::traverseCellPairsOuter(CellProcessor& cellProcessor){
	//TODO ____ Implement
}

template<class CellTemplate>
void MidpointTraversal<CellTemplate>::traverseCellPairsInner(CellProcessor& cellProcessor, unsigned stage, unsigned stageCount){
	//TODO ____ Implement
}


template<class CellTemplate>
void MidpointTraversal<CellTemplate>::processBaseCell(CellProcessor& cellProcessor, unsigned long cellIndex) const{
	//TODO ____ Implement
}

#endif /* SRC_PARTICLECONTAINER_LINKEDCELLTRAVERSALS_MIDPOINTTRAVERSAL_H_ */
