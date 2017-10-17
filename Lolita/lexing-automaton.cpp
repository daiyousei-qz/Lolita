#include "lexing-automaton.h"
#include "regex.h"
#include "arena.hpp"
#include "ext/flat-set.hpp"
#include "text/text-utils.hpp"
#include <algorithm>
#include <numeric>
#include <map>
#include <unordered_map>
#include <iterator>

using namespace std;
using namespace eds;
using namespace eds::text;

namespace eds::loli::lexing
{
	// Regex To Deterministic Finite Automata
	//

	using RegexPositionLabel = const LabelledExpr*;
	using RegexPositionSet = FlatSet<RegexPositionLabel>;
	using AcceptCategoryLookup = unordered_map<RegexPositionLabel, int>;

	struct RegexAst
	{
		RootExprVec regex_defs = {};

		AcceptCategoryLookup acc_lookup = {};
	};

	struct RegexNodeInfo
	{
		// if the node can match empty string
		bool nullable;

		// set of initial positions of the node
		RegexPositionSet firstpos;

		// set of terminal positions of the node
		RegexPositionSet lastpos;
	};

	struct RegexEvalResult
	{
		// extra information for each node
		unordered_map<RegexExprPtr, RegexNodeInfo> info_map;

		// followpos set for each position
		unordered_map<RegexPositionLabel, RegexPositionSet> followpos;
	};

	extern RootExpr* ParseRegex(Arena& arena, const string& str);

	RegexEvalResult CollectRegexNodeInfo(const RootExprVec& defs)
	{
		struct Visitor : public RegexExprVisitor
		{
			unordered_map<RegexExprPtr, RegexNodeInfo> info_map{};
			unordered_map<RegexPositionLabel, RegexPositionSet> followpos{};

			const RegexNodeInfo* last_visited_info;

			auto VisitChildren(const RegexExprVec& v)
			{
				vector<const RegexNodeInfo*> children_info;
				for (auto child : v)
				{
					child->Accept(*this);

					children_info.push_back(last_visited_info);
				}

				return children_info;
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
				UpdateNodeInfo(expr, { false, child_info.firstpos,{ &expr } });
			}
			void Visit(const EntityExpr& expr) override
			{
				UpdateNodeInfo(expr, { false, { &expr },{ &expr } });
			}
			void Visit(const SequenceExpr& expr) override
			{
				auto children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = 
					all_of(children_info.begin(), children_info.end(),
						   [](const auto& info) { return info->nullable; });

				// compute firstpos
				RegexPositionSet firstpos{};
				for (auto child_info : children_info)
				{
					firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());

					if (!child_info->nullable)
						break;
				}

				// compute lastpos
				RegexPositionSet lastpos{};
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

				UpdateNodeInfo(expr, { nullable, firstpos, lastpos });
			}
			void Visit(const ChoiceExpr& expr) override
			{
				auto children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = 
					any_of(children_info.begin(), children_info.end(),
						   [](const auto& info) { return info->nullable; });

				// compute firstpos and lastpos
				RegexPositionSet firstpos{};
				RegexPositionSet lastpos{};
				for (auto child_info : children_info)
				{
					firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());
					lastpos.insert(child_info->lastpos.begin(), child_info->lastpos.end());
				}

				UpdateNodeInfo(expr, { nullable, firstpos, lastpos });
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
				UpdateNodeInfo(expr, { nullable, child_info.firstpos, child_info.lastpos });
			}

		} visitor{};

		for (auto p : defs)
		{
			p->Accept(visitor);
		}

		return RegexEvalResult{ visitor.info_map, visitor.followpos };
	}

	RegexAst CreateRegexAst(Arena& arena, const std::vector<std::string>& defs)
	{
		RegexAst ast;

		int index = 0;
		for (const auto& regex : defs)
		{
			auto expr = ParseRegex(arena, regex);

			ast.regex_defs.push_back(expr);
			ast.acc_lookup.insert_or_assign(expr, index++);
		}

		return ast;
	}

	auto ComputeInitialPositionSet(const RegexEvalResult& lookup, const RootExprVec& regex_defs)
	{
		RegexPositionSet result;
		for (const auto regex : regex_defs)
		{
			const auto& firstpos = lookup.info_map.at(regex).firstpos;
			result.insert(firstpos.begin(), firstpos.end());
		}

		return result;
	}

	auto ComputeTargetPositionSet(const RegexEvalResult& lookup, const RegexPositionSet& src, int ch)
	{
		RegexPositionSet target;
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

	auto ComputeAcceptCategory(const AcceptCategoryLookup& lookup, const RegexPositionSet& set)
	{
		// find out accepting category if any
		auto result = -1;
		for (const auto pos : set)
		{
			auto iter = lookup.find(pos);
			if (iter != lookup.end())
			{
				if (result == -1 || result > iter->second)
				{
					result = iter->second;
				}
			}
		}

		return result;
	}

	unique_ptr<const LexingAutomaton> BuildLexingAutomaton(const std::vector<std::string>& defs)
	{
		// create ast
		Arena arena;
		auto ast = CreateRegexAst(arena, defs);

		// analyze regex trees
		auto eval_result = CollectRegexNodeInfo(ast.regex_defs);

		// NOTE a set of positions of regex node coresponds to a Dfa state
		auto initial_state = ComputeInitialPositionSet(eval_result, ast.regex_defs);

		auto dfa = make_unique<LexingAutomaton>();
		auto dfa_state_lookup = map<RegexPositionSet, DfaState*>{ { initial_state, dfa->NewState() } };

		for (deque<RegexPositionSet> unprocessed{ initial_state }; !unprocessed.empty(); unprocessed.pop_front())
		{
			const auto& src_set = unprocessed.front();
			const auto src_state = dfa_state_lookup[src_set];

			// for each input symbol
			for (int ch = 0; ch < 128; ++ch)
			{
				// construct the target position set
				auto dest_set = ComputeTargetPositionSet(eval_result, src_set, ch);

				// skip empty state, that's invalid
				if (dest_set.empty()) continue;

				// if it's a new state, register and queue it
				// also, we have to find out it's allocated id
				DfaState* dest_state;
				if (auto it = dfa_state_lookup.find(dest_set); it != dfa_state_lookup.end())
				{
					dest_state = it->second;
				}
				else
				{
					// find out accepting category, if any
					auto acc_term = ComputeAcceptCategory(ast.acc_lookup, dest_set);

					// allocate state id
					dest_state = dfa->NewState(acc_term);

					// register and queue the lately created state
					dfa_state_lookup[dest_set] = dest_state;
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
}