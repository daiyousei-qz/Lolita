#pragma once
#include "ast/ast-basic.h"
#include "lang-utils.h"
#include <cassert>

namespace eds::loli::ast
{
    template <typename... Ts>
    class EmptyKlass;

    // DataBundle is a tuple-like class for indexed member access
    template <typename... Ts>
    class DataBundle
    {
    public:
        void SetItem(int ordinal, AstItemWrapper data)
        {
            throw 0;
        }

        template <int Ordinal>
        const auto& GetItem()
        {
            static_assert(false);
            return BasicAstObject();
        }
    };

    template <typename T, typename... Ts>
    class DataBundle<T, Ts...>
    {
    public:
        void SetItem(int ordinal, AstItemWrapper data)
        {
            if (ordinal == 0)
            {
                first_ = data.Extract<T>();
            }
            else
            {
                rest_.SetItem(ordinal - 1, data);
            }
        }

        template <int Ordinal>
        const auto& GetItem() const
        {
            if constexpr (Ordinal == 0)
            {
                return first_;
            }
            else
            {
                static_assert(sizeof...(Ts) > 0);
                return rest_.template GetItem<(Ordinal - 1)>();
            }
        }

    private:
        T first_ = {};
        DataBundle<Ts...> rest_;
    };
}