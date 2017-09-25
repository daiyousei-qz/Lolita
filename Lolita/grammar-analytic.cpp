#include "grammar-analytic.h"

namespace lolita
{
	void TryInsertFIRST(const std::vector<TermSet>& lookup, TermSet& output, Symbol s)
	{
		if (s.IsTerminal())
		{
			output.insert(s.AsTerminal());
		}
		else // non-term
		{
			const auto& source_set = lookup[s.Id()];
			output.insert(source_set.begin(), source_set.end());
		}
	}

	void TryInsertFOLLOW(const std::vector<TermSet>& lookup, TermSet& output, NonTerminal nonterm)
	{
		const auto& source_set = lookup[nonterm.Id()];
		output.insert(source_set.begin(), source_set.end());
	}

	void LoopWhileGrowing(std::vector<TermSet>& monitor, std::function<void()> func)
	{
		auto last_total_term = 0u;
		while (true)
		{
			// callback
			func();

			// if total number of terms in FIRST sets does't grow, stop iteration
			auto total_term = std::accumulate(monitor.begin(), monitor.end(), 0u,
				[](auto acc, const auto& s) {return acc + s.size(); });
			if (total_term > last_total_term)
			{
				last_total_term = total_term;
			}
			else
			{
				break;
			}
		}

		// TODO: take epsilon_deriv and ending_symbol into account when to halt the loop(IMPORTANT)
		// TODO: remove this workaround!!!
		func();
	}

	PredictiveSet PredictiveSet::Create(const Grammar& g)
	{
		const auto nonterm_count = g.NonTerminalCount();

		// compute FIRST sets and recognize epsilon-derivations
		//
		std::vector<bool> produce_epsilon(nonterm_count, false);
		std::vector<TermSet> first(nonterm_count);

		LoopWhileGrowing(first, [&]() {
			for (const auto& production : g.Productions())
			{
				const auto lhs_id = production.Left().Id();
				TermSet& target_set = first[lhs_id];

				bool early_break = false;
				for (auto rhs_elem : production.Right())
				{
					TryInsertFIRST(first, target_set, rhs_elem);

					// break on first non-epsilon-derivable symbol
					if (!(rhs_elem.IsNonTerminal() && produce_epsilon[rhs_elem.Id()]))
					{
						early_break = true;
						break;
					}
				}

				if (!early_break)
				{
					// as the iteration didn't break early
					// it may only produce epsilon symbols
					// NOTE empty production also indicates epsilon
					produce_epsilon[lhs_id] = true;
				}
			}
		});

		// compute FOLLOW sets
		//
		std::vector<bool> ending_symbol(nonterm_count, false); // TODO: add eof detection
		ending_symbol.back() = true; // last non-term, i.e. _ROOT always accepts eof

		std::vector<TermSet> follow(nonterm_count);

		LoopWhileGrowing(follow, [&]() {
			for (const auto& production : g.Productions())
			{
				const auto& lhs = production.Left();
				const auto& rhs = production.Right();

				// empty production provides no information but leads to an error
				if (rhs.empty()) continue;

				bool epsilon_path = true;
				for (auto iter = rhs.rbegin(); iter != rhs.rend(); ++iter)
				{
					auto lookahead_iter = std::next(iter);
					if (lookahead_iter != rhs.rend() && lookahead_iter->IsNonTerminal())
					{
						TryInsertFIRST(first, follow[lookahead_iter->Id()], *iter);
					}

					if (epsilon_path)
					{
						if (iter->IsNonTerminal())
						{
							epsilon_path = produce_epsilon[iter->Id()];

							if (ending_symbol[lhs.Id()])
							{
								ending_symbol[iter->Id()] = true;
							}

							TryInsertFOLLOW(follow, follow[iter->Id()], production.Left());
						}
						else
						{
							epsilon_path = false;
						}
					}
				}
			}
		});

		return PredictiveSet{ first,follow, produce_epsilon, ending_symbol };
	}
}