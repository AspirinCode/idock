#pragma once
#ifndef IDOCK_RECEPTOR_HPP
#define IDOCK_RECEPTOR_HPP

#include <boost/filesystem/path.hpp>
#include "atom.hpp"
#include "box.hpp"
using namespace boost::filesystem;

/// Represents a receptor.
class receptor
{
public:
	/// Constructs a receptor by parsing a receptor file in pdbqt format.
	/// @exception parsing_error Thrown when an atom type is not recognized.
	explicit receptor(const path& p, const box& b);

	vector<atom> atoms; ///< Receptor atoms.
	vector<vector<size_t>> partitions; ///< Heavy atoms in partitions.
};

#endif
