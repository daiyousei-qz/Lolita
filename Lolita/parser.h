#pragma once
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include <string_view>
#include <memory>

namespace eds::loli
{
	template <typename T>
	struct ParsingResult
	{
		std::unique_ptr<Arena> arena;

	};

	class Parser
	{
	public:
		void Parse(std::string_view data)
		{
			// lex and parse
			// two stacks: parsing-symbol-stack and ast-stack
			// once a reduction happens
			// call AstHandle.Invoke(...) on ast-stack
		}

	private:
		std::unique_ptr<ParserBootstrapInfo> info_; // loaded from config file
		std::unique_ptr<AstTraitManager> traits_; // loaded from generated code
		std::unique_ptr<ParsingTable> table_; // runtime generated
	};

	std::string GenerateDataBinding(const ParserBootstrapInfo& info);
}