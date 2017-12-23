#include "lolita/parser.h"
#include "lolita/core/codegen.h"
#include "lolita/lexing/lexing-automaton.h"
#include "lolita/parsing/parsing-automaton.h"
#include <vector>
#include <string>
#include <variant>

using namespace std;
using namespace eds::container;
using namespace eds::loli::ast;

namespace eds::loli
{
	// =====================================================================================
	// Implementation of BootstrapParser
	//

	std::string BootstrapParser(const string& config)
	{
		auto info = ResolveParsingInfo(config, nullptr);

		codegen::CppEmitter e;

		e.Comment("THIS FILE IS GENERATED BY PROJ. LOLITA.");
		e.Comment("PLEASE DO NOT MODIFY!!!");
		e.Comment("");

		e.EmptyLine();
		e.WriteLine("#pragma once");
		e.Include("lolita/lolita-include.h", false);

		e.EmptyLine();
		e.Namespace("eds::loli", [&]() {

			//====================================================
			e.EmptyLine();
			e.Comment("Referred Names");
			e.Comment("");

			e.WriteLine("using eds::loli::ast::BasicAstToken;");
			e.WriteLine("using eds::loli::ast::BasicAstEnum;");
			e.WriteLine("using eds::loli::ast::BasicAstObject;");

			e.WriteLine("using eds::loli::ast::AstVector;");
			e.WriteLine("using eds::loli::ast::AstOptional;");

			e.WriteLine("using eds::loli::ast::DataBundle;");
			e.WriteLine("using eds::loli::ast::BasicAstTypeProxy;");
			e.WriteLine("using eds::loli::ast::AstTypeProxyManager;");

			e.WriteLine("using eds::loli::BasicParser;");

			//====================================================
			e.EmptyLine();
			e.Comment("Forward declarations");
			e.Comment("");

			e.EmptyLine();
			for (const auto& base_def : info->Bases())
			{
				e.WriteLine("class {};", base_def.Name());
			}

			e.EmptyLine();
			for (const auto& klass_def : info->Klasses())
			{
				e.WriteLine("class {};", klass_def.Name());
			}

			//====================================================
			e.EmptyLine();
			e.Comment("Enum definitions");
			e.Comment("");

			e.EmptyLine();
			for (const auto& enum_def : info->Enums())
			{
				e.Enum(enum_def.Name(), "", [&]() {
					for (const auto& item : enum_def.Values())
					{
						e.WriteLine("{},", item);
					}
				});
			}

			//====================================================
			e.EmptyLine();
			e.Comment("Base definitions");
			e.Comment("");

			e.EmptyLine();
			for (const auto& base_def : info->Bases())
			{
				e.Class(base_def.Name(), "public BasicAstObject", [&]() {
					e.WriteLine("public:");

					// visitor
					e.Struct("Visitor", "", [&]() {
						for (auto klass_def : info->Klasses())
						{
							if (klass_def.BaseType() == &base_def)
							{
								e.WriteLine("virtual void Visit({}&) = 0;", klass_def.Name());
							}
						}
					});

					// accept
					e.EmptyLine();
					e.WriteLine("virtual void Accept(Visitor&) = 0;");
				});
			}

			//====================================================
			e.EmptyLine();
			e.Comment("Class definitions");
			e.Comment("");

			e.EmptyLine();
			for (const auto& klass_def : info->Klasses())
			{
				// comma-splitted field types
				auto type_tuple = string{};
				for (const auto& member : klass_def.Members())
				{
					auto type = member.type.type->Name();

					// typename
					if (type == "token")
					{
						type = "BasicAstToken";
					}
					else if (member.type.type->IsEnum())
					{
						type = text::Format("BasicAstEnum<{}>", type);
					}
					else if (member.type.type->IsStoredByRef())
					{
						type.append("*");
					}

					// qualifier
					if (member.type.IsVector())
					{
						type = text::Format("AstVector<{}>*", type);
					}
					else if (member.type.IsOptional())
					{
						type = text::Format("AstOptional<{}>", type);
					}

					// append to tuple
					if (!type_tuple.empty())
						type_tuple.append(", ");

					type_tuple.append(type);
				}

				auto base = klass_def.BaseType();
				auto inh = text::Format("public {}, public DataBundle<{}>", 
					(base ? base->Name() : "BasicAstObject"),
					type_tuple);
				e.Class(klass_def.Name(), inh, [&]() {
					e.WriteLine("public:");

					int index = 0;
					for (const auto& member : klass_def.Members())
					{
						e.WriteLine("const auto& {}() const {{ return GetItem<{}>(); }}", member.name, index++);
					}

					if (base)
					{
						e.EmptyLine();
						e.WriteLine("void Accept({}::Visitor& v) override {{ v.Visit(*this); }}", base->Name());
					}
				});
			}

			//====================================================
			e.EmptyLine();
			e.Comment("Environment");
			e.Comment("");

			e.EmptyLine();
			auto rootName = info->RootVariable().Type().type->Name();
			auto funcName = text::Format("inline BasicParser<{}>::Ptr CreateParser()", rootName);
			e.Block(funcName, [&]() {
				// config
				e.WriteLine("static const auto config = \nu8R\"##########(\n{}\n)##########\";", config);

				// proxy manager
				e.Block("static const auto proxy_manager = []()", [&]() {
					e.WriteLine("AstTypeProxyManager env;");

					e.EmptyLine();
					e.Comment("register enums");
					for (const auto& enum_def : info->Enums())
					{
						const auto& name = enum_def.Name();
						e.WriteLine("env.RegisterEnum<{}>(\"{}\");", name, name);
					}

					e.EmptyLine();
					e.Comment("register bases");
					for (const auto& base_def : info->Bases())
					{
						const auto& name = base_def.Name();
						e.WriteLine("env.RegisterKlass<{}>(\"{}\");", name, name);
					}

					e.EmptyLine();
					e.Comment("register classes");
					for (const auto& klass_def : info->Klasses())
					{
						const auto& name = klass_def.Name();
						e.WriteLine("env.RegisterKlass<{}>(\"{}\");", name, name);
					}

					e.EmptyLine();
					e.WriteLine("return env;");
				});
				e.WriteLine("();");

				// parser
				e.EmptyLine();
				e.WriteLine("return BasicParser<{}>::Create(config, &proxy_manager);", rootName);
			});
		});

		e.EmptyLine();

		return e.ToString();
	}

