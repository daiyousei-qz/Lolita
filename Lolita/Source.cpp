#include "lexer.h"
#include "grammar.h"
#include "debug.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace lolita
{
	Lexer::SharedPtr ConstructLexer(const StringVec& rules);

	Parser::Ptr CreateSLRParser(const Grammar::SharedPtr& g);
	Parser::Ptr CreateLALRParser(const Grammar::SharedPtr& g);
}

using namespace lolita;
using namespace std;

static constexpr auto umax = numeric_limits<unsigned>::max();

string Trim(const string& s)
{
	auto s1 = find_if_not(begin(s), end(s), isspace);
	auto s2 = find_if_not(rbegin(s), rend(s), isspace);

	auto d1 = distance(begin(s), s1);
	auto d2 = distance(rbegin(s), s2);

	return s.substr(d1, s.length() - d1 - d2);
}

vector<string> Split(const string& s, char delimiter)
{
	vector<string> result;

	auto s1 = begin(s);
	while (s1 != end(s))
	{
		auto s2 = find(s1, end(s), delimiter);
		result.push_back(string(s1, s2));
		
		if (s2 == s.end())
		{
			break;
		}
		else
		{
			s1 = next(s2);
		}
	}

	return result;
}

vector<string> SplitTrimmed(const string& s, char delimiter)
{
	auto result = Split(s, delimiter);
	for (auto& elem : result)
	{
		elem = Trim(elem);
	}

	return result;
}

auto LoadTerms(ifstream& file)
{
	vector<pair<string, string>> result;
	char buf[1024];

	while (file)
	{
		file.getline(buf, sizeof buf);

		if (Trim(string(buf)).empty()) continue;
		if (buf[0] == '#') continue;
		if (buf[0] == ';') break;

		auto data = SplitTrimmed(string(buf), ':');

		result.emplace_back(data[0], data[1]);
	}

	return result;
}

auto LoadNonTerms(ifstream& file)
{
	vector<string> result;
	char buf[1024];

	while (file)
	{
		file.getline(buf, sizeof buf);

		if (Trim(string(buf)).empty()) continue;
		if (buf[0] == '#') continue;
		if (buf[0] == ';') break;

		auto name = Trim(string(buf));

		result.emplace_back(name);
	}

	return result;
}

auto LoadProductions(ifstream& file)
{
	vector<vector<string>> result;
	char buf[1024];

	while (file)
	{
		file.getline(buf, sizeof buf);

		if (Trim(string(buf)).empty()) continue;
		if (buf[0] == '#') continue;
		if (buf[0] == ';') break;

		auto data1 = SplitTrimmed(string(buf), ':');
		auto data2 = SplitTrimmed(data1[1], ' ');

		vector<string> v;
		v.push_back(data1.front());
		v.insert(v.end(), begin(data2), end(data2));

		result.push_back(v);
	}

	return result;
}

int main()
{
	ifstream file{ "C:\\Users\\sunks\\Desktop\\aqua-spec.txt" };
	auto term_def = LoadTerms(file);
	auto nonterm_def = LoadNonTerms(file);
	auto production_def = LoadProductions(file);

	// construct lexer
	vector<string> names;
	vector<string> rules;
	for (const auto& item : term_def)
	{
		names.push_back(item.first);
		rules.push_back(item.second);
	}
	auto lexer = ConstructLexer(rules);

	// construct grammar
	GrammarBuilder builder;
	unordered_map<string, Symbol> symbols;

	for (const auto& term_name : names)
	{
		auto s = builder.NewTerm(term_name);
		symbols.insert_or_assign(term_name, s);
	}
	for (const auto& nonterm_name : nonterm_def)
	{
		auto s = builder.NewNonTerm(nonterm_name);
		symbols.insert_or_assign(nonterm_name, s);
	}

	for (const auto& p : production_def)
	{
		vector<Symbol> ss;
		for (const auto& name : p)
		{
			ss.push_back(symbols.at(name));
		}

		auto lhs = ss.front().AsNonTerminal();
		ss.erase(ss.begin());

		builder.NewProduction(lhs, ss);
	}

	auto root = symbols.at(nonterm_def[0]).AsNonTerminal();
	auto grammar = builder.Build(root);
	auto parser = CreateLALRParser(grammar);

	ifstream codefile{ "C:\\Users\\sunks\\Desktop\\test_fab.txt" };
	string code_str((istreambuf_iterator<char>(codefile)),
					 istreambuf_iterator<char>());
	
	auto tokens = Tokenize(*lexer, code_str);
	auto iter = std::remove_if(begin(tokens), end(tokens), [](auto tok) {return tok.category == 25; });
	tokens.erase(iter, tokens.end());
	for (auto tok : tokens)
	{
		printf("(%2d): %s\n", tok.category, code_str.substr(tok.offset, tok.length).c_str());
	}

	ParsingStack ctx;
	for (auto tok : tokens)
	{
		parser->Feed(ctx, tok.category);
	}
	parser->Finalize(ctx);
	
	system("pause");
}