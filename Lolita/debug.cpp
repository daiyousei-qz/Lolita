#include "debug.h"

namespace lolita
{
	std::string QuickName(const Grammar& g, Symbol s, int src, int target)
	{
		char buf[1024] = {};
		sprintf(buf, "%s(%d-%d)", g.LookupName(s).c_str(), src, target);

		return std::string(buf);
	}

	std::string TextifySymbol(const Grammar& g, Symbol s)
	{
		return g.LookupName(s);
	}

	std::string TextifyProduction(const Grammar& g, const Production& p)
	{
		char buf[1024] = {};
		auto offset = sprintf(buf, "%s ->", g.LookupName(p.Left()).c_str());

		for (auto rhs_item : p.Right())
		{
			offset += sprintf(buf + offset, " %s", g.LookupName(rhs_item).c_str());
		}

		return std::string(buf);
	}

	void PrintGrammar(const Grammar& grammar)
	{
		// prints terminals
		//
		printf("==================================\n");
		printf("> Terminals ======================\n\n");

		grammar.EnumerateTerminals([&](Terminal t) {

			printf("%s\n", grammar.LookupName(t).c_str());
		});
		printf("\n");

		// prints non-terminals
		//
		printf("==================================\n");
		printf("> Non-terminals ==================\n\n");

		grammar.EnumerateNonTerminals([&](NonTerminal s) {

			printf("%s\n", grammar.LookupName(s).c_str());
		});
		printf("\n");

		// prints productions
		//
		printf("==================================\n");
		printf("> Productions ====================\n\n");

		grammar.EnumerateProduction([&](const Production& p) {

			printf("%s\n", TextifyProduction(grammar, p).c_str());
		});
		printf("\n");
	}

	void PrintPredictiveSet(const Grammar& g, const PredictiveSet& ps)
	{
		// prints first sets
		//
		printf("==================================\n");
		printf("> FIRST SETS =====================\n\n");

		g.EnumerateNonTerminals([&](NonTerminal s) {

			printf("%s:", g.LookupName(s).c_str());
			for (auto term : ps.First(s))
			{
				printf(" %s", g.LookupName(term).c_str());
			}

			if (ps.ProduceEpsilon(s))
			{
				printf(" _EPSILON");
			}

			printf("\n");
		});

		// prints follow sets
		//
		printf("==================================\n");
		printf("> FOLLOW SETS ====================\n\n");

		g.EnumerateNonTerminals([&](NonTerminal s) {

			printf("%s:", g.LookupName(s).c_str());
			for (auto term : ps.Follow(s))
			{
				printf(" %s", g.LookupName(term).c_str());
			}

			if (ps.EndingSymbol(s))
			{
				printf(" _EOF");
			}

			printf("\n");
		});
	}

	void PrintCanonicalItems(const Grammar& grammar, const std::set<FlatSet<Item>>& collection)
	{
		auto i = 0u;
		for (const auto& state : collection)
		{
			printf("I%d\n", i++);
			for (auto item : state)
			{
				const auto& lhs = item.production->Left();
				const auto& rhs = item.production->Right();

				printf("  %s ->", grammar.LookupName(lhs).c_str());

				for (auto j = 0u; j < rhs.size(); ++j)
				{
					if (item.pos == j)
					{
						printf(" .");
					}

					printf(" %s", grammar.LookupName(rhs[j]).c_str());
				}

				if (item.pos == rhs.size())
				{
					printf(" .");
				}

				printf("\n");
			}

			printf("\n");
		}
	}
}