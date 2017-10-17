#include "parsing-bootstrap.h"

using namespace std;
using namespace eds::loli::config;

namespace eds::loli
{
	using ParserBootstrapContext = ParserBootstrapInfo::ParserBootstrapContext;

	unique_ptr<AstHandle> ConstructAstHandle(const ParserBootstrapContext& ctx, const TypeSpec& var_type, const RuleItem& item)
	{
		/*
						auto gen_hendle = [&]() -> AstHandle::GenHandle {
					if (rule_item->klass_hint)
					{
						// gen
						const auto& hint = *rule_item->klass_hint;

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

				auto manip_handle = [&]() -> AstHandle::ManipHandle {
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

		*/
		// TODO: finish this
		return nullptr;
	}

	void LoadTypeInfo(ParserBootstrapContext& ctx, const Config& config)
	{
		// load enum, base and klass definition
		//

		auto& type_lookup = ctx.type_lookup;
		auto& enums = ctx.enums;
		auto& bases = ctx.bases;
		auto& klasses = ctx.klasses;

		// init type_lookup
		type_lookup.insert_or_assign("token", &TokenDummyType::Instance());

		// copy enum definitions
		enums.reserve(config.enums.size());
		for (const auto& enum_def : config.enums)
		{
			auto& info = enums.emplace_back(enum_def.name);
			info.values = enum_def.choices;

			type_lookup.insert_or_assign(enum_def.name, &info);
		}
		// copy base definitions
		bases.reserve(config.bases.size());
		for (const auto& base_def : config.bases)
		{
			auto& info = bases.emplace_back(base_def.name);

			type_lookup.insert_or_assign(base_def.name, &info);
		}
		// copy klass definitions(stage one: name only)
		klasses.reserve(config.nodes.size());
		for (const auto& klass_def : config.nodes)
		{
			auto& info = klasses.emplace_back(klass_def.name);

			type_lookup.insert_or_assign(klass_def.name, &info);
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
	}
	void LoadGrammarInfo(ParserBootstrapContext& ctx, const Config& config)
	{
		// load tokens, variables and rules
		//

		auto& symbol_lookup = ctx.symbol_lookup;
		auto& tokens = ctx.tokens;
		auto& ignored_tokens = ctx.ignored_tokens;
		auto& variables = ctx.variables;
		auto& productions = ctx.productions;

		// copy tokens
		tokens.reserve(config.tokens.size());
		for (const auto& tok_def : config.tokens)
		{
			auto& info = tokens.emplace_back(tok_def.name);
			info.regex = tok_def.regex;

			symbol_lookup.insert_or_assign(tok_def.name, &info);
			symbol_lookup.insert_or_assign(tok_def.regex, &info);
		}
		ignored_tokens.reserve(config.ignored_tokens.size());
		for (const auto& tok_def : config.ignored_tokens)
		{
			auto& info = ignored_tokens.emplace_back(tok_def.name);
			info.regex = tok_def.regex;

			// NOTE ignored tokens are not allowed to be used
			// so don't make it into symbol lookup
		}

		// copy variables
		int production_cnt = 0;
		variables.reserve(config.rules.size());
		for (const auto& rule_def : config.rules)
		{
			production_cnt += rule_def.items.size();

			auto& info = variables.emplace_back(rule_def.lhs);

			info.type.type = ctx.type_lookup.at(rule_def.type.name);
			info.type.qual = rule_def.type.qual == "vec" ? TypeSpec::Qualifier::Vector : TypeSpec::Qualifier::None;

			symbol_lookup.insert_or_assign(rule_def.lhs, &info);
		}

		// copy productions
		productions.reserve(production_cnt);
		for (const auto& rule_def : config.rules)
		{
			auto lhs = dynamic_cast<VariableInfo*>(symbol_lookup.at(rule_def.lhs));

			for (const auto& rule_item : rule_def.items)
			{
				auto& info = productions.emplace_back();
				
				info.lhs = lhs;
				for (const auto& symbol_name : rule_item.rhs)
				{
					info.rhs.push_back(symbol_lookup.at(symbol_name.symbol));
				}

				info.handle = ConstructAstHandle(ctx, lhs->type, rule_item);

				// inject ProductionInfo back into VariableInfo
				lhs->productions.push_back(&info);
			}
		}
	}

	// TODO: deal with potential type name conflict
	unique_ptr<ParserBootstrapInfo> BootstrapParser(const Config& config)
	{
		ParserBootstrapContext ctx;

		// NOTE call order cannot be reversed due to dependency
		LoadTypeInfo(ctx, config);
		LoadGrammarInfo(ctx, config);
		
		return make_unique<ParserBootstrapInfo>(move(ctx));
	}
}