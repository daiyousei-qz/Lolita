#include "grammar.h"
#include <cassert>

using namespace std;

namespace eds::loli::parsing
{
	// ==============================================================
	// Implementation Of Symbol

	Terminal* Symbol::AsTerminal()
	{
		return dynamic_cast<Terminal*>(this);
	}
	const Terminal* Symbol::AsTerminal() const
	{
		return dynamic_cast<const Terminal*>(this);
	}
	Nonterminal* Symbol::AsNonterminal()
	{
		return dynamic_cast<Nonterminal*>(this);
	}
	const Nonterminal* Symbol::AsNonterminal() const
	{
		return dynamic_cast<const Nonterminal*>(this);
	}

	// ==============================================================
	// Implementation Of Grammar

	void Grammar::ConfigureRootSymbol(Nonterminal* s)
	{
		root_symbol_ = s;
	}

	Terminal* Grammar::NewTerm(int id, int version)
	{
		return &terms_.emplace_back(id, version);
	}

	Nonterminal* Grammar::NewNonterm(int id, int version)
	{
		return &nonterms_.emplace_back(id, version);
	}

	Production* Grammar::NewProduction(int id, Nonterminal* lhs, const vector<const Symbol*>& rhs, int version)
	{
		assert(!rhs.empty());

		auto result = &productions_.emplace_back(id, lhs, rhs, version);
		lhs->productions.push_back(result);

		return result;
	}
}