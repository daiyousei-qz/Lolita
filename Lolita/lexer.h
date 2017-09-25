#pragma once
#include "lolita-basic.h"
#include <unordered_map>
#include <memory>

namespace lolita
{
	class Lexer
	{
	public:
		using SharedPtr = std::shared_ptr<Lexer>;

	private:
		std::vector<int> dfa_table_;
		std::unordered_map<int, std::string> acc_catogories_;
	};

}