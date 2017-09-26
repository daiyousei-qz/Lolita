#pragma once
#include <string_view>

namespace lolita
{
	struct Token
	{
		int category;
		int offset;
		int length;
	};
}