#include "parsing-automaton.h"
#include "grammar.h"
#include "grammar-analytic.h"
#include <set>
#include <map>
#include <unordered_map>
#include <tuple>
#include <cassert>

using namespace std;
using namespace eds;

namespace eds::loli::parsing
{
	// Implmentation of ParsingAutomaton
	//

	struct ParsingItem
	{
	public:
		Production* production;
		int cursor;

	public:
		ParsingItem(Production* p, int cursor)
			: production(p), cursor(cursor)
		{
			assert(p != nullptr);
			assert(cursor >= 0 && cursor <= p->rhs.size());
		}

		const Symbol* NextSymbol() const
		{
			if (IsFinalized()) return nullptr;

			return production->rhs[cursor];
		}

		// if it's not any P -> . \alpha
		// NOTE though production of root symbol is always kernal,
		// this struct is not aware of that
		bool IsKernal() const
		{
			return cursor > 0;
		}

		// if it's some P -> \alpha .
		bool IsFinalized() const
		{
			return cursor == production->rhs.size();
		}
	};

	// Operator Overloads for Item(for FlatSet)
	//

	inline bool operator==(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		return lhs.production == rhs.production && lhs.cursor == rhs.cursor;
	}
	inline bool operator!=(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		return !(lhs == rhs);
	}
	inline bool operator<(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		if (lhs.production < rhs.production) return true;
		if (lhs.production > rhs.production) return false;

		return lhs.cursor < rhs.cursor;
	}
	inline bool operator>(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		return rhs < lhs;
	}
	inline bool operator<=(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		return !(lhs > rhs);
	}
	inline bool operator>=(const ParsingItem& lhs, const ParsingItem& rhs)
	{
		return !(lhs < rhs);
	}

	using ItemSet = FlatSet<ParsingItem>;
	using StateLookup = map<ItemSet, ParsingState*>;

	struct BasicLRAutomaton
	{
		std::unique_ptr<ParsingAutomaton> pda;
		StateLookup lookup;
	};

	// Expands state of kernal items into its closure
	// that includes all of non-kernal items as well,
	// and then enumerate items with a callback
	void EnumerateClosureItems(const Grammar& g, const ItemSet& state, function<void(ParsingItem)> callback)
	{
		auto added = vector<bool>(g.NonterminalCount(), false);
		auto unvisited = vector<const Nonterminal*>{};

		// treat root symbol's items as kernal
		added[g.RootSymbol()->id] = true;

		// helper
		auto try_register_candidate = [&](const Symbol* s) {
			if (auto candidate = s->AsNonterminal(); candidate)
			{
				if (!added[candidate->id])
				{
					added[candidate->id] = true;
					unvisited.push_back(candidate);
				}
			}
		};

		// visit kernal items and record nonterms for closure calculation
		for (const auto& item : state)
		{
			callback(item);

			// Item{ A -> \alpha . }: skip it
			// Item{ A -> \alpha . B \gamma }: queue unvisited nonterm B for further expansion
			auto symbol = item.NextSymbol();
			if (symbol)
			{
				try_register_candidate(symbol);
			}
		}

		// genreate and visit non-kernal items recursively
		while (!unvisited.empty())
		{
			auto lhs = unvisited.back();
			unvisited.pop_back();

			for (const auto p : lhs->productions)
			{
				callback(ParsingItem{ p, 0 });

				const auto& rhs = p->rhs;
				if (!rhs.empty())
				{
					try_register_candidate(rhs.front());
				}
			}
		}
	}

	// claculate target state from a source state with a particular symbol s
	// and enumerate its items with a callback
	ItemSet GenerateGotoItems(const Grammar& g, const ItemSet& src, const Symbol* s)
	{
		ItemSet new_state;

		EnumerateClosureItems(g, src, [&](ParsingItem item) {

			// ignore Item A -> \alpha .
			if (item.IsFinalized())
				return;

			// for Item A -> \alpha . B \beta where B == s
			// advance the cursor
			if (item.production->rhs[item.cursor] == s)
			{
				new_state.insert(ParsingItem{ item.production, item.cursor + 1 });
			}
		});

		return new_state;
	}

