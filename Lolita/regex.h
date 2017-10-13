#pragma once
#include <functional>
#include <cassert>

namespace eds::loli
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

	using RegexExprPtr = const RegexExpr*;
	using RootExprVec = std::vector<RootExpr*>;
	using RegexExprVec = std::vector<const RegexExpr*>;

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
		CharRange(int ch) : CharRange(ch, ch) { }

		int Min() const { return min_; }
		int Max() const { return max_; }

		size_t Length() const { return max_ - min_; }

		bool Contain(int ch) const { return ch >= min_ && ch <= max_; }
		bool Contain(CharRange range) const { return Contain(range.min_) && Contain(range.max_); }

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
		virtual void Visit(const RootExpr&) = 0;
		virtual void Visit(const EntityExpr&) = 0;
		virtual void Visit(const SequenceExpr&) = 0;
		virtual void Visit(const ChoiceExpr&) = 0;
		virtual void Visit(const ClosureExpr&) = 0;
	};

	// Expression Model
	//

	class RegexExpr
	{
	public:
		RegexExpr() = default;
		virtual ~RegexExpr() = default;

		virtual void Accept(RegexExprVisitor&) const = 0;
	};

	// a LabelledExpr may attach a position id on conversion to DFA
	class LabelledExpr
	{
	public:
		LabelledExpr() = default;
		virtual ~LabelledExpr() = default;

		// testify if ch could pass this position
		virtual bool TestPassage(int ch) const = 0;
	};

	class RootExpr : public RegexExpr, public LabelledExpr
	{
	public:
		RootExpr(RegexExprPtr child)
			: child_(child) { }

		auto Child() const { return child_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }
		bool TestPassage(int ch) const override { return true; }

	private:
		RegexExprPtr child_;
	};

	class EntityExpr : public RegexExpr, public LabelledExpr
	{
	public:
		EntityExpr(CharRange rg)
			: range_(rg) { }

		auto Range() const { return range_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }
		bool TestPassage(int ch) const override { return range_.Contain(ch); }

	private:
		CharRange range_;
	};

	class SequenceExpr : public RegexExpr
	{
	public:
		SequenceExpr(const RegexExprVec& seq)
			: seq_(seq) { }

		const auto& Children() const { return seq_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	private:
		RegexExprVec seq_;
	};

	class ChoiceExpr : public RegexExpr
	{
	public:
		ChoiceExpr(const RegexExprVec& any)
			: any_(any) { }

		const auto& Children() const { return any_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	private:
		RegexExprVec any_;
	};

	class ClosureExpr : public RegexExpr
	{
	public:
		ClosureExpr(RegexExprPtr child, RepetitionMode strategy)
			: child_(child), mode_(strategy) { }

		auto Child() const { return child_; }
		auto Mode() const { return mode_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	public:
		RegexExprPtr child_;
		RepetitionMode mode_;
	};
}