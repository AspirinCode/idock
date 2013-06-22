#pragma once
#ifndef IDOCK_MONTE_CARLO_TASK_HPP
#define IDOCK_MONTE_CARLO_TASK_HPP

#include "ligand.hpp"

/// Task for running Monte Carlo Simulated Annealing algorithm to find local minimums of the scoring function.
int monte_carlo_task(result& r, const ligand& lig, const size_t seed, const scoring_function& sf, const receptor& rec);

#endif
