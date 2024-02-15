/******************************************************************************
 * vlc_list.hpp: C++ wrappers on top of vlc_list
 ******************************************************************************
 * Copyright © 2024 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *          Pierre Lamot <pierre@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_LIST_HPP
#define VLC_LIST_HPP 1

#include <vlc_list.h>

#include <iterator>
#include <type_traits>

namespace vlc
{

/**
 * \defgroup cpp_list Linked lists (C++ wrappers)
 * \ingroup cext
 * @{
 * \file
 * This provides convenience helpers for using the C linked lists extension
 * from C++ files.
 *
 * A list wrapper should be used on an existing list:
 *
 * \code
 * struct item {
 *     // ...
 *     vlc_list node;
 * }
 *
 * struct vlc_list c_list;
 * vlc_list_init(&c_list);
 * // ...
 * auto list = vlc::from(&list, &item::node);
 * \endcode
 *
 * Using `vlc::from` will automatically select the correct variant for
 * the list depending on the constness of the `vlc_list` list, and it
 * can allow only iteration if the list is const.
 *
 * Iteration includes standard iterators and for range-based loops, as well
 * as reversed list.
 *
 * \code
 * for (auto it = list.begin(); it != list.end(); ++it)
 * {
 *     // (*it) is of type `struct item`
 * }
 *
 * for (auto &elem : vlc::from(&c_list, &item::node))
 * {
 *     // `elem` type is `struct item`
 *     // ...
 * }
 *
 * for (auto &elem : vlc::from(&c_list, &item::node).as_reverse())
 * {
 *     // `elem` type is `struct item`
 *     // ...
 * }
 * \endcode
 *
 */

/**
 * Compare two vlc_list node and check whether they represent the same element.
 * If the element is not in a list itself, or is not a list itself, then the
 * result is undefined.
 *
 * \param a some node belonging to a vlc_list
 * \param b some node belonging to a vlc_list
 * \return true if they represent the same element or the same list,
 *         false otherwise
 **/
bool operator==(const ::vlc_list& a, const ::vlc_list& b)
{
    return a.prev == b.prev && a.next == b.next;
}

/**
 * Compare two vlc_list node and check whether they representthe same element.
 * If the element is not in a list itself, or is not a list itself, then the
 * result is undefined.
 *
 * \param a some node belonging to a vlc_list
 * \param b some node belonging to a vlc_list
 * \return false if they represent the same element or the same list,
 *         true otherwise
 **/
bool operator!=(const ::vlc_list& a, const ::vlc_list& b)
{
    return !(a == b);
}

/**
 * Base class for iterators on the vlc::list's vlc_list wrapper.
 *
 * The base class c
 *
 * \tparam NodeType the type of each node from the list
 * \tparam ListType either vlc_list or const vlc_list
 */
template <typename NodeType, typename ListType>
class list_iterator_base {
protected:
    ListType* _current, *_next, *_prev;
    vlc_list NodeType::* _node_ptr;

    constexpr std::ptrdiff_t offset() const {
        return reinterpret_cast<std::ptrdiff_t>(
            &(static_cast<NodeType const volatile*>(NULL)->*_node_ptr)
        );
    }

    static constexpr bool is_const = std::is_const<ListType>::value;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::conditional_t<is_const, const NodeType, NodeType>;
    using pointer = std::conditional_t<is_const, const NodeType*, NodeType*>;
    using reference = std::conditional_t<is_const, const NodeType&, NodeType&>;
    using difference_type = std::ptrdiff_t;

    using iterator_type = list_iterator_base<NodeType, ListType>;

    list_iterator_base(ListType& list, vlc_list NodeType::* node_ptr)
        : _current{&list}, _next{list.next}, _prev{list.prev}, _node_ptr{node_ptr} {}

    reference operator*() const
    {
        using char_pointer = std::conditional_t<is_const, const char*, char*>;
        return *reinterpret_cast<pointer>(
                reinterpret_cast<char_pointer>(this->_current) - this->offset());
    }

    pointer operator->() const
    {
        return &operator*();
    }

