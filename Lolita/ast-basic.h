#pragma once
#include <string_view>

namespace eds::loli
{
	class AstObject { };
	class AstVector { };

	union AstItem
	{
	public:
		int enum_value;
		std::string_view token;
		AstObject* object;
		AstVector* vector;

		AstItem();
		AstItem(int);
		AstItem(std::string_view);
		AstItem(AstObject*);
		AstItem(AstVector*);
	};

	class AstItemView
	{
	public:
		bool Empty();
		const AstItem& At(int index);
	};
}