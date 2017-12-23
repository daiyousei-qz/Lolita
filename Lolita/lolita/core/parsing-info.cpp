#include "lolita/core/parsing-info.h"
#include <algorithm>
#include <cassert>
#include <sstream>

using namespace std;
using namespace eds::container;
using namespace eds::loli::regex;
using namespace eds::loli::ast;
using namespace eds::loli::config;

namespace eds::loli
{
	string RemoveQuote(const string& s)
	{
		assert(s.front() == '"' && s.back() == '"');
		return s.substr(1, s.size() - 2);
	}

	class ParsingMetaInfo::Builder
	{
	public:
		unique_ptr<ParsingMetaInfo> Build(const string& config, const AstTypeProxyManager* env)
		{
			auto cc = ParseConfig(config.c_str());

			Initialize(config, env);

			LoadTypeInfo(*cc);
			LoadSymbolInfo(*cc);

			return Finalize();
		}

	private:
		void Initialize(const std::string& config, const AstTypeProxyManager* env)
		{
			site_ = std::make_unique<ParsingMetaInfo>();
			site_->env_ = env;
		}
		unique_ptr<ParsingMetaInfo> Finalize()
		{
			return move(site_);
		}

		void Assert(bool pred, const char* msg)
		{
			if (!pred)
				throw ParserConstructionError{ msg };
		}

		void RegisterTypeInfo(TypeInfo* info)
		{
			if (site_->type_lookup_.count(info->Name()) > 0)
				throw ParserConstructionError{ "ParsingMetaInfoBuilder: duplicate type name" };

			site_->type_lookup_[info->Name()] = info;
		}
		void RegisterSymbolInfo(SymbolInfo* info)
		{
			if (site_->symbol_lookup_.count(info->Name()) > 0)
				throw ParserConstructionError{ "ParsingMetaInfoBuilder: duplicate symbol name" };

			site_->symbol_lookup_[info->Name()] = info;
		}
		
		TypeSpec TranslateTypeSpec(const config::QualType& def)
		{
			using Qualifier = TypeSpec::Qualifier;

			auto qual = def.qual == "vec" ? Qualifier::Vector :
						def.qual == "opt" ? Qualifier::Optional :
											Qualifier::None;

			auto type = site_->type_lookup_.at(def.name);

			return TypeSpec{ qual, type };
		}
		TokenInfo MakeTokenInfo(const TokenDefinition& def, int id)
		{
			auto result = TokenInfo(id, def.name);
			result.text_def_ = RemoveQuote(def.regex);
			result.ast_def_ = ParseRegex(result.text_def_);

			return result;
		}

