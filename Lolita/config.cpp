// # a grammar file is defined with the following grammars
//
// StmtList -> StmtList Stmt
//          -> Stmt
// 
// Stmt -> TokenDecl
//      -> EnumDecl
//      -> NodeDecl
//      -> RuleDecl
// 
// # Token
// TokenDeclInit -> token
//               -> ignore
// TokenDecl -> TokenDeclInit id assign dq_str
// 
// # Enum
// EnumValueList -> EnumValueList comma id
//               -> id
// EnumDecl -> enum id lb EnumValueList rb
// 
// # Node
// NodeInhSpec -> colon id
//             -> \epsilon
// NodeMemberDecl -> id id sc
//                -> token id sc
// NodeMemberList -> NodeMemberList NodeMemberDecl
//                -> \epsilon
// NodeDecl -> node id NodeInhSpec lb NodeMemberList rb
// 
// # Rule
// Symbol -> id
//        -> sq_str
// SymbolList -> SymbolList Symbol
//            -> Symbol
// RefList -> RefList comma ref
//         -> ref
// RuleItemSpec -> pipe id
//              -> pipe id lp RefList rp 
//              -> pipe ref
//              -> pipe con lp RefList rp
//              -> \epsilon
// RuleItem -> assign SymbolList RuleItemSpec
// RuleItemList -> RuleItemList RuleItem
//              -> RuleItem
// RuleDeclInit -> rule
//              -> root
// RuleDecl -> RuleDeclInit id colon id RuleItemList sc
//
// =============================================================
//
// # A sample grammar file should look like this
//
// ignore str = """aha""";
// token int = "[0-9]+";
// ignore whitespace = "[ \t\r\n]";
// 
// enum BinaryOp { Plus, Minus, Asterisk, Slash }
// 
// node Expression { }
// 
// node BinaryExpression : Expression
// {
//     Expression lhs;
//     BinaryOp op;
//     Expression rhs;
// }
// node LiteralExpression : Expression
// {
//     token value;
// }
// 
// rule AddOp : BinaryOp
//     = '\+' |> Plus
//     = '-' |> Minus
//     ;
// rule MulOp : BinaryOp
//     = '\*' |> Asterisk
//     = '/' |> Slash
//     ;
// 
// rule Factor : Expression
//     = int |> LiteralExpression
//     = '(' Expr ')' |> @1
//     ;
// rule AddExpr : Expression
//     = AddExpr AddOp Factor |> BinaryExpression
//     = Factor
//     ;
// rule MulExpr : Expression
//     = MulExpr MulOp AddExpr |> BinaryExpression
//     = AddExpr
//     ;
// 
// 
// root Expr : Expression
//     = MulExpr
//     ;

#include "config.h"
#include "lexer.h"
#include "parser.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <optional>
#include <any>

using namespace std;

namespace lolita
{

	// Grammar specification
	//

	namespace config_analysis
	{
		struct ConfigToken
		{
			enum
			{
				// Keywords
				//
				token,
				ignore,
				enum_,
				node,
				rule,
				root,

				// Symbols
				//
				assign,  // =
				colon,	 // :
				lp,		 // (
				rp,		 // )
				lb,      // {
				rb,      // }
				sc,      // ;
				comma,   // ,
				pipe,    // |>
				con,	 // &

				// Misc
				//
				ref,     // @n
				id,		 // <identifier>
				dq_str,	 // "..."
				sq_str,  // '...'

				// Whitespaces and Comments
				//
				whitespace,
				comment,
			};
		};

		// definition(i.e. regex) should be in exactly the same order
		// with ConfigToken by convention
		static const vector<string> kTokenDefinition{
			// Keywords
			//
			"token",
			"ignore",
			"enum",
			"node",
			"rule",
			"root",

			// Symbols
			//
			"=",
			":",
			"\\(",
			"\\)",
			"{",
			"}",
			";",
			",",
			"\\|>",
			"&",

			// Misc
			//
			"@[1-9][0-9]*",
			"[_a-zA-Z][_a-zA-Z0-9]*",
			"\"([^\"]|\"\")+\"",
			"'([^']|'')+'",

			// WS and Comments
			//
			"[ \t\r\n]+",
			"#[^\n]*(\n)?",
		};

