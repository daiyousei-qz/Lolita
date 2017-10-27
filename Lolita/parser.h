#pragma once
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include <string_view>
#include <memory>

namespace eds::loli
{
	template<typename T>
	struct ParsingResult
	{
		std::unique_ptr<Arena> arena;
		T value;
	};

	struct ParsingContext
	{
		std::unique_ptr<Arena> arena;

		std::vector<PdaStateId> state_stack;
		std::vector<AstTypeWrapper> ast_stack;
	};

	class BasicParser
	{
	public:
		BasicParser(
			std::unique_ptr<const ParserBootstrapInfo> info,
			std::unique_ptr<const AstTypeProxyManager> manager,
			std::unique_ptr<const ParsingTable> table
		)
			: parser_info_(std::move(info))
			, proxy_manager_(std::move(manager))
			, parsing_table_(std::move(table))
		{

		}

		const auto& GetParserInfo() const { return *parser_info_; }
		const auto& GetProxyManager() const { return *proxy_manager_; }
		const auto& GetParsingTable() const { return *parsing_table_; }

		void Parse(std::string_view data) const;

	private:
		Token LoadToken(std::string_view data, int offset) const;
		void FeedParser(ParsingContext& ctx, int tok) const;

		std::unique_ptr<const ParserBootstrapInfo> parser_info_;	// loaded from config file
		std::unique_ptr<const AstTypeProxyManager> proxy_manager_;	// loaded from generated code
		std::unique_ptr<const ParsingTable> parsing_table_;			// runtime generated
	};
}