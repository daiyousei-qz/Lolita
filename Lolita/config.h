#pragma once
#include "lolita-basic.h"
#include <string>
#include <optional>
#include <variant>

namespace lolita
{
	// Ast Model for config file
	//

	struct TokenDecl
	{
		bool ignore;
		string name;
		string regex;
	};

	struct EnumDecl
	{
		string name;
		vector<string> values;
	};

	struct NodeDecl
	{
		string name;
		std::optional<string> parent;

		vector<std::pair<string, string>> members;
	};

	struct AstTypeHint
	{
		string type;
	};
	struct AstTypeCtorHint
	{
		string type;
		vector<int> refs;
	};
	struct AstRefHint
	{
		int ref;
	};
	struct AstConcatHint
	{
		vector<int> refs;
	};

	using AstConstructionHint = std::variant<AstTypeHint,
		AstTypeCtorHint,
		AstRefHint,
		AstConcatHint>;

	struct RuleItem
	{
		vector<string> rhs;
		AstConstructionHint hint;
	};
	struct RuleDecl
	{
		bool root;
		string name;
		string type;
		vector<RuleItem> productions;
	};

	using GrammarStmt = std::variant<TokenDecl, EnumDecl, NodeDecl, RuleDecl>;
	using GrammarStmtVec = vector<GrammarStmt>;

	GrammarStmtVec LoadConfig(const std::string& path);
}