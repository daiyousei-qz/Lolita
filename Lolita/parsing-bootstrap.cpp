#include "parsing-bootstrap.h"

using namespace std;

namespace eds::loli
{
	unique_ptr<ParserBootstrapInfo> BootstrapParser(const Config& config)
	{
		unordered_map<string, TypeInfo*> type_lookup = { { "token"s, &TokenDummyType::Instance() }, };
		vector<EnumInfo> enums;
		vector<BaseInfo> bases;
		vector<KlassInfo> klasses;

		// TODO: deal with potential type name conflict
		// copy enum, base and klass definition
		//

		// copy enum definitions
		enums.reserve(config.enums.size());
		for (const auto& enum_def : config.enums)
		{
			enums.emplace_back(enum_def.name);
			enums.back().values = enum_def.choices;

			type_lookup.insert_or_assign(enum_def.name, &enums.back());
		}
		// copy base definitions
		bases.reserve(config.bases.size());
		for (const auto& base_def : config.bases)
		{
			bases.emplace_back(base_def.name);

			type_lookup.insert_or_assign(base_def.name, &enums.back());
		}
		// copy klass definitions(stage one: name only)
		klasses.reserve(config.nodes.size());
		for (const auto& klass_def : config.nodes)
		{
			klasses.emplace_back(klass_def.name);

			type_lookup.insert_or_assign(klass_def.name, &klasses.back());
		}
		// copy klass definitions(stage two: detailed information)
		for (int i = 0; i < klasses.size(); ++i)
		{
			const auto& def = config.nodes[i];
			auto& info = klasses[i];

			if (!def.parent.empty())
			{
				auto base = type_lookup.at(def.parent);
				assert(base->GetCategory() == TypeInfo::Category::Base);

				info.base = reinterpret_cast<BaseInfo*>(base);
			}

			for (const auto& member_def : def.members)
			{
				auto qual = TypeSpec::Qualifier::None;
				if (!member_def.type.qual.empty())
				{
					assert(member_def.type.qual == "vec");
					qual = TypeSpec::Qualifier::Vector;
				}

				auto type = type_lookup.at(member_def.type.name);
				
				info.members.push_back(
					KlassInfo::Member{ TypeSpec{ qual, type }, member_def.name }
				);
			}
		}
		
		unordered_map<string, TypeInfo*> symbol_lookup;
		vector<TokenInfo> tokens;
		vector<TokenInfo> ignored_tokens;
		vector<VariableInfo> variables;
		vector<ProductionInfo> productions;

		// copy tokens
		tokens.reserve(config.tokens.size());
		for (const auto& tok_def : config.tokens)
		{
			tokens.emplace_back(tok_def.name);
			tokens.back().regex = tok_def.regex;

			symbol_lookup.insert_or_assign(tok_def.name, &tokens.back());
		}
		ignored_tokens.reserve(config.ignored_tokens.size());
		for (const auto& tok_def : config.ignored_tokens)
		{
			ignored_tokens.emplace_back(tok_def.name);
			ignored_tokens.back().regex = tok_def.regex;

			// NOTE ignored tokens are not allowed to be used
			// so don't make it into symbol lookup
		}

		// copy variables
		variables.reserve(config.rules.size());
		for (const auto& rule_def : config.rules)
		{
			variables.push_back(rule_def.lhs);

			symbol_lookup.insert_or_assign(rule_def.lhs, variables.back());
		}

		// copy productions
		for (const auto& rule_def : config.rules)
		{

		}

		unordered_map<string, SymbolInfo*> symbol_lookup;

		// ...

		// copy productions
		//
		unordered_map<string, int> enum_lookup;

		for (const auto& rule : config.rules)
		{
			string klass_name;

			// construct gen handle
			//

			auto gen_hendle = [&]() -> AstHandle::GenHandle {
				if (rule.klass_hint)
				{
					// gen
					const auto& hint = *rule.klass_hint;

					// try enum gen
					auto enum_it = enum_lookup.find(hint.name);
					if (enum_it != enum_lookup.end())
					{
						assert(hint.qual.empty());
						return AstEnumGen{ enum_it->second };
					}

					// object or vector
					klass_name = hint.name;
					if (hint.qual.empty())
					{
						return AstObjectGen{};
					}
					else
					{
						assert(hint.qual == "vec");
						return AstVectorGen{};
					}
				}
				else
				{
					// selector
					auto it = find_if(
						rule.rhs.begin(), rule.rhs.end(),
						[](const auto& elem) { return elem.assign == "!"; }
					);

					assert(it != rule.rhs.end());
					return AstItemSelector{ distance(rule.rhs.begin(), it) };
				}
			}();

			// construct manip handle
			//

			auto manip_handle = [&]() {
				bool vec, bool obj;
				if (vec)
				{
					vector<int> indices;
					for (int i = 0; i < rule.rhs.size(); ++i)
					{
						const auto& item = rule.rhs[i];
						if (item.assign == "&")
						{
							indices.push_back(i);
						}
						else
						{
							assert(item.assign.empty());
						}
					}

					return AstVectorMerger{ indices };
				}
				else if (obj)
				{

				}
			};
		}
	}
}