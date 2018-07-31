#pragma once
#include "ast/ast-basic.h"
#include "ast/ast-proxy.h"
#include "array-ref.h"
#include <vector>
#include <string_view>
#include <variant>

// Handles that perform AstNode generation and folding during reduction
namespace eds::loli::ast
{
    // =====================================================================================
    // Generative Handles
    //

    class AstEnumGen
    {
    public:
        AstEnumGen(int value)
            : value_(value) {}

        AstItemWrapper Invoke(const AstTypeProxy& proxy, Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            return proxy.ConstructEnum(value_);
        }

    private:
        int value_;
    };
    class AstObjectGen
    {
    public:
        AstItemWrapper Invoke(const AstTypeProxy& proxy, Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            return proxy.ConstructObject(arena);
        }
    };
    class AstVectorGen
    {
    public:
        AstItemWrapper Invoke(const AstTypeProxy& proxy, Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            return proxy.ConstructVector(arena);
        }
    };
    class AstOptionalGen
    {
    public:
        AstItemWrapper Invoke(const AstTypeProxy& proxy, Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            return proxy.ConstructOptional();
        }
    };
    class AstItemSelector
    {
    public:
        AstItemSelector(int index)
            : index_(index) {}

        AstItemWrapper Invoke(const AstTypeProxy& proxy, Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            assert(index_ < rhs.Length());
            return rhs.At(index_);
        }

    private:
        int index_;
    };

    // =====================================================================================
    // Manipulative Handles
    //

    class AstManipPlaceholder
    {
    public:
        void Invoke(const AstTypeProxy& proxy, AstItemWrapper item, ArrayRef<AstItemWrapper> rhs) const {}
    };
    class AstObjectSetter
    {
    public:
        struct SetterPair
        {
            int member_index;
            int symbol_index;
        };

        AstObjectSetter(const std::vector<SetterPair>& setters)
            : setters_(setters) {}

        void Invoke(const AstTypeProxy& proxy, AstItemWrapper obj, ArrayRef<AstItemWrapper> rhs) const
        {
            for (auto setter : setters_)
            {
                proxy.AssignField(obj, setter.member_index, rhs.At(setter.symbol_index));
            }
        }

    private:
        std::vector<SetterPair> setters_;
    };
    class AstVectorMerger
    {
    public:
        AstVectorMerger(const std::vector<int>& indices)
            : indices_(indices) {}

        void Invoke(const AstTypeProxy& proxy, AstItemWrapper vec, ArrayRef<AstItemWrapper> rhs) const
        {
            for (auto index : indices_)
            {
                proxy.PushBackElement(vec, rhs.At(index));
            }
        }

    private:
        std::vector<int> indices_;
    };

    // =====================================================================================
    // Ensemble Handle Type
    //

    class AstHandle
    {
    public:
        using GenHandle = std::variant<
            AstEnumGen,
            AstObjectGen,
            AstVectorGen,
            AstOptionalGen,
            AstItemSelector>;

        using ManipHandle = std::variant<
            AstManipPlaceholder,
            AstObjectSetter,
            AstVectorMerger>;

        AstHandle(const AstTypeProxy* proxy, GenHandle gen, ManipHandle manip)
            : proxy_(proxy), gen_handle_(gen), manip_handle_(manip) {}

        AstItemWrapper Invoke(Arena& arena, ArrayRef<AstItemWrapper> rhs) const
        {
            // construct or select a node
            auto gen_visitor = [&](const auto& gen) { return gen.Invoke(*proxy_, arena, rhs); };
            auto result      = std::visit(gen_visitor, gen_handle_);

            // modify members
            auto manip_visitor = [&](const auto& manip) { manip.Invoke(*proxy_, result, rhs); };
            std::visit(manip_visitor, manip_handle_);

            // update location information
            auto front_loc = rhs.Front().GetLocationInfo();
            auto back_loc  = rhs.Back().GetLocationInfo();

            auto offset = front_loc.offset;
            auto length = back_loc.offset + back_loc.length - offset;
            result.UpdateLocationInfo(offset, length);

            // return
            return result;
        }

    private:
        const AstTypeProxy* proxy_;

        GenHandle gen_handle_;
        ManipHandle manip_handle_;
    };
}