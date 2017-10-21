#include "parser.h"
#include <memory>

using namespace std;

namespace eds::loli
{
	/**
	struct ParsingContext
	{
		std::unique_ptr<Arena> arena;
		vector<AstItem> items;
	};

	Token LoadToken(const ParsingTable& table, string_view data, int offset)
	{
		auto last_acc_len = 0;
		auto last_acc_category = -1;

		auto state = table.LexerInitialState();
		for (int i = offset; i < data.length(); ++i)
		{
			state = table.LookupDfaTransition(state, data[i]);

			if (!state.IsValid())
			{
				break;
			}
			else if (auto category = table.LookupAccCategory(state); category != -1)
			{
				last_acc_len = i - offset + 1;
				last_acc_category = category;
			}
		}

		if (last_acc_len != 0)
		{
			return Token{ last_acc_category, offset, last_acc_len };
		}
		else
		{
			return Token{ -1, offset, 1 };
		}
	}

	auto LookupCurrentState(const ParsingTable& table, vector<PdaStateId>& states)
	{
		return states.empty()
			? table.ParserInitialState()
			: states.back();
	}

	bool FeedParser(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
	{
		struct ActionVisitor
		{
			const ParsingTable& table;
			vector<PdaStateId>& states;
			ParsingContext& ctx;
			int tok;

			bool hungry = true;
			bool accepted = false;
			bool error = false;

			ActionVisitor(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
				: table(table), states(states), ctx(ctx), tok(tok) { }

			void operator()(PdaActionShift action)
			{
				states.push_back(action.destination);
				hungry = false;
			}
			void operator()(PdaActionReduce action)
			{
				auto id = action.production;

				const auto& production = info->Productions()[id];

				const auto count = production.rhs.size();
				for (auto i = 0; i < count; ++i)
					states.pop_back();

				auto nonterm_id = distance(info->Variables().data(), production.lhs);
				auto src_state = LookupCurrentState(table, states);
				auto target_state = table.LookupGoto(src_state, nonterm_id);

				states.push_back(target_state);

				auto result = production.handle->Invoke(*manager, ctx.arena, AstItemView(ctx.items, count));
				for (auto i = 0; i < count; ++i)
					ctx.items.pop_back();
				ctx.items.push_back(result);
			}
			void operator()(PdaActionError action)
			{
				hungry = false;
				error = true;
			}
		} visitor{ table, states, ctx, tok };

		while (visitor.hungry)
		{
			auto cur_state = LookupCurrentState(table, states);
			// DEBUG:
			if (!cur_state.IsValid()) return visitor.accepted;

			auto action = (tok != -1)
				? table.LookupAction(cur_state, tok)
				: table.LookupActionOnEof(cur_state);

			visit(visitor, action);
		}

		// if (visitor.error) throw 0;

		return visitor.accepted;
	}

	auto Parse(ParsingContext& ctx, const ParsingTable& table, string_view data)
	{
		int offset = 0;
		vector<PdaStateId> states;

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(table, data, offset);

			// update offset
			offset = tok.offset + tok.length;

			// throw for invalid token
			if (tok.category == -1) throw 0;
			// ignore categories in blacklist
			if (tok.category >= table.TerminalCount()) continue;

			FeedParser(table, states, ctx, tok.category);
			ctx.items.push_back(AstItem::Create<Token>(tok));
		}

		// finalize parsing
		FeedParser(table, states, ctx, -1);
	}
	*/
}