	// =====================================================================================
	// Implementation of ParsingContext
	//

	class ParsingContext
	{
	public:
		ParsingContext(Arena& arena)
			: arena_(arena) { }

		int StackDepth() const
		{
			return state_stack_.size();
		}
		int CurrentState() const
		{
			return state_stack_.empty() ? 0 : state_stack_.back();
		}

		void ExecuteShift(int target_state, const ast::AstItemWrapper& value)
		{
			state_stack_.push_back(target_state);
			ast_stack_.push_back(value);
		}
		ast::AstItemWrapper ExecuteReduce(const ProductionInfo& production)
		{
			// update state stack
			const auto count = production.Right().size();
			for (auto i = 0; i < count; ++i)
				state_stack_.pop_back();

			// update ast stack
			auto ref = ArrayRef<ast::AstItemWrapper>(ast_stack_.data(), ast_stack_.size()).TakeBack(count);
			auto result = production.Handle()->Invoke(arena_, ref);

			for (auto i = 0; i < count; ++i)
				ast_stack_.pop_back();

			return result;
		}

		ast::AstItemWrapper Finalize()
		{
			assert(StackDepth() == 1);
			auto result = ast_stack_.back();
			state_stack_.clear();
			ast_stack_.clear();

			return result;
		}

	private:
		Arena& arena_;

		std::vector<int> state_stack_ = {};
		std::vector<ast::AstItemWrapper> ast_stack_ = {};
	};

	// =====================================================================================
	// Implementation of GenericParser
	//
	GenericParser::GenericParser(const string& config, const ast::AstTypeProxyManager* env)
	{
		Initialize(config, env);
	}

	ParsingAction TranslateAction(parsing::PdaEdge action)
	{
		struct Visitor
		{
			ParsingAction operator()(parsing::PdaEdgeReduce edge)
			{
				return ActionReduce{ edge.production };
			}
			ParsingAction operator()(parsing::PdaEdgeShift edge)
			{
				return ActionShift{ edge.target->Id() };
			}
		};

		return visit(Visitor{}, action);
	}

