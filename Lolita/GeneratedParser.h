#pragma once
#include <deque>
#include <memory>
#include <string_view>

namespace lolita
{
	// Generated Enum Types
	//

	enum BinaryOp
	{
		Plus, Minus, Asterisk, Slash,
	};

	// Generated Ast Types
	//

	struct AstObject
	{
		using Ptr = std::unique_ptr<AstObject>;

		AstObject() = default;
		virtual ~AstObject() = default;
	};

	struct ExpressionVisitor;
	struct Expression : public AstObject
	{
		virtual void Accept(ExpressionVisitor&) = 0;
	};

	struct BinaryExpression : public Expression
	{
		Expression* lhs;
		BinaryOp op;
		Expression* rhs;
	};

	struct LiteralExpression : public Expression
	{
		std::string_view value;
	};

	// Generated Visitor Types
	//

	struct ExpressionVisitor
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