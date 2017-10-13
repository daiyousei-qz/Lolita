#include "GeneratedParser.h"
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <functional>
#include <any>

using namespace std;

namespace lolita
{
	struct Symbol
	{
		bool terminal;
		int id;
	};

	struct Token
	{
		int category;
		int offset;
		int length;
	};

	struct ActionShift
	{
		int target_state;
	};

	struct ActionReduce
	{
		int production_id;
	};

	struct ActionAccept { };
	struct ActionError { };

	using Action 
		= variant<ActionShift, ActionReduce, ActionAccept, ActionError>;

	union AstStackSlot
	{
		string_view tok_data;

		bool bool_val;
		int enum_val;
		AstObject* object;

		template <typename T>
		T Cast()
		{
			if constexpr(is_same_v<T, string_view>)
			{
				return tok_data;
			}
			else if constexpr(is_same_v<T, bool>)
			{
				return bool_val;
			}
			else if constexpr(is_enum_v<T>)
			{
				return static_cast<T>(enum_val);
			}
			else if constexpr(is_pointer_v<T> && is_base_of_v<AstObject, remove_pointer_t<T>>)
			{
				return reinterpret_cast<T>(object);
			}
		}
	};

	class ParsingContext
	{
		void PushToken(string_view tok)
		{

		}

		void Merge(int count, AstStackSlot(*proxy)(ParsingContext&, AstStackSlot*))
		{

		}

		template <typename T>
		T* ConstructAst()
		{
			static_assert(is_base_of_v<AstObject, T>);

			// ...
		}
	private:
		deque<AstObject::Ptr> arena_;
		vector<AstStackSlot> ast_stack_;
	};

	struct StateSlot
	{
		Symbol symbol;
		int state;
	};

	AstStackSlot Merge(ParsingContext& ctx, AstStackSlot* k)
	{
		auto p1 = (k++)->Cast<string_view>();
		auto p2 = (k++)->Cast<bool>();
		auto p3 = (k++)->Cast<BinaryOp>();
		// ...

		return ...;
	}

	namespace details
	{
		// Grammar Metadata
		//
		static const vector<string> kTerms = {};
		static const vector<string> kNonterms = {};
		static const vector<vector<Symbol>> kProductions = {};

		// Lexical Analysis Stuffs
		//
		static const int kDfaInitialState = 0;
		static const int kDfaTableWidth = 128;

		static const vector<int> kDfaTable = {};
		static const vector<int> kAccCategories = {};
		static const vector<bool> kTokenBlacklist = {};

		// Syntactic Analysis Stuffs
		//
		static const int kParserInitialState = 0;
		static const int kActionTableWidth = 0;
		static const int kGotoTableWidth = 0;
		static const int kTermEof = 0;

		static const vector<Action> kActionTable = {};
		static const vector<int> kGotoTable = {};
		static const vector<void(*)(ParsingContext&)> kReductionCallbacks = {};
	}
	using namespace details;
	
	Token LoadToken(string_view data, int offset)
	{
		auto last_acc_len = 0;
		auto last_acc_category = -1;

		auto state = kDfaInitialState;
		for (int i = offset; i < data.length(); ++i)
		{
			state = kDfaTable[state * kDfaTableWidth + data[i]];

			if (state == -1)
			{
				break;
			}
			else if (kAccCategories[state] != -1)
			{
				last_acc_len = i - offset + 1;
				last_acc_category = kAccCategories[state];
			}
		}

		if (last_acc_len != 0)
		{
			return Token{ last_acc_category, offset, last_acc_len };
		}
		else
		{
			return Token{ -1, offset, 1 };
		}
	}

	int LookupCurrentState(vector<StateSlot>& states)
	{
		return states.empty()
			? kParserInitialState
			: states.back().state;
	}

	bool FeedParser(vector<StateSlot>& states, ParsingContext& ctx, int tok)
	{
		struct ActionVisitor
		{
			vector<StateSlot>& states;
			ParsingContext& ctx;
			int tok;

			bool hungry = true;
			bool accepted = false;
			bool error = false;

			ActionVisitor(vector<StateSlot>& states, ParsingContext& ctx, int tok)
				: states(states), ctx(ctx), tok(tok) { }

			void operator()(ActionShift action)
			{
				states.push_back(StateSlot{ Symbol{ true, tok }, action.target_state });

				hungry = false;
			}
			void operator()(ActionReduce action)
			{
				auto id = action.production_id;
				const auto& production = kProductions[id];

				for (auto i = 1; i < production.size(); ++i)
					states.pop_back();

				auto src_state = LookupCurrentState(states);
				auto goto_ind = src_state*kGotoTableWidth + production.front().id;
				auto target_state = kGotoTable[goto_ind];

				states.push_back(StateSlot{ id, target_state });
				kReductionCallbacks[id](ctx);
			}
			void operator()(ActionAccept action)
			{
				hungry = false;
				accepted = true;
			}
			void operator()(ActionError action)
			{
				hungry = false;
				error = true;
			}
		} visitor{states, ctx, tok};

		while (visitor.hungry)
		{
			auto action_ind = LookupCurrentState(states) * kActionTableWidth + tok;

			visit(visitor, kActionTable[action_ind]);
		}

		if (visitor.error) throw 0;

		return visitor.accepted;
	}

	ParsingResult::Ptr Parse(string_view data)
	{
		int offset = 0;
		vector<StateSlot> states;
		ParsingContext ctx;

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(data, offset);

			// throw for invalid token
			if (tok.category == -1) throw 0;
			// ignore categories in blacklist
			if (kTokenBlacklist[tok.category]) continue;

			FeedParser(states, ctx, tok.category);
			ctx.ast_stack.push_back(AstStackSlot{ data.substr(tok.offset, tok.length) });

			// update offset
			offset = tok.offset + tok.length;
		}

		// finalize parsing
		auto accepted = FeedParser(states, ctx, kTermEof);
		if (!accepted) throw 0;

		// construct parsing result
		auto obj = reinterpret_cast<Expression*>(ctx.ast_stack.front().object);
		return make_unique<ParsingResult>(move(ctx.arena), obj);
	}
}