	void GenericParser::Initialize(const string& config, const ast::AstTypeProxyManager* env)
	{
		assert(!config.empty() && env != nullptr);

		// resolve basic grammar information
		//
		info_ = ResolveParsingInfo(config, env);

		// computes automata
		//
		auto dfa = lexing::BuildLexingAutomaton(*info_);
		auto pda = parsing::BuildLALRAutomaton(*info_);

		// initialize stores
		//

		// basic grammar information
		token_num_ = info_->Tokens().Size() + info_->IgnoredTokens().Size();
		term_num_ = info_->Tokens().Size();
		nonterm_num_ = info_->Variables().Size();

		dfa_state_num_ = dfa->StateCount();
		pda_state_num_ = pda->States().size();

		// lexing table
		acc_token_lookup_.Initialize(dfa->StateCount(), nullptr);
		lexing_table_.Initialize(128 * dfa_state_num_, -1);

		// parsing table
		eof_action_table_.Initialize(pda_state_num_, ActionError{});
		action_table_.Initialize(pda_state_num_*term_num_, ActionError{});
		goto_table_.Initialize(pda_state_num_*nonterm_num_, -1);

		// copy lexing automaton
		//
		for (int id = 0; id < dfa_state_num_; ++id)
		{
			const auto state = dfa->LookupState(id);

			acc_token_lookup_[id] = state->acc_token;
			for (const auto edge : state->transitions)
			{
				lexing_table_[id * 128 + edge.first] = edge.second->id;
			}
		}

		// copy parsing automaton
		//
		for (int src_state_id = 0; src_state_id < pda_state_num_; ++src_state_id)
		{
			auto state = pda->LookupState(src_state_id);

			if (state->EofAction())
			{
				eof_action_table_[src_state_id] = TranslateAction(*state->EofAction());
			}

			for (const auto& pair : state->ActionMap())
			{
				const auto tok_id = pair.first->Id();
				
				action_table_[src_state_id * term_num_ + tok_id] = TranslateAction(pair.second);
			}

			for (const auto& pair : state->GotoMap())
			{
				const auto var_id = pair.first->Id();

				goto_table_[src_state_id * nonterm_num_ + var_id] = pair.second->Id();
			}
		}
	}

	AstItemWrapper GenericParser::Parse(Arena& arena, const string& data)
	{
		ParsingContext ctx{ arena };
		int offset = 0;

		// tokenize and feed parser while not exhausted
		while (offset < data.length())
		{
			auto tok = LoadToken(data, offset);

			// update offset
			offset = tok.Offset() + tok.Length();

			// throw for invalid token
			if (!tok.IsValid()) 
				throw ParserInternalError{ "GenericParser: invalid token encountered" };
			// ignore tokens in blacklist
			if (tok.Tag() >= term_num_) 
				continue;

			FeedParsingContext(ctx, tok);
		}

		// finalize parsing
		FeedParsingContext(ctx, {});

		return ctx.Finalize();
	}

	ast::BasicAstToken GenericParser::LoadToken(std::string_view data, int offset)
	{
		auto last_acc_len = 0;
		const TokenInfo* last_acc_token = nullptr;

		auto state = LexerInitialState();
		for (int i = offset; i < data.length(); ++i)
		{
			state = LookupLexingTransition(state, data[i]);

			if (!VerifyLexingState(state))
			{
				break;
			}
			else if (auto acc_token = LookupAcceptedToken(state); acc_token)
			{
				last_acc_len = i - offset + 1;
				last_acc_token = acc_token;
			}
		}

		if (last_acc_len != 0)
		{
			return ast::BasicAstToken{ offset, last_acc_len, last_acc_token->Id() };
		}
		else
		{
			return ast::BasicAstToken{};
		}
	}

	ActionExecutionResult GenericParser::ForwardParsingAction(ParsingContext& ctx, ActionShift action, const ast::BasicAstToken& tok)
	{
		assert(tok.IsValid());

		ctx.ExecuteShift(action.target_state, tok);
		return ActionExecutionResult::Consumed;
	}
	ActionExecutionResult GenericParser::ForwardParsingAction(ParsingContext& ctx, ActionReduce action, const ast::BasicAstToken& tok)
	{
		auto folded = ctx.ExecuteReduce(*action.production);

		auto nonterm_id = action.production->Left()->Id();
		auto src_state = ctx.CurrentState();
		auto target_state = LookupParsingGoto(src_state, nonterm_id);

		ctx.ExecuteShift(target_state, folded);

		if (!tok.IsValid() &&
			ctx.StackDepth() == 1 &&
			action.production->Left() == &info_->RootVariable())
		{
			return ActionExecutionResult::Consumed;
		}
		else
		{
			return ActionExecutionResult::Hungry;
		}
	}
	ActionExecutionResult GenericParser::ForwardParsingAction(ParsingContext& ctx, ActionError action, const ast::BasicAstToken& tok)
	{
		return ActionExecutionResult::Error;
	}

	void GenericParser::FeedParsingContext(ParsingContext& ctx, const ast::BasicAstToken& tok)
	{
		while (true)
		{
			auto cur_state = ctx.CurrentState();
			auto action = tok.IsValid()
				? LookupParsingAction(cur_state, tok.Tag())
				: LookupParsingActionOnEof(cur_state);

			auto action_result
				= visit([&](auto x) { return ForwardParsingAction(ctx, x, tok); }, action);

			if (action_result == ActionExecutionResult::Error)
			{
				throw ParserInternalError{ "parsing error" };
			}
			else if (action_result == ActionExecutionResult::Consumed)
			{
				break;
			}
		}
	}
}