#include "parsing/parsing-automaton.h"
#include "parsing/grammar.h"
#include "container/flat-set.h"
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <cassert>

using namespace std;
using namespace eds;

namespace eds::loli::parsing
{
    // Implmentation of ParsingState
    //
    void ParsingState::RegisterShift(const ParsingState* dest, const SymbolInfo* s)
    {
        if (auto tok = s->AsToken(); tok)
        {
            assert(action_map_.count(tok) == 0);
            action_map_.insert_or_assign(tok, PdaEdgeShift{dest});
        }
        else
        {
            auto var = s->AsVariable();

            assert(goto_map_.count(var) == 0);
            goto_map_.insert_or_assign(var, dest);
        }
    }

    void ParsingState::RegisterReduce(const ProductionInfo* p, const TokenInfo* tok)
    {
        assert(action_map_.count(tok) == 0);
        action_map_.insert_or_assign(tok, PdaEdgeReduce{p});
    }
    void ParsingState::RegisterReduceOnEof(const ProductionInfo* p)
    {
        assert(!eof_action_.has_value());
        eof_action_ = PdaEdgeReduce{p};
    }

    // Implmentation of ParsingAutomaton
    //

    ParsingState* ParsingAutomaton::MakeState(const ItemSet& items)
    {
        auto iter = states_.find(items);
        if (iter == states_.end())
        {
            auto id = static_cast<int>(states_.size());
            iter    = states_.try_emplace(iter, items, ParsingState{id});

            // store the new state
            ptrs_.push_back(&iter->second);
        }

        return &(iter->second);
    }

    void ParsingAutomaton::EnumerateState(std::function<void(const ItemSet&, ParsingState&)> callback)
    {
        for (auto& pair : states_)
            callback(pair.first, pair.second);
    }

    // Construction of Parsing Automaton
    //

    // Expands state of kernel items into its closure
    // that includes all of non-kernel items as well,
    // and then enumerate items with a callback
    template <typename F>
    void EnumerateClosureItems(const ParsingMetaInfo& info, const ItemSet& kernel, F callback)
    {
        static_assert(is_invocable_v<F, ParsingItem>);

        auto added         = unordered_set<const VariableInfo*>{};
        auto to_be_visited = vector<const VariableInfo*>{};

        // TODO: SUSPICIOUS TO BUGS, ROOT SYMBOL SHOULD BE TREATED DIFFERENTLY
        // treat root symbol's items as kernel
        // added.insert(&info.RootSymbol());

        // helper
        const auto try_register_candidate = [&](const SymbolInfo* s) {
            if (auto candidate = s->AsVariable(); candidate)
            {
                if (added.count(candidate) == 0)
                {
                    added.insert(candidate);
                    to_be_visited.push_back(candidate);
                }
            }
        };

        // visit kernel items and record nonterms for closure calculation
        for (const auto& item : kernel)
        {
            callback(item);

            // Item{ A -> \alpha . }: skip it
            // Item{ A -> \alpha . B \gamma }: queue unvisited nonterm B for further expansion
            if (auto s = item.NextSymbol(); s)
            {
                try_register_candidate(s);
            }
        }

        // genreate and visit non-kernel items recursively
        while (!to_be_visited.empty())
        {
            auto variable = to_be_visited.back();
            to_be_visited.pop_back();

            for (const auto& p : variable->Productions())
            {
                callback(ParsingItem{p, 0});

                if (!p->Right().empty())
                {
                    try_register_candidate(p->Right().front());
                }
            }
        }
    }

    // claculate target state from a source state with a particular symbol s
    // and enumerate its items with a callback
    ItemSet ComputeGotoItems(const ParsingMetaInfo& info, const ItemSet& src, const SymbolInfo* s)
    {
        ItemSet new_state;

        EnumerateClosureItems(info, src, [&](ParsingItem item) {

            // for Item A -> \alpha . B \beta where B == s, advance the cursor
            if (item.NextSymbol() == s)
            {
                new_state.insert(item.CreateSuccessor());
            }
        });

        return new_state;
    }

