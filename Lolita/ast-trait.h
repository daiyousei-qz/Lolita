#pragma once
#include "basic.h"
#include "ast-basic.h"
#include "arena.hpp"
#include <type_traits>
#include <unordered_map>

namespace eds::loli
{
	// trait with actual type erased
	// each AstObject implement its own trait
	class AstObjectTrait
	{
	public:
		virtual AstItem ConstructEnum(int value) = 0;
		virtual AstItem ConstructObject(Arena&) = 0;
		virtual AstItem ConstructVector(Arena&) = 0;

		virtual void AssignField(AstItem node, int codinal, AstItem value) = 0;
		virtual void InsertElement(AstItem vec, AstItem node) = 0;
	};

	class AstTraitManager
	{
	private:
		class AstTraitDummy : public AstObjectTrait
		{
		public:
			AstItem ConstructEnum(int value) override
			{
				throw 0;
			}
			AstItem ConstructObject(Arena&) override
			{
				throw 0;
			}
			AstItem ConstructVector(Arena&) override
			{
				throw 0;
			}

			void AssignField(AstItem node, int codinal, AstItem value) override
			{
				throw 0;
			}
			void InsertElement(AstItem vec, AstItem node) override
			{
				throw 0;
			}

		private:
			void Throw()
			{
			}
		};

	public:
		AstObjectTrait& Dummy() const
		{	
			static AstTraitDummy dummy{};
			return dummy;
		}

		AstObjectTrait& Lookup(const std::string& klass) const
		{
			auto it = trait_lookup.find(klass);
			if (it != trait_lookup.end())
			{
				return *it->second;
			}
			else
			{
				throw 0;
			}
		}

		template <typename Trait>
		void Register(const std::string& klass)
		{
			static_assert(std::is_base_of_v<AstObjectTrait, Trait>);
			static_assert(std::is_default_constructible_v<Trait>);

			trait_lookup.insert_or_assign(klass, std::make_unique<Trait>());
		}

	private:
		using TraitLookupMap = std::unordered_map<std::string, std::unique_ptr<AstObjectTrait>>;

		TraitLookupMap trait_lookup;
	};
}