		unique_ptr<AstHandle> ConstructAstHandle(const TypeSpec& var_type, const RuleItem& rule)
		{
			const auto& type_lookup = site_->type_lookup_;
			const auto& symbol_lookup = site_->symbol_lookup_;

			const auto is_vec = var_type.IsVector();
			const auto is_opt = var_type.IsOptional();
			const auto is_qualified = is_vec || is_opt;

			const auto is_enum = !is_vec && (var_type.type->IsEnum());
			const auto is_obj = !is_vec && (var_type.type->IsKlass() || var_type.type->IsBase());

			auto rule_type_info = var_type.type;

			// construct GenHandle
			auto gen_handle = [&]() -> AstHandle::GenHandle {
				if (rule.klass_hint) // generator
				{
					// has hint, i.e arrow-op
					// a new node should be generated
					const auto& hint = *rule.klass_hint;

					// empty optional
					if (is_opt)
					{
						if (hint.name == "_" || hint.qual == "opt")
						{
							return AstOptionalGen{};
						}
					}

					// some value
					if (is_enum)
					{
						// enum
						//

						// find the numeric value
						auto info = reinterpret_cast<EnumTypeInfo*>(var_type.type);
						auto value = distance(
							info->values_.begin(),
							find(info->values_.begin(), info->values_.end(), hint.name)
						);

						Assert(value < info->values_.size(), "ParserMetaInfo::Builder: invalid enum member");
						return AstEnumGen{ value };
					}
					else
					{
						// klass or vector
						//

						// rewrite klass name if any
						if (hint.name != "_")
						{
							rule_type_info = type_lookup.at(hint.name);
						}

						// make genenerator
						if (is_vec)
						{
							return AstVectorGen{};
						}
						else
						{
							assert(is_obj);
							return AstObjectGen{};
						}
					}
				}
				else // selector
				{
					// find the selected item
					auto pred = [](const auto& elem) { return elem.assign == "!"; };
					auto it = find_if(rule.rhs.begin(), rule.rhs.end(), pred);
					auto index = static_cast<int>(distance(rule.rhs.begin(), it));

					Assert(it != rule.rhs.end(), "ParserMetaInfo::Builder: rule does not return");
					Assert(find_if(next(it), rule.rhs.end(), pred) == rule.rhs.end(),
						   "ParserMetaInfo::Builder: multiple item selected to return");

					// rewrite klass name
					auto symbol = symbol_lookup.at(it->symbol);
					if (symbol->IsVariable())
					{
						rule_type_info = symbol->AsVariable()->type_.type;
					}

					return AstItemSelector{ index };
				}
			}();

			// construct ManipHandle
			auto manip_handle = [&]() -> AstHandle::ManipHandle {

				// TODO: what if it's not a klass
				// scan once to collect ops
				const auto& info = *dynamic_cast<KlassTypeInfo*>(rule_type_info);

				vector<int> to_be_pushed; // &
				vector<AstObjectSetter::SetterPair> to_be_assigned; // :name

				for (int i = 0; i < rule.rhs.size(); ++i)
				{
					const auto& symbol = rule.rhs[i];

					if (symbol.assign == "&")
					{
						to_be_pushed.push_back(i);
					}
					else
					{
						if (!symbol.assign.empty() && symbol.assign != "!")
						{
							auto it = find_if(info.members_.begin(), info.members_.end(),
								[&](const KlassTypeInfo::MemberInfo& mem) { return mem.name == symbol.assign; });
							
							Assert(it != info.members_.end(), "ParserMetaInfo::Builder:");

							auto codinal = distance(info.members_.begin(), it);
							to_be_assigned.push_back({ codinal, i });
						}
					}
				}

				if (is_vec)
				{
					Assert(to_be_assigned.empty(), "ParserMetaInfo::Builder: unexpected opertion(assign)");

					if (to_be_pushed.empty())
					{
						return AstManipPlaceholder{};
					}
					else
					{
						return AstVectorMerger{ move(to_be_pushed) };
					}
				}
				else if (is_obj)
				{
					Assert(to_be_pushed.empty(), "ParserMetaInfo::Builder: unexpected opertion(push)");

					if (to_be_assigned.empty())
					{
						return AstManipPlaceholder{};
					}
					else
					{
						return AstObjectSetter{ move(to_be_assigned) };
					}
				}
				else
				{
					assert(is_opt || is_enum);

					Assert(to_be_pushed.empty() && to_be_assigned.empty(),
						   "ParserMetaInfo::Builder: unexpected opertion(assign or push)");

					return AstManipPlaceholder{};
				}
			}();

			const auto& proxy = site_->env_
				? site_->env_->Lookup(rule_type_info->Name())
				: &DummyAstTypeProxy::Instance();

			return make_unique<AstHandle>(proxy, move(gen_handle), move(manip_handle));
		}

