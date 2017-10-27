#include "calc2.h"
#include "config.h"
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include "debug.h"
#include "parser.h"
#include "codegen.h"
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
	std::fstream file{ "d:\\aqua2.loli.txt" };
	return std::string(std::istreambuf_iterator<char>{file}, {});
}

int main()
{
	auto s = LoadConfigText();

	auto config = config::LoadConfig(s.c_str());
	auto parsing_info = BootstrapParser(*config);
	auto parsing_table = CreateParsingTable(*parsing_info);

	// cout << codegen::GenerateBindingCode(*parsing_info);

	auto proxy_manager = CreateAstTypeProxyManager();

	auto parser = BasicParser{ move(parsing_info), move(proxy_manager), move(parsing_table) };

	auto data = 
		"func add(x: int, y: int) -> int { return x+y; }\n"
		"func mul(x: int, y: int) -> int { return x*y; }\n"
		"func main() -> unit { if(true) while(true) if(true) {} else {} else val x:int=41; }\n"
	;
	parser.Parse(data);

	// cout << endl << GenerateDataBind(*parsing_info);
	system("pause");
}