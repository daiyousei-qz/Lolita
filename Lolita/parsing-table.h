#pragma once
#include "parsing-bootstrap.h"
#include <vector>
#include <memory>

namespace eds::loli
{
	class ParsingTable
	{
	public:
		class AutomatonState
		{
		public:
			AutomatonState() : id_(-1) { }
			AutomatonState(int id) : id_(id) 
			{
				assert(id >= 0);
			}

			bool IsValid() const
			{
				return id_ != -1;
			}

			int Id() const
			{
				return id_;
			}

		private:
			int id_;
		};

		class DfaState : public AutomatonState 
		{
		public:
			DfaState() = default;
			explicit DfaState(int id) : AutomatonState(id) { }
		};
		class PdaState : public AutomatonState 
		{
		public:
			PdaState() = default;
			explicit PdaState(int id) : AutomatonState(id) { }
		};

		struct ActionError { };

		struct ActionShift
		{
			PdaState destination;
		};

		struct ActionReduce
		{
			int production;
		};

		using ParsingAction = std::variant<ActionError, ActionShift, ActionReduce>;

		int TokenCount() const { return token_num_; }
		int TerminalCount() const { return term_num_; }
		int NonterminalCount() const { return nonterm_num_; }

		int DfaStateCount() const { return dfa_state_num_; }
		int PdaStateCount() const { return pda_state_num_; }

		AutomatonState LookupDfaTransition(DfaState src, int ch);

		ParsingAction LookupAction(PdaState src, int term);
		ParsingAction LookupActionOnEof(PdaState src);
		ParsingAction LookupGoto(PdaState src, int nonterm);

		static std::unique_ptr<ParsingTable> Create(const ParserBootstrapInfo& info);
	private:
		int token_num_;
		int term_num_;
		int nonterm_num_;

		int dfa_state_num_;
		int pda_state_num_;

		std::vector<int> acc_category_;
		std::vector<DfaState> dfa_table_; // 128 columns, dfa_state_num_ rows
	
		std::vector<ParsingAction> action_table_; // term_num_ columns, pda_state_num_ rows
		std::vector<ParsingAction> eof_action_table_; // 1 column, pda_state_num_ rows
		std::vector<PdaState> goto_table_; // nonterm_num_ columns, pda_state_num_ rows
	};
}