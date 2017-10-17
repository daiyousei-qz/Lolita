#include "parsing-bootstrap.h"
#include "lexing-automaton.h"
#include "parsing-automaton.h"
#include "text\format.hpp"
#include <sstream>

using namespace std;
using namespace eds::text;

namespace eds::loli::debug
{
	string EscapeCharacter(int ch)
	{
		switch (ch)
		{
		case ' ':
			return "<space>";
		case '\t':
			return "<\\t>";
		case '\r':
			return "<\\r>";
		case '\n':
			return "<\\n>";
		default:
			return string{ static_cast<char>(ch) };
		}
	}

	string ToString_Token(const ParserBootstrapInfo& info, int id)
	{
		if (id == -1)
		{
			return "UNACCEPTED";
		}

		auto t = info.Tokens().size();
		if (id < t)
		{
			return info.Tokens()[id].Name();
		}
		else
		{
			return info.IgnoredTokens()[id - t].Name();
		}
	}

	string ToString_Variable(const ParserBootstrapInfo& info, int id)
	{
		return info.Variables()[id].Name();
	}

	string ToString_Production(const ParserBootstrapInfo& info, int id)
	{
		const auto& p = info.Productions()[id];

		stringstream buf;
		buf << "{ ";
		buf << p.lhs->Name();
		buf << " :";

		for (const auto& rhs_elem : p.rhs)
		{
			buf << " ";
			buf << rhs_elem->Name();
		}

		buf << " }";

		return buf.str();
	}

	string ToString_ParsingAction(const ParserBootstrapInfo& info, parsing::ParsingAction action)
	{
		using namespace parsing;
		if (holds_alternative<ActionShift>(action))
		{
			return Format("shift to {}", get<ActionShift>(action).target->id);
		}
		else if (holds_alternative<ActionReduce>(action))
		{
			return Format("reduce {}", ToString_Production(info, get<ActionReduce>(action).production->id));
		}
		else
		{
			throw 0;
		}
	}

	void PrintGrammar(const ParserBootstrapInfo& info)
	{
		PrintFormatted("tokens:\n");
		for(const auto& tok : info.Tokens())
		{
			PrintFormatted("  {}\n", tok.Name());
		}

		PrintFormatted("\n");
		PrintFormatted("ignores:\n");
		for (const auto& tok : info.IgnoredTokens())
		{
			PrintFormatted("  {}\n", tok.Name());
		}

		PrintFormatted("\n");
		PrintFormatted("variables:\n");
		for (const auto& var : info.Variables())
		{
			PrintFormatted("  {}\n", var.Name());
		}
	}

	void PrintLexingAutomaton(const ParserBootstrapInfo& info, const lexing::LexingAutomaton& dfa)
	{
		using namespace eds::loli::lexing;

		for (int id = 0; id < dfa.StateCount(); ++id)
		{
			const auto state = dfa.LookupState(id);
			PrintFormatted("state {}({}):\n", state->id, ToString_Token(info, state->acc_category));

			for (const auto& edge : state->transitions)
			{
				PrintFormatted("  {} -> {}\n", EscapeCharacter(edge.first), edge.second->id);
			}

			PrintFormatted("\n");
		}
	}

	void PrintParsingAutomaton(const ParserBootstrapInfo& info, const parsing::ParsingAutomaton& pda)
	{
		using namespace eds::loli::parsing;

		for (int id = 0; id < pda.StateCount(); ++id)
		{
			const auto state = pda.LookupState(id);
			PrintFormatted("state {}:\n", state->id);

			if (state->eof_action)
			{
				PrintFormatted("  <eof> -> do {}\n", 
							   ToString_ParsingAction(info, *state->eof_action));
			}

			for (const auto& pair : state->action_map)
			{
				PrintFormatted("  {} -> do {}\n", 
							   ToString_Token(info, pair.first->id), 
							   ToString_ParsingAction(info, pair.second));
			}

			for (const auto& pair : state->goto_map)
			{
				PrintFormatted("  {} -> goto state {}\n", 
							   ToString_Variable(info, pair.first->id), 
							   pair.second->id);
			}

			PrintFormatted("\n");
		}
	}
}