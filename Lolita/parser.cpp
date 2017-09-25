#include "parser.h"
#include "grammar.h"
#include "grammar-analytic.h"
#include <set>
#include <map>
#include <unordered_map>
#include <tuple>

using namespace std;

namespace lolita
{
	// Implementation Of Parser
	//
	void Parser::Print() const
	{
		const auto kColumnFormatter = "  %-12s|";

		printf(kColumnFormatter, "ACTION");
		grammar_->EnumerateTerminals([&](Terminal t) {

			printf(kColumnFormatter, grammar_->LookupName(t).c_str());
		});
		printf(kColumnFormatter, "_EOF");
		printf("\n");

		for (auto i = 0u; i < action_table_.size(); ++i)
		{
			if (i % ActionTableWidth() == 0)
			{
				if (i > 0)
				{
					printf("\n");
				}

				printf(kColumnFormatter, std::to_string(i / ActionTableWidth()).c_str());
			}

			std::visit([&](auto action) {
				using T = std::decay_t<decltype(action)>;
				if constexpr (std::is_same_v<T, ActionShift>)
				{
					printf(kColumnFormatter, std::to_string(action.target_state).c_str());
				}
				else if constexpr (std::is_same_v<T, ActionReduce>)
				{
					printf(kColumnFormatter, TextifyProduction(*grammar_, *action.production).c_str());
				}
				else if constexpr (std::is_same_v<T, ActionAccept>)
				{
					printf(kColumnFormatter, "ACCEPT");
				}
				else if constexpr (std::is_same_v<T, ActionError>)
				{
					printf(kColumnFormatter, "");
				}

			}, action_table_[i]);
		}
		printf("\n\n");

		printf(kColumnFormatter, "GOTO");
		grammar_->EnumerateNonTerminals([&](NonTerminal t) {

			printf(kColumnFormatter, grammar_->LookupName(t).c_str());
		});
		printf("\n");

		for (auto i = 0u; i < goto_table_.size(); ++i)
		{
			if (i % GotoTableWidth() == 0)
			{
				if (i > 0)
				{
					printf("\n");
				}

				printf(kColumnFormatter, std::to_string(i / GotoTableWidth()).c_str());
			}

			auto entry = goto_table_[i];
			if (entry)
			{
				printf(kColumnFormatter, std::to_string(*entry).c_str());
			}
			else
			{
				printf(kColumnFormatter, "");
			}
		}
		printf("\n");

	}

	bool Parser::FeedInternal(ParsingStack& ctx, unsigned tok)
	{
		bool accepted = false;
		auto hungry = true;
		while (hungry)
		{
			auto action_ind = LookupCurrentState(ctx) * ActionTableWidth() + tok;
			auto next_action = action_table_[action_ind];

			std::visit([&](auto action) {

				using T = std::decay_t<decltype(action)>;
				if constexpr (std::is_same_v<T, ActionShift>)
				{
					ctx.push(StateSlot{ grammar_->GetTerminal(tok), action.target_state });
					hungry = false;
				}
				else if constexpr (std::is_same_v<T, ActionReduce>)
				{
					auto production = action.production;

					for (auto i = 0u; i < production->Right().size(); ++i)
						ctx.pop();

					auto src_state = LookupCurrentState(ctx);
					auto goto_ind = src_state*GotoTableWidth() + production->Left().Id();
					auto target_state = goto_table_[goto_ind].value();

					ctx.push(StateSlot{ production->Left(), target_state });
				}
				else if constexpr (std::is_same_v<T, ActionAccept>)
				{
					accepted = true;
					hungry = false;
				}
				else if constexpr (std::is_same_v<T, ActionError>)
				{
					throw 0; // throw Parsing Error
				}

			}, next_action);
		}

		return accepted;
	}

	StateId Parser::LookupCurrentState(const ParsingStack& ctx)
	{
		if (ctx.empty())
		{
			// TODO: is initial state 0?
			// return initial state
			return 0;
		}
		else
		{
			return ctx.top().state;
		}
	}

	//
	//

