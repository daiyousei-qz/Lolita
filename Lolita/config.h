#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <memory>

namespace eds::loli::config
{
	// Type
	//

	struct QualType
	{
		std::string name; // id
		std::string qual; // "" or "vec"
	};

	// Token
	//

	struct TokenDefinition
	{
		std::string name; // id
		std::string regex; // "regex" QUOTED
	};

	// Enum
	//

	struct EnumDefinition
	{
		std::string name; // id
		std::vector<std::string> choices; // id
	};

	// Node
	//

	struct BaseDefinition
	{
		std::string name; // id
	};

	struct NodeMember
	{
		QualType type;
		std::string name; // id
	};

	struct NodeDefinition
	{
		std::string name; // id
		std::string parent; // ""

		std::vector<NodeMember> members;
	};

	// Rule
	//

	struct RuleSymbol
	{
		std::string symbol; // id
		std::string assign; // & or ! or id
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

	struct ParsingConfiguration
	{
		std::vector<TokenDefinition> tokens;
		std::vector<TokenDefinition> ignored_tokens;
		std::vector<EnumDefinition> enums;
		std::vector<BaseDefinition> bases;
		std::vector<NodeDefinition> nodes;
		std::vector<RuleDefinition> rules;
	};

	std::unique_ptr<ParsingConfiguration> LoadConfig(const char* data);
	// TODO: void ValidateConfig(const Config& config);
}