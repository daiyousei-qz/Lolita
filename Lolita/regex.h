#pragma once
#include "lolita-basic.h"
#include <functional>

namespace lolita
{

	// Basic Decl
	//

	class RegexExpr;
	class EntityExpr;
	class ConcatenationExpr;
	class AlternationExpr;
	class StarExpr;

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

		int Min() const { return min_; }
		int Max() const { return max_; }

		size_t Length() const { return max_ - min_; }

		bool Contain(int ch) const { return ch >= min_ && ch <= max_; }
		bool Contain(CharRange range) const { return Contain(range.min_) && Contain(range.max_); }

	private:
		int min_, max_;
	};

	// [min, max]
	class Repetition
	{
	public:
		// Constructs a Reptition in a specific range
		// NOTES if max is too large, it may be recognized as infinity
		Repetition(size_t min, size_t max)
			: min_(min), max_(max)
		{
			assert(min <= max && max > 0);
		}

		// Constructs a Reptition that goes infinity
		Repetition(size_t min)
			: Repetition(min, kInfinityThreshold + 1) { }

		size_t Min() const { return min_; }
		size_t Max() const { return max_; }

		bool GoInfinity() const { return max_ > kInfinityThreshold; }

	public:
		static constexpr size_t kInfinityThreshold = 1000;

	private:
		size_t min_, max_;
	};

	// Visit
	//

	class RegexExprVisitor
	{
	public:
		virtual void Visit(const EntityExpr&) = 0;
		virtual void Visit(const ConcatenationExpr&) = 0;
		virtual void Visit(const AlternationExpr&) = 0;
		virtual void Visit(const StarExpr&) = 0;
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

	class EntityExpr : public RegexExpr
	{
	public:
		EntityExpr(CharRange rg)
			: range_(rg) { }

		auto Range() const { return range_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	private:
		CharRange range_;
	};

	class ConcatenationExpr : public RegexExpr
	{
	public:
		ConcatenationExpr(const RegexExprVec& seq)
			: seq_(seq) { }

		const auto& Children() const { return seq_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	private:
		RegexExprVec seq_;
	};

	class AlternationExpr : public RegexExpr
	{
	public:
		AlternationExpr(const RegexExprVec& any)
			: any_(any) { }

		const auto& Children() const { return any_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	private:
		RegexExprVec any_;
	};

	class StarExpr : public RegexExpr
	{
	public:
		StarExpr(RegexExpr* child)
			: child_(child) { }

		auto Child() const { return child_; }

		void Accept(RegexExprVisitor& v) const override { v.Visit(*this); }

	public:
		RegexExpr* child_;
	};
}