    iterator_type operator++()
    {
        _prev = _next->prev;
        _current = _next;
        _next = _next->next;
        return *this;
    }

    iterator_type operator++(int)
    {
        return iterator_type {*_next, _node_ptr};
    }

    iterator_type& operator--()
    {
        _next = _prev->next;
        _current = _prev;
        _prev = _prev->prev;
        return *this;
    }

    iterator_type operator--(int)
    {
        return iterator_type {*_prev, _node_ptr};
    }

    friend bool operator==(const iterator_type& a, const iterator_type& b)
    {
        return a._current == b._current;
    }

    friend bool operator!=(const iterator_type& a, const iterator_type& b)
    {
        return a._current != b._current;
    }
};

/**
 * Iterator on vlc_list with mutable capabilities.
 */
template <typename NodeType>
using list_iterator = list_iterator_base<NodeType, vlc_list>;

/**
 * Iterator on vlc_list with immutable capabilities.
 */
template <typename NodeType>
using list_const_iterator = list_iterator_base<NodeType, const vlc_list>;

template <typename NodeType, typename ListType>
class list_reverse_iterator
    : public std::reverse_iterator<list_iterator_base<NodeType, ListType>>
{
    using iterator = std::reverse_iterator<list_iterator_base<NodeType, ListType>>;
    using inner_iterator = typename iterator::iterator_type;
public:
    list_reverse_iterator(ListType &list, vlc_list NodeType::* node_ptr)
        : iterator {++inner_iterator{list, node_ptr}} {}

    list_reverse_iterator(iterator other)
        : iterator{other.base()} {}
};

/**
 * Wrapper around any type matching with vlc_list, exposing C++ iterator operations.
 *
 * Users should use the vlc::list and vlc::const_list types instead,
 * and initialize them from the vlc::from function.
 *
 * \code
 * struct item {
 *     // ...
 *     vlc_list node;
 * }
 *
 * struct vlc_list c_list;
 * vlc_list_init(&c_list);
 * // ...
 * auto list = vlc::from(&list, &item::node);
 * \endcode
 *
 * \tparam NodeType      the type of each node from the list
 * \tparam ListType      either vlc_list or const vlc_list
 * \tparam Iterator      the iterator type returned by vlc::list_base::begin()
 *                       and vlc::list_base::end()
 * \tparam ConstIterator the iterator type returned by vlc::list_base::cbegin()
 *                       and vlc::list_base::cend()
 */
template <
    typename NodeType,
    typename ListType,
    typename Iterator,
    typename ConstIterator
>
class list_base
{
    /* We use some kind of offsetof, which will only be valid on
     * standard layout types since non-standard might have a variable
     * layout and still be pointer-compatible. */
    static_assert(std::is_standard_layout<NodeType>::value,
                  "list can only iterate standard layout types");

protected:
    ListType& _list;
    vlc_list NodeType::* _node_ptr;

    static bool constexpr is_reverse = !std::is_same<
        Iterator, list_reverse_iterator<NodeType, ListType>>::value;

public:
    list_base(ListType &list, vlc_list NodeType::* node_ptr)
        : _list{list}, _node_ptr{node_ptr} {}

    using list_type = list_base<NodeType, ListType, Iterator, ConstIterator>;

    using iterator = Iterator;
    using const_iterator = ConstIterator;
    using reverse_iterator = std::conditional_t<is_reverse,
        list_reverse_iterator<NodeType, ListType>,
        list_iterator_base<NodeType, ListType>>;
    using const_reverse_iterator = std::conditional_t<is_reverse,
        list_reverse_iterator<NodeType, const ListType>,
        list_iterator_base<NodeType, const ListType>>;

    using reverse_list = list_base<NodeType, ListType,
                                   reverse_iterator, const_reverse_iterator>;

    reverse_list as_reverse()
    {
        return reverse_list{_list, _node_ptr};
    }

    iterator begin() const
    {
        return ++iterator{_list, _node_ptr};
    }

    iterator end() const
    {
        return iterator{_list, _node_ptr};
    }

    const_iterator cbegin() const
    {
        return ++const_iterator{_list, _node_ptr};
    }

