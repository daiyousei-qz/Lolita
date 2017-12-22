#pragma once
#include "lolita/core/errors.h"
#include "ext/type-utils.h"
#include <string_view>
#include <typeindex>
#include <vector>
#include <cassert>
#include <variant>
#include <optional>

template<typename T> class Empty;

namespace eds::loli::ast
{
	// =====================================================================================
	// Node Top Type
	//

	struct AstLocationInfo
	{
		int offset;
		int length;
	};

	// any node in the syntax tree shoule inherit from AstNodeBase
	// to privide with offset and other basic utilities
	class AstNodeBase
	{
	public:
		AstNodeBase(int offset = -1, int length = -1)
			: offset_(offset), length_(length) { }

		const auto& Offset() const { return offset_; }
		const auto& Length() const { return length_; }

		AstLocationInfo GetLocationInfo() const
		{
			return AstLocationInfo{ offset_, length_ };
		}

		void UpdateLocationInfo(int offset, int length)
		{
			offset_ = offset;
			length_ = length;
		}

	private:
		int offset_;
		int length_;
	};

	// =====================================================================================
	// Basic Ast Types
	//

	class BasicAstToken : public AstNodeBase
	{
	public:
		// make BasicAstToken default constructible
		BasicAstToken()
			: AstNodeBase(), tag_(-1) { }
		BasicAstToken(int offset, int length, int tag)
			: AstNodeBase(offset, length), tag_(tag) { }

		const auto& Tag() const { return tag_; }

		bool IsValid() const { return tag_ != -1; }

	private:
		int tag_;
	};

	template<typename EnumType>
	class BasicAstEnum : public AstNodeBase
	{
		static_assert(type::Constraint<EnumType>(type::is_enum_of<int>));

	public:
		using UnderlyingType = EnumType;

		BasicAstEnum() = default;
		BasicAstEnum(EnumType v) : value_(v) { }

		const auto& Value() const { return value_; }

	private:
		EnumType value_ = {};
	};

	class BasicAstObject : public AstNodeBase
	{
	public:
		BasicAstObject() = default;
		virtual ~BasicAstObject() { } // to generate vptr
	};

	// Declaration of Qualified Ast Type
	//
	template<typename T>
	class AstVector;

	template<typename T>
	class AstOptional;

	// =====================================================================================
	// AstType Concept Checkers
	//

	namespace detail
	{
		template<typename T>
		struct IsBasicAstEnum : std::false_type { };
		template<typename T>
		struct IsBasicAstEnum<BasicAstEnum<T>> : std::true_type { };

		static constexpr auto is_ast_token	= type::same_to<BasicAstToken>;
		static constexpr auto is_ast_enum	= type::generic_check<IsBasicAstEnum>;
		static constexpr auto is_ast_object = type::derive_from<BasicAstObject>;

		// An AstItem is either of:
		// - AstToken
		// - AstEnum
		// - AstObject*
		// - AstVector*
		// - AstOptional
		//
		// NOTE AstItem is always pod
		template<typename U>
		struct IsQualifiedAstItem : std::false_type { };
		template<typename U>
		struct IsQualifiedAstItem<AstVector<U>*> : std::true_type { };
		template<typename U>
		struct IsQualifiedAstItem<AstOptional<U>> : std::true_type { };

		template<typename T>
		inline constexpr bool IsAstItem()
		{
			using namespace eds::type;

			return Constraint<T>(
				is_ast_token ||
				is_ast_enum ||
				(convertible_to<BasicAstObject*> && !same_to<nullptr_t>) ||
				generic_check<IsQualifiedAstItem>
			) && std::is_trivially_destructible_v<T>;
		}
	}

	// =====================================================================================
	// AstTypeTrait
	//

	enum class AstTypeCategory
	{
		Token, Enum, Base, Klass
	};

	template<typename T>
	struct AstTypeTrait
	{
		static constexpr AstTypeCategory GetCategory()
		{
			using namespace eds::type;

			if constexpr(Constraint<T>(detail::is_ast_token))
			{
				return AstTypeCategory::Token;
			}
			else if constexpr(Constraint<T>(detail::is_ast_enum))
			{
				return AstTypeCategory::Enum;
			}
			else if constexpr(Constraint<T>(detail::is_ast_object))
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
				static_assert(type::AlwaysFalse<T>::value, "T is not a legitimate AstType.");
			}
		}

		static constexpr bool IsToken() { return GetCategory() == AstTypeCategory::Token; }
		static constexpr bool IsEnum()  { return GetCategory() == AstTypeCategory::Enum;  }
		static constexpr bool IsBase()  { return GetCategory() == AstTypeCategory::Base;  }
		static constexpr bool IsKlass() { return GetCategory() == AstTypeCategory::Klass; }

