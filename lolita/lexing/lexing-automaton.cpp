#include "lexing/lexing-automaton.h"
#include "core/regex.h"
#include "container/flat-set.h"
#include "text/text-utils.h"
#include <algorithm>
#include <numeric>
#include <map>
#include <unordered_map>
#include <iterator>

using namespace std;
using namespace eds;
using namespace eds::text;
using namespace eds::loli::regex;

namespace eds::loli::lexing
{
    // Regex To Deterministic Finite Automata
    //
    using PositionLabel = const LabelledExpr*;
    using PositionSet   = FlatSet<PositionLabel>;

    using RootExprVec          = vector<const RootExpr*>;
    using AcceptCategoryLookup = unordered_map<PositionLabel, const TokenInfo*>;

    struct JointRegexTree
    {
        RootExprVec roots               = {};
        AcceptCategoryLookup acc_lookup = {};
    };

    struct RegexNodeInfo
    {
        // if the node can match empty string
        bool nullable = false;

        // set of initial positions of the node
        PositionSet firstpos = {};

        // set of terminal positions of the node
        PositionSet lastpos = {};
    };

    struct RegexEvalResult
    {
        // extra information for each node
        unordered_map<const RegexExpr*, RegexNodeInfo> info_map = {};

        // followpos set for each position
        unordered_map<PositionLabel, PositionSet> followpos = {};
    };