		// TODO: refine initialization of HeapArray, perhaps vector should be used
		void LoadTypeInfo(const ParsingConfiguration& config)
		{
			// some shortcuts
			//
			auto& type_lookup	= site_->type_lookup_;
			auto& enums			= site_->enums_;
			auto& bases			= site_->bases_;
			auto& klasses		= site_->klasses_;

			// builtin types
			//
			type_lookup.insert_or_assign("token", &TokenTypeInfo::Instance());

			// copy enum definitions
			//
			enums.Initialize(config.enums.size());
			for (int i = 0; i < enums.Size(); ++i)
			{
				const auto& def = config.enums[i];
				auto& info = enums[i];

				info = EnumTypeInfo{ def.name };
				info.values_ = def.choices;

				RegisterTypeInfo(&info);
			}

			// copy base definitions
			//
			bases.Initialize(config.bases.size());
			for (int i = 0; i < bases.Size(); ++i)
			{
				const auto& def = config.bases[i];
				auto& info = bases[i];

				info = BaseTypeInfo{ def.name };

				RegisterTypeInfo(&info);
			}

			// copy klass definitions(stage one: name only)
			//
			klasses.Initialize(config.nodes.size());
			for (int i = 0; i < klasses.Size(); ++i)
			{
				const auto& def = config.nodes[i];
				auto& info = klasses[i];

				info = KlassTypeInfo{ def.name };

				RegisterTypeInfo(&info);
			}
			// copy klass definitions(stage two: detailed information)
			//
			for (int i = 0; i < klasses.Size(); ++i)
			{
				const auto& def = config.nodes[i];
				auto& info = klasses[i];

				if (!def.parent.empty())
				{
					auto it = type_lookup.find(def.parent);
					Assert(it != type_lookup.end() && it->second->IsBase(),
						   "ParserMetaInfo::Builder: invalid base type specified");

					info.base_ = reinterpret_cast<BaseTypeInfo*>(it->second);
				}

				for (const auto& member_def : def.members)
				{
					info.members_.push_back(
						{ TranslateTypeSpec(member_def.type), member_def.name }
					);
				}
			}
		}
		void LoadSymbolInfo(const ParsingConfiguration& config)
		{
			// some shortcuts
			//
			auto& symbol_lookup		= site_->symbol_lookup_;
			auto& tokens			= site_->tokens_;
			auto& ignored_tokens	= site_->ignored_tokens_;
			auto& variables			= site_->variables_;
			auto& productions		= site_->productions_;

			// copy tokens
			//
			tokens.Initialize(config.tokens.size());
			for (int i = 0; i < tokens.Size(); ++i)
			{
				const auto& def = config.tokens[i];
				auto& info = tokens[i];

				info = MakeTokenInfo(def, i);

				RegisterSymbolInfo(&info);
			}
			ignored_tokens.Initialize(config.ignored_tokens.size());
			for (int i = 0; i < ignored_tokens.Size(); ++i)
			{
				const auto& def = config.ignored_tokens[i];
				auto& info = ignored_tokens[i];

				info = MakeTokenInfo(def, i + tokens.Size());

				// TODO: what if ignored tokens have name conflict
				// NOTE ignored tokens are not allowed to be used
				// so don't make it into symbol lookup
			}

			// copy variables
			//
			auto production_cnt = 0;
			variables.Initialize(config.rules.size());
			for (int i = 0; i < variables.Size(); ++i)
			{
				const auto& def = config.rules[i];
				auto& info = variables[i];

				info = VariableInfo(i, def.name);
				info.type_ = TranslateTypeSpec(def.type);

				production_cnt += def.items.size();
				RegisterSymbolInfo(&info);
			}

			// copy productions
			//
			productions.Initialize(production_cnt);
			int pd_index = 0;
			for (const auto& rule_def : config.rules)
			{
				auto lhs = dynamic_cast<VariableInfo*>(symbol_lookup.at(rule_def.name));

				for (const auto& rule_item : rule_def.items)
				{
					auto& info = productions[pd_index++];

					info.lhs_ = lhs;
					for (const auto& symbol_name : rule_item.rhs)
					{
						info.rhs_.push_back(symbol_lookup.at(symbol_name.symbol));
					}

					info.handle_ = ConstructAstHandle(lhs->type_, rule_item);

					// inject ProductionInfo back into VariableInfo
					lhs->productions_.push_back(&info);
				}
			}
		}

		unique_ptr<ParsingMetaInfo> site_;
	};

	std::unique_ptr<ParsingMetaInfo> ResolveParsingInfo(const string& config, const AstTypeProxyManager* env)
	{
		ParsingMetaInfo::Builder builder{};

		return builder.Build(config, env);
	}
}