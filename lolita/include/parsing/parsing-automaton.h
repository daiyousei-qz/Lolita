#pragma once
#include "core/parsing-info.h"
#include "container/flat-set.h"
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <memory>
#include <functional>
#include <cassert>

namespace eds::loli::parsing
{
    class ParsingState;

    // ==============================================================
    // ParsingItem
    //

    class ParsingItem
    {
    public:
        struct Less
        {
            bool operator()(const ParsingItem& lhs, const ParsingItem& rhs)
            {
                if (lhs.production_ < rhs.production_)
                    return true;
                if (lhs.production_ > rhs.production_)
                    return false;

                return lhs.cursor_ < rhs.cursor_;
            }
        };

        ParsingItem(const ProductionInfo* p, int cursor)
            : production_(p), cursor_(cursor)
        {
            assert(p != nullptr);
            assert(cursor >= 0 && cursor <= p->Right().size());
        }

        const auto& Production() const { return production_; }
        const auto& Cursor() const { return cursor_; }

        // construct next item
        ParsingItem CreateSuccessor() const
        {
            assert(!IsFinalized());
            return ParsingItem{production_, cursor_ + 1};
        }

        const SymbolInfo* NextSymbol() const
        {
            if (IsFinalized())
                return nullptr;

            return production_->Right()[cursor_];
        }

        // if it's not any P -> . \alpha
        // NOTE though production of root symbol is always kernel,
        // this struct is not aware of that
        bool IsKernel() const
        {
            return cursor_ > 0;
        }

        // if it's some P -> \alpha .
        bool IsFinalized() const
        {
            return cursor_ == production_->Right().size();
        }

    private:
        const ProductionInfo* production_;
        int cursor_;
    };

    using ItemSet = FlatSet<ParsingItem, ParsingItem::Less>;

    // ==============================================================
    // Parsing Actions
    //

    struct PdaEdgeShift
    {
        const ParsingState* target;
    };
    struct PdaEdgeReduce
    {
        const ProductionInfo* production;
    };

    using PdaEdge = std::variant<PdaEdgeShift, PdaEdgeReduce>;

    // ==============================================================
    // Parsing State
    //

    class ParsingState
    {
    public:
        ParsingState(int id) : id_(id) {}

        const auto& Id() const { return id_; }
        const auto& EofAction() const { return eof_action_; }
        const auto& ActionMap() const { return action_map_; }
        const auto& GotoMap() const { return goto_map_; }

        void RegisterShift(const ParsingState* dest, const SymbolInfo* s);

        void RegisterReduce(const ProductionInfo* p, const TokenInfo* tok);
        void RegisterReduceOnEof(const ProductionInfo* p);

    private:
        int id_;

        std::optional<PdaEdgeReduce> eof_action_;
        std::unordered_map<const TokenInfo*, PdaEdge> action_map_;
        std::unordered_map<const VariableInfo*, const ParsingState*> goto_map_;
    };

    // ==============================================================
    // Parsing Automaton
    //

    class ParsingAutomaton
    {
    public:
        int StateCount() const
        {
            return states_.size();
        }
        const ParsingState* LookupState(int id) const
        {
            return ptrs_.at(id);
        }

        const auto States() const { return states_; }

        ParsingState* MakeState(const ItemSet& items);
        void EnumerateState(std::function<void(const ItemSet&, ParsingState&)> callback);

    private:
        std::vector<ParsingState*> ptrs_;
        std::map<ItemSet, ParsingState> states_;
    };

    std::unique_ptr<const ParsingAutomaton> BuildSLRAutomaton(const ParsingMetaInfo& info);
    std::unique_ptr<const ParsingAutomaton> BuildLALRAutomaton(const ParsingMetaInfo& info);
}