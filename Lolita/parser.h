#pragma once
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include <string_view>
#include <memory>

namespace eds::loli
{
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
		std::unique_ptr<ParserBootstrapInfo> i_; // loaded from deserialized data
		std::unique_ptr<AstTraitManager> m_; // loaded from generated code
		std::unique_ptr<ParsingTable> t_; // runtime generated
	};
}