	ItemSet GenerateInitialItemSet(const Grammar& g)
	{
		ItemSet result;
		for (auto p : g.RootSymbol()->productions)
		{
			result.insert(ParsingItem{ p, 0 });
		}

		return result;
	}

	template <typename F>
	void EnumerateSymbols(const Grammar& g, F callback)
	{
		static_assert(is_invocable_v<F, const Symbol*>);

		for (const auto& term : g.Terminals())
			callback(&term);

		for (const auto& nonterm : g.Nonterminals())
			callback(&nonterm);
	}

	auto GenerateBasicParsingAutomaton(const Grammar& g)
	{
		// NOTE a set of ParsingItem coresponds to a ParsingState

		auto pda = make_unique<ParsingAutomaton>();
		auto initial_set = GenerateInitialItemSet(g);
		StateLookup state_lookup{ { initial_set, pda->NewState() } };

		// for each unvisited LR state, generate a target state for each possible symbol
		for (deque<ItemSet> unprocessed{ initial_set }; !unprocessed.empty(); unprocessed.pop_front())
		{
			const auto& src_item_set = unprocessed.front();
			const auto src_state = state_lookup[src_item_set];

			EnumerateSymbols(g, [&](const Symbol* s) {

				// calculate the target state for symbol s
				ItemSet dest_item_set = GenerateGotoItems(g, src_item_set, s);

				// empty set of item is not a valid state
				if (dest_item_set.empty()) return;

				// compute target state
				auto dest_state = state_lookup[dest_item_set];
				if (dest_state == nullptr)
				{
					// if new_state is not recorded in states, create one
					dest_state = pda->NewState();

					unprocessed.push_back(dest_item_set);
					state_lookup.insert_or_assign(dest_item_set, dest_state);
				}

				// add transition
				pda->RegisterShift(src_state, dest_state, s);
			});
		}

		return BasicLRAutomaton{ move(pda), move(state_lookup) };
	}

	// SLR
	//

	unique_ptr<const ParsingAutomaton> BuildSLRAutomaton(const Grammar& g)
	{
		auto[atm, state_lookup] = GenerateBasicParsingAutomaton(g);

		// calculate FIRST and FOLLOW set first
		const auto ps = ComputePredictiveSet(g);

		for (const auto& state_def : state_lookup)
		{
			const auto& item_set = get<0>(state_def);
			const auto& state = get<1>(state_def);

			for (auto item : item_set)
			{
				const auto& lhs = item.production->lhs;
				const auto& rhs = item.production->rhs;

				const auto& lhs_info = ps.at(lhs);

				if (item.IsFinalized())
				{
					// for all term in FOLLOW do reduce
					for (auto term : lhs_info.follow_set)
					{
						atm->RegisterReduce(state, item.production, term);
					}

					// EOF
					if (lhs_info.may_preceed_eof)
					{
						atm->RegisterReduceOnEof(state, item.production);
					}
				}
			}
		}

		return move(atm);
	}

	// LALR
	//

	ParsingState* LookupTargetState(ParsingState* src, const Symbol* s)
	{
		if (auto term = s->AsTerminal(); term)
		{
			return get<ActionShift>(src->action_map.at(term)).target;
		}
		else
		{
			return src->goto_map.at(s->AsNonterminal());
		}
	}

