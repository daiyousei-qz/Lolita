#pragma once
#include "ast-basic.h"
#include "arena.hpp"
#include <type_traits>

namespace eds::loli
{
	// trait with actual type erased
	// each AstObject implement its own trait
	class AstTraitInterface
	{
	public:
		virtual AstObject* ConstructObject(Arena&) = 0;
		virtual AstVectorBase* ConstructVector(Arena&) = 0;

		virtual void AssignField(AstObject* node, int codinal, AstItem value) = 0;
		virtual void InsertElement(AstVectorBase* vec, AstItem node) = 0;
	};

	template <typename T>
	class AstTraitBase : public AstTraitInterface
	{
	public:
		static_assert(std::is_base_of_v<AstObject, T>);

		AstObject* ConstructObject(Arena& arena) override
		{
			return arena.Construct<T>();
		}

		AstVectorBase* ConstructVector(Arena& arena) override
		{
			return arena.Construct<AstVector<T>>();
		}
	};

	class AstTraitManager
	{
	private:
		class AstTraitDummy : public AstTraitInterface
		{
		public:
			AstObject* ConstructObject(Arena&) override
			{
				Throw();
			}
			AstVectorBase* ConstructVector(Arena&) override
			{
				Throw();
			}

			void AssignField(AstObject* node, int codinal, AstItem value) override
			{
				Throw();
			}
			void InsertElement(AstVectorBase* vec, AstItem node) override
			{
				Throw();
			}

		private:
			void Throw()
			{
				throw 0;
			}
		};

	public:
		AstTraitInterface& Dummy()
		{	
			static AstTraitDummy dummy{};
			return dummy;
		}

		virtual AstTraitInterface& Lookup(const std::string& klass) = 0;
	};
}