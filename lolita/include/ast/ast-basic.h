#pragma once
#include "core/errors.h"
#include "type-utils.h"
#include <string_view>
#include <typeindex>
#include <vector>
#include <cassert>
#include <variant>
#include <optional>

namespace eds::loli::ast
{
    // =====================================================================================
    // AstNode Top Type
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
            : offset_(offset), length_(length) {}

        const auto& Offset() const { return offset_; }
        const auto& Length() const { return length_; }

        AstLocationInfo GetLocationInfo() const
        {
            return AstLocationInfo{offset_, length_};
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
    // Basic Ast Node
    //

    class BasicAstToken : public AstNodeBase
    {
    public:
        // make BasicAstToken default constructible
        BasicAstToken() = default;
        BasicAstToken(int offset, int length, int tag)
            : AstNodeBase(offset, length), tag_(tag) {}

        const auto& Tag() const
        {
            return tag_;
        }
        bool IsValid() const
        {
            return tag_ != -1;
        }

    private:
        int tag_ = -1;
    };

    template <typename EnumType>
    class BasicAstEnum : public AstNodeBase
    {
        static_assert(type::Constraint<EnumType>(type::is_enum_of<int>));

    public:
        using UnderlyingType = EnumType;

        BasicAstEnum() = default;
        BasicAstEnum(EnumType v)
            : value_(static_cast<int>(v)) {}

        int IntValue() const
        {
            return value_;
        }
        EnumType Value() const
        {
            return static_cast<EnumType>(value_);
        }
        bool IsValid() const
        {
            return value_ != -1;
        }

    private:
        int value_ = -1;
    };

    template <typename EnumType>
    inline bool operator==(const BasicAstEnum<EnumType>& lhs, const BasicAstEnum<EnumType>& rhs)
    {
        return lhs.Value() == rhs.Value();
    }
    template <typename EnumType>
    inline bool operator==(const BasicAstEnum<EnumType>& lhs, const EnumType& rhs)
    {
        return lhs.Value() == rhs;
    }
    template <typename EnumType>
    inline bool operator==(const EnumType& lhs, const BasicAstEnum<EnumType>& rhs)
    {
        return lhs == rhs.Value();
    }

    class BasicAstObject : public AstNodeBase
    {
    public:
        BasicAstObject() = default;
        virtual ~BasicAstObject() {} // to generate vptr
    };

    // =====================================================================================
    // Qualified Ast Node
    //

    template <typename T>
    class AstVector : public AstNodeBase
    {
    public:
        using ElementType = T;

        const auto& Value() const
        {
            return container_;
        }

        bool Empty() const
        {
            return container_.empty();
        }
        int Size() const
        {
            return container_.size();
        }

        void PushBack(const T& value)
        {
            container_.push_back(value);
        }

    private:
        std::vector<T> container_;
    };

    // default implenmentation for BasicAstToken and BasicAstEnum
    template <typename T>
    class AstOptional : public AstNodeBase
    {
    public:
        using ElementType = T;

        AstOptional() = default;

        // inherit location info from inner element
        AstOptional(const ElementType& value)
            : AstNodeBase(value.Offset(), value.Length()), value_(value) {}

        bool HasValue() const
        {
            return value.IsValid();
        }

        const auto& Value() const
        {
            assert(HasValue());
            return value_;
        }

    private:
        ElementType value_ = {};
    };

    // specialized implementation for BasicAstObject*
    template <typename T>
    class AstOptional<T*> : public AstNodeBase
    {
    public:
        using ElementType = T*;

        AstOptional() = default;

        // inherit location info from inner element
        AstOptional(const ElementType& obj)
            : AstNodeBase(obj->Offset(), obj->Length()), value_(obj) {}

        bool HasValue() const
        {
            return value_ != nullptr;
        }

        const auto& Value() const
        {
            assert(HasValue());
            return value_;
        }

    private:
        T* value_ = nullptr;
    };

    // =====================================================================================
    // AstType Concept Checkers
    //

    namespace detail
    {
        template <typename T>
        struct IsBasicAstEnum : std::false_type
        {
        };
        template <typename T>
        struct IsBasicAstEnum<BasicAstEnum<T>> : std::true_type
        {
        };

        static constexpr auto is_ast_token  = type::same_to<BasicAstToken>;
        static constexpr auto is_ast_enum   = type::generic_check<IsBasicAstEnum>;
        static constexpr auto is_ast_object = type::derive_from<BasicAstObject>;

        // An AstItem is either of:
        // - AstToken
        // - AstEnum
        // - AstObject*
        // - AstVector*
        // - AstOptional
        //
        // NOTE AstItem is always pod
        template <typename U>
        struct IsAstVectorPtr : std::false_type
        {
        };
        template <typename U>
        struct IsAstVectorPtr<AstVector<U>*> : std::true_type
        {
        };

        template <typename U>
        struct IsAstOptional : std::false_type
        {
        };
        template <typename U>
        struct IsAstOptional<AstOptional<U>> : std::true_type
        {
        };

        static constexpr auto is_astitem_token    = is_ast_token;
        static constexpr auto is_astitem_enum     = is_ast_enum;
        static constexpr auto is_astitem_object   = type::convertible_to<BasicAstObject*> && !type::same_to<nullptr_t>;
        static constexpr auto is_astitem_vector   = type::generic_check<IsAstVectorPtr>;
        static constexpr auto is_astitem_optional = type::generic_check<IsAstOptional>;

        template <typename T>
        inline constexpr bool IsAstItem()
        {
            using namespace eds::type;

            return Constraint<T>(
                       is_astitem_token ||
                       is_astitem_enum ||
                       is_astitem_object ||
                       is_astitem_vector ||
                       is_astitem_optional) &&
                   std::is_trivially_destructible_v<T>;
        }
    }

    // =====================================================================================
    // AstTypeTrait
    //

    enum class AstTypeCategory
    {
        Token,
        Enum,
        Base,
        Klass
    };

    template <typename T>
    struct AstTypeTrait
    {
        static constexpr AstTypeCategory GetCategory()
        {
            using namespace eds::type;

            if constexpr (Constraint<T>(detail::is_ast_token))
            {
                return AstTypeCategory::Token;
            }
            else if constexpr (Constraint<T>(detail::is_ast_enum))
            {
                return AstTypeCategory::Enum;
            }
            else if constexpr (Constraint<T>(detail::is_ast_object))
            {
                if constexpr (Constraint<T>(is_abstract))
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
        static constexpr bool IsEnum() { return GetCategory() == AstTypeCategory::Enum; }
        static constexpr bool IsBase() { return GetCategory() == AstTypeCategory::Base; }
        static constexpr bool IsKlass() { return GetCategory() == AstTypeCategory::Klass; }

        static constexpr bool StoredInHeap()
        {
            return IsBase() || IsKlass();
        }

        // WORKAROUND: FUCK MSVC
        static constexpr bool kStoredInHeapValue = StoredInHeap();

        using SelfType  = T;
        using StoreType = std::conditional_t<kStoredInHeapValue, T*, T>;

        using VectorType   = AstVector<StoreType>;
        using OptionalType = AstOptional<StoreType>;
    };

    // =====================================================================================
    // AstItem
    //

    // TODO: add a backdoor for optional type: when extracted as an optional, element type should be checked as well

    // NOTE an AstItem is always a POD type, copying is cheap
    class AstItemWrapper
    {
    public:
        AstItemWrapper() {}

        template <typename T>
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
        bool DetectInstance()
        {
            AssertAstItem<T>();

            return type_ == GetTypeMetaInfo<T>();
        }

        template <typename T>
        T Extract()
        {
            using namespace eds::type;

            if constexpr (Constraint<T>(detail::is_astitem_object))
            {
                // TODO: to improve the performance, an internal heirachy examination machanism
                //       could be implemented via factorization as scale of our heirachy is always controllable
                // SEE ALSO: http://www.stroustrup.com/fast_dynamic_casting.pdf

                auto result = dynamic_cast<T>(RefAs<BasicAstObject*, false>());

                if (result == nullptr)
                    ThrowTypeMismatch();

                return result;
            }
            else if constexpr (Constraint<T>(detail::is_astitem_optional))
            {
                if (DetectInstance<T>())
                {
                    return RefAs<T, false>();
                }
                else
                {
                    return Extract<typename T::ElementType>();
                }
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

            if constexpr (Constraint<T>(convertible_to<BasicAstObject*> && !same_to<nullptr_t>))
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
        template <typename T>
        void AssertAstItem()
        {
            using namespace eds::type;
            static_assert(detail::IsAstItem<T>(), "T must be a valid AstItem");
            static_assert(Constraint<T>(!is_const && !is_volatile), "T should not be cv-qualified");
            static_assert(Constraint<T>(!is_reference), "T should not be reference");
        }

        template <typename T, bool ForceType>
        T& RefAs()
        {
            if constexpr (ForceType)
            {
                type_ = GetTypeMetaInfo<T>();
            }
            else if (!DetectInstance<T>())
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

        [[noreturn]] void ThrowTypeMismatch()
        {
            throw ParserInternalError{"AstItemWrapper: storage type mismatch"};
        }

        struct TypeMetaInfo
        {
            std::type_index typeinfo;
            bool store_in_heap;
            bool is_object;
        };

        template <typename T>
        static auto GetTypeMetaInfo()
        {
            using namespace eds::type;

            // TODO: make this more organized
            static const TypeMetaInfo instance = {
                typeid(T),
                std::is_pointer_v<T>,
                Constraint<T>(convertible_to<BasicAstObject*> && !same_to<nullptr_t>)};

            return &instance;
        }

        using StorageType = std::aligned_union_t<4, BasicAstToken, BasicAstEnum<AstTypeCategory>, nullptr_t>;

        const TypeMetaInfo* type_ = nullptr;
        StorageType data_         = {};
    };
}