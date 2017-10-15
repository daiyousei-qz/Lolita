#pragma once
#include <string_view>

namespace eds::loli
{
	class AstObject 
	{
	public:
		AstObject() = default;
		virtual ~AstObject() = default;
	};

	class AstVectorBase { };

	template <typename T>
	class AstVector : public AstVectorBase
	{

	};

	union AstItem
	{
	public:
		int enum_value;
		std::string_view token;
		AstObject* object;
		AstVectorBase* vector;

		AstItem();
		AstItem(int);
		AstItem(std::string_view);
		AstItem(AstObject*);
		AstItem(AstVectorBase*);
	};

	class AstItemView
	{
	public:
		bool Empty();
		const AstItem& At(int index);
	};
}