#pragma once
#include "ast-basic.h"
#include "arena.hpp"

namespace eds::loli
{
	// trait with actual type erased
	// each AstObject implement its own trait
	class AstTraitInterface
	{
	public:
		virtual AstObject* ConstructObject(Arena&) = 0;
		virtual AstVector* ConstructVector(Arena&) = 0;
		virtual void AssignField(AstObject* node, int codinal, AstItem value) = 0;
		virtual void InsertElement(AstVector* vec, AstItem node) = 0;
	};
	class AstTraitManager
	{
		virtual AstTraitInterface& GetTrait(int klass_id) = 0;
	};
}