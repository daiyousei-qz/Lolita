#pragma once
#include "lang-utils.h"
#include <stdexcept>
#include <string>

namespace eds::loli
{
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