	unique_ptr<Grammar> ExtendGrammar(const Grammar& old_grammar, const ParsingAutomaton& pda, const StateLookup& lookup)
	{
		auto ext_grammar = make_unique<Grammar>();

		// (id, version)
		// TODO: replace map with unordered_map
		using SymbolKey = tuple<int, int>;
		map<SymbolKey, Terminal*> term_lookup;
		map<SymbolKey, Nonterminal*> nonterm_lookup;
		for (int i = 0; i < pda.StateCount(); ++i)
		{
			const auto& state = pda.LookupState(i);

			// extend nonterms
			for (const auto& goto_edge : state->goto_map)
			{
				auto id = goto_edge.first->id;
				auto version = goto_edge.second->id;
				assert(goto_edge.first->version == 0);

				if (nonterm_lookup.count({ id,version }) == 0)
				{
					auto nonterm = ext_grammar->NewNonterm(id, version);
					nonterm_lookup[{id, version}] = nonterm;
				}
			}

			// extend terms
			for (const auto& action_edge : state->action_map)
			{
				auto id = action_edge.first->id;
				auto version = get<ActionShift>(action_edge.second).target->id;
				assert(action_edge.first->version == 0);

				if (term_lookup.count({ id,version }) == 0)
				{
					auto term = ext_grammar->NewTerm(id, version);
					term_lookup[{id, version}] = term;
				}
			}
		}
		
		// extend root symbol
		auto new_root = ext_grammar->NewNonterm(old_grammar.RootSymbol()->id, 0);
		ext_grammar->ConfigureRootSymbol(new_root);

		// extend productions
		for (const auto& pair : lookup)
		{
			const auto& items = pair.first;
			const auto& state = pair.second;

			EnumerateClosureItems(old_grammar, items, [&](ParsingItem item) {

				// we are only interested in non-kernel items(including intial state)
				// to avoid repetition
				if (item.IsKernal()) return;

				// map left-hand side
				Nonterminal* lhs = new_root;
				if (item.production->lhs != old_grammar.RootSymbol())
				{
					auto lhs_id = item.production->lhs->id;
					auto lhs_version = LookupTargetState(state, item.production->lhs)->id;
					lhs = nonterm_lookup.at({ lhs_id, lhs_version });
				}

				// map right-hand side
				std::vector<const Symbol*> rhs;

				auto cur_state = state;
				for (auto rhs_elem : item.production->rhs)
				{
					auto target_state = LookupTargetState(cur_state, rhs_elem);
					assert(target_state != nullptr);

					if (rhs_elem->AsTerminal())
					{
						rhs.push_back(
							term_lookup.at({ rhs_elem->id, target_state->id })
						);
					}
					else
					{
						rhs.push_back(
							nonterm_lookup.at({ rhs_elem->id, target_state->id })
						);
					}

					cur_state = target_state;
				}

				// create production
				ext_grammar->NewProduction(item.production->id, lhs, rhs, cur_state->id);
			});
		}

		return ext_grammar;
	}

	unique_ptr<const ParsingAutomaton> BuildLALRAutomaton(const Grammar& g)
	{
		auto[atm, state_lookup] = GenerateBasicParsingAutomaton(g);

		auto ext_grammar = ExtendGrammar(g, *atm, state_lookup);
		auto ext_ps = ComputePredictiveSet(*ext_grammar);

		// calculate follows
		set<const Nonterminal*> norm_ending;
		map<const Nonterminal*, PredictiveInfo::TermSet> norm_follow;
		for (const auto& nonterm : ext_grammar->Nonterminals())
		{
			const auto& ps_info = ext_ps.at(&nonterm);
			auto raw_nonterm = g.LookupNonterminal(nonterm.id);

			// normalized ending
			if (ps_info.may_preceed_eof)
			{
				norm_ending.insert(raw_nonterm);
			}

			// normalized FOLLOW set
			auto& follow = norm_follow[raw_nonterm];
			for (auto term : ps_info.follow_set)
			{
				auto raw_term = g.LookupTerminal(term->id);
				follow.insert(raw_term);
			}
		}

		//
		for (const auto& state_def : state_lookup)
		{
			const auto& item_set = get<0>(state_def);
			const auto& state = get<1>(state_def);

			for (auto item : item_set)
			{
				const auto& lhs = item.production->lhs;
				const auto& rhs = item.production->rhs;

				if (item.IsFinalized())
				{
					// for all term in FOLLOW do reduce
					for (auto term : norm_follow.at(lhs))
					{
						atm->RegisterReduce(state, item.production, term);
					}

					// EOF
					if (norm_ending.count(lhs) > 0)
					{
						atm->RegisterReduceOnEof(state, item.production);
					}
				}
			}
		}

		return move(atm);
	}
}