#include "parser.h"
#include <memory>
#include <sstream>
#include "text/format.hpp"
#include "calc2.h"
#include "debug.h"

using namespace std;
using namespace eds::text;

namespace eds::loli
{
	//
	// Implementation of BasicParser
	//

	enum class ActionExecutionResult
	{
		Hungry, Consumed, Error
	};

	struct ParsingContext
	{
		const BasicGenericParser& parser;
		Arena& arena;

		vector<PdaStateId> state_stack   = {};
		vector<AstTypeWrapper> ast_stack = {};

	public:
		ParsingContext(const BasicGenericParser& parser, Arena& arena)
			: parser(parser), arena(arena) { }

		auto LookupCurrentState(const ParsingTable& table)
		{
			return state_stack.empty()
				? table.ParserInitialState()
				: state_stack.back();
		}

		ActionExecutionResult ExecuteAction(PdaActionShift action, int tok)
		{
			assert(tok != -1);

			PrintFormatted("Shift state {}\n", action.destination.Value());
			state_stack.push_back(action.destination);

			return ActionExecutionResult::Consumed;
		}

		ActionExecutionResult ExecuteAction(PdaActionReduce action, int tok)
		{
			const auto& info	= parser.GetParserInfo();
			const auto& manager = parser.GetProxyManager();
			const auto& table	= parser.GetParsingTable();

			const auto& production = info.Productions()[action.production];
			PrintFormatted("Reduce {}\n", debug::ToString_Production(info, action.production));

			// update state stack
			const auto count = production.rhs.size();
			for (auto i = 0; i < count; ++i)
				state_stack.pop_back();

			auto nonterm_id = distance(info.Variables().data(), production.lhs);
			auto src_state = LookupCurrentState(table);
			auto target_state = table.LookupGoto(src_state, nonterm_id);

			state_stack.push_back(target_state);

			// update ast stack
			auto ref = ArrayRef<AstTypeWrapper>(ast_stack.data(), ast_stack.size()).TakeBack(count);
			auto result = production.handle->Invoke(manager, arena, ref);

			for (auto i = 0; i < count; ++i)
				ast_stack.pop_back();

			ast_stack.push_back(result);

			// test if accepted
			if (tok == -1 &&
				state_stack.size() == 1 &&
				production.lhs == &info.RootVariable())
			{
				return ActionExecutionResult::Consumed;
			}
			else
			{
				return ActionExecutionResult::Hungry;
			}
		}

		ActionExecutionResult ExecuteAction(PdaActionError action, int tok)
		{
			return ActionExecutionResult::Error;
		}
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

	void FeedParser(ParsingContext& ctx, int tok)
	{
		const auto& table = ctx.parser.GetParsingTable();
		while (true)
		{
			auto cur_state = ctx.LookupCurrentState(table);
			assert(cur_state.IsValid());

			auto action = (tok != -1)
				? table.LookupAction(cur_state, tok)
				: table.LookupActionOnEof(cur_state);

			auto action_result 
				= visit([&](auto action) { return ctx.ExecuteAction(action, tok); }, action);

			if (action_result == ActionExecutionResult::Error)
			{
				throw ParserInternalError{ "parsing error" };
			}
			else if (action_result == ActionExecutionResult::Consumed)
			{
				break;
			}
		}
	}

	AstTypeWrapper BasicGenericParser::DoParse(Arena& arena, string_view data) const
	{
		int offset = 0;
		ParsingContext ctx{ *this, arena };

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(GetParsingTable(), data, offset);

			// update offset
			offset = tok.offset + tok.length;

			// throw for invalid token
			if (tok.category == -1) throw 0;
			// ignore categories in blacklist
			if (tok.category >= GetParsingTable().TerminalCount()) continue;

			PrintFormatted("Lookahead {} which is {}\n", debug::ToString_Token(GetParserInfo(), tok.category), data.substr(tok.offset, tok.length));
			FeedParser(ctx, tok.category);
			ctx.ast_stack.push_back(AstTypeWrapper::Create<Token>(tok));
		}

		// finalize parsing
		FeedParser(ctx, -1);

		return ctx.ast_stack.front();
	}


}