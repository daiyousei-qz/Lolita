#pragma once
#include "lang-utils.hpp"
#include <stdexcept>
#include <string>

namespace eds::loli
{
	template <typename T>
	struct AlwaysFalse : std::false_type { };

	struct ParserConstructionError : std::runtime_error
	{
	public:
		ParserConstructionError(const std::string& msg)
			: std::runtime_error(msg) { }
	};

	struct ParserInternalError : std::runtime_error
	{
	public:
		ParserInternalError(const std::string& msg)
			: std::runtime_error(msg) { }
	};
}