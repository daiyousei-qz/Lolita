#pragma once
#include "grammar.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>

namespace eds::loli::config
{
	// Type
	//

	struct QualType
	{
		std::string name;
		std::string qual;
	};

	// Token
	//

	struct TokenDefinition
	{
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
		QualType type;
		std::string name;
	};

	struct NodeDefinition
	{
		std::string name;
		std::string parent;

		std::vector<NodeMember> members;
	};

	// Rule
	//

	struct RuleSymbol
	{
		std::string symbol;
		std::string assign;
	};
	struct RuleItem
	{
		std::vector<RuleSymbol> rhs;
		std::optional<QualType> klass_hint;
	};

	struct RuleDefinition
	{
		QualType type;

		std::string lhs;
		std::vector<RuleItem> items;
	};

	// Config
	//

	struct Config
	{
		std::vector<TokenDefinition> tokens;
		std::vector<TokenDefinition> ignored_tokens;
		std::vector<EnumDefinition> enums;
		std::vector<BaseDefinition> bases;
		std::vector<NodeDefinition> nodes;
		std::vector<RuleDefinition> rules;
	};

	std::unique_ptr<Config> ParseConfig(const char* data);
	// TODO: void ValidateConfig(const Config& config);
}