#pragma once
#include "core/errors.h"
#include "ast/ast-basic.h"
#include "memory/arena.h"
#include <type_traits>
#include <unordered_map>

namespace eds::loli::ast
{
    // =====================================================================================
    // Type Proxy
    //

    // A proxy object enables basic construction and modification operations
    // of an AstItemType with its actual type erased
    class AstTypeProxy
    {
    public:
        // NOTE a proxy function may only work for a limited set of items
        // AstEnum: ConstructEnum
        // AstObject: ConstructObject, AssignField
        // AstVector: ConstructVector, InsertElement

        virtual AstItemWrapper ConstructEnum(int value) const = 0;
        virtual AstItemWrapper ConstructObject(Arena&) const  = 0;
        virtual AstItemWrapper ConstructVector(Arena&) const  = 0;
        virtual AstItemWrapper ConstructOptional() const      = 0;

        virtual void AssignField(AstItemWrapper obj, int codinal, AstItemWrapper value) const = 0;
        virtual void PushBackElement(AstItemWrapper vec, AstItemWrapper elem) const           = 0;
    };

    // placeholder proxy
    class DummyAstTypeProxy : public AstTypeProxy
    {
    private:
        [[noreturn]] static void Throw()
        {
            throw ParserInternalError{"DummyAstTypeProxy: cannot perform any proxy operation"};
        }

    public:
        AstItemWrapper ConstructEnum(int value) const override
        {
            Throw();
        }
        AstItemWrapper ConstructObject(Arena&) const override
        {
            Throw();
        }
        AstItemWrapper ConstructVector(Arena&) const override
        {
            Throw();
        }
        AstItemWrapper ConstructOptional() const override
        {
            Throw();
        }

        void AssignField(AstItemWrapper obj, int codinal, AstItemWrapper value) const override
        {
            Throw();
        }
        void PushBackElement(AstItemWrapper vec, AstItemWrapper elem) const override
        {
            Throw();
        }

        static const AstTypeProxy& Instance()
        {
            static DummyAstTypeProxy dummy{};
            return dummy;
        }
    };

    // a proxy template
    template <typename T>
    class BasicAstTypeProxy final : public AstTypeProxy
    {
    public:
        using TraitType = AstTypeTrait<T>;

        using SelfType     = typename TraitType::SelfType;
        using StoreType    = typename TraitType::StoreType;
        using VectorType   = typename TraitType::VectorType;
        using OptionalType = typename TraitType::OptionalType;

        AstItemWrapper ConstructEnum(int value) const override
        {
            if constexpr (TraitType::IsEnum())
            {
                return SelfType{static_cast<typename SelfType::UnderlyingType>(value)};
            }
            else
            {
                throw ParserInternalError{"BasicAstTypeProxy: T is not an enum type"};
            }
        }
        AstItemWrapper ConstructObject(Arena& arena) const override
        {
            if constexpr (TraitType::IsKlass())
            {
                return arena.Construct<SelfType>();
            }
            else
            {
                throw ParserInternalError{"BasicAstTypeProxy: T is not a klass type"};
            }
        }

        AstItemWrapper ConstructVector(Arena& arena) const override
        {
            return arena.Construct<VectorType>();
        }
        AstItemWrapper ConstructOptional() const override
        {
            return OptionalType{};
        }

        void AssignField(AstItemWrapper obj, int ordinal, AstItemWrapper value) const override
        {
            if constexpr (TraitType::IsKlass())
            {
                obj.Extract<SelfType*>()->SetItem(ordinal, value);
            }
            else
            {
                throw ParserInternalError{"BasicAstTypeProxy: T is not a klass type"};
            }
        }
        void PushBackElement(AstItemWrapper vec, AstItemWrapper elem) const override
        {
            vec.Extract<VectorType*>()->PushBack(elem.Extract<StoreType>());
        }
    };

    // =====================================================================================
    // Proxy Manager
    //

    // TODO: use TypeInfo* to lookup proxy?
    class AstTypeProxyManager
    {
    public:
        const AstTypeProxy* Lookup(const std::string& klass) const
        {
            auto it = proxies_.find(klass);
            if (it != proxies_.end())
            {
                return it->second.get();
            }
            else
            {
                throw ParserInternalError{"AstTypeProxyManager: specific type proxy not found"};
            }
        }

        template <typename EnumType>
        void RegisterEnum(const std::string& name)
        {
            using namespace eds::type;
            static_assert(Constraint<EnumType>(is_enum_of<int>));

            RegisterTypeInternal<BasicAstEnum<EnumType>>(name);
        }

        template <typename KlassType>
        void RegisterKlass(const std::string& name)
        {
            using namespace eds::type;
            static_assert(Constraint<KlassType>(derive_from<BasicAstObject>));

            RegisterTypeInternal<KlassType>(name);
        }

    private:
        template <typename AstType>
        void RegisterTypeInternal(const std::string& name)
        {
            proxies_[name] = std::make_unique<BasicAstTypeProxy<AstType>>();
        }

        using ProxyLookup = std::unordered_map<std::string, std::unique_ptr<AstTypeProxy>>;

        ProxyLookup proxies_;
    };
}