#pragma once
#include "grammar.h"
#include "ext\flat-set.hpp"
#include <unordered_map>

namespace eds::loli::parsing
{
	struct PredictiveInfo
	{
		using TermSet = FlatSet<const Terminal*>;

		bool may_produce_epsilon = false;
		bool may_preceed_eof     = false;

		TermSet first_set        = {};
		TermSet follow_set       = {};
	};

	using PredictiveSet = std::unordered_map<const Nonterminal*, PredictiveInfo>;

	PredictiveSet ComputePredictiveSet(const Grammar& g);
}