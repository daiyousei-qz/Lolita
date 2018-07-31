#pragma once
#include "lang-utils.h"
#include <functional>
#include <cassert>
#include <memory>

namespace eds::loli::regex
{
    // Basic Decl
    //

    class RegexExpr;
    class LabelledExpr;

    class RootExpr;
    class EntityExpr;
    class SequenceExpr;
    class ChoiceExpr;
    class ClosureExpr;

    std::unique_ptr<RootExpr> ParseRegex(const std::string& regex);

    // Data classes
    //

    // TODO: make max exclusive
    // [min, max]
    class CharRange
    {
    public:
        CharRange(int min, int max)
            : min_(min), max_(max)
        {
            assert(min <= max);
        }
        CharRange(int ch)
            : CharRange(ch, ch) {}

        int Min() const { return min_; }
        int Max() const { return max_; }

        // Number of character included
        int Length() const { return max_ - min_ + 1; }

        bool Contain(int ch) const { return ch >= min_ && ch <= max_; }
        bool Contain(CharRange rg) const { return rg.min_ >= min_ && rg.max_ <= max_; }

    private:
        int min_, max_;
    };

    enum class RepetitionMode
    {
        Optional,
        Star,
        Plus,
    };

    // Visitor
    //

    class RegexExprVisitor
    {
    public:
        virtual void Visit(const RootExpr&)     = 0;
        virtual void Visit(const EntityExpr&)   = 0;
        virtual void Visit(const SequenceExpr&) = 0;
        virtual void Visit(const ChoiceExpr&)   = 0;
        virtual void Visit(const ClosureExpr&)  = 0;
    };

    // Expression Model
    //

    class RegexExpr : NonCopyable, NonMovable
    {
    public:
        using Ptr = std::unique_ptr<RegexExpr>;

        RegexExpr()          = default;
        virtual ~RegexExpr() = default;

        virtual void Accept(RegexExprVisitor&) const = 0;
    };

    // a LabelledExpr may be attached with a position id during DFA generation
    class LabelledExpr
    {
    public:
        LabelledExpr()          = default;
        virtual ~LabelledExpr() = default;

        // testify if ch could pass this position
        virtual bool TestPassage(int ch) const = 0;
    };

    class RootExpr : public RegexExpr, public LabelledExpr
    {
    public:
        RootExpr(RegexExpr::Ptr child)
            : child_(std::move(child)) {}

        const auto& Child() const { return child_; }

        void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

        // always fail as this is last position
        bool TestPassage(int ch) const override { return false; }

    private:
        RegexExpr::Ptr child_;
    };

    class EntityExpr : public RegexExpr, public LabelledExpr
    {
    public:
        EntityExpr(CharRange rg)
            : range_(rg) {}

        auto Range() const { return range_; }

        void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

        // pass if ch is contained in range_
        bool TestPassage(int ch) const override { return range_.Contain(ch); }

    private:
        CharRange range_;
    };

    class SequenceExpr : public RegexExpr
    {
    public:
        SequenceExpr(std::vector<RegexExpr::Ptr> seq)
            : seq_(std::move(seq))
        {
            assert(!seq_.empty());
        }

        const auto& Children() const { return seq_; }

        void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

    private:
        std::vector<RegexExpr::Ptr> seq_;
    };

    class ChoiceExpr : public RegexExpr
    {
    public:
        ChoiceExpr(std::vector<RegexExpr::Ptr> any)
            : any_(std::move(any))
        {
            assert(!any_.empty());
        }

        const auto& Children() const { return any_; }

        void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

    private:
        std::vector<RegexExpr::Ptr> any_;
    };

    class ClosureExpr : public RegexExpr
    {
    public:
        ClosureExpr(RegexExpr::Ptr child, RepetitionMode strategy)
            : child_(std::move(child)), mode_(strategy) {}

        const auto& Child() const { return child_; }
        const auto& Mode() const { return mode_; }

        void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

    public:
        RegexExpr::Ptr child_;
        RepetitionMode mode_;
    };
}