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

	auto LookupCurrentState(const ParsingTable& table, vector<PdaStateId>& state_stack)
	{
		return state_stack.empty()
			? table.ParserInitialState()
			: state_stack.back();
	}

	Token BasicParser::LoadToken(string_view data, int offset) const
	{
		const auto& table = GetParsingTable();

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

	void BasicParser::FeedParser(ParsingContext& ctx, int tok) const
	{
		struct ActionVisitor
		{
			const BasicParser& parser;
			ParsingContext& ctx;
			int tok;

			bool hungry		= true;
			bool error		= false;

			ActionVisitor(const BasicParser& parser, ParsingContext& ctx, int tok)
				: parser(parser), ctx(ctx), tok(tok) { }

			void operator()(PdaActionShift action)
			{
				PrintFormatted("Shift state {}\n", action.destination.Value());
				ctx.state_stack.push_back(action.destination);
				hungry = false;
			}
			void operator()(PdaActionReduce action)
			{
				const auto& info = parser.GetParserInfo();
				const auto& manager = parser.GetProxyManager();
				const auto& table = parser.GetParsingTable();

				const auto& production = info.Productions()[action.production];
				PrintFormatted("Reduce {}\n", debug::ToString_Production(info, action.production));

				// update state stack
				const auto count = production.rhs.size();
				for (auto i = 0; i < count; ++i)
					ctx.state_stack.pop_back();

				auto nonterm_id = distance(info.Variables().data(), production.lhs);
				auto src_state = LookupCurrentState(table, ctx.state_stack);
				auto target_state = table.LookupGoto(src_state, nonterm_id);

				ctx.state_stack.push_back(target_state);

				// update ast stack
				auto ref = ArrayRef<AstTypeWrapper>(ctx.ast_stack.data(), ctx.ast_stack.size()).TakeBack(count);
				auto result = production.handle->Invoke(manager, *ctx.arena, ref);

				for (auto i = 0; i < count; ++i)
					ctx.ast_stack.pop_back();

				ctx.ast_stack.push_back(result);

				// test if accepted
				if (tok == -1 &&
					ctx.state_stack.size() == 1 &&
					production.lhs == &info.RootVariable())
				{
					hungry = false;
				}
			}
			void operator()(PdaActionError action)
			{
				hungry = false;
				error = true;
			}
		} visitor{ *this, ctx, tok };

		const auto& table = GetParsingTable();
		while (visitor.hungry)
		{
			auto cur_state = LookupCurrentState(table, ctx.state_stack);
			assert(cur_state.IsValid());

			auto action = (tok != -1)
				? table.LookupAction(cur_state, tok)
				: table.LookupActionOnEof(cur_state);

			visit(visitor, action);

			if (visitor.error)
			{
				throw ParserInternalError{ "parsing error" };
			}
		}
	}

	void BasicParser::Parse(string_view data) const
	{
		int offset = 0;
		ParsingContext ctx;
		ctx.arena = std::make_unique<Arena>();

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(data, offset);

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

		// TODO: refine parser interface
		auto result = ctx.ast_stack.front().Extract<TranslationUnit*>();
	}
}