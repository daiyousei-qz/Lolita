#pragma once
#include "grammar.h"
#include "grammar-analytic.h"
#include "parser.h"
#include <set>

namespace lolita
{
	std::string QuickName(const Grammar& g, Symbol s, int src, int target);

	std::string TextifySymbol(const Grammar& g, Symbol s);

	std::string TextifyProduction(const Grammar& g, const Production& p);

	void PrintGrammar(const Grammar& grammar);

	void PrintPredictiveSet(const Grammar& g, const PredictiveSet& ps);

	void PrintCanonicalItems(const Grammar& grammar, const std::set<FlatSet<Item>>& collection);
}