	void EnumerateClosure(const Grammar& grammar, const FlatSet<Item>& state, std::function<void(Item)> callback)
	{
		// assme state given contains only kernel items

		std::vector<bool> added(grammar.NonTerminalCount(), false);
		added[grammar.RootSymbol().Id()] = true; // exclude ROOT as it's kernel as well

		std::vector<NonTerminal> queue;
		for (const auto& item : state)
		{
			callback(item);

			// skip Item A -> \alpha.
			if (item.pos == item.production->Right().size())
				continue;

			// for each A -> \alpha . B \gamma, queue unvisited B for further expansion
			auto symbol = item.production->Right()[item.pos];
			if (symbol.IsNonTerminal() && !added[symbol.Id()])
			{
				queue.push_back(symbol.AsNonTerminal());
				added[symbol.Id()] = true;
			}
		}

		while (!queue.empty())
		{
			auto symbol = queue.back();
			queue.pop_back();

			grammar.EnumerateProduction(symbol, [&](const auto& production) {
				
				callback(Item{ &production, 0 });

				const auto& rhs = production.Right();
				if (!rhs.empty())
				{

					auto candidate = rhs.front();
					if (candidate.IsNonTerminal() && !added[candidate.Id()])
					{
						queue.push_back(candidate.AsNonTerminal());
						added[candidate.Id()] = true;
					}
				}
			});
		}
	}

	void EnumerateGoto(const Grammar& grammar, const FlatSet<Item>& state, std::function<void(Item)> callback, Symbol s)
	{
		EnumerateClosure(grammar, state, [&](Item item) {

			// ignore Item A -> \alpha .
			if (item.pos == item.production->Right().size())
				return;

			// for Item A -> \alpha . B \beta where B == s
			// advance the cursor
			if (item.production->Right()[item.pos] == s)
			{
				callback(Item{ item.production, item.pos + 1 });
			}
		});
	}

	FlatSet<Item> EvaluateGoto(const Grammar& grammar, const FlatSet<Item>& state, Symbol s)
	{
		FlatSet<Item> new_state;
		EnumerateGoto(grammar, state, [&](Item item) { new_state.insert(item); }, s);

		return new_state;
	}

	using LRState = FlatSet<Item>;

	struct LRAutomaton
	{
		std::vector<LRState> states;
		std::vector<StateIdOpt> term_jumptable;
		std::vector<StateIdOpt> nonterm_jumptable;
	};

	LRAutomaton GenerateLRAutomaton(const Grammar& grammar)
	{
		auto initial_state = FlatSet<Item>{ Item{ &grammar.RootProduction(), 0 } };

		std::vector<LRState> states{ initial_state };
		std::vector<std::tuple<StateId, StateId, Symbol>> edges;

		// calculate states and edges
		for (auto src_index = 0u; src_index < states.size(); ++src_index)
		{
			grammar.EnumerateSymbol([&](Symbol s) {
				
				FlatSet<Item> new_state = EvaluateGoto(grammar, states[src_index], s);

				if (new_state.empty()) return;
				
				auto iter = std::find(states.begin(), states.end(), new_state);
				if (iter == states.end())
				{
					iter = states.insert(states.end(), new_state);
				}

				auto target_index = std::distance(states.begin(), iter);
				edges.emplace_back(src_index, target_index, s);
			});
		}

		// convert edges into transition tables
		const auto state_count = states.size();
		const auto term_count = grammar.TerminalCount();
		const auto nonterm_count = grammar.NonTerminalCount();
		std::vector<StateIdOpt> term_jumptable(state_count * term_count, nullopt);
		std::vector<StateIdOpt> nonterm_jumptable(state_count * nonterm_count, nullopt);
		for (auto item : edges)
		{
			const auto[src_ind, target_ind, symbol] = item;

			if (symbol.IsTerminal())
			{
				term_jumptable[src_ind * term_count + symbol.Id()] = target_ind;
			}
			else
			{
				nonterm_jumptable[src_ind * nonterm_count + symbol.Id()] = target_ind;
			}
		}
		
		// construct automaton
		return LRAutomaton{ states, term_jumptable, nonterm_jumptable };
	}

	StateIdOpt LookupTargetState(const Grammar& g, const LRAutomaton& atm, StateId src, Symbol s)
	{
		if (s.IsTerminal())
		{
			return atm.term_jumptable[src * g.TerminalCount() + s.Id()];
		}
		else
		{
			return atm.nonterm_jumptable[src * g.NonTerminalCount() + s.Id()];
		}
	}

