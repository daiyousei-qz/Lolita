#pragma once
#include "ast-handle.h"
#include "grammar.h"
#include "config.h"
#include <string>
#include <vector>

namespace eds::loli
{
	struct TokenInfo
	{
		int id;
		std::string name;
		std::string regex;
	};

	struct KlassInfo
	{
		int id;
		std::string name;
	};

	struct ProductionInfo
	{
		int id;

		int klass_id;
		std::vector<int> derivation;
		AstHandle handle;
	};

	struct GrammarInfo
	{
		std::vector<TokenInfo> tokens_;
		std::vector<TokenInfo> ignored_tokens_;
		std::vector<KlassInfo> klasses_;
		std::vector<ProductionInfo> productions_;
	};

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

	struct ParserBootstrapInfo
	{
		GrammarInfo grammar;

		LexingInfo dfa;
		ParsingInfo pda;
	};

	std::unique_ptr<ParserBootstrapInfo> ConstructParserBootstrapInfo(const Grammar& g, AstConstructionLookup& lookup);
}