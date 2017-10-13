#pragma once
#include "symbol.h"
#include <vector>
#include <memory>

namespace eds::loli
{
	class Grammar
	{
	public:

		// Basic Info
		//

		int TerminalCount() const
		{
			return terms_.size();
		}
		int NonTerminalCount() const
		{
			return nonterms_.size();
		}
		int ProductionCount() const
		{
			return productions_.size();
		}

		NonTerminal* RootSymbol() const
		{
			return root_symbol_;
		}

		const auto& Grammar::Terminals() const
		{
			return terms_;
		}
		const auto& Grammar::IgnoredTerminals() const
		{
			return ignored_terms_;
		}
		const auto& Grammar::NonTerminals() const
		{
			return nonterms_;
		}
		const auto& Grammar::Productions() const
		{
			return productions_;
		}

		// Construction
		//

		void ConfigureRootSymbol(NonTerminal* s);

		Terminal* NewTerm(const std::string& name, const std::string& regex, bool ignored);
		NonTerminal* NewNonTerm(const std::string& name);
		Production* NewProduction(NonTerminal* lhs, const std::vector<Symbol*>& rhs);

	private:
		NonTerminal* root_symbol_;

		std::vector<std::unique_ptr<Terminal>> terms_;
		std::vector<std::unique_ptr<Terminal>> ignored_terms_;

		std::vector<std::unique_ptr<NonTerminal>> nonterms_;
		std::vector<std::unique_ptr<Production>> productions_;
	};
}