#include "grammar-analytic.h"
#include "lang-helper.h"

using namespace std;

namespace eds::loli::parsing
{
	using TermSet = PredictiveInfo::TermSet;

	// try to insert FIRST SET of s into output
	static bool TryInsertFIRST(const PredictiveSet& lookup, TermSet& output, Symbol* s)
	{
		// remember output's size
		const auto old_output_sz = output.size();

		// try insert terminals that start s into output
		if (auto term = s->AsTerminal(); term)
		{
			output.insert(term);
		}
		else
		{
			const auto& source_set = lookup.at(s->AsNonterminal()).first_set;
			output.insert(source_set.begin(), source_set.end());
		}

		// return if output is changed
		return output.size() != old_output_sz;
	}

	// try to insert FOLLOW SET of s into output
	static bool TryInsertFOLLOW(const PredictiveSet& lookup, TermSet& output, Nonterminal* s)
	{
		// remember output's size
		const auto old_output_sz = output.size();

		// try insert terminals that may follow s into output
		const auto& source_set = lookup.at(s->AsNonterminal()).follow_set;
		output.insert(source_set.begin(), source_set.end());

		// return if output is changed
		return output.size() != old_output_sz;
	}

	static void ComputeFIRST(PredictiveSet& set, const Grammar& g)
	{
		// iteratively compute first set until it's not growing
		for (auto growing = true; growing;)
		{
			growing = false;

			for (const auto& production : g.Productions())
			{
				auto& target_info = set[production.lhs];

				bool may_produce_epsilon = true;
				for (const auto rhs_elem : production.rhs)
				{
					growing |= TryInsertFIRST(set, target_info.first_set, rhs_elem);

					// break on first non-epsilon-derivable symbol

					if (rhs_elem->AsTerminal() 
						|| !set[rhs_elem->AsNonterminal()].may_produce_epsilon)
					{
						may_produce_epsilon = false;
						break;
					}
				}

				if (may_produce_epsilon && !target_info.may_produce_epsilon)
				{
					// as the iteration didn't break early
					// it may produce epsilon symbol
					// NOTE empty production also indicates epsilon
					growing = true;
					target_info.may_produce_epsilon = true;
				}
			}
		}
	}

	static void ComputeFOLLOW(PredictiveSet& set, const Grammar& g)
	{
		// root symbol is always able to preceed eof
		set[g.RootSymbol()].may_preceed_eof = true;

		// iteratively compute follow set until it's not growing
		for (auto growing = true; growing;)
		{
			growing = false;

			for (const auto& production : g.Productions())
			{
				// shortcuts alias
				const auto& lhs = production.lhs;
				const auto& rhs = production.rhs;

				// epsilon_path is set false when any non-nullable symbol is encountered
				bool epsilon_path = true;
				for (auto iter = rhs.rbegin(); iter != rhs.rend(); ++iter)
				{
					// process lookahead symbol
					if (const auto la_iter = std::next(iter); la_iter != rhs.rend())
					{
						const auto la_symbol = *la_iter;
						if (auto nonterm = la_symbol->AsNonterminal(); nonterm)
						{
							auto& la_follow = set[nonterm].follow_set;
							growing |= TryInsertFIRST(set, la_follow, *iter);
						}
					}

					// process current symbol
					if (epsilon_path)
					{
						const auto cur_symbol = *iter;
						if (auto nonterm = cur_symbol->AsNonterminal(); nonterm)
						{
							auto& cur_symbol_info = set[cur_symbol->AsNonterminal()];

							if (!cur_symbol_info.may_produce_epsilon)
							{
								epsilon_path = false;
							}

							// propagate may_preceed_eof on epsilon path
							if (!cur_symbol_info.may_preceed_eof
								&& set[lhs].may_preceed_eof)
							{
								growing = true;
								cur_symbol_info.may_preceed_eof = true;
							}

							// propagate follow_set on epsilon path
							growing |= TryInsertFOLLOW(set, cur_symbol_info.follow_set, lhs);
						}
						else
						{
							epsilon_path = false;
						}
					}
				}
			}
		}
	}

	PredictiveSet ComputePredictiveSet(const Grammar& g)
	{
		PredictiveSet result;

		for (const auto& nonterm : g.Nonterminals())
		{
			result[&nonterm];
		}

		ComputeFIRST(result, g);
		ComputeFOLLOW(result, g);

		return result;
	}
}