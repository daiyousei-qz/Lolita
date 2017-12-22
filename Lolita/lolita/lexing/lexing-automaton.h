#pragma once
#include "lolita/core/regex.h"
#include "lolita/core/parsing-info.h"
#include <functional>
#include <optional>

namespace eds::loli::lexing
{
	struct DfaState;
	struct DfaTransition;

	struct DfaState
	{
		int id;
		const TokenInfo* acc_token;
		std::unordered_map<int, DfaState*> transitions;

	public:
		DfaState(int id, const TokenInfo* acc_token = nullptr)
			: id(id), acc_token(acc_token) { }
	};

	class LexingAutomaton : NonCopyable, NonMovable
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

		DfaState* NewState(const TokenInfo* acc_category = nullptr)
		{
			int id = StateCount();

			return states_.emplace_back(
				std::make_unique<DfaState>(id, acc_category)
			).get();
		}
		void NewTransition(DfaState* src, DfaState* target, int ch)
		{
			assert(ch >= 0 && ch < 128);
			assert(src->transitions.count(ch) == 0);
			src->transitions.insert_or_assign(ch, target);
		}

	private:
		std::vector<std::unique_ptr<DfaState>> states_;
	};

	std::unique_ptr<const LexingAutomaton> BuildLexingAutomaton(const ParsingMetaInfo& info);
}