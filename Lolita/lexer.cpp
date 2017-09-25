#include "lexer.h"
#include "lex-rule.h"
#include "regex.h"
#include "flat-set.hpp"
#include <algorithm>
#include <numeric>
#include <unordered_map>

using namespace std;

namespace lolita
{
	int LookupRegexPos(const RegexExprVec& labels, const EntityExpr& expr)
	{
		for (int i = 0; i < labels.size(); ++i)
		{
			if (labels[i] == &expr)
			{
				return i;
			}
		}

		throw 0; // impossible path
	}

	// the process of labelling
	RegexExprVec CollectEntityExpr(const RegexExprVec& defs)
	{
		struct Visitor: public RegexExprVisitor
		{
			RegexExprVec output;

			void Visit(const EntityExpr& expr) override
			{
				output.push_back(&expr);
			}
			void Visit(const ConcatenationExpr& expr) override
			{
				for (auto child : expr.Children())
					child->Accept(*this);
			}
			void Visit(const AlternationExpr& expr) override
			{
				for (auto child : expr.Children())
					child->Accept(*this);
			}
			void Visit(const StarExpr& expr) override
			{
				expr.Child()->Accept(*this);
			}

		} visitor;

		for (auto p : defs)
		{
			p->Accept(visitor);
		}

		return visitor.output;
	}

	struct RegexNodeInfo
	{
		bool nullable;
		FlatSet<int> firstpos;
		FlatSet<int> lastpos;
	};