    RegexEvalResult CollectRegexNodeInfo(const RootExprVec& defs)
    {
        struct Visitor : public RegexExprVisitor
        {
            unordered_map<const RegexExpr*, RegexNodeInfo> info_map{};
            unordered_map<PositionLabel, PositionSet> followpos{};

            const RegexNodeInfo* last_visited_info;

            auto VisitRegexExprVec(const vector<RegexExpr::Ptr>& v)
            {
                vector<const RegexNodeInfo*> result;
                for (const auto& expr : v)
                {
                    expr->Accept(*this);

                    result.push_back(last_visited_info);
                }

                return result;
            }

            void UpdateNodeInfo(const RegexExpr& expr, RegexNodeInfo&& info)
            {
                last_visited_info =
                    &(info_map[&expr] = forward<RegexNodeInfo>(info));
            }

            void Visit(const RootExpr& expr) override
            {
                expr.Child()->Accept(*this);

                const auto& child_info = *last_visited_info;
                for (auto pos : child_info.lastpos)
                {
                    followpos[pos].insert(&expr);
                }

                // TODO: should I update follow pos for this?
                UpdateNodeInfo(expr, {false, child_info.firstpos, {&expr}});
            }
            void Visit(const EntityExpr& expr) override
            {
                UpdateNodeInfo(expr, {false, {&expr}, {&expr}});
            }
            void Visit(const SequenceExpr& expr) override
            {
                auto children_info = VisitRegexExprVec(expr.Children());

                // compute nullable
                bool nullable =
                    all_of(children_info.begin(), children_info.end(),
                           [](const auto& info) { return info->nullable; });

                // compute firstpos
                PositionSet firstpos{};
                for (auto child_info : children_info)
                {
                    firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());

                    if (!child_info->nullable)
                        break;
                }

                // compute lastpos
                PositionSet lastpos{};
                for (auto it = children_info.rbegin(); it != children_info.rend(); ++it)
                {
                    auto child_info = *it;
                    lastpos.insert(child_info->lastpos.begin(), child_info->lastpos.end());

                    if (!child_info->nullable)
                        break;
                }

                // update followpos
                const RegexNodeInfo* last_child_info = nullptr;
                for (auto child_info : children_info)
                {
                    if (last_child_info)
                    {
                        for (auto pos : last_child_info->lastpos)
                        {
                            followpos[pos].insert(child_info->firstpos.begin(), child_info->firstpos.end());
                        }
                    }

                    last_child_info = child_info;
                }

                UpdateNodeInfo(expr, {nullable, firstpos, lastpos});
            }
            void Visit(const ChoiceExpr& expr) override
            {
                auto children_info = VisitRegexExprVec(expr.Children());

                // compute nullable
                bool nullable =
                    any_of(children_info.begin(), children_info.end(),
                           [](const auto& info) { return info->nullable; });

                // compute firstpos and lastpos
                PositionSet firstpos{};
                PositionSet lastpos{};
                for (auto child_info : children_info)
                {
                    firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());
                    lastpos.insert(child_info->lastpos.begin(), child_info->lastpos.end());
                }

                UpdateNodeInfo(expr, {nullable, firstpos, lastpos});
            }
            void Visit(const ClosureExpr& expr) override
            {
                expr.Child()->Accept(*this);

                const auto& child_info = *last_visited_info;

                auto strategy = expr.Mode();
                if (strategy != RepetitionMode::Optional) // * or +
                {
                    for (auto pos : child_info.lastpos)
                    {
                        followpos[pos].insert(child_info.firstpos.begin(), child_info.firstpos.end());
                    }
                }

                auto nullable = strategy != RepetitionMode::Plus;
                UpdateNodeInfo(expr, {nullable, child_info.firstpos, child_info.lastpos});
            }

        } visitor{};

        for (const auto& root : defs)
        {
            root->Accept(visitor);
        }

        return RegexEvalResult{visitor.info_map, visitor.followpos};
    }

    auto ComputeInitialPositionSet(const RegexEvalResult& lookup, const RootExprVec& roots)
    {
        PositionSet result;
        for (const auto& expr : roots)
        {
            const auto& firstpos = lookup.info_map.at(expr).firstpos;
            result.insert(firstpos.begin(), firstpos.end());
        }

        return result;
    }

    auto ComputeTargetPositionSet(const RegexEvalResult& lookup, const PositionSet& src, int ch)
    {
        PositionSet target;
        for (auto pos : src)
        {
            if (pos->TestPassage(ch))
            {
                const auto& t = lookup.followpos.at(pos);
                target.insert(t.begin(), t.end());
            }
        }

        return target;
    }

    auto ComputeAcceptCategory(const AcceptCategoryLookup& lookup, const PositionSet& set)
    {
        // find out accepting category if any
        const TokenInfo* result = nullptr;
        for (const auto pos : set)
        {
            auto iter = lookup.find(pos);
            if (iter != lookup.end())
            {
                // NOTE token with smaller id comes forehand and is prior
                if (result == nullptr || result->Id() > iter->second->Id())
                {
                    result = iter->second;
                }
            }
        }

        return result;
    }

    unique_ptr<const LexingAutomaton> BuildDfaAutomaton(const JointRegexTree& trees)
    {
        // analyze regex trees
        auto eval_result = CollectRegexNodeInfo(trees.roots);

        // NOTE a set of positions of regex node coresponds to a Dfa state
        auto initial_state = ComputeInitialPositionSet(eval_result, trees.roots);

        auto dfa              = make_unique<LexingAutomaton>();
        auto dfa_state_lookup = map<PositionSet, DfaState*>{{initial_state, dfa->NewState()}};

        for (deque<PositionSet> unprocessed{initial_state}; !unprocessed.empty(); unprocessed.pop_front())
        {
            const auto& src_set  = unprocessed.front();
            const auto src_state = dfa_state_lookup.at(src_set);

            // for each input symbol
            for (int ch = 0; ch < 128; ++ch)
            {
                // construct the target position set
                auto dest_set = ComputeTargetPositionSet(eval_result, src_set, ch);

                // skip empty state, that's invalid
                if (dest_set.empty())
                    continue;

                // find the correct state object
                auto& dest_state = dfa_state_lookup[dest_set];

                // in case state is not yet created, make one and queue the set
                if (dest_state == nullptr)
                {
                    // find out accepting category, if any
                    auto acc_term = ComputeAcceptCategory(trees.acc_lookup, dest_set);

                    // allocate state object
                    dest_state = dfa->NewState(acc_term);

                    // register and queue it
                    unprocessed.push_back(dest_set);
                }

                // update DFA transition table
                dfa->NewTransition(src_state, dest_state, ch);
            }
        }

        return dfa;
    }

    unique_ptr<const LexingAutomaton> OptimizeLexingAutomaton(const LexingAutomaton& atm)
    {
        // TODO: finish this
        throw 0;
    }

    auto PrepareRegexBatch(const ParsingMetaInfo& info)
    {
        JointRegexTree result;

        auto process_token = [&](const TokenInfo& tok) {
            // parse the regex
            result.roots.push_back(tok.TreeDefinition().get());
            // update acc lookup
            result.acc_lookup[result.roots.back()] = &tok;
        };

        for (const auto& tok : info.Tokens())
            process_token(tok);

        for (const auto& tok : info.IgnoredTokens())
            process_token(tok);

        return result;
    }

    std::unique_ptr<const LexingAutomaton> BuildLexingAutomaton(const ParsingMetaInfo& info)
    {
        auto joint_regex = PrepareRegexBatch(info);
        auto dfa         = BuildDfaAutomaton(joint_regex);

        // TODO: optimize?
        return dfa;
    }
}