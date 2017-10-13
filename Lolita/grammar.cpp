#include "grammar.h"

using namespace std;

namespace eds::loli
{
	// ==============================================================
	// Implementation Of Grammar

	// Construction
	//

	void Grammar::ConfigureRootSymbol(NonTerminal* s)
	{
		root_symbol_ = s;
	}

	Terminal* Grammar::NewTerm(const string& name, const string& regex, bool ignored)
	{
		terms_.push_back(
			make_unique<Terminal>(terms_.size(), name, regex, ignored)
		);

		return terms_.back().get();
	}

	NonTerminal* Grammar::NewNonTerm(const string& name)
	{
		nonterms_.push_back(
			make_unique<NonTerminal>(nonterms_.size(), name)
		);

		return nonterms_.back().get();
	}

	Production* Grammar::NewProduction(NonTerminal* lhs, const vector<Symbol*>& rhs)
	{
		assert(!rhs.empty());

		productions_.push_back(
			make_unique<Production>(productions_.size(), lhs, rhs)
		);

		// update records in nonterm definition
		auto result = productions_.back().get();
		lhs->productions.push_back(result);

		return result;
	}
}