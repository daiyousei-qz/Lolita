#pragma once
#include "symbol.h"
#include "grammar.h"
#include "debug.h"
#include <stack>
#include <vector>
#include <variant>
#include <optional>
#include <memory>
#include <type_traits>

namespace lolita
{
	struct StateSlot
	{
		Symbol symbol;
		unsigned state;
	};

	using ParsingStack = std::stack<StateSlot>;

	struct ActionShift 
	{
		unsigned target_state;
	};
	struct ActionReduce 
	{
		const Production* production;
	};
	struct ActionAccept { };
	struct ActionError { };

	using Action = std::variant<ActionShift,
								ActionReduce,
								ActionAccept,
								ActionError>;

	using GotoSlot = std::optional<unsigned>;

	class Parser
	{
	private:
		friend class ParserBuilder;
		struct ConstructionDummy { };
	public:
		using Ptr = std::unique_ptr<Parser>;

		Parser(Grammar::SharedPtr g, std::vector<Action> actions, std::vector<GotoSlot> gotos, ConstructionDummy = {})
			: grammar_(std::move(g))
			, action_table_(std::move(actions))
			, goto_table_(std::move(gotos)) { }

		auto ActionTableWidth() const { return grammar_->TerminalCount() + 1; }
		auto GotoTableWidth() const { return grammar_->NonTerminalCount(); }

		size_t StateCount() const
		{
			return goto_table_.size() / GotoTableWidth();
		}
		const auto& Grammar() const
		{
			return *grammar_;
		}

		void Feed(ParsingStack& ctx, unsigned tok)
		{
			assert(tok < grammar_->TerminalCount());

			auto result = FeedInternal(ctx, tok);
			assert(!result); // i.e. parser should require more inputs
		}

		bool Finalize(ParsingStack& ctx)
		{
			return FeedInternal(ctx, ActionTableWidth() - 1);
		}

		void Print() const;

	private:
		bool FeedInternal(ParsingStack& ctx, unsigned tok);

		unsigned LookupCurrentState(const ParsingStack& ctx);

	private:
		const Grammar::SharedPtr grammar_;

		const std::vector<Action> action_table_;
		const std::vector<GotoSlot> goto_table_;
	};

	class ParserBuilder
	{
	public:
		ParserBuilder(unsigned state_num, const Grammar::SharedPtr& grammar)
			: grammar_(grammar)
			, action_table_(state_num * (grammar->TerminalCount() + 1), ActionError{})
			, goto_table_(state_num * grammar->NonTerminalCount(), std::nullopt) 
		{ }

		auto ActionTableWidth() const { return grammar_->TerminalCount() + 1; }
		auto GotoTableWidth() const { return grammar_->NonTerminalCount(); }

		void Shift(unsigned old_state, Terminal tok, unsigned new_state)
		{
			auto offset = old_state * ActionTableWidth() + tok.Id();
			auto& entry = action_table_[offset];

			assert(std::holds_alternative<ActionError>(entry));
			entry = ActionShift{ new_state };
		}

		void Reduce(unsigned state, Terminal tok, const Production* p)
		{
			auto offset = state * ActionTableWidth() + tok.Id();
			auto& entry = action_table_[offset];

			assert(std::holds_alternative<ActionError>(entry));
			entry = ActionReduce{ p };
		}

		void ReduceOnEOF(unsigned state, const Production* p)
		{
			auto offset = state * ActionTableWidth() + ActionTableWidth() - 1;
			auto& entry = action_table_[offset];

			assert(std::holds_alternative<ActionError>(entry));
			entry = ActionReduce{ p };
		}

		void Accept(unsigned state)
		{
			auto offset = state * ActionTableWidth() + ActionTableWidth() - 1;
			auto& entry = action_table_[offset];

			assert(std::holds_alternative<ActionError>(entry));
			entry = ActionAccept{};
		}

		void Goto(unsigned old_state, NonTerminal s, unsigned new_state)
		{
			auto offset = old_state * GotoTableWidth() + s.Id();
			goto_table_[offset] = new_state;
		}

		Parser::Ptr Build()
		{
			return std::make_unique<Parser>(grammar_, action_table_, goto_table_);
		}

	private:
		Grammar::SharedPtr grammar_;

		std::vector<Action> action_table_;
		std::vector<GotoSlot> goto_table_;
	};
}