#pragma once
#include <vector>
#include <deque>
#include <memory>

namespace eds::loli::parsing
{
	struct Terminal;
	struct Nonterminal;
	struct Production;

	struct Symbol
	{
		int id; // different symbol may have the same id
		int version;

	public:
		Symbol(int id, int version)
			: id(id), version(version) { }

		virtual ~Symbol() = default;

		Terminal* AsTerminal();
		const Terminal* AsTerminal() const;
		Nonterminal* AsNonterminal();
		const Nonterminal* AsNonterminal() const;
	};

	struct Terminal : public Symbol
	{
	public:
		Terminal(int id, int version)
			: Symbol(id, version) { }
	};

	struct Nonterminal : public Symbol
	{
		// a shortcut reference to outgoing transitions
		std::vector<Production*> productions;

	public:
		Nonterminal(int id, int version)
			: Symbol(id, version) { }
	};

	struct Production
	{
		int id;
		Nonterminal* lhs;
		std::vector<Symbol*> rhs;

	public:
		Production(int id, Nonterminal* lhs, const std::vector<Symbol*>& rhs)
			: id(id), lhs(lhs), rhs(rhs) { }
	};

	class Grammar
	{
	public:
		// Basic Info
		//

		int TerminalCount() const
		{
			return terms_.size();
		}
		int NonterminalCount() const
		{
			return nonterms_.size();
		}
		int ProductionCount() const
		{
			return productions_.size();
		}

		Nonterminal* RootSymbol() const
		{
			return root_symbol_;
		}

		const auto& Grammar::Terminals() const
		{
			return terms_;
		}
		const auto& Grammar::Nonterminals() const
		{
			return nonterms_;
		}
		const auto& Grammar::Productions() const
		{
			return productions_;
		}

		void ConfigureRootSymbol(Nonterminal* s);

		Terminal* NewTerm(int id, int version = 0);
		Nonterminal* NewNonterm(int id, int version = 0);
		Production* NewProduction(int id, Nonterminal* lhs, const std::vector<Symbol*>& rhs);

	private:
		Nonterminal* root_symbol_;

		// use deque to avoid pointer invalidation
		std::deque<Terminal> terms_;
		std::deque<Nonterminal> nonterms_;
		std::deque<Production> productions_;
	};
}