#pragma once
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace lolita
{
	// Library Ast Types
	//
	struct AstObject
	{
		using Ptr = std::unique_ptr<AstObject>;

		AstObject() = default;
		virtual ~AstObject() = default;
	};

	template <typename T>
	struct AstObjectList
	{
		std::vector<T> data;
	};

	template <typename T>
	struct AstObjectOptional
	{
		std::optional<T> data;
	};

	template <typename T>
	struct AstObjectList : public AstObject
	{
		static_assert(std::is_base_of_v<AstObject, T>);

		std::vector<T*> data;
	};

	// Generated Enum Types
	//
	enum BinaryOp
	{
		Plus, Minus, Asterisk, Slash,
	};

	// Generated Base Classes
	//
	struct Expression : public AstObject
	{
		struct Visitor;

		virtual void Accept(Visitor&) = 0;
	};

	// Generated Node Classes
	//
	struct BinaryExpression : public Expression
	{
		Expression* lhs;
		BinaryOp op;
		Expression* rhs;

		void Accept(Expression::Visitor& v) override { v.Visit(this); }
	};

	struct LiteralExpression : public Expression
	{
		std::string value;

		void Accept(Expression::Visitor& v) override { v.Visit(this); }
	};

	// Generated Visitor Types
	//

	struct Expression::Visitor
	{
		virtual void Visit(BinaryExpression&) = 0;
		virtual void Visit(LiteralExpression&) = 0;
	};

	// Generated Parsing Utilities
	//

	class ParsingResult
	{
	public:
		using Ptr = unique_ptr<ParsingResult>;

		Expression* Object() const
		{
			return obj_;
		}

	private:
		Expression* obj_;
		std::deque<AstObject::Ptr> arena_;
	};

	ParsingResult::Ptr Parse(string_view data);
}