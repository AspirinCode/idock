#include "scoring_function.hpp"

const double scoring_function::Cutoff = static_cast<double>(8);
const double scoring_function::Cutoff_Sqr = Cutoff * Cutoff;
const double scoring_function::Factor = static_cast<double>(256);
const double scoring_function::Factor_Inverse = 1 / Factor;
const size_t scoring_function::Num_Samples = static_cast<size_t>(Factor * Cutoff_Sqr) + 1;

double scoring_function::score(const size_t t1, const size_t t2, const double r)
{
	BOOST_ASSERT(r <= Cutoff_Sqr);

	// Calculate the surface distance d.
	const double d = r - (xs_vdw_radius(t1) + xs_vdw_radius(t2));

	// The scoring function is a weighted sum of 5 terms.
	// The first 3 terms depend on d only, while the latter 2 terms depend on t1, t2 and d.
	return (-0.035579) * exp(-sqr(d * 2))
		+  (-0.005156) * exp(-sqr((d - 3.0) * 0.5))
		+  ( 0.840245) * (d > 0 ? 0.0 : d * d)
		+  (-0.035069) * ((xs_is_hydrophobic(t1) && xs_is_hydrophobic(t2)) ? ((d >= 1.5) ? 0.0 : ((d <= 0.5) ? 1.0 : 1.5 - d)) : 0.0)
		+  (-0.587439) * ((xs_hbond(t1, t2)) ? ((d >= 0) ? 0.0 : ((d <= -0.7) ? 1 : d * (-1.428571))): 0.0);
}

void scoring_function::precalculate(const size_t t1, const size_t t2, const vector<double>& rs)
{
	vector<scoring_function_element>& p = (*this)[triangular_matrix_restrictive_index(t1, t2)];
	BOOST_ASSERT(p.size() == Num_Samples);

	// Calculate the value of scoring function evaluated at (t1, t2, d).
	for (size_t i = 0; i < Num_Samples; ++i)
	{
		p[i].e = score(t1, t2, rs[i]);
	}

	// Calculate the dor of scoring function evaluated at (t1, t2, d).
	for (size_t i = 1; i < Num_Samples - 1; ++i)
	{
		p[i].dor = (p[i + 1].e - p[i].e) / ((rs[i + 1] - rs[i]) * rs[i]);
	}
	p.front().dor = 0;
	p.back().dor = 0;
}

scoring_function_element scoring_function::evaluate(const size_t type_pair_index, const double r2) const
{
	BOOST_ASSERT(r2 <= Cutoff_Sqr);
	return (*this)[type_pair_index][static_cast<size_t>(Factor * r2)];
}
