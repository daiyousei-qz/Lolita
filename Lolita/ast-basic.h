#pragma once
#include <string_view>
#include <typeindex>
#include <vector>
#include <cassert>

namespace eds::loli
{
	struct Token
	{
		int category;
		int offset;
		int length;
	};

	class AstObjectBase 
	{
	public:
		AstObjectBase() = default;
		virtual ~AstObjectBase() = default;
	};
	class AstVectorBase { };

	class AstItem
	{
	public:
		AstItem() { }

		template <typename T>
		T Extract()
		{
			if (type_ != std::type_index(typeid(T)))
			{
				// TODO: add type check compatible with inheritence
				// throw 0; // type conflict
			}

			return RefAs<T>();
		}

		template <typename T>
		void Assign(T value)
		{
			type_ = std::type_index{ typeid(T) };

			RefAs<T>() = value;
		}

		template <typename T>
		static AstItem Create(T value)
		{
			AstItem result;
			result.Assign<T>(value);

			return result;
		}

	private:

		template <typename T>
		T& RefAs()
		{
			using PtrUnderlyingType = std::remove_pointer_t<T>;

			if constexpr (std::is_enum_v<T>)
			{
				static_assert(std::is_same_v<std::underlying_type_t<T>, int>);
				return *reinterpret_cast<T*>(&data_.enum_value);
			}
			else if constexpr (std::is_same_v<T, Token>)
			{
				return data_.token;
			}
			else if constexpr (std::is_base_of_v<AstObjectBase, PtrUnderlyingType>)
			{
				static_assert(std::is_same_v<T, PtrUnderlyingType*>);
				return *reinterpret_cast<PtrUnderlyingType**>(&data_.object);
			}
			else if constexpr (std::is_base_of_v<AstVectorBase, PtrUnderlyingType>)
			{
				static_assert(std::is_same_v<T, PtrUnderlyingType*>);
				return *reinterpret_cast<PtrUnderlyingType**>(&data_.vector);
			}
			else
			{
				static_assert(AlwaysFalse<T>::value);
			}
		}

		union DataType
		{
			nullptr_t dummy;

			int enum_value;
			Token token;
			AstObjectBase* object;
			AstVectorBase* vector;

		public:
			DataType() : dummy(nullptr) { }
		};

		std::type_index type_ = typeid(void);
		DataType data_ = {};
	};

	class AstItemView
	{
	public:
		AstItemView(std::vector<AstItem>& stack, int count)
			: ptr_(stack.data() + (stack.size() - count)), count_(count)
		{
			assert(stack.size() >= count);
			assert(count >= 0);
		}

		bool Empty() const 
		{
			return count_ == 0; 
		}
		const AstItem& At(int index)
		{
			assert(index >= 0 && index < count_);
			return ptr_[index];
		}

	private:
		AstItem* ptr_;
		int count_;
	};
}