#include "lexer.h"
#include "regex.h"
#include "flat-set.hpp"
#include "arena.hpp"
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <iterator>

using namespace std;

namespace lolita
{
	using RegexPositionLabel = const LabelledExpr*;
	using RegexPositionSet = FlatSet<RegexPositionLabel>;

	struct RegexNodeInfo
	{
		bool nullable;
		RegexPositionSet firstpos;
		RegexPositionSet lastpos;
	};

	auto EvaluateRegexExpr(const RegexExprVec& defs)
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
				last_visited_info = &(info_map[&expr] = forward<RegexNodeInfo>(info));
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
				UpdateNodeInfo(expr, { false, child_info.firstpos, { &expr } });
			}
			void Visit(const EntityExpr& expr) override
			{
				UpdateNodeInfo(expr, { false, { &expr }, { &expr } });
			}
			void Visit(const ConcatenationExpr& expr) override
			{
				auto children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = std::all_of(children_info.begin(), children_info.end(), 
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
			void Visit(const AlternationExpr& expr) override
			{
				auto children_info = VisitChildren(expr.Children());

				// compute nullable
				bool nullable = std::any_of(children_info.begin(), children_info.end(),
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

				auto strategy = expr.Strategy();
				if (strategy != ClosureStrategy::Optional)
				{
					for (auto pos : child_info.lastpos)
					{
						followpos[pos].insert(child_info.firstpos.begin(), child_info.firstpos.end());
					}
				}

				auto nullable = strategy != ClosureStrategy::Plus;
				UpdateNodeInfo(expr, { nullable, child_info.firstpos, child_info.lastpos });
			}

		} visitor{};

		for (auto p : defs)
		{
			p->Accept(visitor);
		}
		return make_pair(visitor.info_map, visitor.followpos);
	}

	class LexerBuilder
	{
	public:
		LexerBuilder(vector<string> category_names)
			: category_names_(category_names) { }

		int NewState()
		{
			jumptable_.resize(jumptable_.size() + 128, -1);
			acc_lookup_.push_back(-1);

			return next_state_++;
		}
		int NewAcceptingState(int category)
		{
			assert(category >= 0 && category < category_names_.size());

			auto state = NewState();
			acc_lookup_[state] = category;

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
			return std::make_shared<Lexer>(jumptable_, acc_lookup_, category_names_);
		}
	private:
		int next_state_ = 0;

		vector<int> jumptable_;
		vector<int> acc_lookup_;
		vector<string> category_names_;
	};

	RegexExprPtr ParseRegex(Arena& arena, zstring str);

	Lexer::SharedPtr ConstructLexer(const StringVec& rules)
	{
		Arena arena;
		RegexExprVec defs;
		for (const auto& regex : rules)
		{
			defs.push_back(ParseRegex(arena, regex.c_str()));
		}

		auto[info_map, followpos] = EvaluateRegexExpr(defs);

		using DfaState = RegexPositionSet;

		// construct initial state set and accepting catogory lookup
		DfaState initial_state{};
		unordered_map<RegexPositionLabel, int> acc_category_lookup;
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

		LexerBuilder builder{ rules };
		std::map<DfaState, int> dfa_state_lookup{};
		dfa_state_lookup[initial_state] = builder.NewState();
		for (deque<DfaState> unprocessed{ initial_state }; !unprocessed.empty(); unprocessed.pop_front())
		{
			const auto& src_state = unprocessed.front();
			const auto src_id = dfa_state_lookup[src_state];

			// for each input symbol
			for (int ch = 0; ch < 128; ++ch)
			{
				// construct the target DFA state
				DfaState target_state;
				for (auto src_pos : src_state)
				{
					if (src_pos->TestPassage(ch))
					{
						const auto& t = followpos[src_pos];
						target_state.insert(t.begin(), t.end());
					}
				}

				// skip empty state, that's invalid
				if (target_state.empty()) continue;

				// if it's a new state, register and queue it
				// also, we have to find out it's allocated id
				int target_id;
				if (auto it = dfa_state_lookup.find(target_state); it != dfa_state_lookup.end())
				{
					target_id = it->second;
				}
				else
				{
					// find out accepting category, if any
					auto acc_category =
						std::accumulate(target_state.begin(), target_state.end(), -1,
							[&](int acc, RegexPositionLabel pos) {

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
					dfa_state_lookup[target_state] = target_id;
					unprocessed.push_back(target_state);
				}

				// update DFA transition table
				builder.NewTransition(src_id, target_id, ch);
			}
		}

		return builder.Build();
	}

	// Tokenize
	//

	vector<Token> Tokenize(const Lexer& lexer, const string& s)
	{
		vector<Token> result;

		auto len = 0;
		auto last_acc_len = 0;
		auto last_acc_category = -1;
		auto state = lexer.InitialState();
		for (int i = 0; i < s.length(); ++i)
		{
			state = lexer.Transit(state, s[i]);
			++len;

			if (lexer.IsAccepting(state))
			{
				last_acc_len = len;
				last_acc_category = lexer.LookupAcceptingCategory(state);
			}
			
			if (lexer.IsInvalid(state) || i + 1 == s.length())
			{
				if (last_acc_category != -1)
				{
					// there is a intermediate accepted location
					// use that anyway
					result.push_back(Token{ last_acc_category, i - len + 1, last_acc_len });
					i = i - len + last_acc_len;
				}
				else
				{
					result.push_back(Token{ -1, i - len + 1, 1 });
					i = i - len + 1;
				}

				state = lexer.InitialState();
				len = 0;
				last_acc_len = 0;
				last_acc_category = -1;
			}
		}
		return result;
	}
}