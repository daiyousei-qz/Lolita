#pragma once
#include "parsing-bootstrap.h"
#include "lexing-automaton.h"
#include "parsing-automaton.h"
#include <vector>
#include <memory>

namespace eds::loli
{
	class AutomatonStateId
	{
	public:
		AutomatonStateId() : value_(-1) { }
		AutomatonStateId(int id) : value_(id)
		{
			assert(id >= 0);
		}

		bool IsValid() const
		{
			return value_ >= 0;
		}

		int Value() const
		{
			return value_;
		}

	private:
		int value_;
	};

	class DfaStateId : public AutomatonStateId
	{
	public:
		DfaStateId() = default;
		explicit DfaStateId(int value) : AutomatonStateId(value) { }
	};
	class PdaStateId : public AutomatonStateId
	{
	public:
		PdaStateId() = default;
		explicit PdaStateId(int value) : AutomatonStateId(value) { }
	};

	struct PdaActionError { };

	struct PdaActionShift
	{
		PdaStateId destination;
	};

	struct PdaActionReduce
	{
		int production;
	};

	using ParsingAction = std::variant<PdaActionError, PdaActionShift, PdaActionReduce>;

	class ParsingTable
	{
	public:
		ParsingTable(const ParserBootstrapInfo& info,
					 const lexing::LexingAutomaton& dfa,
					 const parsing::ParsingAutomaton& pda);

		int TokenCount() const { return token_num_; }
		int TerminalCount() const { return term_num_; }
		int NonterminalCount() const { return nonterm_num_; }

		int DfaStateCount() const { return dfa_state_num_; }
		int PdaStateCount() const { return pda_state_num_; }

		DfaStateId LexerInitialState() const { return DfaStateId{ 0 }; }
		PdaStateId ParserInitialState() const { return PdaStateId{ 0 }; }

		DfaStateId LookupDfaTransition(DfaStateId src, int ch) const
		{
			assert(src.IsValid() && ch >= 0 && ch < 128);
			return dfa_table_[128 * src.Value() + ch];
		}
		int LookupAccCategory(DfaStateId src) const
		{
			return acc_category_[src.Value()];
		}

		ParsingAction LookupAction(PdaStateId src, int term) const
		{
			assert(src.IsValid() && term >= 0 && term < term_num_);
			return action_table_[term_num_ * src.Value() + term];
		}
		ParsingAction LookupActionOnEof(PdaStateId src) const
		{
			assert(src.IsValid());
			return eof_action_table_[src.Value()];
		}
		PdaStateId LookupGoto(PdaStateId src, int nonterm) const
		{
			assert(src.IsValid() && nonterm >= 0 && nonterm <= nonterm_num_);
			return goto_table_[nonterm_num_ * src.Value() + nonterm];
		}

	private:
		int token_num_;
		int term_num_;
		int nonterm_num_;

		int dfa_state_num_;
		int pda_state_num_;

		std::unique_ptr<int[]> acc_category_; // 1 column, token_num_ rows
		std::unique_ptr<DfaStateId[]> dfa_table_; // 128 columns, dfa_state_num_ rows
	
		std::unique_ptr<ParsingAction[]> action_table_; // term_num_ columns, pda_state_num_ rows
		std::unique_ptr<ParsingAction[]> eof_action_table_; // 1 column, pda_state_num_ rows
		std::unique_ptr<PdaStateId[]> goto_table_; // nonterm_num_ columns, pda_state_num_ rows
	};

	std::vector<uint8_t> DumpParsingTable(const ParserBootstrapInfo& info);

	std::unique_ptr<const ParsingTable> CreateParsingTable(const ParserBootstrapInfo& info);
	std::unique_ptr<const ParsingTable> RecoverParsingTable(const ParserBootstrapInfo& info, const std::vector<uint8_t>& data);
}