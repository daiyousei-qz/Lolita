#include "config.h"
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include <fstream>

std::string LoadConfigText()
{
	std::fstream file{ "d:\\config.txt" };
	return std::string(std::istreambuf_iterator<char>{file}, {});
}

int main()
{
	using namespace std;
	using namespace eds::loli;

	auto s = LoadConfigText();

	auto config = config::ParseConfig(s.c_str());
	auto parsing_info = BootstrapParser(*config);
	auto parsing_table = ParsingTable::Create(*parsing_info);

	system("pause");
}