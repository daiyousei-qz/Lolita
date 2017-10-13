#include "parsing-automaton.h"
#include "grammar.h"
#include "grammar-analytic.h"
#include <set>
#include <map>
#include <unordered_map>
#include <tuple>

using namespace std;
using namespace eds;

namespace eds::loli
{
	// by convention, if production is set nullptr
	// it's an imaginary root production
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
			assert(cursor >= 0 && cursor <= p->Right().size());
		}

		Symbol* NextSymbol() const
		{
			if (IsFinalized()) return nullptr;

			return production->Right()[cursor];
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
			return cursor == production->Right().size();
		}
	};

	// Operator Overloads for Item(for FlatSet)
	//

	inline bool operator==(ParsingItem lhs, ParsingItem rhs)
	{
		return lhs.production == rhs.production && lhs.cursor == rhs.cursor;
	}
	inline bool operator!=(ParsingItem lhs, ParsingItem rhs)
	{
		return !(lhs == rhs);
	}
	inline bool operator>(ParsingItem lhs, ParsingItem rhs)
	{
		if (lhs.production > rhs.production) return true;
		if (lhs.production < rhs.production) return false;

		return lhs.cursor > rhs.cursor;
	}
	inline bool operator>=(ParsingItem lhs, ParsingItem rhs)
	{
		return lhs == rhs || lhs > rhs;
	}
	inline bool operator<(ParsingItem lhs, ParsingItem rhs)
	{
		return !(lhs >= rhs);
	}
	inline bool operator<=(ParsingItem lhs, ParsingItem rhs)
	{
		return !(lhs > rhs);
	}

	using ItemSet = FlatSet<ParsingItem>;

	// Expands state of kernal items into its closure
	// that includes all of non-kernal items as well,
	// and then enumerate items with a callback
	void EnumerateClosureItems(const Grammar& g, const ItemSet& state, function<void(ParsingItem)> callback)
	{
		unordered_map<NonTerminal*, bool> added;
		added[g.RootSymbol()] = true; // treat root symbol's items as kernal

		vector<NonTerminal*> unvisited;

		auto try_register_candidate = [&](Symbol* s) {
			if (s->IsNonTerminal())
			{
				auto candidate = s->AsNonTerminal();

				if (!added[candidate])
				{
					added[candidate] = true;
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

			for (const auto p : lhs->Productions())
			{
				callback(ParsingItem{ p, 0 });

				const auto& rhs = p->Right();
				if (!rhs.empty())
				{
					try_register_candidate(rhs.front());
				}
			}
		}
	}

	// TODO: directly generate vector
	// claculate target state from a source state with a particular symbol s
	// and enumerate its items with a callback
	ItemSet GenerateGotoItems(const Grammar& g, const ItemSet& src, Symbol* s)
	{
		ItemSet new_state;

		EnumerateClosureItems(g, src, [&](ParsingItem item) {

			// ignore Item A -> \alpha .
			if (item.IsFinalized())
				return;

			// for Item A -> \alpha . B \beta where B == s
			// advance the cursor
			if (item.production->Right()[item.cursor] == s)
			{
				new_state.insert(ParsingItem{ item.production, item.cursor + 1 });
			}
		});

		return new_state;
	}

	ItemSet GenerateInitialItemSet(const Grammar& g)
	{
		ItemSet result;
		for (auto p : g.RootSymbol()->Productions())
		{
			result.insert(ParsingItem{ p, 0 });
		}

		return result;
	}

	// F should be some void(Symbol*)
	template <typename F>
	void EnumerateSymbols(const Grammar& g, F callback)
	{
		for (const auto& term : g.Terminals())
			callback(term.get());

		for (const auto& nonterm : g.NonTerminals())
			callback(nonterm.get());
	}

	auto GenerateBasicParsingAutomaton(const Grammar& g)
	{
		// NOTE a set of ParsingItem coresponds to a ParsingState

		auto pda = make_unique<ParsingAutomaton>();
		auto initial_set = GenerateInitialItemSet(g);
		map<ItemSet, ParsingState*> state_lookup{ { initial_set, pda->NewState() } };

		// for each unvisited LR state, generate a target state for each possible symbol
		for (deque<ItemSet> unprocessed{ initial_set }; !unprocessed.empty(); unprocessed.pop_front())
		{
			const auto& src_item_set = unprocessed.front();
			const auto src_state = state_lookup[src_item_set];

			EnumerateSymbols(g, [&](Symbol* s) {

				// calculate the target state for symbol s
				ItemSet dest_item_set = GenerateGotoItems(g, src_item_set, s);

				// empty set of item is not a valid state
				if (dest_item_set.empty()) return;

				// compute target state
				ParsingState* dest_state;
				auto iter = std::find(state_lookup.begin(), state_lookup.end(), dest_item_set);
				if (iter != state_lookup.end())
				{
					dest_state = iter->second;
				}
				else
				{
					// if new_state is not recorded in states, create one
					dest_state = pda->NewState();
					state_lookup.insert_or_assign(dest_item_set, dest_state);
				}

				// add transition
				pda->RegisterShift(src_state, dest_state, s);
			});
		}

		return make_tuple(move(pda), move(state_lookup));
	}

	unique_ptr<ParsingAutomaton> BuildSLRParsingTable(const Grammar& g)
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
				const auto& lhs = item.production->Left();
				const auto& rhs = item.production->Right();

				const auto& lhs_info = ps.at(lhs);

				if (item.IsFinalized())
				{
					if (lhs == g.RootSymbol())
					{
						atm->MakeAccept(state);
					}

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
}