	void CopyTransitions(ParserBuilder& builder, const Grammar& g, const LRAutomaton& atm)
	{
		auto state_count = atm.states.size();
		auto term_count = g.TerminalCount();;
		auto nonterm_count = g.NonTerminalCount();

		// construct goto table
		for (auto i = 0u; i < state_count; ++i)
		{
			for (auto j = 0u; j < nonterm_count; ++j)
			{
				auto target = atm.nonterm_jumptable[i*nonterm_count + j];
				if (target)
				{
					builder.Goto(i, g.GetNonTerminal(j), *target);
				}
			}
		}

		// construct action table(shift)
		for (auto i = 0u; i < state_count; ++i)
		{
			for (auto j = 0u; j < term_count; ++j)
			{
				const auto target = atm.term_jumptable[i*term_count + j];
				if (target)
				{
					builder.Shift(i, g.GetTerminal(j), *target);
				}
			}
		}
	}

	Parser::Ptr CreateSLRParser(const Grammar::SharedPtr& grammar)
	{
		auto atm = GenerateLRAutomaton(*grammar);

		// builder
		const auto state_count = atm.states.size();
		ParserBuilder builder{ state_count, grammar };
		
		// construct goto table and shift commands in action table
		CopyTransitions(builder, *grammar, atm);

		// construct action table(reduce&accept)
		auto ps = PredictiveSet::Create(*grammar);

		for (auto id = 0u; id < state_count; ++id)
		{
			const auto& state = atm.states[id];
			for (auto item : state)
			{
				const auto& lhs = item.production->Left();
				const auto& rhs = item.production->Right();

				if (lhs == grammar->RootSymbol() && item.pos == 1)
				{
					builder.Accept(id);
				}
				else if (item.pos == rhs.size())
				{
					// for all term in FOLLOW do reduce
					for (auto term : ps.Follow(lhs))
					{
						builder.Reduce(id, term, item.production);
					}

					// EOF
					if (ps.EndingSymbol(lhs))
					{
						builder.ReduceOnEOF(id, item.production);
					}
				}
			}
		}

		return builder.Build();
	}

