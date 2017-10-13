#pragma once
#include "symbol.h"
#include "grammar.h"
#include "ext\flat-set.hpp"
#include <unordered_map>

namespace eds::loli
{
	using TermSet = FlatSet<Terminal*>;

	struct PredictiveInfo
	{
		bool may_produce_epsilon = false;
		bool may_preceed_eof = false;

		TermSet first_set = {};
		TermSet follow_set = {};
	};

	using PredictiveSet = std::unordered_map<NonTerminal*, PredictiveInfo>;

	PredictiveSet ComputePredictiveSet(const Grammar& g);
}