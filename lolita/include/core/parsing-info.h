#pragma once
#include "ast/ast-handle.h"
#include "ast/ast-proxy.h"
#include "core/config.h"
#include "core/regex.h"
#include "container/heap-array.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace eds::loli
{
    // Forward Decl
    //

    class TypeInfo;
    class EnumTypeInfo;
    class BaseTypeInfo;
    class KlassTypeInfo;

    class SymbolInfo;
    class TokenInfo;
    class VariableInfo;

    class ProductionInfo;

    // ParsingMetaInfo
    //

    class ParsingMetaInfo
    {
    public:
        class Builder;

        const auto& Environment() const { return env_; }

        // assumes last variable is root
        const auto& RootVariable() const { return variables_.Back(); }

        const auto& Enums() const { return enums_; }
        const auto& Bases() const { return bases_; }
        const auto& Klasses() const { return klasses_; }

        const auto& Tokens() const { return tokens_; }
        const auto& IgnoredTokens() const { return ignored_tokens_; }
        const auto& Variables() const { return variables_; }
        const auto& Productions() const { return productions_; }

        const auto& LookupType(const std::string& name) const
        {
            return type_lookup_.at(name);
        }
        const auto& LookupSymbol(const std::string& name) const
        {
            return symbol_lookup_.at(name);
        }

        const auto& RootSymbol() const
        {
            return variables_.Back();
        }

    private:
        using TypeInfoMap   = std::unordered_map<std::string, TypeInfo*>;
        using SymbolInfoMap = std::unordered_map<std::string, SymbolInfo*>;

        const ast::AstTypeProxyManager* env_;

        TypeInfoMap type_lookup_;
        container::HeapArray<EnumTypeInfo> enums_;
        container::HeapArray<BaseTypeInfo> bases_;
        container::HeapArray<KlassTypeInfo> klasses_;

        SymbolInfoMap symbol_lookup_;
        container::HeapArray<TokenInfo> tokens_;
        container::HeapArray<TokenInfo> ignored_tokens_;
        container::HeapArray<VariableInfo> variables_;
        container::HeapArray<ProductionInfo> productions_;
    };

    // pass nullptr if there's no proxy manager
    // dummy proxy would be used and ast handle would not work
    std::unique_ptr<ParsingMetaInfo> ResolveParsingInfo(const std::string& config, const ast::AstTypeProxyManager* env);

    // Type metadata
    //

    // runtime type information of an AST node
    class TypeInfo
    {
    public:
        enum class Category
        {
            Token,
            Enum,
            Base,
            Klass,
        };

        TypeInfo(const std::string& name)
            : name_(name) {}

        const auto& Name() const { return name_; }

        virtual Category GetCategory() const = 0;

        bool IsToken() const { return GetCategory() == Category::Token; }
        bool IsEnum() const { return GetCategory() == Category::Enum; }
        bool IsBase() const { return GetCategory() == Category::Base; }
        bool IsKlass() const { return GetCategory() == Category::Klass; }

        // stored as pointer in AST
        bool IsStoredByRef() const
        {
            auto category = GetCategory();
            return category == Category::Base || category == Category::Klass;
        }

    private:
        std::string name_;
    };

    struct TypeSpec
    {
        enum class Qualifier
        {
            None,
            Vector,
            Optional
        };

        Qualifier qual;
        TypeInfo* type;

    public:
        bool IsNoneQualified() const
        {
            return qual == Qualifier::None;
        }
        bool IsVector() const
        {
            return qual == Qualifier::Vector;
        }
        bool IsOptional() const
        {
            return qual == Qualifier::Optional;
        }
    };

    struct TokenTypeInfo : public TypeInfo
    {
    private:
        TokenTypeInfo() : TypeInfo("token") {}

    public:
        Category GetCategory() const override
        {
            return Category::Token;
        }

        static TokenTypeInfo& Instance()
        {
            static TokenTypeInfo dummy{};

            return dummy;
        }
    };

    class EnumTypeInfo : public TypeInfo
    {
    public:
        EnumTypeInfo(const std::string& name = "")
            : TypeInfo(name) {}

        Category GetCategory() const override
        {
            return Category::Enum;
        }

        const auto& Values() const { return values_; }

    private:
        friend class ParsingMetaInfo::Builder;

        std::vector<std::string> values_;
    };

    class BaseTypeInfo : public TypeInfo
    {
    public:
        BaseTypeInfo(const std::string& name = "")
            : TypeInfo(name) {}

        Category GetCategory() const override
        {
            return Category::Base;
        }
    };

    class KlassTypeInfo : public TypeInfo
    {
    public:
        struct MemberInfo
        {
            TypeSpec type;
            std::string name;
        };

        KlassTypeInfo(const std::string& name = "")
            : TypeInfo(name) {}

        Category GetCategory() const override
        {
            return Category::Klass;
        }

        const auto& BaseType() const { return base_; }
        const auto& Members() const { return members_; }

    private:
        friend class ParsingMetaInfo::Builder;

        BaseTypeInfo* base_ = nullptr;
        std::vector<MemberInfo> members_;
    };

    // Symbol metadata
    //

    class SymbolInfo
    {
    public:
        enum class Category
        {
            Token,
            Variable
        };

        SymbolInfo(int id, const std::string& name)
            : id_(id), name_(name) {}

        const auto& Id() const { return id_; }
        const auto& Name() const { return name_; }

        virtual Category GetCategory() const = 0;

        auto AsToken() const
        {
            return IsToken() ? reinterpret_cast<const TokenInfo*>(this) : nullptr;
        }
        auto AsVariable() const
        {
            return IsVariable() ? reinterpret_cast<const VariableInfo*>(this) : nullptr;
        }

        bool IsToken() const { return GetCategory() == Category::Token; }
        bool IsVariable() const { return GetCategory() == Category::Variable; }

    private:
        int id_;
        std::string name_;
    };

    class TokenInfo : public SymbolInfo
    {
    public:
        TokenInfo(int id = -1, const std::string& name = "")
            : SymbolInfo(id, name) {}

        Category GetCategory() const override
        {
            return Category::Token;
        }

        const auto& TextDefinition() const { return text_def_; }
        const auto& TreeDefinition() const { return ast_def_; }

    private:
        friend class ParsingMetaInfo::Builder;

        std::string text_def_;
        std::unique_ptr<regex::RootExpr> ast_def_;
    };

    class VariableInfo : public SymbolInfo
    {
    public:
        VariableInfo(int id = -1, const std::string& name = "")
            : SymbolInfo(id, name) {}

        Category GetCategory() const override
        {
            return Category::Variable;
        }

        const auto& Type() const { return type_; }
        const auto& Productions() const { return productions_; }

    private:
        friend class ParsingMetaInfo::Builder;

        TypeSpec type_;
        std::vector<ProductionInfo*> productions_;
    };

    // Production metadata
    //

    class ProductionInfo
    {
    public:
        const auto& Left() const { return lhs_; }
        const auto& Right() const { return rhs_; }

        const auto& Handle() const { return handle_; }

    private:
        friend class ParsingMetaInfo::Builder;

        VariableInfo* lhs_;
        std::vector<SymbolInfo*> rhs_;

        std::unique_ptr<ast::AstHandle> handle_;
    };
}