	void GenerateLALRReduction(const Grammar& grammar, ParserBuilder& builder, const LRAutomaton& atm)
	{
		// first, construct an extended grammar
		GrammarBuilder builder2;

		const auto state_count = atm.states.size();
		const auto term_count = grammar.TerminalCount();
		const auto nonterm_count = grammar.NonTerminalCount();

		// (non_term, start_state)
		using ExtendedNonTerm = tuple<NonTerminal, StateId>;
		vector<ExtendedNonTerm> nonterm_reg_lookup;
		map<ExtendedNonTerm, NonTerminal> nonterm_map;

		for (auto i = 0u; i < state_count; ++i)
		{
			for (auto j = 0u; j < nonterm_count; ++j)
			{
				auto target = atm.nonterm_jumptable[i*nonterm_count + j];
				if (!target.has_value())
					continue;

				auto old_symbol = grammar.GetNonTerminal(j);
				// TODO: displace debug function QuickName with empty string
				auto new_symbol = builder2.NewNonTerm(QuickName(grammar, old_symbol, i, *target));

				auto extended = make_tuple(old_symbol, i);
				nonterm_reg_lookup.push_back(extended);
				nonterm_map.insert_or_assign(extended, new_symbol);
			}
		}
		
		// (term, state_state)
		using ExtendedTerm = tuple<Terminal, StateId>;
		vector<ExtendedTerm> term_reg_lookup;
		map<ExtendedTerm, Terminal> term_map;

		for (auto i = 0u; i < state_count; ++i)
		{
			for (auto j = 0u; j < term_count; ++j)
			{
				auto target = atm.term_jumptable[i*term_count + j];
				if (!target.has_value())
					continue;

				auto old_symbol = grammar.GetTerminal(j);
				// TODO: displace debug function QuickName with empty string
				auto new_symbol = builder2.NewTerm(QuickName(grammar, old_symbol, i, *target));

				auto extended = make_tuple(old_symbol, i);
				term_reg_lookup.push_back(extended);
				term_map.insert_or_assign(extended, new_symbol);
			}
		}

		// (raw_production, mapped_production, final_state)
		using ProductionMappingInfo = tuple<const Production*, const Production*, StateId>;
		vector<ProductionMappingInfo> production_record;

		int initial_state;
		for (auto i = 0u; i < state_count; ++i)
		{
			EnumerateClosure(grammar, atm.states[i], [&](Item item) {

				// we are only interested in non-kernel states(including intial state)
				// which describe a higher-level of productions
				if (item.pos != 0) return;

				// we process start symbol(ROOT) differently
				if (item.production == &grammar.RootProduction())
				{
					initial_state = i;
					return;
				}

				// map left-hand side
				auto lhs = nonterm_map.at(make_tuple(item.production->Left(), i));

				// map right-hand side
				std::vector<Symbol> rhs;

				auto src_state = i;
				for (auto rhs_elem : item.production->Right())
				{
					auto target_state = LookupTargetState(grammar, atm, src_state, rhs_elem);
					assert(target_state.has_value());

					auto mapped_elem = rhs_elem.IsTerminal()
						? Symbol{ term_map.at(make_tuple(rhs_elem.AsTerminal(), src_state)) }
					: Symbol{ nonterm_map.at(make_tuple(rhs_elem.AsNonTerminal(), src_state)) };

					rhs.push_back(mapped_elem);

					src_state = *target_state;
				}

				// create production
				const auto& mapped_production = builder2.NewProduction(lhs, rhs);

				production_record.emplace_back(item.production, &mapped_production, src_state);
			});
		}

		auto raw_top = grammar.RootProduction().Right().front().AsNonTerminal();
		auto mapped_top = nonterm_map.at(make_tuple(raw_top, initial_state));
		auto ext_grammar = builder2.Build(mapped_top);
		auto ext_ps = PredictiveSet::Create(*ext_grammar);

		// merge the same terminals
		std::vector<bool> norm_ending;
		std::vector<FlatSet<Terminal>> norm_follow;
		for (auto id = 0u; id < ext_grammar->NonTerminalCount(); ++id)
		{
			auto nonterm = ext_grammar->GetNonTerminal(id);

			// normalized ending
			norm_ending.push_back(ext_ps.EndingSymbol(nonterm));

			// normalized FOLLOW set
			FlatSet<Terminal> tmp_follow;
			for (auto term : ext_ps.Follow(nonterm))
			{
				auto[raw_term, pos] = term_reg_lookup[term.Id()];
				tmp_follow.insert(raw_term);
			}

			norm_follow.push_back(tmp_follow);
		}

		// finally, reduce!
		// construct action table(reduce&accept)

		// (raw_production, final_state, FOLLOW, ending)
		using ReductionBuffer = tuple<const Production*, StateId, FlatSet<Terminal>, bool>;
		std::vector<ReductionBuffer> buffer;
		for (auto branch : production_record)
		{
			auto[raw_p, ext_p, final_state] = branch;
			auto lhs_id = ext_p->Left().Id();
			const auto& local_follow = norm_follow[lhs_id];
			bool local_ending = norm_ending[lhs_id];

			bool success = false;
			for (auto& buf_item : buffer)
			{
				auto&[pp, ss, ff, ee] = buf_item;
				if (pp == raw_p && ss == final_state)
				{
					ff.insert(local_follow.begin(), local_follow.end());
					ee |= local_ending;
					success = true;
					break;
				}
			}

			if (!success)
			{
				buffer.emplace_back(raw_p, final_state, local_follow, local_ending);
			}
		}

		for (auto& buf_item : buffer)
		{
			auto&[pp, ss, ff, ee] = buf_item;

			for (auto term : ff)
			{
				builder.Reduce(ss, term, pp);
			}

			if (ee)
			{
				builder.ReduceOnEOF(ss, pp);
			}
		}
	}

	Parser::Ptr CreateLALRParser(const Grammar::SharedPtr& grammar)
	{
		auto atm = GenerateLRAutomaton(*grammar);

		// builder
		const auto state_count = atm.states.size();
		const auto term_count = grammar->TerminalCount();
		const auto nonterm_count = grammar->NonTerminalCount();

		ParserBuilder builder{ state_count, grammar };

		// construct goto table and shift commands in action table
		CopyTransitions(builder, *grammar, atm);

		// construct action table(reduce)
		GenerateLALRReduction(*grammar, builder, atm);

		// construct action table(Accept)
		for (auto state_id = 0u; state_id < atm.states.size(); ++state_id)
		{
			const auto& state = atm.states[state_id];
			for (auto item : state)
			{
				const auto& lhs = item.production->Left();
				const auto& rhs = item.production->Right();

				if (lhs == grammar->RootSymbol() && item.pos == 1)
				{
					builder.Accept(state_id);
				}
			}
		}

		return builder.Build();
	}
}