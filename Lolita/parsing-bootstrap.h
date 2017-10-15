#pragma once
#include "ast-handle.h"
#include "grammar.h"
#include "config.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

namespace eds::loli
{
	class TypeInfo;
	class EnumInfo;
	class BaseInfo;
	class KlassInfo;

	class SymbolInfo;
	class TokenInfo;
	class VariableInfo;

	class ProductionInfo;

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
	};

	struct TokenDummyType : public TypeInfo
	{
	private:
		TokenDummyType() : TypeInfo("token"s) { }

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
		std::vector<ProductionInfo*> productions;

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
		VariableInfo* lhs;
		std::vector<SymbolInfo*> rhs;

		std::unique_ptr<AstHandle> handle;
	};

	class ParserBootstrapInfo
	{
	public:

		const auto& Enums() const { return enums_; }
		const auto& Bases() const { return bases_; }
		const auto& Klasses() const { return klasses_; }

		const auto& Tokens() const { return tokens_; }
		const auto& IgnoredTokens() const { return ignored_tokens_; }
		const auto& Variables() const { return variables_; }
		const auto& Productions() const { return productions_; }


	private:
		std::vector<EnumInfo> enums_;
		std::vector<BaseInfo> bases_;
		std::vector<KlassInfo> klasses_;

		std::vector<TokenInfo> tokens_;
		std::vector<TokenInfo> ignored_tokens_;
		std::vector<VariableInfo> variables_;

		std::vector<ProductionInfo> productions_;

		std::unordered_map<std::string, TypeInfo*> type_lookup_;
		std::unordered_map<std::string, SymbolInfo*> symbol_lookup_;
	};

	std::unique_ptr<ParserBootstrapInfo> BootstrapParser(const Config& config);
}