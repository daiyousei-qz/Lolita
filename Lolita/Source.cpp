#include "lexer.h"
#include "grammar.h"
#include "config.h"
#include "debug.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

using namespace lolita;
using namespace std;

int main()
{
	fstream file{ "C:\\Users\\sunks\\Desktop\\calc.loli" };
	string content(istreambuf_iterator<char>(file), {});
	auto config = LoadConfig(content);

	system("pause");
}