		static constexpr bool StoredInHeap()
		{
			return IsBase() || IsKlass();
		}

		using SelfType = T;
		using StoreType = std::conditional_t<AstTypeTrait::StoredInHeap(), T*, T>;
		
		using VectorType = AstVector<StoreType>;
		using OptionalType = AstOptional<StoreType>;
	};

	// =====================================================================================
	// Enhancing Ast Nodes
	//

	template<typename T>
	class AstVector : public AstNodeBase
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

	template<typename T>
	class AstOptional : public AstNodeBase
	{
	public:
		bool HasValue() const
		{
			return value_.has_value();
		}

		const auto& Data() const
		{
			assert(HasValue());
			return *value_;
		}

	private:
		// TODO: make use of null value for pointer
		std::optional<T> value_ = {};
	};

	// =====================================================================================
	// AstItem
	//

	class AstItemWrapper
	{
	public:
		AstItemWrapper() { }

		template<typename T>
		AstItemWrapper(const T& value)
		{
			Assign(value);
		}

		bool HasValue() 
		{
			return type_ != nullptr;
		}
		void Clear() 
		{
			type_ = nullptr;
		}

		template <typename T>
		T Extract()
		{
			using namespace eds::type;

			if constexpr(Constraint<T>(convertible_to<BasicAstObject*> && !same_to<nullptr_t>))
			{
				// TODO: to improve the performance, an internal heirachy examination machanism
				//       could be implemented via factorization as scale is always controllable
				// SEE ALSO: http://www.stroustrup.com/fast_dynamic_casting.pdf

				auto result = dynamic_cast<T>(RefAs<BasicAstObject*, false>());

				if (result == nullptr)
					ThrowTypeMismatch();

				return result;
			}
			else
			{
				return RefAs<T, false>();
			}
		}

		template <typename T>
		void Assign(T value)
		{
			using namespace eds::type;

			if constexpr(Constraint<T>(convertible_to<BasicAstObject*> && !same_to<nullptr_t>))
			{
				assert(value != nullptr);
				RefAs<BasicAstObject*, true>() = value;
			}
			else
			{
				RefAs<T, true>() = value;
			}
		}

		AstLocationInfo GetLocationInfo()
		{
			return RefNodeBase().GetLocationInfo();
		}
		
		void UpdateLocationInfo(int offset, int length)
		{
			RefNodeBase().UpdateLocationInfo(offset, length);
		}

	private:
		template <typename T, bool ForceType>
		T& RefAs()
		{
			using namespace eds::type;
			static_assert(detail::IsAstItem<T>(), "T must be a valid AstItem");
			static_assert(Constraint<T>(!is_const && !is_volatile), "T should not be cv-qualified");
			static_assert(Constraint<T>(!is_reference), "T should not be reference");

			if constexpr(ForceType)
			{
				type_ = GetTypeMetaInfo<T>();
			}
			else if(type_ != GetTypeMetaInfo<T>())
			{
				ThrowTypeMismatch();
			}

			return *reinterpret_cast<T*>(&data_);
		}

		AstNodeBase& RefNodeBase()
		{
			if (!HasValue())
				ThrowTypeMismatch();

			// TODO: make this more organized
			if (type_->store_in_heap)
			{
				if (type_->is_object)
				{
					return **reinterpret_cast<BasicAstObject**>(&data_);
				}
				else
				{
					return **reinterpret_cast<AstNodeBase**>(&data_);
				}
			}
			else
			{
				return *reinterpret_cast<AstNodeBase*>(&data_);
			}
		}

		[[noreturn]]
		void ThrowTypeMismatch()
		{
			throw ParserInternalError{ "AstItemWrapper: storage type mismatch" };
		}

		struct TypeMetaInfo
		{
			std::type_index typeinfo;
			bool store_in_heap;
			bool is_object;
		};

		template<typename T>
		static auto GetTypeMetaInfo()
		{
			using namespace eds::type;

			// TODO: make this more organized
			static const TypeMetaInfo instance = {
				typeid(T), 
				std::is_pointer_v<T>, 
				Constraint<T>(convertible_to<BasicAstObject*> && !same_to<nullptr_t>)
			};
			
			return &instance;
		}

		using StorageType
			= std::aligned_union_t<4, BasicAstToken, BasicAstEnum<AstTypeCategory>, nullptr_t>;

		const TypeMetaInfo* type_ = nullptr;
		StorageType data_ = {};
	};
}