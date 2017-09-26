#include "regex.h"
#include "arena.hpp"

namespace lolita
{
	// Parsing
	//

	void MergeSequence(Arena& arena, RegexExprVec& any, RegexExprVec& seq)
	{
		assert(!seq.empty());

		auto expr = arena.Construct<ConcatenationExpr>(seq);
		seq.clear();
		any.push_back(expr);
	}

	int EscapeRawCharacter(int ch)
	{
		switch (ch)
		{
		case 'a':
			return '\a';
		case 'b':
			return '\b';
		case 't':
			return '\t';
		case 'r':
			return '\r';
		case 'v':
			return '\v';
		case 'f':
			return '\f';
		case 'n':
			return '\n';
		case 'e':
			return '\e';
		default:
			return ch;
		}
	}

	int ParseCharacter(zstring& str)
	{
		if (*str != '\\')
		{
			return *str;
		}
		else
		{
			++str;
			return EscapeRawCharacter(*str);
		}
	}

	RegexExprPtr ParseEscapedExpr(Arena& arena, zstring& str)
	{
		auto rg = CharRange{ EscapeRawCharacter(*str) };
		return arena.Construct<EntityExpr>(rg);
	}

	// TODO: support simple escaping in char class
	RegexExprPtr ParseCharClass(Arena& arena, zstring& str)
	{
		auto p = str;

		// check leading '^'
		bool reverse = false;
		if (*p == '^')
		{
			throw 0; // not supported yet

			reverse = true;
			++p;
		}

		// load char class
		int last_ch;
		vector<CharRange> ranges;

		bool joinable = false;
		for (; *p && *p != ']'; ++p)
		{
			if (joinable) // i.e. a value pending in last_ch
			{
				if (*p == '-')
				{
					++p; // discard '-'
					if (*p == '\0') throw 0; // unexpected eof

					joinable = false; // disable joining

					// if '-' is not last character
					if (*p != ']')
					{
						int min = last_ch;
						int max = ParseCharacter(p);
						if (max < min) std::swap(min, max);

						ranges.push_back(CharRange{ min, max });
					}
					else // tailing '-' is a raw character
					{
						ranges.push_back(CharRange{ '-' });
						break; // early break for closing bracket
					}
				}
				else
				{
					ranges.push_back(CharRange{ last_ch });

					last_ch = ParseCharacter(p);
				}
			}
			else
			{
				joinable = true;

				last_ch = ParseCharacter(p); // don't save it into ranges yet
			}
		}

		if (joinable) ranges.push_back(CharRange{ last_ch });

		if (!*p) throw 0; // unexpected eof
		str = p; // don't discard ']'

		RegexExprVec result;
		for (auto rg : ranges)
		{
			result.push_back(arena.Construct<EntityExpr>(rg));
		}

		return result.size() == 1
			? result.front()
			: arena.Construct<AlternationExpr>(result);
	}

	RegexExprPtr ParseRegexInternal(Arena& arena, zstring& str, char term)
	{
		RegexExprVec any{};
		RegexExprVec seq{};

		bool allow_closure = false;
		auto p = str;
		for (; *p && *p != term; ++p)
		{
			if (*p == '|')
			{
				if (seq.empty()) throw 0;

				allow_closure = false;

				MergeSequence(arena, any, seq);
			}
			else if(*p == '(')
			{
				allow_closure = true;

				auto expr = ParseRegexInternal(arena, ++p, ')');
				seq.push_back(expr);
			}
			else if (*p == '*' || *p == '+' || *p == '?')
			{
				if (seq.empty() || !allow_closure) throw 0;

				allow_closure = false;

				ClosureStrategy strategy;
				switch (*p)
				{
				case '?':
					strategy = ClosureStrategy::Optional;
					break;
				case '*':
					strategy = ClosureStrategy::Star;
					break;
				case '+':
					strategy = ClosureStrategy::Plus;
					break;
				}

				auto to_repeat = seq.back();
				seq.pop_back();

				auto expr = arena.Construct<ClosureExpr>(to_repeat, strategy);
				seq.push_back(expr);
			}
			else if (*p == '[')
			{
				allow_closure = true;

				auto expr = ParseCharClass(arena, ++p);
				seq.push_back(expr);
			}
			else if (*p == '\\')
			{
				allow_closure = true;

				auto expr = ParseEscapedExpr(arena, ++p);
				seq.push_back(expr);
			}
			else
			{
				allow_closure = true;

				auto expr = arena.Construct<EntityExpr>(CharRange{ *p });
				seq.push_back(expr);
			}
		}

		if (seq.empty()) throw 0;

		str = p;
		MergeSequence(arena, any, seq);

		return any.size() == 1
			? any.front()
			: arena.Construct<AlternationExpr>(any);
	}

	RegexExprPtr ParseRegex(Arena& arena, zstring str)
	{
		auto expr = ParseRegexInternal(arena, str, '\0');

		return arena.Construct<RootExpr>(expr);
	}
}