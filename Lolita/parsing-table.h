#pragma once
#include <vector>

namespace eds::loli
{
	struct LexingInfo
	{
		struct CharRange
		{
			int min;
			int max;
		};

		struct Transition
		{
			int src;
			int dest;

			CharRange rg;
		};

		int state_num;
		std::vector<Transition> transitions;
	};

	struct ParsingInfo
	{
		enum class ActionType
		{
			Shift,
			Reduce,
			Accept,
		};

		struct Transition
		{
			ActionType type;
			int src_state;
			int look_ahead;

			int parameter; // state for Shift, production_id for Reduce and Accept
		};

		int state_num;
		std::vector<Transition> transitions_;
	};

	class ParsingTable
	{

	};
}