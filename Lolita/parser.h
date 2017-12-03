#pragma once
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include <string_view>
#include <memory>

namespace eds::loli
{
	class BasicGenericParser
	{
	public:
		BasicGenericParser(
			std::unique_ptr<const ParserBootstrapInfo> info,
			std::unique_ptr<const AstTypeProxyManager> manager,
			std::unique_ptr<const ParsingTable> table
		)
			: parser_info_(std::move(info))
			, proxy_manager_(std::move(manager))
			, parsing_table_(std::move(table)) { }

		const auto& GetParserInfo()   const { return *parser_info_;   }
		const auto& GetProxyManager() const { return *proxy_manager_; }
		const auto& GetParsingTable() const { return *parsing_table_; }

		AstTypeWrapper DoParse(Arena& arena, std::string_view data) const;

	protected:
		std::unique_ptr<const ParserBootstrapInfo> parser_info_;	// loaded from config file
		std::unique_ptr<const AstTypeProxyManager> proxy_manager_;	// loaded from generated code
		std::unique_ptr<const ParsingTable> parsing_table_;			// runtime generated
	};

	template<typename T>
	class BasicParser : public BasicGenericParser
	{
	public:
		static_assert(AstTypeTrait<T>::IsObject());

		class ParsingResult
		{
		public:
			ParsingResult() = default;

			T* GetParseTreeRoot() const noexcept
			{
				return value_;
			}

		private:
			friend class BasicParser<T>;

			Arena arena_ = {};
			T* value_ = {};
		};

		std::unique_ptr<ParsingResult> Parse(std::string_view data)
		{
			auto result = std::make_unique<ParsingResult>();
			
			auto x = DoParse(result->arena_, data);
			result->value_ = dynamic_cast<T*>(x);

			return result;
		}
	};
}