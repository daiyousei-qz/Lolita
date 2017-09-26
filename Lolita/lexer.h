#pragma once
#include "lolita-basic.h"
#include "token.h"
#include <unordered_map>
#include <memory>

namespace lolita
{
	class Lexer
	{
	public:
		using SharedPtr = std::shared_ptr<Lexer>;

		Lexer(vector<int> dfa_table, vector<int> acc_categories, vector<string> category_names)
			: dfa_table_(dfa_table)
			, acc_categories_(acc_categories)
			, category_names_(category_names) { }

		size_t StateCount() const
		{
			return acc_categories_.size();
		}

		bool IsInvalid(int state) const
		{
			return state == -1;
		}

		bool IsAccepting(int state) const
		{
			return state != -1 && acc_categories_[state] != -1;
		}

		int LookupAcceptingCategory(int state) const
		{
			return acc_categories_[state];
		}

		const auto& LookupCategoryName(int category) const
		{
			return category_names_[category];
		}

		int InitialState() const
		{
			return 0;
		}

		int Transit(int src, int ch) const
		{
			assert(src >= 0 && src <= StateCount());
			assert(ch >= 0 && ch < 128);

			return dfa_table_[src * 128 + ch];
		}

	private:
		vector<int> dfa_table_;
		vector<int> acc_categories_;
		vector<string> category_names_;
	};

	vector<Token> Tokenize(const Lexer& lexer, const string& s);
}