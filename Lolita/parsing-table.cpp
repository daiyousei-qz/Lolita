#include "parsing-table.h"
#include "lexing-automaton.h"
#include "parsing-automaton.h"
#include "debug.h"
#include "text\format.hpp"

using namespace std;

namespace eds::loli
{
	template <typename T>
	struct AlwaysFalse : public false_type { };

	string RemoveQuote(const string& s)
	{
		assert(s.size() > 2);
		assert(s.front() == '"' && s.back() == '"');

		return s.substr(1, s.size() - 2);
	}

	auto GetRegex(const ParserBootstrapInfo& info)
	{
		vector<string> result;

		for (const auto& tok : info.Tokens())
			result.push_back(RemoveQuote(tok.regex));

		for (const auto& tok : info.IgnoredTokens())
			result.push_back(RemoveQuote(tok.regex));

		return result;
	}

	auto GetGrammar(const ParserBootstrapInfo& info)
	{
		using namespace eds::loli::parsing;

		auto result = make_unique<Grammar>();
		
		unordered_map<const SymbolInfo*, Symbol*> symbol_lookup;

		for (int id = 0; id < info.Tokens().size(); ++id)
		{
			symbol_lookup[&info.Tokens()[id]] = result->NewTerm(id);
		}

		for (int id = 0; id < info.Variables().size(); ++id)
		{
			symbol_lookup[&info.Variables()[id]] = result->NewNonterm(id);
		}

		for (int id = 0; id < info.Productions().size(); ++id)
		{
			const auto& p = info.Productions()[id];

			auto lhs = symbol_lookup[p.lhs]->AsNonterminal();
			auto rhs = vector<Symbol*>{};

			for (auto elem : p.rhs)
				rhs.push_back(symbol_lookup[elem]);

			result->NewProduction(id, lhs, rhs);
		}

		result->ConfigureRootSymbol(
			symbol_lookup[&info.Variables().back()]->AsNonterminal()
		);

		return result;
	}

	auto TranslateAction(parsing::ParsingAction action)
	{
		return visit([](auto t) -> ParsingTable::ParsingAction {
			using T = decay_t<decltype(t)>;
			if constexpr(is_same_v<T, parsing::ActionReduce>)
			{
				return ParsingTable::ActionReduce{ t.production->id };
			}
			else if constexpr(is_same_v<T, parsing::ActionShift>)
			{
				return ParsingTable::ActionShift{
					ParsingTable::PdaState{ t.target->id } 
				};
			}
			else
			{
				static_assert(AlwaysFalse<T>::value);
			}
		}, action);
	}

	std::unique_ptr<ParsingTable> ParsingTable::Create(const ParserBootstrapInfo& info)
	{
		debug::PrintGrammar(info);
		printf("\n");

		auto result = make_unique<ParsingTable>();

		// copy basic information
		const auto token_cnt 
			= result->token_num_ 
			= info.Tokens().size() + info.IgnoredTokens().size();
		
		const auto term_cnt
			= result->term_num_ 
			= info.Tokens().size();
		
		const auto nonterm_cnt
			= result->nonterm_num_ 
			= info.Variables().size();

		// genreate dfa
		{
			auto regex = GetRegex(info);
			auto dfa = lexing::BuildLexingAutomaton(regex);

			debug::PrintLexingAutomaton(info, *dfa);
			printf("\n");

			auto state_cnt 
				= result->dfa_state_num_ 
				= dfa->StateCount();

			// shortcuts
			auto& acc_table = result->acc_category_;
			auto& dfa_table = result->dfa_table_;

			acc_table.resize(state_cnt, -1);
			dfa_table.resize(128 * state_cnt, {});

			for (int id = 0; id < state_cnt; ++id)
			{
				const auto state = dfa->LookupState(id);
			
				acc_table[id] = state->acc_category;
				for (const auto edge : state->transitions)
				{
					dfa_table[id * 128 + edge.first] = DfaState{ edge.second->id };
				}
			}
		}

		// generate pda
		{
			const auto grammar = GetGrammar(info);
			const auto pda = parsing::BuildSLRAutomaton(*grammar);

			debug::PrintParsingAutomaton(info, *pda);
			printf("\n");

			const auto state_cnt
				= result->pda_state_num_ 
				= pda->StateCount();

			// shortcuts
			auto& eof_action_table = result->eof_action_table_;
			auto& action_table = result->action_table_;
			auto& goto_table = result->goto_table_;

			eof_action_table.resize(state_cnt, ActionError{});
			action_table.resize(term_cnt * state_cnt, ActionError{});
			goto_table.resize(nonterm_cnt * state_cnt, {});

			for (int id = 0; id < pda->StateCount(); ++id)
			{
				auto state = pda->LookupState(id);

				if (state->eof_action)
				{
					eof_action_table[id] = TranslateAction(*state->eof_action);
				}

				for (const auto& pair : state->action_map)
				{
					action_table[id * term_cnt + pair.first->id] = TranslateAction(pair.second);
				}

				for (const auto& pair : state->goto_map)
				{
					goto_table[id * nonterm_cnt + pair.first->id] = PdaState{ pair.second->id };
				}
			}
		}

		return result;
	}
}