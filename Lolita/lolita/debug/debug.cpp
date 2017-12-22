#include "lolita/debug/debug.h"
#include "text/format.h"
#include <sstream>

using namespace std;
using namespace eds::text;

namespace eds::loli::debug
{
	//*
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

	string ToString_Token(const ParsingMetaInfo& info, int id)
	{
		if (id == -1)
		{
			return "UNACCEPTED";
		}

		auto t = info.Tokens().Size();
		if (id < t)
		{
			return info.Tokens()[id].Name();
		}
		else
		{
			return info.IgnoredTokens()[id - t].Name();
		}
	}

	string ToString_Variable(const ParsingMetaInfo& info, int id)
	{
		return info.Variables()[id].Name();
	}

	string ToString_Production(const ProductionInfo& p)
	{
		stringstream buf;
		buf << p.Left()->Name();
		buf << " :=";

		for (const auto& rhs_elem : p.Right())
		{
			buf << " ";
			buf << rhs_elem->Name();
		}

		return buf.str();
	}

	string ToString_ParsingAction(const ParsingMetaInfo& info, parsing::PdaEdge action)
	{
		using namespace parsing;
		if (holds_alternative<PdaEdgeShift>(action))
		{
			return Format("shift to {}", get<PdaEdgeShift>(action).target->Id());
		}
		else if (holds_alternative<PdaEdgeReduce>(action))
		{
			return Format("reduce ({})", ToString_Production(*get<PdaEdgeReduce>(action).production));
		}
		else
		{
			throw 0;
		}
	}

	void PrintParsingMetaInfo(const ParsingMetaInfo& info)
	{
		PrintFormatted("[Grammar]\n");

		PrintFormatted("tokens:\n");
		for (const auto& tok : info.Tokens())
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

		PrintFormatted("\n");
		PrintFormatted("productions:\n");
		for (const auto& p : info.Productions())
		{
			PrintFormatted("{}\n", ToString_Production(p));
		}
	}
	
	void PrintGrammar(const parsing::Grammar& g)
	{
		PrintFormatted("Extended Productions:\n");
		for (const auto& p : g.Productions())
		{
			auto ver = p->Left()->Version() ? p->Left()->Version()->Id() : -1;
			PrintFormatted("{}_{} :=", p->Left()->Info()->Name(), ver);
			for (const auto& s : p->Right())
			{
				PrintFormatted(" {}_{}", s->Key()->Name(), s->Version()->Id());
			}
			PrintFormatted("\n");
		}
		PrintFormatted("\n");

		PrintFormatted("Predicative Sets\n");
		for (const auto& pair : g.Nonterminals())
		{
			const auto& var = pair.second;

			auto ver = var.Version() ? var.Version()->Id() : -1;
			PrintFormatted("{}_{}\n", var.Info()->Name(), ver);

			PrintFormatted("FIRST = {{ ");
			for (const auto& s : var.FirstSet())
			{
				PrintFormatted("{} ", s->Info()->Name());
			}
			if (var.MayProduceEpsilon())
			{
				PrintFormatted("$epsilon ");
			}
			PrintFormatted("}}\n");

			PrintFormatted("FOLLOW = {{ ");
			for (const auto& s : var.FollowSet())
			{
				PrintFormatted("{} ", s->Info()->Name());
			}
			if (var.MayProduceEpsilon())
			{
				PrintFormatted("$eof ");
			}
			PrintFormatted("}}\n");
		}
	}


	void PrintLexingAutomaton(const ParsingMetaInfo& info, const lexing::LexingAutomaton& dfa)
	{
		using namespace eds::loli::lexing;
		PrintFormatted("[Lexing Automaton]\n");

		for (int id = 0; id < dfa.StateCount(); ++id)
		{
			const auto state = dfa.LookupState(id);
			PrintFormatted("state {}({}):\n", state->id, state->acc_token ? state->acc_token->Name() : "NOT ACCEPTED");

			for (const auto& edge : state->transitions)
			{
				PrintFormatted("  {} -> {}\n", EscapeCharacter(edge.first), edge.second->id);
			}

			PrintFormatted("\n");
		}
	}

	void PrintParsingAutomaton(const ParsingMetaInfo& info, const parsing::ParsingAutomaton& pda)
	{
		using namespace eds::loli::parsing;
		PrintFormatted("[Parsing Automaton]\n");

		for (int id = 0; id < pda.StateCount(); ++id)
		{
			const auto& state = *pda.LookupState(id);

			PrintFormatted("state {}:\n", id);

			if (state.EofAction())
			{
				PrintFormatted("  <eof> -> do {}\n",
					ToString_ParsingAction(info, *state.EofAction()));
			}

			for (const auto& pair : state.ActionMap())
			{
				PrintFormatted("  {} -> do {}\n",
					pair.first->Name(),
					ToString_ParsingAction(info, pair.second));
			}

			for (const auto& pair : state.GotoMap())
			{
				PrintFormatted("  {} -> goto state {}\n",
					pair.first->Name(),
					pair.second->Id());
			}

			PrintFormatted("\n");

		}
	}
	//*/
}