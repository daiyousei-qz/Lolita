#pragma once
#include "basic.h"
#include "ast-basic.h"
#include "arena.hpp"
#include <type_traits>
#include <unordered_map>

namespace eds::loli
{
	// =====================================================================================
	// Type proxy
	//

	// trait with actual type erased
	// each AstObject implement its own trait
	class AstTypeProxy
	{
	public:
		virtual AstTypeWrapper ConstructEnum(int value) = 0;
		virtual AstTypeWrapper ConstructObject(Arena&) = 0;
		virtual AstTypeWrapper ConstructVector(Arena&) = 0;

		virtual void AssignField(AstTypeWrapper node, int codinal, AstTypeWrapper value) = 0;
		virtual void InsertElement(AstTypeWrapper vec, AstTypeWrapper node) = 0;
	};

	// placeholder proxy
	class DummyAstTypeProxy : public AstTypeProxy
	{
	private:
		static const auto& GetErrorMessage()
		{
			static std::string msg = "dummy proxy cannot perform any ast operation";
			return msg;
		}

	public:
		AstTypeWrapper ConstructEnum(int value) override
		{
			throw ParserInternalError{ GetErrorMessage() };
		}
		AstTypeWrapper ConstructObject(Arena&) override
		{
			throw ParserInternalError{ GetErrorMessage() };
		}
		AstTypeWrapper ConstructVector(Arena&) override
		{
			throw ParserInternalError{ GetErrorMessage() };
		}

		void AssignField(AstTypeWrapper node, int codinal, AstTypeWrapper value) override
		{
			throw ParserInternalError{ GetErrorMessage() };
		}
		void InsertElement(AstTypeWrapper vec, AstTypeWrapper node) override
		{
			throw ParserInternalError{ GetErrorMessage() };
		}
	};

	// a proxy template
	template <typename T>
	class BasicAstTypeProxy : public AstTypeProxy
	{
	public:
		using TraitType = AstTypeTrait<T>;

		using SelfType = typename TraitType::SelfType;
		using StoreType = typename TraitType::StoreType;

		using VectorType = AstVector<SelfType>;

		AstTypeWrapper ConstructEnum(int value) override
		{
			if constexpr (TraitType::IsEnum())
			{
				return AstTypeWrapper::Create<SelfType>(static_cast<SelfType>(value));
			}
			else
			{
				throw ParserInternalError{ "T is not an enum type" };
			}
		}
		AstTypeWrapper ConstructObject(Arena& arena) override
		{
			if constexpr (TraitType::IsKlass())
			{
				return AstTypeWrapper::Create<SelfType*>(arena.Construct<SelfType>());
			}
			else
			{
				throw ParserInternalError{ "T is not a klass type" };
			}
		}

		AstTypeWrapper ConstructVector(Arena& arena) override
		{
			return AstTypeWrapper::Create<VectorType*>(arena.Construct<VectorType>());
		}

		void AssignField(AstTypeWrapper obj, int codinal, AstTypeWrapper value) override
		{
			throw ParserInternalError{ "T is not a klass type" };
		}
		void InsertElement(AstTypeWrapper vec, AstTypeWrapper elem) override
		{
			vec.Extract<VectorType*>()->Push(elem.Extract<StoreType>());
		}
	};

	// =====================================================================================
	// Proxy Manager
	//

	class AstTypeProxyManager
	{
	public:
		AstTypeProxy& Lookup(const std::string& klass) const
		{
			auto it = traits.find(klass);
			if (it != traits.end())
			{
				return *it->second;
			}
			else
			{
				throw ParserInternalError{ "specific trait not found" };
			}
		}

		template <typename Proxy>
		void Register(const std::string& klass)
		{
			static_assert(std::is_base_of_v<AstTypeProxy, Proxy>);
			static_assert(std::is_default_constructible_v<Proxy>);

			traits.insert_or_assign(klass, std::make_unique<Proxy>());
		}

		AstTypeProxy& DummyProxy() const
		{
			static DummyAstTypeProxy dummy{};
			return dummy;
		}

	private:
		using TraitLookup = std::unordered_map<std::string, std::unique_ptr<AstTypeProxy>>;

		TraitLookup traits;
	};
}