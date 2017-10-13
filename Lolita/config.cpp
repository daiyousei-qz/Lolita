#include "config.h"
#include "text\text-utils.hpp"

using namespace std;
using namespace eds::text;

namespace eds::loli
{
	void SkipWhitespace(zstring& s, bool toggle = true)
	{
		if (!toggle) return;

		while (*s && ConsumeIfAny(s, " \r\n\t")) { }
	}

	bool TryParseConstant(zstring& s, const char* text, bool skip_ws = true)
	{
		SkipWhitespace(s, skip_ws);
		return ConsumeIfSeq(s, text);
	}

	void ParseConstant(zstring& s, const char* text, bool skip_ws = true)
	{
		SkipWhitespace(s, skip_ws);

		// expecting constant
		if (!ConsumeIfSeq(s, text))
		{
			throw 0;
		}
	}

	string ParseIdentifier(zstring& s, bool skip_ws = true)
	{
		SkipWhitespace(s, skip_ws);

		// expecting letter
		if (!isalpha(*s))
		{
			throw 0;
		}

		string buf;
		while (isalnum(*s) || *s == '_')
		{
			buf.push_back(Consume(s));
		}

		return buf;
	}

	string ParseString(zstring& s, bool skip_ws = true)
	{
		SkipWhitespace(s, skip_ws);

		// expecting string
		if (!ConsumeIf(s, '"'))
		{
			throw 0;
		}

		string buf;
		while (*s)
		{
			if (ConsumeIf(s, '"'))
			{
				if (ConsumeIf(s, '"'))
				{
					buf.push_back('"');
				}
				else
				{
					return buf;
				}
			}
			else
			{
				buf.push_back(Consume(s));
			}
		}

		// unexpected eof
		throw 0;
	}

	TypeSpec ParseTypeSpec(zstring& s)
	{
		auto type = ParseIdentifier(s);
		auto qual = ""s;
		if (TryParseConstant(s, "'", false))
		{
			qual = ParseIdentifier(s, false);
		}

		return TypeSpec{ type, qual };
	}

	void ParseTokenDefinition(Config& config, zstring& s, bool ignore)
	{
		auto name = ParseIdentifier(s);
		ParseConstant(s, "=");
		auto regex = ParseString(s);
		ParseConstant(s, ";");

		config.tokens.push_back(
			TokenDefinition{ ignore, name, regex }
		);
	}

	void ParseEnumDefinition(Config& config, zstring& s)
	{
		auto name = ParseIdentifier(s);
		ParseConstant(s, "{");

		vector<string> values;
		while (!TryParseConstant(s, "}"))
		{
			values.push_back(ParseIdentifier(s));
			ParseConstant(s, ";");
		}

		config.enums.push_back(
			EnumDefinition{ name, values }
		);
	}

	void ParseBaseDefinition(Config& config, zstring& s)
	{
		auto name = ParseIdentifier(s);
		ParseConstant(s, ";");

		config.bases.push_back(
			BaseDefinition{ name }
		);
	}

	void ParseNodeDefinition(Config& config, zstring& s)
	{
		auto name = ParseIdentifier(s);
		auto parent = ""s;
		if (TryParseConstant(s, ":"))
		{
			parent = ParseIdentifier(s);
		}

		ParseConstant(s, "{");

		vector<NodeMember> members;
		while (!TryParseConstant(s, "}"))
		{
			auto type = ParseTypeSpec(s);

			auto field_name = ParseIdentifier(s);
			ParseConstant(s, ";");

			members.push_back(
				NodeMember{ type, field_name }
			);
		}

		config.nodes.push_back(
			NodeDefinition{ name, parent, members }
		);
	}

	void ParseNodeDefinition(Config& config, zstring& s)
	{
		auto name = ParseIdentifier(s);
		auto parent = ""s;
		if (TryParseConstant(s, ":"))
		{
			parent = ParseIdentifier(s);
		}

		ParseConstant(s, "{");

		vector<NodeMember> members;
		while (!TryParseConstant(s, "}"))
		{
			auto type = ParseTypeSpec(s);

			auto field_name = ParseIdentifier(s);
			ParseConstant(s, ";");

			members.push_back(
				NodeMember{ type, field_name }
			);
		}

		config.nodes.push_back(
			NodeDefinition{ name, parent, members }
		);
	}

	void ParseRuleDefinition(Config& config, zstring& s)
	{
		auto name = ParseIdentifier(s);
		ParseConstant(s, ":");
		auto type = ParseTypeSpec(s);

		while (true)
		{
			ParseConstant(s, "=");

			vector<RuleItem> items;

			SkipWhitespace(s);
			while (isalpha(*s) || *s == '"')
			{
				auto symbol = ""s;
				if (*s == '"')
				{
					symbol = ParseString(s);
				}
				else
				{
					symbol = ParseIdentifier(s);
				}

				auto assign = ""s;
				if (TryParseConstant(s, "!"))
				{
					assign = "!";
				}
				else if (TryParseConstant(s, ":"))
				{
					assign = ParseIdentifier(s);
				}

				items.push_back(
					RuleItem{ symbol, assign }
				);

				// TODO: make this look better
				SkipWhitespace(s);
			}

			auto klass_hint = ""s;
			if (TryParseConstant(s, "->"))
			{
				klass_hint = ParseIdentifier(s);
			}

			config.rules.push_back(
				RuleDefinition{ type, name, klass_hint, items }
			);

			if (TryParseConstant(s, ";")) break;
		}
	}

	unique_ptr<Config> ParseConfig(zstring data)
	{
		auto result = make_unique<Config>();
		auto p = data;
		while (*p)
		{
			if (TryParseConstant(p, "token"))
			{
				ParseTokenDefinition(*result, p, false);
			}
			else if (TryParseConstant(p, "ignore"))
			{
				ParseTokenDefinition(*result, p, true);
			}
			else if (TryParseConstant(p, "enum"))
			{
				ParseEnumDefinition(*result, p);
			}
			else if (TryParseConstant(p, "base"))
			{
				ParseBaseDefinition(*result, p);
			}
			else if (TryParseConstant(p, "node"))
			{
				ParseNodeDefinition(*result, p);
			}
			else if (TryParseConstant(p, "rule"))
			{
				ParseRuleDefinition(*result, p);
			}
			else
			{
				throw 0;
			}

			SkipWhitespace(p);
		}

		return result;
	}
}