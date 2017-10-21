#pragma once
#include "parsing-bootstrap.h"
#include "lexing-automaton.h"
#include "parsing-automaton.h"

namespace eds::loli::debug
{
	std::string ToString_Token(const ParserBootstrapInfo& info, int id);
	std::string ToString_Variable(const ParserBootstrapInfo& info, int id);
	std::string ToString_Production(const ParserBootstrapInfo& info, int id);

	void PrintGrammar(const ParserBootstrapInfo& info);

	void PrintLexingAutomaton(const ParserBootstrapInfo& info, const lexing::LexingAutomaton& dfa);
	void PrintParsingAutomaton(const ParserBootstrapInfo& info, const parsing::ParsingAutomaton& pda);

}