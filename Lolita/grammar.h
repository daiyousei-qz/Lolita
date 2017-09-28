#pragma once
#include "symbol.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <numeric>
#include <deque>

namespace lolita
{
	// TODO: make this shared utilities for both lexer and parser
	//	     i.e. add lexical definitions of terminals

	// representation of a grammar
	// NOTE nonterm with largest id is root by convention
	// NOTE last production in list is root by convention
	class Grammar
	{
	private:
		friend class GrammarBuilder;
		struct ConstructionDummy { };

	public:
		using Ptr = std::unique_ptr<Grammar>;
		using SharedPtr = std::shared_ptr<Grammar>;

		Grammar(ConstructionDummy = {}) { }

		size_t TerminalCount() const { return terms_.size(); }
		size_t NonTerminalCount() const { return nonterms_.size(); }
		size_t ProductionCount() const { return productions_.size(); }

		NonTerminal RootSymbol() const { return GetNonTerminal(NonTerminalCount() - 1); }
		const auto& RootProduction() const { return productions_.back(); }

		const auto& TerminalNames()const { return terms_; }
		const auto& NonTerminalNames()const { return nonterms_; }
		const auto& Productions() const { return productions_; }

		void EnumerateTerminals(std::function<void(Terminal)> callback) const
		{
			for (auto id = 0u; id < terms_.size(); ++id)
			{
				callback(Terminal{ id });
			}
		}
		void EnumerateNonTerminals(std::function<void(NonTerminal)> callback) const
		{
			for (auto id = 0u; id < nonterms_.size(); ++id)
			{
				callback(NonTerminal{ id });
			}
		}
		void EnumerateSymbol(std::function<void(Symbol)> callback) const
		{
			EnumerateTerminals(callback);
			EnumerateNonTerminals(callback);
		}

		void EnumerateProduction(std::function<void(const Production&)> callback) const
		{
			for (const auto& p : productions_)
			{
				callback(p);
			}
		}
		void EnumerateProduction(NonTerminal lhs, std::function<void(const Production&)> callback) const
		{
			for (const auto& p : productions_)
			{
				if (p.Left() == lhs)
				{
					callback(p);
				}
			}
		}

		Terminal GetTerminal(unsigned id) const
		{
			assert(id < TerminalCount());
			return Terminal{ id };
		}
		NonTerminal GetNonTerminal(unsigned id) const
		{
			assert(id < NonTerminalCount());
			return NonTerminal{ id };
		}
		const auto& LookupName(Symbol s) const
		{
			return (s.IsTerminal() ? terms_ : nonterms_)[s.Id()];
		}

	private:
		StringVec terms_; // names of terminal symbols
		StringVec nonterms_; // names of non-terminal symbols

		std::deque<Production> productions_;
	};

	// TODO: adds grammar validation?
	class GrammarBuilder
	{
	public:
		Terminal NewTerm(const std::string& name)
		{
			auto id = terms_.size();
			terms_.push_back(name);

			return Terminal(id);
		}

		NonTerminal NewNonTerm(const std::string& name)
		{
			auto id = nonterms_.size();
			nonterms_.push_back(name);

			return NonTerminal(id);
		}

		const Production& NewProduction(NonTerminal lhs, const std::vector<Symbol>& rhs)
		{
			productions_.emplace_back(lhs, rhs);
			return productions_.back();
		}

		Grammar::SharedPtr Build(NonTerminal top);

	private:
		StringVec terms_;
		StringVec nonterms_;

		// use deque to prevent reference invalidation
		std::deque<Production> productions_;
	};
}