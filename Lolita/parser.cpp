#include "parser.h"
#include <memory>
#include <sstream>
#include "text/format.hpp"

using namespace std;
using namespace eds::text;

namespace eds::loli
{
	/**
	struct ParsingContext
	{
		std::unique_ptr<Arena> arena;
		vector<AstItem> items;
	};

	Token LoadToken(const ParsingTable& table, string_view data, int offset)
	{
		auto last_acc_len = 0;
		auto last_acc_category = -1;

		auto state = table.LexerInitialState();
		for (int i = offset; i < data.length(); ++i)
		{
			state = table.LookupDfaTransition(state, data[i]);

			if (!state.IsValid())
			{
				break;
			}
			else if (auto category = table.LookupAccCategory(state); category != -1)
			{
				last_acc_len = i - offset + 1;
				last_acc_category = category;
			}
		}

		if (last_acc_len != 0)
		{
			return Token{ last_acc_category, offset, last_acc_len };
		}
		else
		{
			return Token{ -1, offset, 1 };
		}
	}

	auto LookupCurrentState(const ParsingTable& table, vector<PdaStateId>& states)
	{
		return states.empty()
			? table.ParserInitialState()
			: states.back();
	}

	bool FeedParser(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
	{
		struct ActionVisitor
		{
			const ParsingTable& table;
			vector<PdaStateId>& states;
			ParsingContext& ctx;
			int tok;

			bool hungry = true;
			bool accepted = false;
			bool error = false;

			ActionVisitor(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
				: table(table), states(states), ctx(ctx), tok(tok) { }

			void operator()(PdaActionShift action)
			{
				states.push_back(action.destination);
				hungry = false;
			}
			void operator()(PdaActionReduce action)
			{
				auto id = action.production;

				const auto& production = info->Productions()[id];

				const auto count = production.rhs.size();
				for (auto i = 0; i < count; ++i)
					states.pop_back();

				auto nonterm_id = distance(info->Variables().data(), production.lhs);
				auto src_state = LookupCurrentState(table, states);
				auto target_state = table.LookupGoto(src_state, nonterm_id);

				states.push_back(target_state);

				auto result = production.handle->Invoke(*manager, ctx.arena, AstItemView(ctx.items, count));
				for (auto i = 0; i < count; ++i)
					ctx.items.pop_back();
				ctx.items.push_back(result);
			}
			void operator()(PdaActionError action)
			{
				hungry = false;
				error = true;
			}
		} visitor{ table, states, ctx, tok };

		while (visitor.hungry)
		{
			auto cur_state = LookupCurrentState(table, states);
			// DEBUG:
			if (!cur_state.IsValid()) return visitor.accepted;

			auto action = (tok != -1)
				? table.LookupAction(cur_state, tok)
				: table.LookupActionOnEof(cur_state);

			visit(visitor, action);
		}

		// if (visitor.error) throw 0;

		return visitor.accepted;
	}

	auto Parse(ParsingContext& ctx, const ParsingTable& table, string_view data)
	{
		int offset = 0;
		vector<PdaStateId> states;

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(table, data, offset);

			// update offset
			offset = tok.offset + tok.length;

			// throw for invalid token
			if (tok.category == -1) throw 0;
			// ignore categories in blacklist
			if (tok.category >= table.TerminalCount()) continue;

			FeedParser(table, states, ctx, tok.category);
			ctx.items.push_back(AstItem::Create<Token>(tok));
		}

		// finalize parsing
		FeedParser(table, states, ctx, -1);
	}
	*/


