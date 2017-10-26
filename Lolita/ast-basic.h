#pragma once
#include "ext\type-utils.hpp"
#include <string_view>
#include <typeindex>
#include <vector>
#include <cassert>
#include <variant>

namespace eds::loli
{
	// Basic Types
	//

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

	private:
		int offset;
	};
	class AstVectorBase : public AstObjectBase { };

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
	class AstOptional;

	// Type meta
	//

	enum class AstTypeCategory
	{
		Token, Enum, Base, Klass, Vector
	};

	template<typename T>
	struct AstTypeTrait
	{
		static constexpr AstTypeCategory GetCategory()
		{
			using namespace eds::type;

			if constexpr(Constraint<T>(same_to<Token>))
			{
				return AstTypeCategory::Token;
			}
			else if constexpr(Constraint<T>(is_enum_of<int>))
			{
				return AstTypeCategory::Enum;
			}
			else if constexpr(Constraint<T>(derive_from<AstVectorBase>))
			{
				return AstTypeCategory::Vector;
			}
			else if constexpr(Constraint<T>(derive_from<AstObjectBase>))
			{
				if constexpr(Constraint<T>(is_abstract))
				{
					return AstTypeCategory::Base;
				}
				else
				{
					return AstTypeCategory::Klass;
				}
			}
			else
			{
				static_assert(AlwaysFalse<T>::value, "T is not a legitimate AstItem.");
			}
		}


		static constexpr bool IsToken()  { return GetCategory() == AstTypeCategory::Token;  }
		static constexpr bool IsEnum()   { return GetCategory() == AstTypeCategory::Enum;   }
		static constexpr bool IsBase()   { return GetCategory() == AstTypeCategory::Base;   }
		static constexpr bool IsKlass()  { return GetCategory() == AstTypeCategory::Klass;  }
		static constexpr bool IsVector() { return GetCategory() == AstTypeCategory::Vector; }
		
		static constexpr bool AllowProxy() 
		{
			return !IsVector();
		}

		static constexpr bool StoredInPtr()
		{
			return IsBase() || IsKlass() || IsVector();
		}

		using SelfType = T;
		using StoreType = std::conditional_t<AstTypeTrait::StoredInPtr(), T*, T>;
		using VectorType = AstVector<StoreType>;
	};

	// NOTE this class is unsafe
	// TODO: add some runtime type validation?
	class AstTypeWrapper
	{
	public:
		template <typename T>
		using AstStoreType = typename AstTypeTrait<T>::StoreType;

		AstTypeWrapper() { }

		template <typename T>
		T Extract()
		{
			return RefAs<T>();
		}

		template <typename T>
		void Assign(T value)
		{
			RefAs<T>() = value;
		}

		template <typename T>
		static AstTypeWrapper Create(T value)
		{
			AstTypeWrapper result;
			result.Assign<T>(value);

			return result;
		}

	private:
		template <typename T>
		T& RefAs()
		{
			using namespace eds::type;

			static_assert(Constraint<T>(!is_const && !is_volatile), "T should not be cv-qualified");

			if constexpr(Constraint<T>(same_to<Token> || is_enum_of<int>))
			{
				return *reinterpret_cast<T*>(&data_);
			}
			else if constexpr(Constraint<T>(convertible_to<AstObjectBase*> && !same_to<nullptr_t>))
			{
				return *reinterpret_cast<T*>(&data_);
			}
			else
			{
				static_assert(AlwaysFalse<T>, "T is not a valid AstType in store");
			}
		}

		using StorageType
			= std::aligned_union_t<4, int, Token, nullptr_t>;

		StorageType data_;
	};
}