    ItemSet GenerateInitialItems(const ParsingMetaInfo& info)
    {
        ItemSet result;
        for (auto p : info.RootSymbol().Productions())
        {
            result.insert(ParsingItem{p, 0});
        }

        return result;
    }

    template <typename F>
    void EnumerateSymbols(const ParsingMetaInfo& info, F callback)
    {
        static_assert(is_invocable_v<F, const SymbolInfo*>);

        for (const auto& tok : info.Tokens())
            callback(&tok);

        for (const auto& var : info.Variables())
            callback(&var);
    }

    auto BootstrapParsingAutomaton(const ParsingMetaInfo& info)
    {
        // NOTE a set of ParsingItem coresponds to a ParsingState
        auto pda = make_unique<ParsingAutomaton>();

        auto initial_set = GenerateInitialItems(info);

        // for each unvisited LR state, generate a target state for each possible symbol
        for (deque<ItemSet> unprocessed{initial_set}; !unprocessed.empty(); unprocessed.pop_front())
        {
            const auto& src_items = unprocessed.front();
            const auto src_state  = pda->MakeState(src_items);

            EnumerateSymbols(info, [&](const SymbolInfo* s) {

                // calculate the target state for symbol s
                auto dest_items = ComputeGotoItems(info, src_items, s);

                // empty set of item is not a valid state
                if (dest_items.empty()) return;

                // compute target state
                auto old_state_cnt = pda->States().size();
                auto dest_state    = pda->MakeState(dest_items);
                if (pda->States().size() > old_state_cnt)
                {
                    // if dest_state is newly created, pipe it into unprocessed queue
                    unprocessed.push_back(dest_items);
                }

                // add transition
                src_state->RegisterShift(dest_state, s);
            });
        }

        return pda;
    }

    // SLR
    //

    unique_ptr<Grammar> CreateSimpleGrammar(const ParsingMetaInfo& info)
    {
        GrammarBuilder builder;

        for (const auto& p : info.Productions())
        {
            auto lhs = builder.MakeNonterminal(p.Left(), nullptr);
            auto rhs = SymbolVec{};

            for (auto elem : p.Right())
                rhs.push_back(builder.MakeGenericSymbol(elem, nullptr));

            builder.CreateProduction(&p, lhs, rhs);
        }

        auto root = builder.MakeNonterminal(&info.RootSymbol(), nullptr);
        return builder.Build(root);
    }

    unique_ptr<const ParsingAutomaton> BuildSLRAutomaton(const ParsingMetaInfo& info)
    {
        auto pda = BootstrapParsingAutomaton(info);

        // calculate FIRST and FOLLOW set first
        const auto grammar = CreateSimpleGrammar(info);

        pda->EnumerateState([&](const ItemSet& items, ParsingState& state) {
            for (auto item : items)
            {
                if (item.IsFinalized())
                {
                    // shortcuts
                    const auto& production = item.Production();
                    const auto& lhs        = production->Left();

                    // lookup grammar symbol for predicative information
                    const auto& lhs_info = grammar->LookupNonterminal({lhs, nullptr});

                    // for all term in FOLLOW do reduce
                    for (auto term : lhs_info->FollowSet())
                    {
                        auto tok = term->Info();
                        assert(tok != nullptr);

                        state.RegisterReduce(production, tok);
                    }

                    // EOF
                    if (lhs_info->MayPreceedEof())
                    {
                        state.RegisterReduceOnEof(production);
                    }
                }
            }
        });

        return pda;
    }

    // LALR
    //

    const ParsingState* LookupTargetState(const ParsingState* src, const SymbolInfo* s)
    {
        if (auto tok = s->AsToken(); tok)
        {
            return get<PdaEdgeShift>(src->ActionMap().at(tok)).target;
        }
        else
        {
            return src->GotoMap().at(s->AsVariable());
        }
    }

