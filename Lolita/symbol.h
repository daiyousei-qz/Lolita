#pragma once
#include "lolita-basic.h"
#include "flat-set.hpp"
#include <string>
#include <numeric>
#include <cassert>
#include <variant>
#include <vector>

namespace lolita
{
	using SymbolId = unsigned;

	class Symbol;
	class Terminal;
	class NonTerminal;

	using TermSet = FlatSet<Terminal>;

	namespace detail
	{
		class SymbolBase
		{
		public:
			SymbolBase(SymbolId id)
				: id_(id) { }

			SymbolId Id() const { return id_; }

		private:
			SymbolId id_;
		};
	}

	class Terminal : public detail::SymbolBase
	{
	private:
		friend class Grammar;
		friend class GrammarBuilder;
		explicit Terminal(SymbolId id) : SymbolBase(id) { }
	};

	class NonTerminal : public detail::SymbolBase
	{
	private:
		friend class Grammar;
		friend class GrammarBuilder;
		explicit NonTerminal(SymbolId id) : SymbolBase(id) { }
	};

	class Symbol
	{
	public:
		Symbol(Terminal term) : data_(term) { }
		Symbol(NonTerminal nonterm) : data_(nonterm) { }

		SymbolId Id() const
		{
			return std::visit([](auto s) { return s.Id(); }, data_);
		}

		auto Data() const
		{
			return data_;
		}

		auto AsTerminal() const { return std::get<Terminal>(data_); }
		auto AsNonTerminal() const { return std::get<NonTerminal>(data_); }

		bool IsTerminal() const { return std::holds_alternative<Terminal>(data_); }
		bool IsNonTerminal() const { return std::holds_alternative<NonTerminal>(data_); }

	private:
		std::variant<Terminal, NonTerminal> data_;
	};

	class Production
	{
	public:
		Production(NonTerminal lhs, std::vector<Symbol> rhs)
			: lhs_(lhs), rhs_(rhs) { }

		auto Left() const { return lhs_; }
		const auto& Right() const { return rhs_; }

	private:
		NonTerminal lhs_;
		std::vector<Symbol> rhs_;
	};

	struct Item
	{
		const Production* production;
		unsigned pos;
	};

	// Operator Overloads for Item
	//

	inline bool operator==(Item lhs, Item rhs)
	{
		return lhs.production == rhs.production && lhs.pos == rhs.pos;
	}
	inline bool operator!=(Item lhs, Item rhs)
	{
		return !(lhs == rhs);
	}
	inline bool operator>(Item lhs, Item rhs)
	{
		if (lhs.production > rhs.production) return true;
		if (lhs.production < rhs.production) return false;

		return lhs.pos > rhs.pos;
	}
	inline bool operator>=(Item lhs, Item rhs)
	{
		return lhs == rhs || lhs > rhs;
	}
	inline bool operator<(Item lhs, Item rhs)
	{
		return !(lhs >= rhs);
	}
	inline bool operator<=(Item lhs, Item rhs)
	{
		return !(lhs > rhs);
	}

	// Operator Overloads Of Terminal
	//

	inline bool operator==(Terminal lhs, Terminal rhs)
	{
		return lhs.Id() == rhs.Id();
	}
	inline bool operator!=(Terminal lhs, Terminal rhs)
	{
		return !(lhs == rhs);
	}
	inline bool operator>(Terminal lhs, Terminal rhs)
	{
		return lhs.Id() > rhs.Id();
	}
	inline bool operator>=(Terminal lhs, Terminal rhs)
	{
		return lhs.Id() >= rhs.Id();
	}
	inline bool operator<(Terminal lhs, Terminal rhs)
	{
		return !(lhs >= rhs);
	}
	inline bool operator<=(Terminal lhs, Terminal rhs)
	{
		return !(lhs > rhs);
	}

	// Operator Overloads Of NonTerminal
	//

	inline bool operator==(NonTerminal lhs, NonTerminal rhs)
	{
		return lhs.Id() == rhs.Id();
	}
	inline bool operator!=(NonTerminal lhs, NonTerminal rhs)
	{
		return !(lhs == rhs);
	}
	inline bool operator>(NonTerminal lhs, NonTerminal rhs)
	{
		return lhs.Id() > rhs.Id();
	}
	inline bool operator>=(NonTerminal lhs, NonTerminal rhs)
	{
		return lhs.Id() >= rhs.Id();
	}
	inline bool operator<(NonTerminal lhs, NonTerminal rhs)
	{
		return !(lhs >= rhs);
	}
	inline bool operator<=(NonTerminal lhs, NonTerminal rhs)
	{
		return !(lhs > rhs);
	}

	// Operator Overloads Of Symbol
	//
	inline bool operator==(Symbol lhs, Symbol rhs)
	{
		return lhs.Data() == rhs.Data();
	}
	inline bool operator!=(Symbol lhs, Symbol rhs)
	{
		return !(lhs == rhs);
	}
}