		template <typename T>
		struct AnyWrapper
		{
			AnyWrapper(any& x) : value(x) { }

			any& value;
		};

		template <typename... TArgs>
		class Agent
		{
		public:
			template <typename TFunc>
			static function<void(vector<any>&)> Create(TFunc func)
			{
				return [=](vector<any>& stack) {
					Invoke(stack, func, index_sequence_for<TArgs...>{});
				};
			}

		private:
			template <typename TFunc, int... I>
			static void Invoke(vector<any>& stack, TFunc&& func, index_sequence<I...>)
			{
				constexpr int count = sizeof...(I);
				auto result = Apply(forward<TFunc>(func), stack[stack.size() + I - count]...);

				for (auto i = 0; i < count; ++i)
					stack.pop_back();

				stack.push_back(move(result));
			}

			template <typename TFunc>
			static decltype(auto) Apply(TFunc&& func, AnyWrapper<TArgs>&& ...args)
			{
				return func(any_cast<TArgs>(args.value)...);
			}
		};

		template <typename T>
		vector<T> ConcatVector(const vector<T> v, const T& item)
		{
			auto result = v;
			result.push_back(item);

			return result;
		}

		int ParserConfigRef(const string& s)
		{
			assert(s.front() == '@');
			return stoi(s.substr(1));
		}
		
		string EscapeConfigQuotedString(const string& s)
		{
			assert(s.size() > 2);
			
			auto quote_ch = s.front();
			assert(quote_ch == '"' || quote_ch == '\'');
			assert(s.back() == quote_ch);

			string result{};
			bool met_quote = false;
			for (auto ch : s.substr(1, s.size() - 2))
			{
				if (met_quote) 
				{
					met_quote = false;
					continue;
				}
				else
				{
					result.push_back(ch);
					met_quote = (ch == quote_ch);
				}
			}

			assert(!met_quote);
			return result;
		}

		class ConfigAnalyzer
		{
		private:
			ConfigAnalyzer(Lexer::SharedPtr lexer,
				Parser::SharedPtr parser,
				function<void(vector<any>&, const Production*)> reduction_cbk)
				: lexer_(move(lexer))
				, parser_(move(parser))
				, reduction_cbk_(move(reduction_cbk)) { }

		public:

			GrammarStmtVec Analyze(const string& data) const
			{
				// tokenize grammar file
				auto tokens = Tokenize(*lexer_, data);
				// detect tokenization failure
				for (auto tok : tokens)
				{
					if (tok.category == -1)
						throw 0;
				}
				// erase whitespaces and comments
				auto iter = remove_if(tokens.begin(), tokens.end(),
					[](auto tok) { return tok.category >= ConfigToken::whitespace; });
				tokens.erase(iter, tokens.end());

				vector<any> v;
				ParsingContext ctx = { {}, [&](const Production* p) { reduction_cbk_(v, p); } };
				for (auto tok : tokens)
				{
					parser_->Feed(ctx, tok.category);
					v.push_back(data.substr(tok.offset, tok.length));
				}

				parser_->Finalize(ctx);

				return any_cast<GrammarStmtVec>(v.front());
			}

