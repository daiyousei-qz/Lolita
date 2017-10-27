#include "parsing-bootstrap.h"
#include <algorithm>
#include <cassert>
#include <sstream>

using namespace std;
using namespace eds::loli::config;

namespace eds::loli
{
	using TypeInfoMap = ParserBootstrapInfo::TypeInfoMap;
	using SymbolInfoMap = ParserBootstrapInfo::SymbolInfoMap;
	using ParserBootstrapContext = ParserBootstrapInfo::ParserBootstrapContext;

	void RegisterTypeInfo(TypeInfoMap& lookup, TypeInfo* info)
	{
		if (lookup.count(info->Name()) > 0)
			throw ParserConstructionError{ "duplicate registered type info" };

		lookup[info->Name()] = info;
	}
	void RegisterSymbolInfo(SymbolInfoMap& lookup, SymbolInfo* info)
	{
		if (lookup.count(info->Name()) > 0)
			throw ParserConstructionError{ "duplicate registered symbol info" };

		lookup[info->Name()] = info;
	}

	// TODO: support enum vec?
	// TODO: refine this function, maybe map would help
	unique_ptr<AstHandle> ConstructAstHandle(const ParserBootstrapContext& ctx, const TypeSpec& var_type, const RuleItem& rule)
	{
		bool is_vec = var_type.IsVector();
		bool is_obj = (var_type.type->IsBase() || var_type.type->IsKlass()) && !is_vec;

		auto klass_name = var_type.type->Name();

		// construct gen handle
		//

		auto gen_handle = [&]() -> AstHandle::GenHandle {
			if (rule.klass_hint)
			{
				// a new item should be generated
				const auto& hint = *rule.klass_hint;

				// enum
				//

				if (var_type.type->IsEnum())
				{
					assert(hint.qual.empty());

					auto info = reinterpret_cast<EnumInfo*>(var_type.type);
					auto value = distance(
						info->values.begin(),
						find(info->values.begin(), info->values.end(), hint.name)
					);

					assert(value < info->values.size());
					return AstEnumGen{ value };
				}

				// object or vector
				//

				// override klass name
				klass_name = (hint.name=="_" ? var_type.type->Name() : hint.name);

				// make genenerator
				if (is_vec)
				{
					return AstVectorGen{};
				}
				else
				{
					return AstObjectGen{};
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

				auto index = distance(rule.rhs.begin(), it);

				// override klass name
				auto symbol = ctx.symbol_lookup.at(it->symbol);
				auto category = symbol->GetCategory();
				if (category == SymbolInfo::Category::Variable)
				{
					klass_name = reinterpret_cast<VariableInfo*>(symbol)->type.type->Name();
				}

				return AstItemSelector{ index };
			}
		}();

		// construct manip handle
		//

		auto manip_handle = [&]() -> AstHandle::ManipHandle {
			if (is_vec)
			{
				vector<int> indices;
				for (int i = 0; i < rule.rhs.size(); ++i)
				{
					const auto& symbol = rule.rhs[i];
					if (symbol.assign == "&")
					{
						indices.push_back(i);
					}
					else
					{
						assert(symbol.assign.empty() || symbol.assign == "!");
					}
				}

				if (indices.empty())
				{
					return AstManipPlaceholder{};
				}
				else
				{
					return AstVectorMerger{ move(indices) };
				}
			}
			else if (is_obj)
			{
				vector<AstObjectSetter::SetterPair> setters;
				for (int i = 0; i < rule.rhs.size(); ++i)
				{
					const auto& info = *dynamic_cast<KlassInfo*>(ctx.type_lookup.at(klass_name));
					const auto& symbol = rule.rhs[i];

					if (!symbol.assign.empty() && symbol.assign != "!")
					{
						auto it = find_if(info.members.begin(), info.members.end(),
										  [&](const KlassInfo::Member& mem) { return mem.name == symbol.assign; });
						assert(it != info.members.end());
						
						auto codinal = distance(info.members.begin(), it);
						setters.push_back({ codinal, i });
					}
				}

				if (setters.empty())
				{
					return AstManipPlaceholder{};
				}
				else
				{
					return AstObjectSetter{ move(setters) };
				}
			}
			else // enum
			{
				// do nothing
				return AstManipPlaceholder{};
			}
		}();

		return make_unique<AstHandle>(move(klass_name), move(gen_handle), move(manip_handle));
	}

	void LoadTypeInfo(ParserBootstrapContext& ctx, const ParsingConfiguration& config)
	{
		// load enum, base and klass definition
		//

		auto& type_lookup = ctx.type_lookup;
		auto& enums = ctx.enums;
		auto& bases = ctx.bases;
		auto& klasses = ctx.klasses;

		// init containers
		//
		type_lookup.insert_or_assign("token", &TokenDummyType::Instance());

		enums.reserve(config.enums.size());
		bases.reserve(config.bases.size());
		klasses.reserve(config.nodes.size());

		// copy enum definitions
		for (const auto& enum_def : config.enums)
		{
			auto& info = enums.emplace_back(enum_def.name);
			info.values = enum_def.choices;

			RegisterTypeInfo(type_lookup, &info);
		}
		// copy base definitions
		for (const auto& base_def : config.bases)
		{
			auto& info = bases.emplace_back(base_def.name);

			RegisterTypeInfo(type_lookup, &info);
		}
		// copy klass definitions(stage one: name only)
		for (const auto& klass_def : config.nodes)
		{
			auto& info = klasses.emplace_back(klass_def.name);

			RegisterTypeInfo(type_lookup, &info);
		}

		// copy klass definitions(stage two: detailed information)
		for (int i = 0; i < klasses.size(); ++i)
		{
			const auto& def = config.nodes[i];
			auto& info = klasses[i];

			if (!def.parent.empty())
			{
				auto base = type_lookup.at(def.parent);
				assert(base->IsBase());

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
	void LoadGrammarInfo(ParserBootstrapContext& ctx, const ParsingConfiguration& config)
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

			RegisterSymbolInfo(symbol_lookup, &info);
		}
		ignored_tokens.reserve(config.ignored_tokens.size());
		for (const auto& tok_def : config.ignored_tokens)
		{
			auto& info = ignored_tokens.emplace_back(tok_def.name);
			info.regex = tok_def.regex;

			// TODO: what if ignored tokens have name conflict
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

			RegisterSymbolInfo(symbol_lookup, &info);
		}

		// copy productions
		productions.reserve(production_cnt);
		for (const auto& rule_def : config.rules)
		{
			auto lhs = dynamic_cast<VariableInfo*>(symbol_lookup.at(rule_def.lhs));
			assert(lhs != nullptr);

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
	unique_ptr<ParserBootstrapInfo> BootstrapParser(const ParsingConfiguration& config)
	{
		ParserBootstrapContext ctx;

		// NOTE call order cannot be reversed due to dependency
		LoadTypeInfo(ctx, config);
		LoadGrammarInfo(ctx, config);
		
		return make_unique<ParserBootstrapInfo>(move(ctx));
	}
}