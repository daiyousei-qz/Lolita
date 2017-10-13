#pragma once
#include "regex.h"
#include "grammar.h"
#include <functional>
#include <optional>

namespace eds::loli
{
	class LexingState
	{
	public:
		struct Hasher
		{
			size_t operator()(LexingState s) const
			{
				return s.Id();
			}
		};

		explicit LexingState(int id) : id_(id) { }
		explicit LexingState() : LexingState(-1) { }

		bool IsValid() const noexcept
		{
			return id_ >= 0;
		}

		int Id() const noexcept
		{
			return id_;
		}

	private:
		int id_;
	};

	inline bool operator==(LexingState lhs, LexingState rhs)
	{
		return lhs.Id() == rhs.Id();
	}
	inline bool operator!=(LexingState lhs, LexingState rhs)
	{
		return !(lhs == rhs);
	}

	class LexingAutomaton
	{
	public:
		// Accessor
		//

		// (ch, target_state)
		using TransitionVisitor = std::function<void(int, LexingState)>;

		LexingState InitialState() const;
		Terminal* LookupAcceptCategory(LexingState state) const;
		void EnumerateTransitions(LexingState src, TransitionVisitor callback) const;

		// Construction
		//

		LexingState NewState(Terminal* acc_term = nullptr);
		void NewTransition(LexingState src, LexingState dest, int ch);

	private:
		std::unordered_map<LexingState, Terminal*, LexingState::Hasher> acc_lookup_;
		std::vector<LexingState> jumptable_;
	};

	std::unique_ptr<LexingAutomaton> BuildLexingAutomaton(const Grammar& g);
	std::unique_ptr<LexingAutomaton> OptimizeLexingAutomaton(const LexingAutomaton& atm);
}