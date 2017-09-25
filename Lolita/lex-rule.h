#pragma once
#include "lolita-basic.h"
#include "regex.h"
#include "arena.hpp"

namespace lolita
{
	class LexRule
	{
	public:
		LexRule(Arena arena, std::vector<RegexExpr*> rules)
			: guard_(std::move(arena)), rules_(std::move(rules)) { }

		const auto& Rules() const { return rules_; }

	private:
		Arena guard_;
		std::vector<RegexExpr*> rules_;
	};
}