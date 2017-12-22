#pragma once
#include "lolita/core/parsing-info.h"
#include "ext/flat-set.h"
#include <memory>
#include <vector>
#include <map>

namespace eds::loli::parsing
{
	class ParsingState;

	class Symbol;
	class Terminal;
	class Nonterminal;
	class Production;

	using TerminalSet = FlatSet<Terminal*>;
	using SymbolVec = std::vector<Symbol*>;

	// (id, version)
	using SymbolKey = std::tuple<const SymbolInfo*, const ParsingState*>;

	// ==============================================================
	// Symbols
	//

	class Symbol
	{
	public:
		Symbol(const SymbolInfo* key, const ParsingState* version)
			: key_(key), version_(version) { }

		virtual ~Symbol() = default; // to enable vtable

		const auto& Key() const { return key_; }
		const auto& Version() const { return version_; }

		Terminal*			AsTerminal();
		Nonterminal*		AsNonterminal();

	private:
		const SymbolInfo* key_;
		const ParsingState* version_;
	};

	class Terminal : public Symbol
	{
	public:
		Terminal(const TokenInfo* info, const ParsingState* version)
			: info_(info), Symbol(info, version) { }

		const auto& Info() const { return info_; }

	private:
		const TokenInfo* info_;
	};

	class Nonterminal : public Symbol
	{
	public:
		Nonterminal(const VariableInfo* info, const ParsingState* version)
			: info_(info), Symbol(info, version) { }

		const auto& Info() const { return info_; }
		const auto& Productions() const { return productions_; }

		const auto& MayProduceEpsilon() const { return may_produce_epsilon_; }
		const auto& MayPreceedEof() const { return may_preceed_eof_; }	
		const auto& FirstSet() const { return first_set_; }
		const auto& FollowSet() const { return follow_set_; }

	private:
		friend class GrammarBuilder;

		const VariableInfo* info_;
		std::vector<Production*> productions_ = {};

		// predicative information
		bool may_produce_epsilon_ = false;
		bool may_preceed_eof_     = false;

		TerminalSet first_set_    = {};
		TerminalSet follow_set_   = {};
	};

	class Production
	{
	public:
		Production(const ProductionInfo* info, Nonterminal* lhs, const SymbolVec& rhs)
			: info_(info), lhs_(lhs), rhs_(rhs) { }

		const auto& Info() const { return info_; }
		const auto& Left() const { return lhs_; }
		const auto& Right() const { return rhs_; }

	private:
		friend class GrammarBuilder;

		const ProductionInfo* info_;

		Nonterminal* lhs_;
		SymbolVec rhs_;
	};

	// ==============================================================
	// Grammar
	//

	class Grammar
	{
	public:
		const auto& RootSymbol() const { return root_symbol_; }
		const auto& Terminals() const { return terms_; }
		const auto& Nonterminals() const { return nonterms_; }
		const auto& Productions() const { return productions_; }

		Terminal* LookupTerminal(SymbolKey key);
		Nonterminal* LookupNonterminal(SymbolKey key);

	private:
		friend class GrammarBuilder;

		Nonterminal* root_symbol_;

		std::map<SymbolKey, Terminal> terms_;
		std::map<SymbolKey, Nonterminal> nonterms_;

		std::vector<std::unique_ptr<Production>> productions_;
	};

	class GrammarBuilder
	{
	public:
		Terminal*	 MakeTerminal(const TokenInfo* info, const ParsingState* version);
		Nonterminal* MakeNonterminal(const VariableInfo* info, const ParsingState* version);
		Symbol*      MakeGenericSymbol(const SymbolInfo* info, const ParsingState* version);

		void CreateProduction(const ProductionInfo* info, Nonterminal* lhs, const SymbolVec& rhs);

		std::unique_ptr<Grammar> Build(Nonterminal* root);

	private:
		void ComputeFirstSet();
		void ComputeFollowSet();

		std::unique_ptr<Grammar> site_ = std::make_unique<Grammar>();
	};
}
