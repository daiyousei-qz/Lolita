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
}