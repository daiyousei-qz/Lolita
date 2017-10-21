#pragma once
#include "ast-basic.h"
#include "ast-trait.h"

namespace eds::loli
{
	template <typename T>
	class AstVector : public AstVectorBase
	{
	public:
		const auto& Data() const
		{
			return container_;
		}

		void Push(const T& value)
		{
			container_.push_back(value);
		}

	private:
		std::vector<T> container_;
	};

	template <typename T>
	class BasicAstTrait : public AstObjectTrait
	{
	public:
		static_assert(std::is_base_of_v<AstObjectBase, T> || std::is_enum_v<T>);

		using SelfType = T;
		using StoreType = std::conditional_t<std::is_enum_v<T>, T, T*>;
		using VectorType = AstVector<StoreType>;
		using VectorStoreType = AstVector<StoreType>*;

		AstItem ConstructEnum(int value) override
		{
			if constexpr (std::is_enum_v<T>)
			{
				return AstItem::Create<StoreType>(static_cast<SelfType>(value));
			}
			else
			{
				throw 0;
			}
		}
		AstItem ConstructObject(Arena& arena) override
		{
			if constexpr (std::is_base_of_v<AstObjectBase, SelfType> && std::is_default_constructible_v<SelfType>)
			{
				return AstItem::Create<StoreType>(arena.Construct<SelfType>());
			}
			else
			{
				throw 0;
			}
		}

		AstItem ConstructVector(Arena& arena) override
		{
			return AstItem::Create<VectorStoreType>(arena.Construct<VectorType>());
		}

		void AssignField(AstItem obj, int codinal, AstItem value) override
		{
			throw 0;
		}
		void InsertElement(AstItem vec, AstItem node) override
		{
			vec.Extract<VectorStoreType>()->Push(node.Extract<StoreType>());
		}
	};

	template <typename T>
	void QuickAssignField(T& dest, AstItem item)
	{
		dest = item.Extract<T>();
	}
}