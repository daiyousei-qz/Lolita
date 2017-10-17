#pragma once
#include "parsing-bootstrap.h"
#include "lexing-automaton.h"
#include "parsing-automaton.h"

namespace eds::loli::debug
{
	void PrintGrammar(const ParserBootstrapInfo& info);

	void PrintLexingAutomaton(const ParserBootstrapInfo& info, const lexing::LexingAutomaton& dfa);
	void PrintParsingAutomaton(const ParserBootstrapInfo& info, const parsing::ParsingAutomaton& pda);

}