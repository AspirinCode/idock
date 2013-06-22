#include <boost/random.hpp>
#include "monte_carlo_task.hpp"

int monte_carlo_task(result& r, const ligand& lig, const size_t seed, const scoring_function& sf, const receptor& rec)
{
	// Define constants.
	const size_t num_alphas = 5; // Number of alpha values for determining step size in BFGS
	const size_t num_mc_iterations = 50; // The number of iterations.
	const size_t num_entities  = 2 + lig.num_active_torsions; // Number of entities to mutate.
	const size_t num_variables = 6 + lig.num_active_torsions; // Number of variables to optimize.
	const float e_upper_bound = 40.0f * lig.num_heavy_atoms; // A conformation will be droped if its free energy is not better than e_upper_bound.
	const float pi = 3.1415926535897932f; ///< Pi.

	// On Linux, the std namespace contains std::mt19937 and std::normal_distribution.
	// In order to avoid ambiguity, use the complete scope.
	using boost::random::mt19937_64;
	using boost::random::variate_generator;
	using boost::random::uniform_real_distribution;
	using boost::random::uniform_int_distribution;
	using boost::random::normal_distribution;
	mt19937_64 eng(seed);
	uniform_real_distribution<float> uniform_11(-1.0f, 1.0f);

	// Generate an initial random conformation c0, and evaluate it.
	conformation c0(lig.num_active_torsions);
	float e0, f0;
	vector<float> g0(6 + lig.num_active_torsions);
	// Randomize conformation c0.
	c0.position = rec.center + (uniform_11(eng) * rec.span);
	c0.orientation = qtn4(uniform_11(eng), uniform_11(eng), uniform_11(eng), uniform_11(eng)).normalize();
	for (size_t i = 0; i < lig.num_active_torsions; ++i)
	{
		c0.torsions[i] = uniform_11(eng);
	}
	lig.evaluate(c0, sf, rec, e_upper_bound, e0, f0, g0);
	r = lig.compose_result(e0, c0);

	// Initialize necessary variables for BFGS.
	conformation c1(lig.num_active_torsions), c2(lig.num_active_torsions); // c2 = c1 + ap.
	float e1, f1, e2, f2;
	vector<float> g1(6 + lig.num_active_torsions), g2(6 + lig.num_active_torsions);
	vector<float> p(6 + lig.num_active_torsions); // Descent direction.
	float alpha, pg1, pg2; // pg1 = p * g1. pg2 = p * g2.
	size_t num_alpha_trials;

	// Initialize the inverse Hessian matrix to identity matrix.
	// An easier option that works fine in practice is to use a scalar multiple of the identity matrix,
	// where the scaling factor is chosen to be in the range of the eigenvalues of the true Hessian.
	// See N&R for a recipe to find this initializer.
	triangular_matrix<float> identity_hessian(num_variables, 0); // Symmetric triangular matrix.
	for (size_t i = 0; i < num_variables; ++i)
		identity_hessian[triangular_matrix_restrictive_index(i, i)] = 1;

	// Initialize necessary variables for updating the Hessian matrix h.
	triangular_matrix<float> h(identity_hessian);
	vector<float> y(6 + lig.num_active_torsions); // y = g2 - g1.
	vector<float> mhy(6 + lig.num_active_torsions); // mhy = -h * y.
	float yhy, yp, ryp, pco;

	for (size_t mc_i = 0; mc_i < num_mc_iterations; ++mc_i)
	{
		// Make a copy, so the previous conformation is retained.
		c1 = c0;
//		c1.position += vec3(1, 1, 1);
		c1.position += vec3(uniform_11(eng), uniform_11(eng), uniform_11(eng));
		lig.evaluate(c1, sf, rec, e_upper_bound, e1, f1, g1);

		// Initialize the Hessian matrix to identity.
		h = identity_hessian;

		// Given the mutated conformation c1, use BFGS to find a local minimum.
		// The conformation of the local minimum is saved to c2, and its derivative is saved to g2.
		// http://en.wikipedia.org/wiki/BFGS_method
		// http://en.wikipedia.org/wiki/Quasi-Newton_method
		// The loop breaks when an appropriate alpha cannot be found.
		while (true)
		{
			// Calculate p = -h*g, where p is for descent direction, h for Hessian, and g for gradient.
			for (size_t i = 0; i < num_variables; ++i)
			{
				float sum = 0.0f;
				for (size_t j = 0; j < num_variables; ++j)
					sum += h[triangular_matrix_permissive_index(i, j)] * g1[j];
				p[i] = -sum;
			}

			// Calculate pg = p*g = -h*g^2 < 0
			pg1 = 0;
			for (size_t i = 0; i < num_variables; ++i)
				pg1 += p[i] * g1[i];

			// Perform a line search to find an appropriate alpha.
			// Try different alpha values for num_alphas times.
			// alpha starts with 1, and shrinks to alpha_factor of itself iteration by iteration.
			alpha = 1.0;
			for (num_alpha_trials = 0; num_alpha_trials < num_alphas; ++num_alpha_trials)
			{
				// Calculate c2 = c1 + ap.
				c2.position = c1.position + alpha * vec3(p[0], p[1], p[2]);
				assert(c1.orientation.is_normalized());
				c2.orientation = qtn4(alpha * vec3(p[3], p[4], p[5])) * c1.orientation;
				assert(c2.orientation.is_normalized());
				for (size_t i = 0; i < lig.num_active_torsions; ++i)
				{
					c2.torsions[i] = c1.torsions[i] + alpha * p[6 + i];
				}

				// Evaluate c2, subject to Wolfe conditions http://en.wikipedia.org/wiki/Wolfe_conditions
				// 1) Armijo rule ensures that the step length alpha decreases f sufficiently.
				// 2) The curvature condition ensures that the slope has been reduced sufficiently.
				if (lig.evaluate(c2, sf, rec, e1 + 0.0001f * alpha * pg1, e2, f2, g2))
				{
					pg2 = 0;
					for (size_t i = 0; i < num_variables; ++i)
						pg2 += p[i] * g2[i];
					if (pg2 >= 0.9f * pg1)
						break; // An appropriate alpha is found.
				}

				alpha *= 0.1f;
			}

			// If an appropriate alpha cannot be found, exit the BFGS loop.
			if (num_alpha_trials == num_alphas) break;

			// Update Hessian matrix h.
			for (size_t i = 0; i < num_variables; ++i) // Calculate y = g2 - g1.
				y[i] = g2[i] - g1[i];
			for (size_t i = 0; i < num_variables; ++i) // Calculate mhy = -h * y.
			{
				float sum = 0.0f;
				for (size_t j = 0; j < num_variables; ++j)
					sum += h[triangular_matrix_permissive_index(i, j)] * y[j];
				mhy[i] = -sum;
			}
			yhy = 0;
			for (size_t i = 0; i < num_variables; ++i) // Calculate yhy = -y * mhy = -y * (-hy).
				yhy -= y[i] * mhy[i];
			yp = 0;
			for (size_t i = 0; i < num_variables; ++i) // Calculate yp = y * p.
				yp += y[i] * p[i];
			ryp = 1 / yp;
			pco = ryp * (ryp * yhy + alpha);
			for (size_t i = 0; i < num_variables; ++i)
			for (size_t j = i; j < num_variables; ++j) // includes i
			{
				h[triangular_matrix_restrictive_index(i, j)] += ryp * (mhy[i] * p[j] + mhy[j] * p[i]) + pco * p[i] * p[j];
			}

			// Move to the next iteration.
			c1 = c2;
			e1 = e2;
			f1 = f2;
			g1 = g2;
		}

		// Accept c1 according to Metropolis criteria.
		if (e1 < e0)
		{
			r = lig.compose_result(e1, c1);

			// Save c1 into c0.
			c0 = c1;
			e0 = e1;
		}
	}
	return 0;
}
