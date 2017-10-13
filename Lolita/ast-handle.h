#pragma once
#include "ast-basic.h"
#include "ast-trait.h"
#include <vector>
#include <string_view>
#include <variant>

// given Arena&, AstTraitInterface&, AstItemView
// CHOICE ->
// - return one item
// * return null_opt
// * return one item as AstOptional
// - return empty_vec
// - return one item as AstVector
// - construct a new AstObject and merge items into it

namespace eds::loli
{
	class AstSelector
	{
	public:
		AstItem Invoke(Arena& arena, AstTraitInterface& trait, AstItemView rhs)
		{
			return rhs.At(choice_);
		}

	private:
		int choice_;
	};
	class AstEmptyVecGen
	{
	public:
		AstItem Invoke(Arena& arena, AstTraitInterface& trait, AstItemView rhs)
		{
			return trait.ConstructVector(arena);
		}
	};
	class AstVecMerger
	{
	public:
		AstItem Invoke(Arena& arena, AstTraitInterface& trait, AstItemView rhs)
		{
			auto vec = rhs.At(vec_index).vector;
			trait.InsertElement(vec, rhs.At(item_index));

			return vec;
		}

	private:
		int vec_index;
		int item_index;
	};
	class AstObjectGen
	{

	};

	class AstHandle
	{
		AstItem Invoke(Arena& arena, AstTraitInterface& trait, AstItemView rhs)
		{
			auto visitor = [&](auto& handle) { return handle.Invoke(arena, trait, rhs); };

			return std::visit(visitor, data_);
		}

	private:
		using VarType =
			std::variant<
			AstSelector,
			AstEmptyVecGen,
			AstVecMerger,
			AstObjectGen
			>;

		VarType data_;
	};
}