			// this function really sucks
			// I write this with literally no performance optimization
			// as manual construction of a grammar is really a mass
			static ConfigAnalyzer Create()
			{
				GrammarBuilder b;

				// build terminals
				//
				auto token = b.NewTerm("");
				auto ignore = b.NewTerm("");
				auto enum_ = b.NewTerm("");
				auto node = b.NewTerm("");
				auto rule = b.NewTerm("");
				auto root = b.NewTerm("");

				auto assign = b.NewTerm("");
				auto colon = b.NewTerm("");
				auto lp = b.NewTerm("");
				auto rp = b.NewTerm("");
				auto lb = b.NewTerm("");
				auto rb = b.NewTerm("");
				auto sc = b.NewTerm("");
				auto comma = b.NewTerm("");
				auto pipe = b.NewTerm("");
				auto con = b.NewTerm("");

				auto ref = b.NewTerm("");
				auto id = b.NewTerm("");
				auto dq_str = b.NewTerm("");
				auto sq_str = b.NewTerm("");

				// NOTE whitespaces and comments should be filtered first

				// build nonterminals
				//
				auto stmt_list = b.NewNonTerm("");
				auto stmt = b.NewNonTerm("");

				auto token_decl_init = b.NewNonTerm("");
				auto token_decl = b.NewNonTerm("");

				auto enum_value_list = b.NewNonTerm("");
				auto enum_decl = b.NewNonTerm("");

				auto node_inh_spec = b.NewNonTerm("");
				auto node_member_decl = b.NewNonTerm("");
				auto node_member_list = b.NewNonTerm("");
				auto node_decl = b.NewNonTerm("");

				auto symbol = b.NewNonTerm("");
				auto symbol_list = b.NewNonTerm("");
				auto ref_list = b.NewNonTerm("");
				auto rule_item_spec = b.NewNonTerm("");
				auto rule_item = b.NewNonTerm("");
				auto rule_item_list = b.NewNonTerm("");
				auto rule_decl_init = b.NewNonTerm("");
				auto rule_decl = b.NewNonTerm("");

				// build productions
				//

				auto stmt_list_1
					= &b.NewProduction(stmt_list, { stmt_list, stmt });
				auto stmt_list_2
					= &b.NewProduction(stmt_list, { stmt });

				auto stmt_1
					= &b.NewProduction(stmt, { token_decl });
				auto stmt_2
					= &b.NewProduction(stmt, { enum_decl });
				auto stmt_3
					= &b.NewProduction(stmt, { node_decl });
				auto stmt_4
					= &b.NewProduction(stmt, { rule_decl });

				auto token_decl_init_1
					= &b.NewProduction(token_decl_init, { token });
				auto token_decl_init_2
					= &b.NewProduction(token_decl_init, { ignore });
				auto token_decl_1
					= &b.NewProduction(token_decl, { token_decl_init, id, assign, dq_str, sc });

				auto enum_value_list_1
					= &b.NewProduction(enum_value_list, { enum_value_list, comma, id });
				auto enum_value_list_2
					= &b.NewProduction(enum_value_list, { id });
				auto enum_decl_1
					= &b.NewProduction(enum_decl, { enum_, id, lb, enum_value_list, rb });

				auto node_inh_spec_1
					= &b.NewProduction(node_inh_spec, { colon, id });
				auto node_inh_spec_2
					= &b.NewProduction(node_inh_spec, {});
				auto node_member_decl_1
					= &b.NewProduction(node_member_decl, { id, id, sc });
				auto node_member_decl_2
					= &b.NewProduction(node_member_decl, { token, id, sc });
				auto node_member_list_1
					= &b.NewProduction(node_member_list, { node_member_list, node_member_decl });
				auto node_member_list_2
					= &b.NewProduction(node_member_list, {});
				auto node_decl_1
					= &b.NewProduction(node_decl, { node, id, node_inh_spec, lb, node_member_list, rb });

				auto symbol_1
					= &b.NewProduction(symbol, { id });
				auto symbol_2
					= &b.NewProduction(symbol, { sq_str });
				auto symbol_list_1
					= &b.NewProduction(symbol_list, { symbol_list, symbol });
				auto symbol_list_2
					= &b.NewProduction(symbol_list, { symbol });
				auto ref_list_1
					= &b.NewProduction(ref_list, { ref_list, comma, ref });
				auto ref_list_2
					= &b.NewProduction(ref_list, { ref });
				auto rule_item_spec_1
					= &b.NewProduction(rule_item_spec, { pipe, id });
				auto rule_item_spec_2
					= &b.NewProduction(rule_item_spec, { pipe, id, lp, ref_list, rp });
				auto rule_item_spec_3
					= &b.NewProduction(rule_item_spec, { pipe, ref });
				auto rule_item_spec_4
					= &b.NewProduction(rule_item_spec, { pipe, con, lp, ref_list, rp });
				auto rule_item_spec_5
					= &b.NewProduction(rule_item_spec, {});
				auto rule_item_1
					= &b.NewProduction(rule_item, { assign, symbol_list, rule_item_spec });
				auto rule_item_list_1
					= &b.NewProduction(rule_item_list, { rule_item_list, rule_item });
				auto rule_item_list_2
					= &b.NewProduction(rule_item_list, { rule_item });
				auto rule_decl_init_1
					= &b.NewProduction(rule_decl_init, { rule });
				auto rule_decl_init_2
					= &b.NewProduction(rule_decl_init, { root });
				auto rule_decl_1
					= &b.NewProduction(rule_decl, { rule_decl_init, id, colon, id, rule_item_list, sc });

				auto lexer = ConstructLexer(kTokenDefinition);
				auto parser = CreateLALRParser(b.Build(stmt_list));
				auto reduction_cbk = [=](vector<any>& stack, const Production* p) {

					unordered_map<const Production*, function<void(vector<any>&)>> cbk_map{
						{
							stmt_list_1,
							Agent<GrammarStmtVec, GrammarStmt>::Create([](auto v, auto s) {
								return ConcatVector(v, s);
							})
						},
						{
							stmt_list_2,
							Agent<GrammarStmt>::Create([](auto s) {
								return GrammarStmtVec{ s };
							})
						},

						{
							stmt_1,
							Agent<TokenDecl>::Create([](auto x) {
								return GrammarStmt{ x };
							})
						},
						{
							stmt_2,
							Agent<EnumDecl>::Create([](auto x) {
								return GrammarStmt{ x };
							})
						},
						{
							stmt_3,
							Agent<NodeDecl>::Create([](auto x) {
								return GrammarStmt{ x };
							})
						},
						{
							stmt_4,
							Agent<RuleDecl>::Create([](auto x) {
								return GrammarStmt{ x };
							})
						},

						{
							token_decl_init_1,
							Agent<string>::Create([](auto x) {
								return false;
							})
						},
						{
							token_decl_init_2,
							Agent<string>::Create([](auto x) {
								return true;
							})
						},
						{
							token_decl_1,
							Agent<bool, string, string, string, string>::Create([](auto init, auto name, auto, auto regex, auto) {
								return TokenDecl{ init, name, EscapeConfigQuotedString(regex) };
							})
						},

						{
							enum_value_list_1,
							Agent<vector<string>, string, string>::Create([](auto v, auto, auto name) {
								return ConcatVector(v, name);
							})
						},
						{
							enum_value_list_2,
							Agent<string>::Create([](auto name) {
								return vector<string>{name};
							})
						},
						{
							enum_decl_1,
							Agent<string, string, string, vector<string>, string>::Create(
								[](auto, auto name, auto, auto v, auto) {
								return EnumDecl{ name , v };
							})
						},

						{
							node_inh_spec_1,
							Agent<string, string>::Create([](auto, auto name) -> optional<string> {
								return name;
							})
						},
						{
							node_inh_spec_2,
							Agent<>::Create([]() -> optional<string> {
								return nullopt;
							})
						},
						{
							node_member_decl_1,
							Agent<string, string, string>::Create([](auto type, auto name, auto) {
								return make_pair(type, name);
							})
						},
						{
							node_member_decl_2,
							Agent<string, string, string>::Create([](auto, auto name, auto) {
								return make_pair(string("token"), name);
							})
						},
						{
							node_member_list_1,
							Agent<vector<pair<string, string>>, pair<string, string>>::Create([](auto v, auto member) {
								return ConcatVector(v, member);
							})
						},
						{
							node_member_list_2,
							Agent<>::Create([]() -> vector<pair<string, string>> {
								return {};
							})
						},
						{
							node_decl_1,
							Agent<string, string, optional<string>, string, vector<pair<string, string>>, string>::Create(
								[](auto, auto name, auto inh, auto, auto members, auto) {
								return NodeDecl{ name, inh, members };
							})
						},

						{
							symbol_1,
							Agent<string>::Create([](auto name) {
								return name;
							})
						},
						{
							symbol_2,
							Agent<string>::Create([](auto name) {
								return EscapeConfigQuotedString(name);
							})
						},
						{
							symbol_list_1,
							Agent<vector<string>, string>::Create([](auto v, auto name) {
								return ConcatVector(v, name);
							})
						},
						{
							symbol_list_2,
							Agent<string>::Create([](auto name) {
								return vector<string>{name};
							})
						},
						{
							ref_list_1,
							Agent<vector<int>, int>::Create([](auto v, auto n) {
								return ConcatVector(v, n);
							})
						},
						{
							ref_list_2,
							Agent<string>::Create([](auto ref_str) {
								return vector<int>{ParserConfigRef(ref_str)};
							})
						},
						// TODO: refine this
						{
							rule_item_spec_1,
							Agent<string, string>::Create([](auto , auto t) -> AstConstructionHint {
								return AstTypeHint{ t };
							})
						},
						{
							rule_item_spec_2,
							Agent<string, string, string, vector<int>, string>::Create(
								[](auto , auto t, auto, auto refs, auto) -> AstConstructionHint {
								return AstTypeCtorHint{ t, refs };
							})
						},
						{
							rule_item_spec_3,
							Agent<string, string>::Create([](auto , auto n) -> AstConstructionHint {
								return AstRefHint{ ParserConfigRef(n) };
							})
						},
						{
							rule_item_spec_4,
							Agent<string, string, string, vector<int>, string>::Create(
								[](auto , auto , auto, auto refs, auto) -> AstConstructionHint {
								return AstConcatHint{ refs };
							})
						},
						{
							rule_item_spec_5,
							Agent<>::Create([]() -> AstConstructionHint {
								return AstRefHint{ 0 };
							})
						},
						{
							rule_item_1,
							Agent<string, vector<string>, AstConstructionHint>::Create([](auto , auto v, auto hint) {
								return RuleItem{ v, hint };
							})
						},
						{
							rule_item_list_1,
							Agent<vector<RuleItem>, RuleItem>::Create([](auto v, auto item) {
								return ConcatVector(v, item);
							})
						},
						{
							rule_item_list_2,
							Agent<RuleItem>::Create([](auto item) {
								return vector<RuleItem>{item};
							})
						},
						{
							rule_decl_init_1,
							Agent<string>::Create([](auto) {
								return false;
							})
						},
						{
							rule_decl_init_2,
							Agent<string>::Create([](auto) {
								return true;
							})
						},
						{
							rule_decl_1,
							Agent<bool, string, string, string, vector<RuleItem>, string>::Create(
								[](auto root, auto name, auto, auto type, auto items, auto) {
								return RuleDecl{ root, name, type, items };
							})
						},
					};

					cbk_map[p](stack);
				};

				return ConfigAnalyzer(lexer, parser, reduction_cbk);
			}

		private:
			Lexer::SharedPtr lexer_;
			Parser::SharedPtr parser_;
			function<void(vector<any>&, const Production*)> reduction_cbk_;
		};
	}

	// Config processing
	//

	GrammarStmtVec LoadConfig(const string& data)
	{
		static const auto analyzer = config_analysis::ConfigAnalyzer::Create();

		return analyzer.Analyze(data);
	}
}