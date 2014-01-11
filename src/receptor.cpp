#include <boost/filesystem/fstream.hpp>
#include "scoring_function.hpp"
#include "receptor.hpp"

receptor::receptor(const path& p, const box& b) : partitions(b.num_partitions)
{
	// Initialize necessary variables for constructing a receptor.
	atoms.reserve(5000); // A receptor typically consists of <= 5,000 atoms.

	// Initialize helper variables for parsing.
	string residue = "XXXX"; // Current residue sequence, used to track residue change, initialized to a dummy value.
	size_t residue_start; // The starting atom of the current residue.
	size_t num_lines = 0; // Used to track line number for reporting parsing errors, if any.
	string line;
	line.reserve(79); // According to PDBQT specification, the last item AutoDock atom type locates at 1-based [78, 79].

	// Parse ATOM/HETATM.
	boost::filesystem::ifstream in(p); // Parsing starts. Open the p stream as late as possible.
	while (getline(in, line))
	{
		++num_lines;
		const string record = line.substr(0, 6);
		if (record == "ATOM  " || record == "HETATM")
		{
			// Parse the residue sequence located at 1-based [23, 26].
			if ((line[25] != residue[3]) || (line[24] != residue[2]) || (line[23] != residue[1]) || (line[22] != residue[0])) // This line is the start of a new residue.
			{
				residue[3] = line[25];
				residue[2] = line[24];
				residue[1] = line[23];
				residue[0] = line[22];
				residue_start = atoms.size();
			}

			// Parse and validate AutoDock4 atom type.
			const string ad_type_string = line.substr(77, isspace(line[78]) ? 1 : 2);
			const size_t ad = parse_ad_type_string(ad_type_string);
			if (ad == AD_TYPE_SIZE) continue;

			// Skip non-polar hydrogens.
			if (ad == AD_TYPE_H) continue;

			// Parse the Cartesian coordinate.
			atom a(stoul(line.substr(6, 5)), line.substr(12, 4), vec3(stod(line.substr(30, 8)), stod(line.substr(38, 8)), stod(line.substr(46, 8))), ad);

			// For a polar hydrogen, the bonded hetero atom must be a hydrogen bond donor.
			if (ad == AD_TYPE_HD)
			{
				for (size_t i = atoms.size(); i > residue_start;)
				{
					atom& b = atoms[--i];
					if (b.is_hetero() && b.is_neighbor(a))
					{
						b.donorize();
						break;
					}
				}
			}
			else if (a.is_hetero()) // It is a hetero atom.
			{
				for (size_t i = atoms.size(); i > residue_start;)
				{
					atom& b = atoms[--i];
					if (!b.is_hetero() && b.is_neighbor(a))
					{
						b.dehydrophobicize();
					}
				}
			}
			else // It is a carbon atom.
			{
				for (size_t i = atoms.size(); i > residue_start;)
				{
					const atom& b = atoms[--i];
					if (b.is_hetero() && b.is_neighbor(a))
					{
						a.dehydrophobicize();
						break;
					}
				}
			}
			atoms.push_back(a);
		}
		else if (record == "TER   ")
		{
			residue = "XXXX";
		}
	}
	in.close(); // Parsing finishes. Close the p stream as soon as possible.

	// Find all the heavy receptor atoms that are within 8A of the box.
	vector<size_t> receptor_atoms_within_cutoff;
	receptor_atoms_within_cutoff.reserve(atoms.size());
	const size_t num_rec_atoms = atoms.size();
	for (size_t i = 0; i < num_rec_atoms; ++i)
	{
		const atom& a = atoms[i];
		if (b.project_distance_sqr(a.coordinate) < scoring_function::Cutoff_Sqr)
		{
			receptor_atoms_within_cutoff.push_back(i);
		}
	}
	const size_t num_receptor_atoms_within_cutoff = receptor_atoms_within_cutoff.size();

	// Allocate each nearby receptor atom to its corresponding partition.
	for (size_t x = 0; x < b.num_partitions[0]; ++x)
	for (size_t y = 0; y < b.num_partitions[1]; ++y)
	for (size_t z = 0; z < b.num_partitions[2]; ++z)
	{
		vector<size_t>& par = partitions(x, y, z);
		par.reserve(num_receptor_atoms_within_cutoff);
		const array<size_t, 3> index1 = {{ x,     y,     z     }};
		const array<size_t, 3> index2 = {{ x + 1, y + 1, z + 1 }};
		const vec3 corner1 = b.partition_corner1(index1);
		const vec3 corner2 = b.partition_corner1(index2);
		for (size_t l = 0; l < num_receptor_atoms_within_cutoff; ++l)
		{
			const size_t i = receptor_atoms_within_cutoff[l];
			const atom& a = atoms[i];
			const fl proj_dist_sqr = b.project_distance_sqr(corner1, corner2, a.coordinate);
			if (proj_dist_sqr < scoring_function::Cutoff_Sqr)
			{
				par.push_back(i);
			}
		}
	}
}
