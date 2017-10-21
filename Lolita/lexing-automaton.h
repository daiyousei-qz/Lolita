#pragma once
#include "regex.h"
#include "parsing-bootstrap.h"
#include <functional>
#include <optional>

namespace eds::loli::lexing
{
	struct DfaState;
	struct DfaTransition;

	struct DfaState
	{
		int id;
		int acc_category;
		std::unordered_map<int, DfaState*> transitions;

	public:
		DfaState(int id, int acc_category)
			: id(id), acc_category(acc_category) { }
	};

	class LexingAutomaton : NonCopyable
	{
	public:
		// Accessor
		//

		int StateCount() const
		{
			return states_.size();
		}
		const DfaState* LookupState(int id) const
		{
			return states_.at(id).get();
		}

		// Construction
		//

		DfaState* NewState(int acc_category = -1)
		{
			int id = StateCount();

			return states_.emplace_back(
				std::make_unique<DfaState>(id, acc_category)
			).get();
		}
		void NewTransition(DfaState* src, DfaState* target, int ch)
		{
			assert(src->transitions.count(ch) == 0);
			src->transitions.insert_or_assign(ch, target);
		}

	private:
		std::vector<std::unique_ptr<DfaState>> states_;
	};

	std::unique_ptr<const LexingAutomaton> BuildDfaAutomaton(const std::vector<std::string>& regex);
	std::unique_ptr<const LexingAutomaton> OptimizeLexingAutomaton(const LexingAutomaton& atm);
}