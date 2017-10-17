#pragma once
#include "grammar.h"
#include <stack>
#include <vector>
#include <variant>
#include <optional>
#include <memory>
#include <type_traits>
#include <functional>
#include <cassert>

namespace eds::loli::parsing
{
	// Parsing Actions
	//

	class ParsingState;

	struct ActionShift
	{
		ParsingState* target;
	};
	struct ActionReduce
	{
		const Production* production;
	};

	using ParsingAction = std::variant<ActionShift, ActionReduce>;

	struct ParsingState
	{
		int id;

		std::optional<ParsingAction> eof_action;
		std::unordered_map<const Terminal*, ParsingAction> action_map;
		std::unordered_map<const Nonterminal*, ParsingState*> goto_map;

	public:
		ParsingState(int id)
			: id(id) { }
	};

	class ParsingAutomaton
	{
	public:
		int StateCount() const
		{
			return states_.size();
		}
		const ParsingState* LookupState(int id) const
		{
			return states_.at(id).get();
		}

		ParsingState* NewState()
		{
			auto id = StateCount();

			return states_.emplace_back(
				std::make_unique<ParsingState>(id)
			).get();
		}

		void RegisterShift(ParsingState* src, ParsingState* dest, const Symbol* s)
		{
			if (auto term = s->AsTerminal(); term)
			{
				assert(src->action_map.count(term) == 0);
				src->action_map.insert_or_assign(term, ActionShift{ dest });
			}
			else
			{
				auto nonterm = s->AsNonterminal();
				assert(src->goto_map.count(nonterm) == 0);
				src->goto_map.insert_or_assign(nonterm, dest);
			}
		}
		void RegisterReduce(ParsingState* src, Production* p, const Terminal* s)
		{
			assert(src->action_map.count(s) == 0);
			src->action_map.insert_or_assign(s, ActionReduce{ p });
		}
		void RegisterReduceOnEof(ParsingState* src, const Production* p)
		{
			assert(!src->eof_action.has_value());
			src->eof_action = ActionReduce{ p };
		}

	private:
		std::vector<std::unique_ptr<ParsingState>> states_;
	};

	std::unique_ptr<const ParsingAutomaton> BuildSLRAutomaton(const Grammar& g);
}