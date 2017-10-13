#include "grammar-analytic.h"

using namespace std;

namespace eds::loli
{
	// try to insert FIRST SET of s into output
	static bool TryInsertFIRST(const PredictiveSet& lookup, TermSet& output, Symbol* s)
	{
		// remember output's size
		const auto old_output_sz = output.size();

		// try insert terminals that start s into output
		if (s->IsTerminal())
		{
			output.insert(s->AsTerminal());
		}
		else // non-term
		{
			const auto& source_set = lookup.at(s->AsNonTerminal()).first_set;
			output.insert(source_set.begin(), source_set.end());
		}

		// return if output is changed
		return output.size() != old_output_sz;
	}

	// try to insert FOLLOW SET of s into output
	static bool TryInsertFOLLOW(const PredictiveSet& lookup, TermSet& output, NonTerminal* s)
	{
		// remember output's size
		const auto old_output_sz = output.size();

		// try insert terminals that may follow s into output
		const auto& source_set = lookup.at(s->AsNonTerminal()).follow_set;
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
				auto& target_info = set.at(production->Left());

				bool may_produce_epsilon = true;
				for (const auto rhs_elem : production->Right())
				{
					growing |= TryInsertFIRST(set, target_info.first_set, rhs_elem);

					// break on first non-epsilon-derivable symbol
					if (rhs_elem->IsTerminal() 
						|| !set.at(rhs_elem->AsNonTerminal()).may_produce_epsilon)
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
		set.at(g.RootSymbol()).may_preceed_eof = true;

		// iteratively compute follow set until it's not growing
		for (auto growing = true; growing;)
		{
			growing = false;

			for (const auto& production : g.Productions())
			{
				// shortcuts alias
				const auto& lhs = production->Left();
				const auto& rhs = production->Right();

				// epsilon_path is set false when any non-nullable symbol is encountered
				bool epsilon_path = true;
				for (auto iter = rhs.rbegin(); iter != rhs.rend(); ++iter)
				{
					// process lookahead symbol
					if (const auto la_iter = std::next(iter); la_iter != rhs.rend())
					{
						const auto la_symbol = *la_iter;
						if (la_symbol->IsNonTerminal())
						{
							auto& la_follow = set.at(la_symbol->AsNonTerminal()).follow_set;
							growing |= TryInsertFIRST(set, la_follow, *iter);
						}
					}

					// process current symbol
					if (epsilon_path)
					{
						const auto cur_symbol = *iter;

						if (cur_symbol->IsTerminal())
						{
							epsilon_path = false;
						}
						else
						{
							auto& cur_symbol_info = set.at(cur_symbol->AsNonTerminal());

							if (!cur_symbol_info.may_produce_epsilon)
							{
								epsilon_path = false;
							}

							// propagate may_preceed_eof on epsilon path
							if (!cur_symbol_info.may_preceed_eof
								&& set.at(lhs).may_preceed_eof)
							{
								growing = true;
								cur_symbol_info.may_preceed_eof = true;
							}

							// propagate follow_set on epsilon path
							growing |= TryInsertFOLLOW(set, cur_symbol_info.follow_set, lhs);
						}
					}
				}
			}
		}
	}

	PredictiveSet ComputePredictiveSet(const Grammar& g)
	{
		PredictiveSet result;

		ComputeFIRST(result, g);
		ComputeFOLLOW(result, g);

		return result;
	}
}