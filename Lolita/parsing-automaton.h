#pragma once
#include "symbol.h"
#include "grammar.h"
#include <stack>
#include <vector>
#include <variant>
#include <optional>
#include <memory>
#include <type_traits>
#include <functional>

namespace eds::loli
{
	// Parsing Actions
	//

	class ParsingState;

	struct ActionShift
	{
		ParsingState* target;
	};
	struct ActionReduce
	{
		Production* production;
	};

	using ParsingAction = std::variant<ActionShift, ActionReduce>;

	class ParsingState
	{
	public:
		bool IsFinal() const { return is_final_; }

		const auto& ActionMap() const { return action_map_; }
		const auto& GotoMap() const { return goto_map_; }

	private:
		friend class ParsingAutomaton;

		bool is_final_ = false;

		std::unordered_map<Terminal*, ParsingAction> action_map_;
		std::unordered_map<NonTerminal*, ParsingState*> goto_map_;
	};

	class ParsingAutomaton
	{
	public:
		ParsingState* InitialState() const;

		ParsingState* NewState();

		void RegisterShift(ParsingState* src, ParsingState* dest, Symbol* s);
		void RegisterReduce(ParsingState* src, Production* p, Terminal* s);
		void RegisterReduceOnEof(ParsingState* src, Production* p);

	private:
		std::vector<std::unique_ptr<ParsingState>> states_;
	};

	std::unique_ptr<ParsingAutomaton> BuildSLRParsingTable(const Grammar& grammar);
}