    const_iterator cend() const
    {
        return const_iterator{_list, _node_ptr};
    }

    reverse_iterator rbegin()
    {
        return ++reverse_iterator{_list, _node_ptr};
    }

    reverse_iterator rend()
    {
        return reverse_iterator{_list, _node_ptr};
    }

    const_reverse_iterator crbegin() const
    {
        return ++const_reverse_iterator{_list, _node_ptr};
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator{_list, _node_ptr};
    }

    friend bool operator==(const list_type& a, const list_type& b)
    {
        return a._list == b._list;
    }

    friend bool operator!=(const list_type& a, const list_type& b)
    {
        return a._list != b._list;
    }

    bool empty() const
    {
        return vlc_list_is_empty(_list);
    }
};

/**
 * Public type-safe wrapper around const vlc_list, providing const iterator
 * and iteration functions.
 *
 * It is advised to use ::vlc::list::from() to get the correct
 * wrapper directly in an inferenced way.
 *
 * \tparam NodeType the type of each node from the list
 **/
template <typename NodeType>
struct const_list : public list_base<
    NodeType,
    const vlc_list,
    list_const_iterator<NodeType>,
    list_const_iterator<NodeType>
>{
public:
    using iterator = ::vlc::list_const_iterator<NodeType>;
    using const_iterator = ::vlc::list_const_iterator<NodeType>;

    const_list(const vlc_list &l, vlc_list NodeType::* node_ptr)
        : list_base<
            NodeType, const vlc_list, iterator, const_iterator
        >(l, node_ptr) {};
};

/**
 * Public type-safe wrapper around mutable vlc_list, providing iterators,
 * iteration functions and mutation on the list itself.
 *
 * \tparam NodeType the type of each node from the list
 **/
template <typename NodeType>
struct list : public list_base<
    NodeType,
    vlc_list,
    list_iterator<NodeType>,
    list_const_iterator<NodeType>
>{

public:
    using iterator = ::vlc::list_iterator<NodeType>;
    using const_iterator = ::vlc::list_const_iterator<NodeType>;

    /**
     * Construct a ::vlc::list from an existing vlc_list.
     *
     * It is advised to use ::vlc::list::from() to get the correct
     * wrapper directly in an inferenced way.
     **/
    list(vlc_list &l, vlc_list NodeType::* node_ptr)
        : list_base<
            NodeType, vlc_list, iterator, const_iterator
        >(l, node_ptr) {};

    template <typename IteratorType>
    IteratorType erase(IteratorType it)
    {
        vlc_list_remove(&((*it).*(this->_node_ptr)));
        return it;
    }

    void push_front(NodeType &item)
    {
        struct vlc_list *node = &(item.*(this->_node_ptr));
        vlc_list_prepend(node, &this->_list);
    }

    void push_back(NodeType &item)
    {
        struct vlc_list *node = &(item.*(this->_node_ptr));
        vlc_list_append(node, &this->_list);
    }
};

/**
 * Construct a vlc::list (mutable list) object from a mutable
 * vlc_list reference
 *
 * \tparam NodeType      the type of each node from the list
 *
 * \param list the vlc_list object to wrap around
 * \param node_ptr a pointer to the intrusive vlc_list member from the
 *                 type being stored in the list.
 * \return a vlc::list instance
 * */
template <typename NodeType>
::vlc::list<NodeType> from(vlc_list &list, vlc_list NodeType::* node_ptr)
{
    return ::vlc::list<NodeType>{list, node_ptr};
}

/**
 * Construct a vlc::const_list (immutable list) object from a const
 * vlc_list reference
 *
 * \tparam NodeType      the type of each node from the list
 *
 * \param list the vlc_list object to wrap around
 * \param node_ptr a pointer to the intrusive vlc_list member from the
 *                 type being stored in the list.
 * \return a vlc::const_list instance which cannot modify the vlc_list
 * */
template <typename NodeType>
::vlc::const_list<NodeType> from(const vlc_list &list, vlc_list NodeType::* node_ptr)
{
    return ::vlc::const_list<NodeType>{list, node_ptr};
}

/** @} */
}

#endif /* VLC_LIST_HPP */
