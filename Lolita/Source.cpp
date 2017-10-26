#include "calc2.h"
#include "config.h"
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include "debug.h"
#include "parser.h"
#include "arena.hpp"
#include "text/format.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <any>

using namespace std;
using namespace eds;
using namespace eds::loli;
using namespace eds::text;

std::string LoadConfigText()
{
	std::fstream file{ "d:\\aqua.loli.txt" };
	return std::string(std::istreambuf_iterator<char>{file}, {});
}

int main()
{
	auto s = LoadConfigText();

	auto config = config::LoadConfig(s.c_str());
	auto parsing_info = BootstrapParser(*config);
	auto parsing_table = CreateParsingTable(*parsing_info);

	// cout << G2(*parsing_info);

	auto proxy_manager = CreateAstTypeProxyManager();

	auto parser = BasicParser{ move(parsing_info), move(proxy_manager), move(parsing_table) };

	auto data = "while(true) { let x=41; break; }";
	parser.Parse(data);

	// cout << endl << GenerateDataBind(*parsing_info);
	system("pause");
}