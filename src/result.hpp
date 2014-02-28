#pragma once
#ifndef IDOCK_RESULT_HPP
#define IDOCK_RESULT_HPP

#include <boost/ptr_container/ptr_vector.hpp>
#include "array.hpp"
using boost::ptr_vector;

//! Represents a ligand conformation.
class conformation
{
public:
	array<double, 3> position; //!< Ligand origin coordinate.
	array<double, 4> orientation; //!< Ligand orientation.
	vector<double> torsions; //!< Ligand torsions.

	//! Constructs an initial conformation.
	explicit conformation(const size_t num_active_torsions) : position(zero3), orientation(qtn4id), torsions(num_active_torsions, 0) {}
};

//! Represents a transition from one conformation to another.
class change : public vector<double>
{
public:
	//! Constructs a zero change.
	explicit change(const size_t num_active_torsions) : vector<double>(6 + num_active_torsions, 0) {}
};

//! Represents a result found by BFGS local optimization for later clustering.
class result
{
public:
	double e; //!< Free energy.
	double f; //!< Inter-molecular free energy.
	double e_nd; //!< Normalized free energy, only for output purpose.
	vector<array<double, 3>> heavy_atoms; //!< Heavy atom coordinates.
	vector<array<double, 3>> hydrogens; //!< Hydrogen atom coordinates.

	//! Constructs a result from free energy e, force f, heavy atom coordinates and hydrogen atom coordinates.
	explicit result(const double e, const double f, vector<array<double, 3>>&& heavy_atoms_, vector<array<double, 3>>&& hydrogens_) : e(e), f(f), heavy_atoms(static_cast<vector<array<double, 3>>&&>(heavy_atoms_)), hydrogens(static_cast<vector<array<double, 3>>&&>(hydrogens_)) {}

	//! Move constructor.
	result(result&& r) : e(r.e), f(r.f), heavy_atoms(static_cast<vector<array<double, 3>>&&>(r.heavy_atoms)), hydrogens(static_cast<vector<array<double, 3>>&&>(r.hydrogens)) {}

	//! For sorting ptr_vector<result>.
	bool operator<(const result& r) const
	{
		return e < r.e;
	}
};

// TODO: Do not inline large functions.
// TODO: Consider using double linked list std::list<> to store results because of frequent insertions and deletions.
//! Clusters a result into an existing result set with a minimum RMSD requirement.
inline void add_to_result_container(ptr_vector<result>& results, result&& r, const double required_square_error)
{
	// If this is the first result, simply save it.
	if (results.empty())
	{
		results.push_back(new result(static_cast<result&&>(r)));
		return;
	}

	// If the container is not empty, find in a coordinate that is closest to the given newly found r.coordinate.
	size_t index = 0;
	double best_square_error = distance_sqr(r.heavy_atoms, results.front().heavy_atoms);
	for (size_t i = 1; i < results.size(); ++i)
	{
		const double this_square_error = distance_sqr(r.heavy_atoms, results[i].heavy_atoms);
		if (this_square_error < best_square_error)
		{
			index = i;
			best_square_error = this_square_error;
		}
	}

	if (best_square_error < required_square_error) // The result r is very close to results[index].
	{
		if (r.e < results[index].e) // r is better than results[index], so substitute r for results[index].
		{
			results.replace(index, new result(static_cast<result&&>(r)));
		}
	}
	else // Cannot find in results a result that is similar to r.
	{
		if (results.size() < results.capacity())
		{
			results.push_back(new result(static_cast<result&&>(r)));
		}
		else // Now the container is full.
		{
			if (r.e < results.back().e) // If r is better than the worst one, then replace it.
			{
				results.replace(results.size() - 1, new result(static_cast<result&&>(r)));
			}
		}
	}
	results.sort();
}

#endif
