#pragma once
#include "ast-basic.h"
#include "ast-trait.h"
#include <vector>
#include <string_view>
#include <variant>

// given Arena&, AstTraitInterface&,s AstItemView
// CHOICE ->
// - return one item
// * return null_opt
// * return one item as AstOptional
// - return empty_vec
// - return one item as AstVector
// - construct a new AstObject and merge items into it

namespace eds::loli
{
	namespace detail
	{
		template <bool Value>
		class AllowDummyTrait
		{
		public:
			bool AllowDummyTrait() const
			{
				return Value;
			}
		};
	}

	// Gen
	//

	class AstEnumGen : public detail::AllowDummyTrait<true>
	{
	public:
		AstEnumGen(int value)
			: value_(value) { }

		AstItem Invoke(AstTraitInterface& trait, Arena& arena, AstItemView rhs) const
		{
			return value_;
		}

	private:
		int value_;
	};
	class AstObjectGen : public detail::AllowDummyTrait<false>
	{
	public:
		AstItem Invoke(AstTraitInterface& trait, Arena& arena, AstItemView rhs) const
		{
			return trait.ConstructObject(arena);
		}
	};	
	class AstVectorGen : public detail::AllowDummyTrait<false>
	{
	public:
		AstItem Invoke(AstTraitInterface& trait, Arena& arena, AstItemView rhs) const
		{
			return trait.ConstructVector(arena);
		}
	};
	class AstItemSelector : public detail::AllowDummyTrait<true>
	{
	public:
		AstItemSelector(int index)
			: index_(index) { }

		AstItem Invoke(AstTraitInterface& trait, Arena& arena, AstItemView rhs) const
		{
			return rhs.At(index_).object;
		}

	private:
		int index_;
	};

	// Manip
	//

	class AstManipPlaceholder : public detail::AllowDummyTrait<true>
	{
	public:
		void Invoke(AstTraitInterface& trait, AstItem item, AstItemView rhs) const { }
	};
	class AstObjectSetter : public detail::AllowDummyTrait<false>
	{
	public:
		struct SetterPair
		{
			int cordinal;
			int index;
		};

		AstObjectSetter(const std::vector<SetterPair>& setters)
			: setters_(setters) { }

		void Invoke(AstTraitInterface& trait, AstItem item, AstItemView rhs) const
		{
			for (auto pair : setters_)
			{
				trait.AssignField(item.object, pair.cordinal, rhs.At(pair.index));
			}
		}

	private:
		std::vector<SetterPair> setters_;
	};
	class AstVectorMerger : public detail::AllowDummyTrait<false>
	{
	public:
		AstVectorMerger(const std::vector<int>& indices)
			: indices_(indices) { }

		void Invoke(AstTraitInterface& trait, AstItem item, AstItemView rhs) const
		{
			for (auto index : indices_)
			{
				trait.InsertElement(item.vector, rhs.At(index));
			}
		}

	private:
		std::vector<int> indices_;
	};

	// Handle
	//

	class AstHandle
	{
	public:
		using GenHandle = std::variant<
			AstEnumGen,
			AstObjectGen,
			AstVectorGen,
			AstItemSelector
		>;

		using ManipHandle = std::variant<
			AstManipPlaceholder,
			AstObjectSetter,
			AstVectorMerger
		>;

		AstHandle(const std::string& klass, GenHandle gen, ManipHandle manip)
			: klass_(klass), gen_handle_(gen), manip_handle_(manip) { }

		bool AllowDummyTrait() const
		{
			auto test_dummy_visitor = 
				[](const auto& handle) { return handle.AllowDummyTrait(); };

			return std::visit(test_dummy_visitor, gen_handle_)
				&& std::visit(test_dummy_visitor, manip_handle_);
		}

		AstItem Invoke(AstTraitManager& ctx, Arena& arena, AstItemView rhs) const
		{
			auto& trait = AllowDummyTrait()
				? ctx.Dummy()
				: ctx.Lookup(klass_);

			auto gen_visitor = [&](const auto& gen) { return gen.Invoke(trait, arena, rhs); };
			auto result = std::visit(gen_visitor, gen_handle_);

			auto manip_visitor = [&](const auto& manip) { manip.Invoke(trait, result, rhs); };
			std::visit(manip_visitor, manip_handle_);

			return result;
		}

	private:
		std::string klass_;

		GenHandle gen_handle_;
		ManipHandle manip_handle_;
	};
}