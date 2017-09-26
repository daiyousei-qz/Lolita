#pragma once
#include "grammar.h"
#include "grammar-analytic.h"
#include "parser.h"
#include "lexer.h"
#include <set>

namespace lolita
{
	std::string QuickName(const Grammar& g, Symbol s, int src, int target);

	std::string TextifySymbol(const Grammar& g, Symbol s);

	std::string TextifyProduction(const Grammar& g, const Production& p);

	void PrintGrammar(const Grammar& grammar);

	void PrintPredictiveSet(const Grammar& g, const PredictiveSet& ps);

	void PrintCanonicalItems(const Grammar& grammar, const std::set<FlatSet<Item>>& collection);

	inline void PrintLexer(const Lexer& lexer)
	{
		for (int s = 0; s < lexer.StateCount(); ++s)
		{
			printf("DfaState %d", s);
			if (lexer.IsAccepting(s))
			{
				auto category = lexer.LookupAcceptingCategory(s);
				printf(" | %d - %s", category, lexer.LookupCategoryName(category).c_str());
			}
			printf("\n");

			for (int ch = 0; ch < 128; ++ch)
			{
				auto target = lexer.Transit(s, ch);
				if (target != -1)
				{
					printf("  char of %c --> DfaState %d\n", ch, target);
				}
			}
		}
	}
}