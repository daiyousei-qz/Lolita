#pragma once
#include "core/parsing-info.h"
#include "lexing/lexing-automaton.h"
#include "parsing/parsing-automaton.h"
#include "parsing/grammar.h"

namespace eds::loli::debug
{
    std::string ToString_Token(const ParsingMetaInfo& info, int id);
    std::string ToString_Variable(const ParsingMetaInfo& info, int id);
    std::string ToString_Production(const ProductionInfo& p);

    void PrintParsingMetaInfo(const ParsingMetaInfo& info);
    void PrintGrammar(const parsing::Grammar& g);

    void PrintLexingAutomaton(const ParsingMetaInfo& info, const lexing::LexingAutomaton& dfa);
    void PrintParsingAutomaton(const ParsingMetaInfo& info, const parsing::ParsingAutomaton& pda);
}