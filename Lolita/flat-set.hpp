/*=================================================================================
*  Copyright (c) 2016 Edward Cheng
*
*  edslib is an open-source library in C++ and licensed under the MIT License.
*  Refer to: https://opensource.org/licenses/MIT
*================================================================================*/

#pragma once
#include <cstddef>
#include <vector>
#include <algorithm>
#include <type_traits>

namespace lolita
{
    // is_iterator
    //

    // test via if category_tag could be found
    template <typename T, typename = void>
    struct is_iterator : public std::false_type { };
    template <typename T>
    struct is_iterator<T, std::void_t<typename std::iterator_traits<T>::iterator_category>>
        : public std::true_type { };

    // FlatSet
    //

    template <typename Key,
              typename Compare = std::less<Key>,
              typename Allocator = std::allocator<Key>>
    class FlatSet
    {
        static_assert(std::is_move_constructible<Key>::value, "T in CompactSet<T> must be move constructible");

    public:
        using key_type = Key;
        using value_type = Key;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using key_compare = Compare;
        using value_compare = Compare;
        using allocator_type = Allocator;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename std::allocator_traits<Allocator>::pointer;
        using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

        using underlying_container = std::vector<Key, Allocator>;
        using iterator = typename underlying_container::iterator;
        using const_iterator = typename underlying_container::const_iterator;
        using reverse_iterator = typename underlying_container::reverse_iterator;
        using const_reverse_iterator = typename underlying_container::const_reverse_iterator;

    public:
        // ctor
        FlatSet() { }
        template <typename InputIt>
        FlatSet(InputIt first, InputIt last)
        {
            assign(first, last);
        }
        FlatSet(const FlatSet& other)
        {
            assign(other.begin(), other.end());
        }
        FlatSet(FlatSet&& other)
        {
            swap(other);
        }
        FlatSet(std::initializer_list<Key> ilist)
        {
            assign(ilist);
        }

        FlatSet& operator=(const FlatSet& other)
        {
            assign(other.begin(), other.end());
            return *this;
        }
        FlatSet& operator=(FlatSet&& other)
        {
            clear();
            swap(other);
            return *this;
        }

        //
        // dtor
        //
        ~FlatSet() = default;

    public:
        //
        // assign
        //
        template <typename InputIt>
        std::enable_if_t<is_iterator<InputIt>::value>
            assign(InputIt first, InputIt last)
        {
            clear();
            insert(first, last);
        }

        void assign(std::initializer_list<Key> ilist)
        {
            assign(ilist.begin(), ilist.end());
        }

        //
        // access
        //
        reference at(size_type pos)
        {
            return container_[pos];
        }
        const_reference at(size_type pos) const
        {
            return container_[pos];
        }
        reference operator[](size_type pos)
        {
            return container_[pos];
        }
        const_reference operator[](size_type pos) const
        {
            return container_[pos];
        }

        //
        // iterator
        //
        iterator        begin()        noexcept { return container_.begin(); }
        iterator        end()          noexcept { return container_.end(); }
        const_iterator  begin()  const noexcept { return container_.cbegin(); }
        const_iterator  end()    const noexcept { return container_.cend(); }
        const_iterator  cbegin() const noexcept { return container_.cbegin(); }
        const_iterator  cend()   const noexcept { return container_.cend(); }

        reverse_iterator       rbegin()        noexcept { return container_.begin(); }
        reverse_iterator       rend()          noexcept { return container_.end(); }
        const_reverse_iterator rbegin()  const noexcept { return container_.cbegin(); }
        const_reverse_iterator rend()    const noexcept { return container_.cend(); }
        const_reverse_iterator rcbegin() const noexcept { return container_.cbegin(); }
        const_reverse_iterator rcend()   const noexcept { return container_.cend(); }

        //
        // capacity
        //
        bool empty() const noexcept
        {
            return container_.empty();
        }
        size_type capacity() const noexcept
        {
            return container_.capacity();
        }
        size_type size() const noexcept
        {
            return container_.size();
        }
        size_type max_size() const noexcept
        {
            return container_.max_size();
        }

        //
        // modifiers
        //
        void clear()
        {
            container_.clear();
        }

        template <typename ... TArgs>
        std::pair<iterator, bool> emplace(TArgs&& ... args)
        {
            // TODO: performance warning
            Key value = Key(std::forward<TArgs>(args)...);
            iterator lb = lower_bound(value);
            if (lb != container_.end() && *lb == value)
            {
				return std::make_pair(lb, false);
            }
            else
            {
                iterator where = container_.emplace(lb, std::forward<TArgs>(args)...);
				return std::make_pair(lb, false);
            }
        }
        std::pair<iterator, bool> insert(const value_type& value)
        {
            return emplace(value);
        }
        std::pair<iterator, bool> insert(value_type&& value)
        {
            return emplace(std::forward<value_type>(value));
        }
        iterator insert(const_iterator hint, const value_type& value)
        {
            // hint is ignored
            return emplace(value);
        }
        iterator insert(const_iterator hint, value_type&& value)
        {
            // hint is ignored
            return emplace(std::forward<value_type>(value));
        }
        template <typename InputIt>
        void insert(InputIt first, InputIt last)
        {
            container_.insert(container_.end(), first, last);
            std::sort(container_.begin(), container_.end(), key_comp());
			
			auto correct_end = std::unique(container_.begin(), container_.end());
			container_.erase(correct_end, container_.end());
		}
        void insert(std::initializer_list<value_type> ilist)
        {
            insert(ilist.begin(), ilist.end());
        }

        void erase(const Key& x)
        {
            auto where = find(x);
            if (where != container_.end())
            {
                container_.erase(where);
            }
        }

        void swap(FlatSet& other)
        {
            container_.swap(other.container_);
        }

        // lookup
        size_type count(const Key& value) const
        {
            return std::find(container_.begin(), container_.end(), value) == end() ? 0 : 1;
        }
        iterator find(const Key& value)
        {
            return std::find(container_.begin(), container_.end(), value);
        }
        const_iterator find(const Key& value) const
        {
            return std::find(container_.cbegin(), container_.cend(), value);
        }
        iterator lower_bound(const Key& value)
        {
            return std::lower_bound(begin(), end(), value, key_comp());
        }
        const_iterator lower_bound(const Key& value) const
        {
            return std::lower_bound(begin(), end(), value, key_comp());
        }
        iterator upper_bound(const Key& value)
        {
            return std::upper_bound(begin(), end(), value, key_comp());
        }
        const_iterator upper_bound(const Key& value) const
        {
            return std::upper_bound(begin(), end(), value, key_comp());
        }

        // observers
        key_compare   key_comp()   const { return key_compare{}; }
        value_compare value_comp() const { return value_compare{}; }

    private:
        std::vector<Key, Allocator> container_;
    };

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator ==(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
    }

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator !=(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return !(lhs == rhs);
    }

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator <(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), Compare{});
    }

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator <=(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return operator==(lhs, rhs) || operator<(lhs, rhs);
    }

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator >(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return !operator<=(lhs, rhs);
    }

    template <typename Key,
        typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
        inline bool operator >=(const FlatSet<Key, Compare, Allocator> &lhs,
            const FlatSet<Key, Compare, Allocator> &rhs)
    {
        return !operator<(lhs, rhs);
    }
}