// Data classes in this header should not be marked as const by convention
// DON'T modify members manually

#pragma once
#include "ext/flat-set.hpp"
#include <string>
#include <numeric>
#include <cassert>
#include <variant>
#include <vector>

namespace eds::loli
{
	// ============================================================
	// Pre-declaration
	//

	class Symbol;
	class Terminal;
	class NonTerminal;

	class Production;

	// ============================================================
	// Definition
	//

	class Symbol
	{
	public:

		// ctors
		//

		Symbol(int id, const std::string& name)
			: id(id), name(name)
		{
			assert(id >= 0);
			assert(!name.empty());
		}

		virtual ~Symbol() = default;

		// Member accessor
		//

		int Id() const
		{
			return id;
		}
		const auto& Name() const
		{
			return name;
		}

		// Type checkers
		//

		virtual bool IsTerminal() const = 0;

		bool IsNonTerminal() const
		{
			return !IsTerminal();
		}

		// Cast shortcuts
		//

		Terminal* Symbol::AsTerminal()
		{
			assert(IsTerminal());
			return reinterpret_cast<Terminal*>(this);
		}
		NonTerminal* Symbol::AsNonTerminal()
		{
			assert(IsNonTerminal());
			return reinterpret_cast<NonTerminal*>(this);
		}

	private:
		int id;
		std::string name;
	};

	class Terminal final : public Symbol
	{
	public:
		explicit Terminal(int id, const std::string& name, const std::string& regex, bool ignored)
			: Symbol(id, name)
			, regex(regex)
			, ignored(ignored)
			, priority(ignored ? -1 : id) { }

		bool IsTerminal() const override
		{
			return true;
		}

		// Accessor
		//

		const auto& Regex() const
		{
			return regex;
		}
		bool Ignored() const
		{
			return ignored;
		}
		int Priority() const
		{
			return priority;
		}

	private:
		friend class Grammar;

		std::string regex;
		bool ignored;
		int priority;
	};

	class NonTerminal final : public Symbol
	{
	public:
		explicit NonTerminal(int id, const std::string& name)
			: Symbol(id, name) { }

		bool IsTerminal() const override
		{
			return false;
		}

		// Accessor
		//

		const auto& Productions() const
		{
			return productions;
		}

	private:
		friend class Grammar;

		std::vector<Production*> productions;
	};

	class Production
	{
	public:
		explicit Production(int id, NonTerminal* lhs, const std::vector<Symbol*>& rhs)
			: id(id), lhs(lhs), rhs(rhs) { }

		auto Left() const
		{
			return lhs;
		}
		const auto& Right() const
		{
			return rhs;
		}

	private:
		int						id;
		NonTerminal*			lhs;
		std::vector<Symbol*>	rhs;
	};
}