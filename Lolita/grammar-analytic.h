#pragma once
#include "symbol.h"
#include "grammar.h"

namespace lolita
{
	class PredictiveSet
	{
	private:
		friend PredictiveSet GeneratePredictiveSet(const Grammar& g);
			
		PredictiveSet(std::vector<TermSet> first, std::vector<TermSet> follow,
					  std::vector<bool> produce_e, std::vector<bool> ending)
			: first_sets_(std::move(first))
			, follow_sets_(std::move(follow))
			, produce_epsilon_(std::move(produce_e))
			, ending_symbol_(std::move(ending)) { }

	public:
		const TermSet& First(NonTerminal s) const
		{
			return first_sets_[s.Id()];
		}
		const TermSet& Follow(NonTerminal s) const
		{
			return follow_sets_[s.Id()];
		}
		bool ProduceEpsilon(NonTerminal s) const
		{
			return produce_epsilon_[s.Id()];
		}
		bool EndingSymbol(NonTerminal s) const
		{
			return ending_symbol_[s.Id()];
		}

	private:
		std::vector<bool> produce_epsilon_;
		std::vector<bool> ending_symbol_;
		std::vector<TermSet> first_sets_;
		std::vector<TermSet> follow_sets_;
	};

	PredictiveSet GeneratePredictiveSet(const Grammar& g);
}