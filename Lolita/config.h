#pragma once
#include "grammar.h"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace eds::loli
{
	struct AstConstructionHint
	{
		std::string command;
		std::vector<int> params;

		// Possible commands:
		// - NodeName
		// - &
		// - @n
		//
		// NOTE only first two commands accept parameters
		// if command is empty, it goes default
	};

	using AstConstructionLookup =
		std::unordered_map<Production*, AstConstructionHint>;


	// Type
	//

	struct TypeSpec
	{
		std::string name;
		std::string qual;
	};

	// Token
	//

	struct TokenDefinition
	{
		bool ignore;

		std::string name;
		std::string regex;
	};

	// Enum
	//

	struct EnumDefinition
	{
		std::string name;
		std::vector<std::string> choices;
	};

	// Node
	//

	struct BaseDefinition
	{
		std::string name;
	};

	struct NodeMember
	{
		TypeSpec type;
		std::string name;
	};

	struct NodeDefinition
	{
		std::string parent;
		std::string name;

		std::vector<NodeMember> members;
	};

	// Rule
	//

	struct RuleItem
	{
		std::string symbol;
		std::string assign;
	};

	struct RuleDefinition
	{
		TypeSpec type;

		std::string lhs;
		std::string klass_hint;
		std::vector<RuleItem> rhs;
	};

	// Config
	//

	struct Config
	{
		std::vector<TokenDefinition> tokens;
		std::vector<EnumDefinition> enums;
		std::vector<BaseDefinition> bases;
		std::vector<NodeDefinition> nodes;
		std::vector<RuleDefinition> rules;
	};

	std::unique_ptr<Config> LoadConfig();
	void ValidateConfig(const Config& config);
}