	auto EvaluateRegexExpr(const RegexExprVec& defs, const RegexExprVec& labels)
	{
		struct Visitor : public RegexExprVisitor
		{
			unordered_map<const RegexExpr*, RegexNodeInfo> func_map;
			vector<FlatSet<int>> followpos;
			
			const RegexExprVec& labels;

			RegexNodeInfo* last_visited_info;

			Visitor(const RegexExprVec& v) : labels(v), followpos(v.size()) { }

			vector<RegexNodeInfo*> VisitChildren(const RegexExprVec& v)
			{
				vector<RegexNodeInfo*> children_info;
				for (auto child : v)
				{
					child->Accept(*this);

					children_info.push_back(last_visited_info);
				}

				return children_info;
			}

			void UpdateNodeInfo(const RegexExpr& expr, RegexNodeInfo&& info)
			{
				last_visited_info = &(func_map[&expr] = forward<RegexNodeInfo>(info));
			}

			void Visit(const EntityExpr& expr) override
			{
				auto pos = LookupRegexPos(labels, expr);
				UpdateNodeInfo(expr, { false, {pos}, {pos} });
			}
			void Visit(const ConcatenationExpr& expr) override
			{
				vector<RegexNodeInfo*> children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = std::all_of(children_info.begin(), children_info.end(), 
											[](const auto& info) { return info->nullable; });

				// compute firstpos
				FlatSet<int> firstpos{};
				for (auto child_info : children_info)
				{
					firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());

					if (!child_info->nullable)
						break;
				}

				// compute lastpos
				FlatSet<int> lastpos{};
				for (auto it = children_info.rbegin(); it != children_info.rend(); ++it)
				{
					auto child_info = *it;
					lastpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());
				}
				
				// update followpos
				RegexNodeInfo* last_child_info = nullptr;
				for (auto child_info : children_info)
				{
					if (last_child_info)
					{
						for (auto i : last_child_info->lastpos)
						{
							followpos[i].insert(child_info->firstpos.begin(), child_info->firstpos.end());
						}
					}

					last_child_info = child_info;	
				}

				UpdateNodeInfo(expr, { nullable, firstpos, lastpos });
			}
			void Visit(const AlternationExpr& expr) override
			{
				std::vector<RegexNodeInfo*> children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = std::any_of(children_info.begin(), children_info.end(),
											[](const auto& info) { return info->nullable; });

				// compute firstpos and lastpos
				FlatSet<int> firstpos{};
				FlatSet<int> lastpos{};
				for (auto child_info : children_info)
				{
					firstpos.insert(child_info->firstpos.begin(), child_info->firstpos.end());
					lastpos.insert(child_info->lastpos.begin(), child_info->lastpos.end());
				}

				UpdateNodeInfo(expr, { nullable, firstpos, lastpos });
			}
			void Visit(const StarExpr& expr) override
			{
				expr.Child()->Accept(*this);

				const auto& child_info = *last_visited_info;
				for (auto i : child_info.lastpos)
				{
					followpos[i].insert(child_info.firstpos.begin(), child_info.firstpos.end());
				}

				UpdateNodeInfo(expr, { true, child_info.firstpos, child_info.lastpos });
			}

		} visitor{ labels };

		for (auto p : defs)
		{
			p->Accept(visitor);
		}
		return make_pair(visitor.func_map, visitor.followpos);
	}

	class LexerBuilder
	{
	public:
		int NewState()
		{
			jumptable_.resize(jumptable_.size() + 128, -1);
			return next_state_++;
		}
		int NewAcceptingState(int category)
		{
			auto state = NewState();
			acceptance_lookup_[state] = category;

			return state;
		}

		void NewTransition(int src, int target, int ch)
		{
			assert(ch >= 0 && ch < 128);
			assert(src >= 0 && target >= 0);
			assert(src < next_state_ && target < next_state_);

			jumptable_[src * 128 + ch] = target;
		}

		Lexer::SharedPtr Build()
		{

		}
	private:
		int next_state_ = 0;

		std::vector<int> acceptance_lookup_;
		vector<int> jumptable_;
	};

	Lexer::SharedPtr ConstructLexer(const RegexExprVec& defs)
	{
		auto labels = CollectEntityExpr(defs);
		auto[info_map, followpos] = EvaluateRegexExpr(defs, labels);

		using DfaState = FlatSet<int>;

		// construct initial state set and accepting catogory lookup
		DfaState initial_state{};
		std::unordered_map<int, int> acc_category_lookup;
		for (int i = 0; i<defs.size(); ++i)
		{
			const auto& info = info_map[defs[i]];

			// construct initial state
			initial_state.insert(info.firstpos.begin(), info.firstpos.end());

			// records accepting categories
			for (auto pos : info.lastpos)
			{
				acc_category_lookup[pos] = i;
			}
		}

		LexerBuilder builder;
		std::map<DfaState, int> state_lookup{};
		state_lookup[initial_state] = builder.NewState();
		for (deque<DfaState> unprocessed{ initial_state }; !unprocessed.empty(); unprocessed.pop_front())
		{
			const auto& src_state = unprocessed.front();
			const auto src_id = state_lookup[src_state];

			// for each input symbol
			for (int ch = 0; ch < 128; ++ch)
			{
				// construct the target DFA state
				DfaState target_state;
				for (auto pos : src_state)
				{
					const auto& t = followpos[pos];
					target_state.insert(t.begin(), t.end());
				}

				// skip empty state, that's invalid
				if (target_state.empty()) continue;

				// if it's a new state, register and queue it
				// also, we have to find out it's allocated id
				int target_id;
				if (auto it = state_lookup.find(target_state); it != state_lookup.end())
				{
					target_id = it->second;
				}
				else
				{
					// find out accepting category, if any
					auto acc_category =
						std::accumulate(target_state.begin(), target_state.end(), -1,
							[&](int acc, int pos) {

						int result = acc;
						if (auto it2 = acc_category_lookup.find(pos); it2 != acc_category_lookup.end())
						{
							if (acc == -1)
							{
								result = it2->second;
							}
							else
							{
								result = min(acc, it2->second);
							}
						}

						return result;
					});

					// allocate state id
					target_id = acc_category == -1
						? builder.NewState()
						: builder.NewAcceptingState(acc_category);

					// register and queue
					state_lookup[target_state] = target_id;
					unprocessed.push_back(target_state);
				}

				// update DFA transition table
				builder.NewTransition(src_id, target_id, ch);
			}
		}

		return builder.Build();
	}
}