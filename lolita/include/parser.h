#pragma once
#include "ast/ast-basic.h"
#include "core/parsing-info.h"
#include "memory/arena.h"
#include <memory>

namespace eds::loli
{
    // =====================================================================================
    // BootstrapParser
    //

    // generate code binding
    std::string BootstrapParser(const std::string& config);

    // =====================================================================================
    // Parsing Actions
    //

    struct ActionError
    {
    };

    struct ActionShift
    {
        int target_state;
    };
    struct ActionReduce
    {
        const ProductionInfo* production;
    };
    using ParsingAction = std::variant<ActionError, ActionShift, ActionReduce>;

    // =====================================================================================
    // Parsing Context
    //

    enum class ActionExecutionResult
    {
        Hungry,
        Consumed,
        Error
    };

    class ParsingContext;

    // =====================================================================================
    // Parsing Context
    //

    class GenericParser
    {
    public:
        GenericParser(const std::string& config, const ast::AstTypeProxyManager* env);

        const auto& GrammarInfo() const { return *info_; }

        void Initialize(const std::string& config, const ast::AstTypeProxyManager* env);

        ast::AstItemWrapper Parse(Arena& arena, const std::string& data);

    private:
        int LexerInitialState() const { return 0; }
        int ParserInitialState() const { return 0; }

        bool VerifyCharacter(int ch) const
        {
            return ch >= 0 && ch < 128;
        }
        bool VerifyLexingState(int state) const
        {
            return state >= 0 && state < dfa_state_num_;
        }
        bool VerifyParsingState(int state) const
        {
            return state >= 0 && state < pda_state_num_;
        }

        int LookupLexingTransition(int state, int ch) const
        {
            assert(VerifyLexingState(state) && VerifyCharacter(ch));
            return lexing_table_[128 * state + ch];
        }
        const TokenInfo* LookupAcceptedToken(int state) const
        {
            assert(VerifyLexingState(state));
            return acc_token_lookup_[state];
        }
        ParsingAction LookupParsingAction(int state, int term_id) const
        {
            assert(VerifyParsingState(state) && term_id >= 0 && term_id < term_num_);
            return action_table_[term_num_ * state + term_id];
        }
        ParsingAction LookupParsingActionOnEof(int state) const
        {
            assert(VerifyParsingState(state));
            return eof_action_table_[state];
        }
        int LookupParsingGoto(int state, int nonterm_id) const
        {
            assert(VerifyParsingState(state) && nonterm_id >= 0 && nonterm_id <= nonterm_num_);
            return goto_table_[nonterm_num_ * state + nonterm_id];
        }

        ast::BasicAstToken LoadToken(std::string_view data, int offset);

        ActionExecutionResult ForwardParsingAction(ParsingContext& ctx, ActionShift action, const ast::BasicAstToken& tok);
        ActionExecutionResult ForwardParsingAction(ParsingContext& ctx, ActionReduce action, const ast::BasicAstToken& tok);
        ActionExecutionResult ForwardParsingAction(ParsingContext& ctx, ActionError action, const ast::BasicAstToken& tok);

        void FeedParsingContext(ParsingContext& ctx, const ast::BasicAstToken& tok);

    private:
        // meta information
        std::unique_ptr<ParsingMetaInfo> info_;

        // parser
        int token_num_;
        int term_num_;
        int nonterm_num_;

        int dfa_state_num_;
        int pda_state_num_;

        container::HeapArray<const TokenInfo*> acc_token_lookup_; // 1 column, token_num_ rows
        container::HeapArray<int> lexing_table_;                  // 128 columns, dfa_state_num_ rows

        container::HeapArray<ParsingAction> action_table_;     // term_num_ columns, pda_state_num_ rows
        container::HeapArray<ParsingAction> eof_action_table_; // 1 column, pda_state_num_ rows
        container::HeapArray<int> goto_table_;                 // nonterm_num_ columns, pda_state_num_ rows
    };

    template <typename T>
    class BasicParser
    {
    public:
        using Ptr        = std::unique_ptr<BasicParser>;
        using ResultType = typename ast::AstTypeTrait<T>::StoreType;

        ResultType Parse(Arena& arena, const std::string& data)
        {
            auto result = parser_->Parse(*arena, data);

            return result.Extract<ResultType>();
        }

        static Ptr Create(const std::string& config, const ast::AstTypeProxyManager* env)
        {
            auto result     = std::make_unique<BasicParser<T>>();
            result->parser_ = std::make_unique<GenericParser>(config, env);

            return result;
        }

    private:
        std::unique_ptr<GenericParser> parser_;
    };
}