    unique_ptr<Grammar> CreateExtendedGrammar(const ParsingMetaInfo& info, ParsingAutomaton& pda)
    {
        GrammarBuilder builder;

        // extend symbols
        pda.EnumerateState([&](const ItemSet& items, ParsingState& state) {
            // extend nonterms
            for (const auto& goto_edge : state.GotoMap())
            {
                auto var_info = goto_edge.first;
                auto version  = goto_edge.second;

                builder.MakeNonterminal(var_info, version);
            }

            // extend terms
            for (const auto& action_edge : state.ActionMap())
            {
                auto tok_info = action_edge.first;
                auto version  = get<PdaEdgeShift>(action_edge.second).target;

                builder.MakeTerminal(tok_info, version);
            }
        });

        auto new_root = builder.MakeNonterminal(&info.RootVariable(), nullptr);

        // extend productions
        pda.EnumerateState([&](const ItemSet& items, ParsingState& state) {
            EnumerateClosureItems(info, items, [&](ParsingItem item) {

                // NOTE we are only interested in non-kernel items(including intial state)
                // to avoid repetition
                if (item.IsKernel()) return;

                // shortcuts
                const auto& production_info = item.Production();

                // map left-hand side
                auto lhs = new_root;
                if (state.Id() != 0 || production_info->Left() != new_root->Info())
                {
                    // TODO: remove hard-coded state id 0

                    // exclude root nonterm because its version is set to nullptr
                    auto version = LookupTargetState(&state, production_info->Left());
                    lhs          = builder.MakeNonterminal(production_info->Left(), version);
                }

                // map right-hand side
                vector<Symbol*> rhs = {};

                const ParsingState* current_state = &state;
                for (auto rhs_elem : production_info->Right())
                {
                    auto next_state = LookupTargetState(current_state, rhs_elem);
                    assert(next_state != nullptr);

                    if (auto tok = rhs_elem->AsToken(); tok)
                    {
                        rhs.push_back(
                            builder.MakeTerminal(tok, next_state));
                    }
                    else
                    {
                        auto var = rhs_elem->AsVariable();

                        rhs.push_back(
                            builder.MakeNonterminal(var, next_state));
                    }

                    current_state = next_state;
                }

                // create production
                builder.CreateProduction(production_info, lhs, rhs);
            });
        });

        return builder.Build(new_root);
    }

    unique_ptr<const ParsingAutomaton> BuildLALRAutomaton(const ParsingMetaInfo& info)
    {
        auto pda         = BootstrapParsingAutomaton(info);
        auto ext_grammar = CreateExtendedGrammar(info, *pda);

        // (state where reduction is done, production)
        using LocatedProduction = tuple<const ParsingState*, const ProductionInfo*>;

        // merge follow set
        set<LocatedProduction> merged_ending;
        map<LocatedProduction, FlatSet<const TokenInfo*>> merged_follow;
        for (const auto& p : ext_grammar->Productions())
        {
            const auto& lhs = p->Left();
            const auto& rhs = p->Right();

            // TODO: assumed production to be non-empty
            auto key = LocatedProduction{rhs.back()->Version(), p->Info()};

            // normalized ending
            if (lhs->MayPreceedEof())
            {
                merged_ending.insert(key);
            }

            // normalized FOLLOW set
            auto& follow = merged_follow[key];
            for (auto term : lhs->FollowSet())
            {
                follow.insert(term->Info());
            }
        }

        // register reductions
        pda->EnumerateState([&](const ItemSet& items, ParsingState& state) {
            for (auto item : items)
            {
                const auto& production = item.Production();
                auto key               = LocatedProduction{&state, production};

                if (item.IsFinalized())
                {
                    // EOF
                    if (merged_ending.count(key) > 0)
                    {
                        state.RegisterReduceOnEof(production);
                    }

                    // for all term in FOLLOW do reduce
                    for (auto term : merged_follow.at(key))
                    {
                        state.RegisterReduce(production, term);
                    }
                }
            }
        });

        return pda;
    }
}