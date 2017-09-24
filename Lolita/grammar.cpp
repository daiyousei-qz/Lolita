#include "grammar.h"

namespace lolita
{
	// Implementation Of Grammar
	//


	// Implementation Of GrammarBuilder
	//

	Grammar::SharedPtr GrammarBuilder::Build(NonTerminal root)
	{
		// create a dummy non-terminal as a topmost one
		//
		auto root_dummy = NewNonTerm("_ROOT");
		NewProduction(root_dummy, { root });

		const size_t nonterm_count = nonterms_.size();

		// generate Grammar instance and clear builder
		//
		auto result = std::make_shared<Grammar>();

		result->terms_ = std::move(terms_);
		result->nonterms_ = std::move(nonterms_);
		result->productions_ = std::move(productions_);

		terms_ = {};
		nonterms_ = {};
		productions_ = {};

		return result;
	}
}