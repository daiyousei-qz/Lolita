#pragma once
#include "ast-handle.h"
#include "config.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace eds::loli
{
	class TypeInfo;
	struct EnumInfo;
	struct BaseInfo;
	struct KlassInfo;

	class SymbolInfo;
	struct TokenInfo;
	struct VariableInfo;

	struct ProductionInfo;

	// Type metadata
	//

	class TypeInfo
	{
	public:
		enum class Category
		{
			Token, Enum, Base, Klass,
		};

		TypeInfo(const std::string& name)
			: name_(name) { }

		const auto& Name() const { return name_; }

		virtual Category GetCategory() const = 0;

		bool IsToken() const { return GetCategory() == Category::Token; }
		bool IsEnum()  const { return GetCategory() == Category::Enum;  }
		bool IsBase()  const { return GetCategory() == Category::Base;  }
		bool IsKlass() const { return GetCategory() == Category::Klass; }

		// stored as pointer in AST
		bool IsStoredByRef() const 
		{
			auto category = GetCategory();
			return category == Category::Base || category == Category::Klass;
		}

	private:
		std::string name_;
	};

	struct TypeSpec
	{
		enum class Qualifier
		{
			None, Vector
		};

		Qualifier qual;
		TypeInfo* type;

	public:
		bool IsVector() const
		{
			return qual == Qualifier::Vector;
		}
	};

	struct TokenDummyType : public TypeInfo
	{
	private:
		TokenDummyType() : TypeInfo("token") { }

	public:

		static TokenDummyType& Instance()
		{
			static TokenDummyType dummy{};

			return dummy;
		}

		Category GetCategory() const override
		{
			return Category::Token;
		}
	};

	struct EnumInfo : public TypeInfo
	{
		std::vector<std::string> values;

	public:
		EnumInfo(const std::string& name)
			: TypeInfo(name) { }

		Category GetCategory() const override
		{
			return Category::Enum;
		}
	};

	struct BaseInfo : public TypeInfo
	{
	public:
		BaseInfo(const std::string& name)
			: TypeInfo(name) { }

		Category GetCategory() const override
		{
			return Category::Base;
		}
	};

	struct KlassInfo : public TypeInfo
	{
		struct Member
		{
			TypeSpec type;
			std::string name;
		};

		BaseInfo* base;
		std::vector<Member> members;

	public:
		KlassInfo(const std::string& name)
			: TypeInfo(name) { }

		Category GetCategory() const override
		{
			return Category::Klass;
		}
	};

	// Symbol metadata
	//

	class SymbolInfo
	{
	public:
		enum class Category
		{
			Token, Variable
		};

		SymbolInfo(const std::string& name)
			: name_(name) { }

		const auto& Name() const { return name_; }

		virtual Category GetCategory() const = 0;

		bool IsToken()    const { return GetCategory() == Category::Token; }
		bool IsVariable() const { return GetCategory() == Category::Variable; }

	private:
		std::string name_;
	};

	struct TokenInfo : public SymbolInfo
	{
		std::string regex;

	public:
		TokenInfo(const std::string& name)
			: SymbolInfo(name) { }

		Category GetCategory() const override 
		{
			return Category::Token; 
		}
	};

	struct VariableInfo : public SymbolInfo
	{
		TypeSpec type;
		std::vector<const ProductionInfo*> productions;

	public:
		VariableInfo(const std::string& name)
			: SymbolInfo(name) { }

		Category GetCategory() const override 
		{
			return Category::Variable; 
		}
	};

	// Production metadata
	//

	struct ProductionInfo
	{
		const VariableInfo* lhs;
		std::vector<const SymbolInfo*> rhs;

		std::unique_ptr<AstHandle> handle;
	};

	class ParserBootstrapInfo
	{
	public:
		using TypeInfoMap = std::unordered_map<std::string, TypeInfo*>;
		using SymbolInfoMap = std::unordered_map<std::string, SymbolInfo*>;

		struct ParserBootstrapContext
		{
			TypeInfoMap type_lookup;
			std::vector<EnumInfo> enums;
			std::vector<BaseInfo> bases;
			std::vector<KlassInfo> klasses;

			SymbolInfoMap symbol_lookup;
			std::vector<TokenInfo> tokens;
			std::vector<TokenInfo> ignored_tokens;
			std::vector<VariableInfo> variables;
			std::vector<ProductionInfo> productions;
		};

		ParserBootstrapInfo(ParserBootstrapContext data)
			: ctx_(std::move(data)) { }

		const auto& RootVariable()  const { return ctx_.variables.back(); }

		const auto& Enums()			const { return ctx_.enums; }
		const auto& Bases()			const { return ctx_.bases; }
		const auto& Klasses()		const { return ctx_.klasses; }

		const auto& Tokens()		const { return ctx_.tokens; }
		const auto& IgnoredTokens() const { return ctx_.ignored_tokens; }
		const auto& Variables()		const { return ctx_.variables; }
		const auto& Productions()	const { return ctx_.productions; }

		const auto& LookupType(const std::string& name)
		{
			return *ctx_.type_lookup.at(name);
		}
		const auto& LookupSymbol(const std::string& name)
		{
			return *ctx_.symbol_lookup.at(name);
		}

	private:
		friend std::unique_ptr<ParserBootstrapInfo> BootstrapParser(const config::ParsingConfiguration&);

		ParserBootstrapContext ctx_;
	};

	std::unique_ptr<ParserBootstrapInfo> BootstrapParser(const config::ParsingConfiguration& config);
}