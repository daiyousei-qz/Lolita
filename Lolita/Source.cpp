#include <cstdio>
#include "grammar.h"
#include "debug.h"

namespace lolita
{
	Parser::Ptr CreateSLRParser(const Grammar::SharedPtr& g);
	Parser::Ptr CreateLALRParser(const Grammar::SharedPtr& g);
}

using namespace lolita;

Grammar::SharedPtr CreateGrammar()
{
	GrammarBuilder builder{};

	auto term_id = builder.NewTerm("id");
	auto term_plus = builder.NewTerm("+");
	auto term_times = builder.NewTerm("*");
	auto term_lp = builder.NewTerm("(");
	auto term_rp = builder.NewTerm(")");

	auto nonterm_E = builder.NewNonTerm("E");
	auto nonterm_T = builder.NewNonTerm("T");
	auto nonterm_F = builder.NewNonTerm("F");

	builder.NewProduction(nonterm_E, { nonterm_T });
	builder.NewProduction(nonterm_E, { nonterm_E, term_plus, nonterm_T });

	builder.NewProduction(nonterm_T, { nonterm_F});
	builder.NewProduction(nonterm_T, { nonterm_T, term_times, nonterm_F });

	builder.NewProduction(nonterm_F, { term_lp, nonterm_E, term_rp });
	builder.NewProduction(nonterm_F, { term_id });

	return builder.Build(nonterm_E);
}

Grammar::SharedPtr CreateGrammar2()
{
	GrammarBuilder builder{};

	auto term_eq = builder.NewTerm("=");
	auto term_x = builder.NewTerm("x");
	auto term_star = builder.NewTerm("*");

	auto nonterm_N = builder.NewNonTerm("N");
	auto nonterm_E = builder.NewNonTerm("E");
	auto nonterm_V = builder.NewNonTerm("V");

	builder.NewProduction(nonterm_N, { nonterm_V, term_eq, nonterm_E });
	builder.NewProduction(nonterm_N, { nonterm_E });

	builder.NewProduction(nonterm_E, { nonterm_V });

	builder.NewProduction(nonterm_V, { term_x });
	builder.NewProduction(nonterm_V, { term_star, nonterm_E });

	return builder.Build(nonterm_N);
}

int main()
{
	auto grammar = CreateGrammar2();
	PrintGrammar(*grammar);

	auto parser = CreateLALRParser(grammar);
	printf("\n");
	parser->Print();

	ParsingStack ctx;
	parser->Feed(ctx, 2);
	parser->Feed(ctx, 1);
	parser->Finalize(ctx);

	system("pause");
}