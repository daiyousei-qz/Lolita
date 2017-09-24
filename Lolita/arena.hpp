#pragma once
#include <cassert>
#include <deque>
#include <algorithm>
#include <type_traits>

namespace lolita
{
	class Arena
	{
	public:
		Arena() = default;

		Arena(const Arena&) = delete;
		Arena& operator=(const Arena&) = delete;

		Arena(Arena&& other)
		{
			allocated_ = std::move(other.allocated_);
		}
		Arena& operator=(Arena&& other)
		{
			Clear();
			allocated_ = std::move(other.allocated_);

			return *this;
		}

		~Arena()
		{
			Clear();
		}

		void Clear()
		{
			for (const auto& item : allocated_)
			{
				assert(item.ptr != nullptr);

				if (item.cleaner != nullptr)
					item.cleaner(item.ptr);
			}

			allocated_.clear();
		}

		template <typename T>
		void Include(T* ptr)
		{
			auto cleaner = std::is_trivially_destructible_v<T>
				? reinterpret_cast<void(*)(void*)>(nullptr)
				: [](void* p) { reinterpret_cast<T*>(p)->~T(); };

			allocated_.push_back(ArenaObject{ ptr, cleaner });
		}

		template <typename T>
		void Exclude(T* ptr)
		{
			auto decayed_ptr = reinterpret_cast<void*>(ptr);

			auto iter = std::find_if(
				allocated_.begin(), allocated_.end(),
				[=](const auto& item) { return item.ptr == decayed_ptr; });

			if (iter != allocated_.end())
				allocated_.erase(iter);
		}

		template <typename T, typename ...TArgs>
		T* Construct(TArgs&& ...args)
		{
			T* ptr = new T(std::forward<TArgs>(args)...);
			Include(ptr);

			return ptr;
		}

	private:
		struct ArenaObject
		{
			// pointer to object, this cannot be nullptr
			void* const ptr;

			// destructor proxy if object is non-trivial
			// i.e. there is a destructor
			// otherwise, this field should be set nullptr
			void(* const cleaner)(void*);
		};

		std::deque<ArenaObject> allocated_;
	};
}