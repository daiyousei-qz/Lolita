#pragma once
#include "lang-utils.hpp"

namespace eds::loli
{
	template <typename T>
	struct AlwaysFalse : std::false_type { };
}