	// TODO: refine code generation
	string GenerateDataBinding(const ParserBootstrapInfo& info)
	{
		stringstream buf;

		buf << "// THIS FILE IS GENERATED BY PROJ LOLITA\n";
		buf << "// DO NOT MODIFY!!!\n\n";
		buf << "#include \"ast-include.h\"\n";

		buf << "namespace eds::loli\n";
		buf << "{\n";

		buf << "// Forward declarations\n";
		buf << "// \n\n";

		for (const auto& base_def : info.Bases())
			buf << "struct " << base_def.Name() << ";\n";

		buf << "\n";

		for (const auto& klass_def : info.Klasses())
			buf << "struct " << klass_def.Name() << ";\n";

		buf << "\n";
		buf << "// Class definitions\n";
		buf << "// \n\n";

		for (const auto& enum_def : info.Enums())
		{
			buf << "enum " << enum_def.Name() << endl;
			buf << "{\n";

			for (const auto& value : enum_def.values)
			{
				buf << value << ",\n";
			}

			buf << "};\n\n";
		}

		for (const auto& base_def : info.Bases())
		{
			buf << "struct " << base_def.Name() << " : public AstObjectBase\n";
			buf << "{\n";

			buf << "    struct Visitor\n";
			buf << "    {\n";
			for (auto klass_def : info.Klasses())
			{
				if (klass_def.base == &base_def)
				{
					buf << "        virtual void Visit(" << klass_def.Name() << "&) = 0;\n";
				}
			}
			buf << "    };\n\n";

			buf << "    virtual void Accept(Visitor&) = 0;\n";

			buf << "};\n";
		}

		for (const auto& klass_def : info.Klasses())
		{
			auto inh = klass_def.base ? klass_def.base->Name() : "AstObjectBase";

			buf << "struct " << klass_def.Name() << Format(" : public {}\n", inh);
			buf << "{\n";

			for (const auto& member : klass_def.members)
			{
				auto type = member.type.type->Name();

				if (type == "token") type = "Token";

				if (member.type.type->IsStoredByRef())
				{
					type.append("*");
				}
				if (member.type.qual == TypeSpec::Qualifier::Vector)
				{
					type = Format("AstVector<{}>*", type);
				}

				buf << "    " << type << " " << member.name << ";\n";
			}

			if (klass_def.base)
			{
				buf << "\n";
				buf << "public:\n";
				buf << Format("    void Accept({}::Visitor& v) override {{ v.Visit(*this); }}\n", klass_def.base->Name());
			}

			buf << "};\n\n";
		}

		buf << "\n";
		buf << "// Trait definitions\n";
		buf << "// \n\n";

		for (const auto& enum_def : info.Enums())
		{
			buf << Format("class AstTrait_{0} : public BasicAstTrait<{0}> {{ }};\n", enum_def.Name());
		}
		for (const auto& base_def : info.Bases())
		{
			buf << Format("class AstTrait_{0} : public BasicAstTrait<{0}> {{ }};\n", base_def.Name());
		}
		for (const auto& klass_def : info.Klasses())
		{
			const auto& klass_name = klass_def.Name();
			buf << Format("class AstTrait_{0} : public BasicAstTrait<{0}>\n", klass_name);
			buf << "{\n";
			buf << "public:\n";

			buf << "    void AssignField(AstItem obj, int codinal, AstItem value) override\n";
			buf << "    {\n";
			buf << "        auto p = obj.Extract<StoreType>();\n";
			buf << "        switch(codinal)\n";
			buf << "        {\n";
			for (int i = 0; i < klass_def.members.size(); ++i)
			{
				const auto& member = klass_def.members[i];

				buf << Format("        case {}:\n", i);
				buf << Format("        QuickAssignField(p->{}, value);\n", member.name);
				buf << Format("        break;\n", i);
			}
			buf << "        default: throw 0;\n";

			buf << "        }\n";
			buf << "    }\n";
			buf << "};\n";
		}


		buf << "\n";
		buf << "// External handle\n";
		buf << "// \n\n";

		buf << "inline std::unique_ptr<AstTraitManager> GetAstTraitManager()\n";
		buf << "{\n";
		buf << "    auto result = std::make_unique<AstTraitManager>();\n\n";
		for (const auto& enum_def : info.Enums())
		{
			buf << Format("    result->Register<AstTrait_{0}>(\"{0}\");\n", enum_def.Name());
		}
		for (const auto& base_def : info.Bases())
		{
			buf << Format("    result->Register<AstTrait_{0}>(\"{0}\");\n", base_def.Name());
		}
		for (const auto& klass_def : info.Klasses())
		{
			buf << Format("    result->Register<AstTrait_{0}>(\"{0}\");\n", klass_def.Name());
		}
		buf << "\n";
		buf << "    return result;\n";
		buf << "}\n";

		buf << "}\n";

		return buf.str();
	}
}