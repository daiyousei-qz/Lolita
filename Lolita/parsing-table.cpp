#include "parsing-table.h"
#include "debug.h"
#include "text\format.hpp"

using namespace std;

namespace eds::loli
{
	// Implementation of ParsingTable
	//

	auto TranslateAction(parsing::ParsingAction action)
	{
		return visit([](auto t) -> ParsingAction {
			using T = decay_t<decltype(t)>;
			if constexpr(is_same_v<T, parsing::ActionReduce>)
			{
				return PdaActionReduce{ t.production->id };
			}
			else if constexpr(is_same_v<T, parsing::ActionShift>)
			{
				return PdaActionShift{
					PdaStateId{ t.target->id } 
				};
			}
			else
			{
				static_assert(AlwaysFalse<T>::value);
			}
		}, action);
	}

	ParsingTable::ParsingTable(const ParserBootstrapInfo& info,
							   const lexing::LexingAutomaton& dfa,
							   const parsing::ParsingAutomaton& pda)
		: token_num_(info.Tokens().size() + info.IgnoredTokens().size())
		, term_num_(info.Tokens().size())
		, nonterm_num_(info.Variables().size())
		, dfa_state_num_(dfa.StateCount())
		, pda_state_num_(pda.StateCount())
	{
		token_num_ = info.Tokens().size() + info.IgnoredTokens().size();
		term_num_ = info.Tokens().size();
		nonterm_num_ = info.Variables().size();

		// copy dfa
		//

		acc_category_ = make_unique<int[]>(dfa_state_num_);
		dfa_table_ = make_unique<DfaStateId[]>(128 * dfa_state_num_);

		fill_n(acc_category_.get(), dfa_state_num_, -1);

		for (int id = 0; id < dfa_state_num_; ++id)
		{
			const auto state = dfa.LookupState(id);

			acc_category_[id] = state->acc_category;
			for (const auto edge : state->transitions)
			{
				dfa_table_[id * 128 + edge.first] = DfaStateId{ edge.second->id };
			}
		}

		// copy pda
		//

		eof_action_table_ = make_unique<ParsingAction[]>(pda_state_num_);
		action_table_ = make_unique<ParsingAction[]>(pda_state_num_*term_num_);
		goto_table_ = make_unique<PdaStateId[]>(pda_state_num_*nonterm_num_);

		for (int id = 0; id < pda_state_num_; ++id)
		{
			auto state = pda.LookupState(id);

			if (state->eof_action)
			{
				eof_action_table_[id] = TranslateAction(*state->eof_action);
			}

			for (const auto& pair : state->action_map)
			{
				action_table_[id * term_num_ + pair.first->id] = TranslateAction(pair.second);
			}

			for (const auto& pair : state->goto_map)
			{
				goto_table_[id * nonterm_num_ + pair.first->id] = PdaStateId{ pair.second->id };
			}
		}
	}

	//
	//

	string RemoveQuote(const string& s)
	{
		assert(s.size() > 2);
		assert(s.front() == '"' && s.back() == '"');

		return s.substr(1, s.size() - 2);
	}

	auto ConstructLexingAutomaton(const ParserBootstrapInfo& info)
	{
		vector<string> regex;

		for (const auto& tok : info.Tokens())
			regex.push_back(RemoveQuote(tok.regex));

		for (const auto& tok : info.IgnoredTokens())
			regex.push_back(RemoveQuote(tok.regex));

		return lexing::BuildDfaAutomaton(regex);
	}

	auto ConstructParsingGrammar(const ParserBootstrapInfo& info)
	{
		using namespace eds::loli::parsing;

		Grammar grammar;
		unordered_map<const SymbolInfo*, Symbol*> symbol_lookup;

		for (int id = 0; id < info.Tokens().size(); ++id)
		{
			symbol_lookup[&info.Tokens()[id]] = grammar.NewTerm(id);
		}

		for (int id = 0; id < info.Variables().size(); ++id)
		{
			symbol_lookup[&info.Variables()[id]] = grammar.NewNonterm(id);
		}

		for (int id = 0; id < info.Productions().size(); ++id)
		{
			const auto& p = info.Productions()[id];

			auto lhs = symbol_lookup[p.lhs]->AsNonterminal();
			auto rhs = vector<Symbol*>{};

			for (auto elem : p.rhs)
				rhs.push_back(symbol_lookup[elem]);

			grammar.NewProduction(id, lhs, rhs);
		}

		grammar.ConfigureRootSymbol(
			symbol_lookup[&info.Variables().back()]->AsNonterminal()
		);

		return grammar;
	}

	vector<uint8_t> DumpParsingTable(const ParserBootstrapInfo& info)
	{
		throw 0;
	}

	unique_ptr<const ParsingTable> CreateParsingTable(const ParserBootstrapInfo& info)
	{
		auto dfa = ConstructLexingAutomaton(info);

		// TODO: deal with the mess
		auto grammar = ConstructParsingGrammar(info);
		auto pda = parsing::BuildSLRAutomaton(grammar);

		return make_unique<ParsingTable>(info, *dfa, *pda);
	}
	unique_ptr<const ParsingTable> RecoverParsingTable(const ParserBootstrapInfo& info, const vector<uint8_t>& data)
	{
		throw 0;
	}
}