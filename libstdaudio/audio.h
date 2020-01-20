// libstdaudio
//{{{
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
//}}}
#pragma once
//{{{  includes
#define NOMINMAX
#include <optional>
#include <cassert>
#include <chrono>
#include <cctype>
#include <codecvt>
#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include <thread>
#include <forward_list>
#include <atomic>
#include <string_view>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <variant>
#include <array>
//}}}

//{{{  span.h - TODO: this is a temporary measure until std::span becomes available
/*
This is an implementation of std::span from P0122R7
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0122r7.pdf
*/
//          Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <cstddef>
#include <type_traits>

#ifndef TCB_SPAN_NO_EXCEPTIONS
// Attempt to discover whether we're being compiled with exception support
#if !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
#define TCB_SPAN_NO_EXCEPTIONS
#endif
#endif

#ifndef TCB_SPAN_NO_EXCEPTIONS
#include <cstdio>
#include <stdexcept>
#endif

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
#define TCB_SPAN_NAMESPACE_NAME tcb
#endif

#ifdef TCB_SPAN_STD_COMPLIANT_MODE
#define TCB_SPAN_NO_DEPRECATION_WARNINGS
#endif

#ifndef TCB_SPAN_NO_DEPRECATION_WARNINGS
#define TCB_SPAN_DEPRECATED_FOR(msg) [[deprecated(msg)]]
#else
#define TCB_SPAN_DEPRECATED_FOR(msg)
#endif

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define TCB_SPAN_HAVE_CPP17
#endif

#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
#define TCB_SPAN_HAVE_CPP14
#endif

namespace TCB_SPAN_NAMESPACE_NAME {

// Establish default contract checking behavior
#if !defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION) &&                          \
    !defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION) &&                      \
    !defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#if defined(NDEBUG) || !defined(TCB_SPAN_HAVE_CPP14)
#define TCB_SPAN_NO_CONTRACT_CHECKING
#else
#define TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION
#endif
#endif

#if defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION)
struct contract_violation_error : std::logic_error {
    explicit contract_violation_error(const char* msg) : std::logic_error(msg)
    {}
};

inline void contract_violation(const char* msg)
{
    throw contract_violation_error(msg);
}

#elif defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION)
[[noreturn]] inline void contract_violation(const char* /*unused*/)
{
    std::terminate();
}
#endif

#if !defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#define TCB_SPAN_STRINGIFY(cond) #cond
#define TCB_SPAN_EXPECT(cond)                                                  \
    cond ? (void) 0 : contract_violation("Expected " TCB_SPAN_STRINGIFY(cond))
#else
#define TCB_SPAN_EXPECT(cond)
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_inline_variables)
#define TCB_SPAN_INLINE_VAR inline
#else
#define TCB_SPAN_INLINE_VAR
#endif

#if defined(TCB_SPAN_HAVE_CPP14) ||                                                 \
    (defined(__cpp_constexpr) && __cpp_constexpr >= 201304)
#define TCB_SPAN_CONSTEXPR14 constexpr
#else
#define TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#define TCB_SPAN_CONSTEXPR11 constexpr
#else
#define TCB_SPAN_CONSTEXPR11 TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_deduction_guides)
#define TCB_SPAN_HAVE_DEDUCTION_GUIDES
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_byte)
#define TCB_SPAN_HAVE_STD_BYTE
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_array_constexpr)
#define TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC
#endif

#if defined(TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC)
#define TCB_SPAN_ARRAY_CONSTEXPR constexpr
#else
#define TCB_SPAN_ARRAY_CONSTEXPR
#endif

#ifdef TCB_SPAN_HAVE_STD_BYTE
using byte = std::byte;
#else
using byte = unsigned char;
#endif

TCB_SPAN_INLINE_VAR constexpr std::ptrdiff_t dynamic_extent = -1;

template <typename ElementType, std::ptrdiff_t Extent = dynamic_extent>
class span;

namespace detail {

template <typename E, std::ptrdiff_t S>
struct span_storage {
    constexpr span_storage() noexcept = default;

    constexpr span_storage(E* ptr, std::ptrdiff_t /*unused*/) noexcept
        : ptr(ptr)
    {}

    E* ptr = nullptr;
    static constexpr std::ptrdiff_t size = S;
};

template <typename E>
struct span_storage<E, dynamic_extent> {
    constexpr span_storage() noexcept = default;

    constexpr span_storage(E* ptr, std::ptrdiff_t size) noexcept
        : ptr(ptr), size(size)
    {}

    E* ptr = nullptr;
    std::ptrdiff_t size = 0;
};

// Reimplementation of C++17 std::size() and std::data()
#if defined(TCB_SPAN_HAVE_CPP17) ||                                            \
    defined(__cpp_lib_nonmember_container_access)
using std::data;
using std::size;
#else
template <class C>
constexpr auto size(const C& c) -> decltype(c.size())
{
    return c.size();
}

template <class T, std::size_t N>
constexpr std::size_t size(const T (&)[N]) noexcept
{
    return N;
}

template <class C>
constexpr auto data(C& c) -> decltype(c.data())
{
    return c.data();
}

template <class C>
constexpr auto data(const C& c) -> decltype(c.data())
{
    return c.data();
}

template <class T, std::size_t N>
constexpr T* data(T (&array)[N]) noexcept
{
    return array;
}

template <class E>
constexpr const E* data(std::initializer_list<E> il) noexcept
{
    return il.begin();
}
#endif // TCB_SPAN_HAVE_CPP17

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_void_t)
using std::void_t;
#else
template <typename...>
using void_t = void;
#endif

template <typename T>
using uncvref_t =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template <typename>
struct is_span : std::false_type {};

template <typename T, std::ptrdiff_t S>
struct is_span<span<T, S>> : std::true_type {};

template <typename>
struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename, typename = void>
struct has_size_and_data : std::false_type {};

template <typename T>
struct has_size_and_data<T, void_t<decltype(detail::size(std::declval<T>())),
                                   decltype(detail::data(std::declval<T>()))>>
    : std::true_type {};

template <typename C, typename U = uncvref_t<C>>
struct is_container {
    static constexpr bool value =
        !is_span<U>::value && !is_std_array<U>::value &&
        !std::is_array<U>::value && has_size_and_data<C>::value;
};

template <typename T>
using remove_pointer_t = typename std::remove_pointer<T>::type;

template <typename, typename, typename = void>
struct is_container_element_type_compatible : std::false_type {};

template <typename T, typename E>
struct is_container_element_type_compatible<
    T, E, void_t<decltype(detail::data(std::declval<T>()))>>
    : std::is_convertible<
          remove_pointer_t<decltype(detail::data(std::declval<T>()))> (*)[],
          E (*)[]> {};

template <typename, typename = size_t>
struct is_complete : std::false_type {};

template <typename T>
struct is_complete<T, decltype(sizeof(T))> : std::true_type {};

} // namespace detail

template <typename ElementType, std::ptrdiff_t Extent>
class span {
    static_assert(Extent == dynamic_extent || Extent >= 0,
                  "A span must have an extent greater than or equal to zero, "
                  "or a dynamic extent");
    static_assert(std::is_object<ElementType>::value,
                  "A span's ElementType must be an object type (not a "
                  "reference type or void)");
    static_assert(detail::is_complete<ElementType>::value,
                  "A span's ElementType must be a complete type (not a forward "
                  "declaration)");
    static_assert(!std::is_abstract<ElementType>::value,
                  "A span's ElementType cannot be an abstract class type");

    using storage_type = detail::span_storage<ElementType, Extent>;

public:
    // constants and types
    using element_type = ElementType;
    using value_type = typename std::remove_cv<ElementType>::type;
    using index_type = std::ptrdiff_t;
    using difference_type = std::ptrdiff_t;
    using pointer = ElementType*;
    using reference = ElementType&;
    using iterator = pointer;
    using const_iterator = const ElementType*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr index_type extent = Extent;

    // [span.cons], span constructors, copy, assignment, and destructor
    template <std::ptrdiff_t E = Extent,
              typename std::enable_if<E <= 0, int>::type = 0>
    constexpr span() noexcept
    {}

    TCB_SPAN_CONSTEXPR11 span(pointer ptr, index_type count)
        : storage_(ptr, count)
    {
        TCB_SPAN_EXPECT(extent == dynamic_extent || count == extent);
    }

    TCB_SPAN_CONSTEXPR11 span(pointer first_elem, pointer last_elem)
        : storage_(first_elem, last_elem - first_elem)
    {
        TCB_SPAN_EXPECT(extent == dynamic_extent ||
                        last_elem - first_elem == extent);
    }

    template <
        std::size_t N, std::ptrdiff_t E = Extent,
        typename std::enable_if<
            (E == dynamic_extent || static_cast<std::ptrdiff_t>(N) == E) &&
                detail::is_container_element_type_compatible<
                    element_type (&)[N], ElementType>::value,
            int>::type = 0>
    constexpr span(element_type (&arr)[N]) noexcept : storage_(arr, N)
    {}

    template <
        std::size_t N, std::ptrdiff_t E = Extent,
        typename std::enable_if<
            (E == dynamic_extent || static_cast<std::ptrdiff_t>(N) == E) &&
                detail::is_container_element_type_compatible<
                    std::array<value_type, N>&, ElementType>::value,
            int>::type = 0>
    TCB_SPAN_ARRAY_CONSTEXPR span(std::array<value_type, N>& arr) noexcept
        : storage_(arr.data(), N)
    {}

    template <
        std::size_t N, std::ptrdiff_t E = Extent,
        typename std::enable_if<
            (E == dynamic_extent || static_cast<std::ptrdiff_t>(N) == E) &&
                detail::is_container_element_type_compatible<
                    const std::array<value_type, N>&, ElementType>::value,
            int>::type = 0>
    TCB_SPAN_ARRAY_CONSTEXPR span(const std::array<value_type, N>& arr) noexcept
        : storage_(arr.data(), N)
    {}

    template <typename Container,
              typename std::enable_if<
                  detail::is_container<Container>::value &&
                      detail::is_container_element_type_compatible<
                          Container&, ElementType>::value,
                  int>::type = 0>
    TCB_SPAN_CONSTEXPR11 span(Container& cont)
        : storage_(detail::data(cont), detail::size(cont))
    {
        TCB_SPAN_EXPECT(extent == dynamic_extent ||
                        static_cast<std::ptrdiff_t>(detail::size(cont)) ==
                            extent);
    }

    template <typename Container,
              typename std::enable_if<
                  detail::is_container<Container>::value &&
                      detail::is_container_element_type_compatible<
                          const Container&, ElementType>::value,
                  int>::type = 0>
    TCB_SPAN_CONSTEXPR11 span(const Container& cont)
        : storage_(detail::data(cont), detail::size(cont))
    {
        TCB_SPAN_EXPECT(extent == dynamic_extent ||
                        static_cast<std::ptrdiff_t>(detail::size(cont)) ==
                            extent);
    }

    constexpr span(const span& other) noexcept = default;

    template <typename OtherElementType, std::ptrdiff_t OtherExtent,
              typename std::enable_if<
                  (Extent == OtherExtent || Extent == dynamic_extent) &&
                      std::is_convertible<OtherElementType (*)[],
                                          ElementType (*)[]>::value,
                  int>::type = 0>
    constexpr span(const span<OtherElementType, OtherExtent>& other) noexcept
        : storage_(other.data(), other.size())
    {}

    ~span() noexcept = default;

    TCB_SPAN_CONSTEXPR14 span& operator=(const span& other) noexcept = default;

    // [span.sub], span subviews
    template <std::ptrdiff_t Count>
    TCB_SPAN_CONSTEXPR11 span<element_type, Count> first() const
    {
        TCB_SPAN_EXPECT(Count >= 0 && Count <= size());
        return {data(), Count};
    }

    template <std::ptrdiff_t Count>
    TCB_SPAN_CONSTEXPR11 span<element_type, Count> last() const
    {
        TCB_SPAN_EXPECT(Count >= 0 && Count <= size());
        return {data() + (size() - Count), Count};
    }

    template <std::ptrdiff_t Offset, std::ptrdiff_t Count = dynamic_extent>
    using subspan_return_t =
        span<ElementType, Count != dynamic_extent
                              ? Count
                              : (Extent != dynamic_extent ? Extent - Offset
                                                          : dynamic_extent)>;

    template <std::ptrdiff_t Offset, std::ptrdiff_t Count = dynamic_extent>
    TCB_SPAN_CONSTEXPR11 subspan_return_t<Offset, Count> subspan() const
    {
        TCB_SPAN_EXPECT((Offset >= 0 && Offset <= size()) &&
                        (Count == dynamic_extent ||
                         (Count >= 0 && Offset + Count <= size())));
        return {data() + Offset,
                Count != dynamic_extent
                    ? Count
                    : (Extent != dynamic_extent ? Extent - Offset
                                                : size() - Offset)};
    }

    TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent>
    first(index_type count) const
    {
        TCB_SPAN_EXPECT(count >= 0 && count <= size());
        return {data(), count};
    }

    TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent>
    last(index_type count) const
    {
        TCB_SPAN_EXPECT(count >= 0 && count <= size());
        return {data() + (size() - count), count};
    }

    TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent>
    subspan(index_type offset, index_type count = dynamic_extent) const
    {
        TCB_SPAN_EXPECT((offset >= 0 && offset <= size()) &&
                        (count == dynamic_extent ||
                         (count >= 0 && offset + count <= size())));
        return {data() + offset,
                count == dynamic_extent ? size() - offset : count};
    }

    // [span.obs], span observers
    constexpr index_type size() const noexcept { return storage_.size; }

    constexpr index_type size_bytes() const noexcept
    {
        return size() * sizeof(element_type);
    }

    constexpr bool empty() const noexcept { return size() == 0; }

    // [span.elem], span element access
    TCB_SPAN_CONSTEXPR11 reference operator[](index_type idx) const
    {
        TCB_SPAN_EXPECT(idx >= 0 && idx < size());
        return *(data() + idx);
    }

    /* Extension: not in P0122 */
#ifndef TCB_SPAN_STD_COMPLIANT_MODE
    TCB_SPAN_CONSTEXPR14 reference at(index_type idx) const
    {
#ifndef TCB_SPAN_NO_EXCEPTIONS
        if (idx < 0 || idx >= size()) {
            char msgbuf[64] = {
                0,
            };
            std::snprintf(msgbuf, sizeof(msgbuf),
                          "Index %td is out of range for span of size %td", idx,
                          size());
            throw std::out_of_range{msgbuf};
        }
#endif // TCB_SPAN_NO_EXCEPTIONS
        return this->operator[](idx);
    }

    TCB_SPAN_CONSTEXPR11 reference front() const
    {
        TCB_SPAN_EXPECT(!empty());
        return *data();
    }

    TCB_SPAN_CONSTEXPR11 reference back() const
    {
        TCB_SPAN_EXPECT(!empty());
        return *(data() + (size() - 1));
    }

#endif // TCB_SPAN_STD_COMPLIANT_MODE

#ifndef TCB_SPAN_NO_FUNCTION_CALL_OPERATOR
    TCB_SPAN_DEPRECATED_FOR("Use operator[] instead")
    constexpr reference operator()(index_type idx) const
    {
        return this->operator[](idx);
    }
#endif // TCB_SPAN_NO_FUNCTION_CALL_OPERATOR

    constexpr pointer data() const noexcept { return storage_.ptr; }

    // [span.iterators], span iterator support
    constexpr iterator begin() const noexcept { return data(); }

    constexpr iterator end() const noexcept { return data() + size(); }

    constexpr const_iterator cbegin() const noexcept { return begin(); }

    constexpr const_iterator cend() const noexcept { return end(); }

    TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }

    TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    TCB_SPAN_ARRAY_CONSTEXPR const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    TCB_SPAN_ARRAY_CONSTEXPR const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

private:
    storage_type storage_{};
};

#ifdef TCB_SPAN_HAVE_DEDUCTION_GUIDES

/* Deduction Guides */
template <class T, size_t N>
span(T (&)[N])->span<T, N>;

template <class T, size_t N>
span(std::array<T, N>&)->span<T, N>;

template <class T, size_t N>
span(const std::array<T, N>&)->span<const T, N>;

template <class Container>
span(Container&)->span<typename Container::value_type>;

template <class Container>
span(const Container&)->span<const typename Container::value_type>;

#endif // TCB_HAVE_DEDUCTION_GUIDES

template <typename ElementType, std::ptrdiff_t Extent>
constexpr span<ElementType, Extent>
make_span(span<ElementType, Extent> s) noexcept
{
    return s;
}

template <typename T, std::size_t N>
constexpr span<T, N> make_span(T (&arr)[N]) noexcept
{
    return {arr};
}

template <typename T, std::size_t N>
TCB_SPAN_ARRAY_CONSTEXPR span<T, N> make_span(std::array<T, N>& arr) noexcept
{
    return {arr};
}

template <typename T, std::size_t N>
TCB_SPAN_ARRAY_CONSTEXPR span<const T, N>
make_span(const std::array<T, N>& arr) noexcept
{
    return {arr};
}

template <typename Container>
constexpr span<typename Container::value_type> make_span(Container& cont)
{
    return {cont};
}

template <typename Container>
constexpr span<const typename Container::value_type>
make_span(const Container& cont)
{
    return {cont};
}

/* Comparison operators */
// Implementation note: the implementations of == and < are equivalent to
// 4-legged std::equal and std::lexicographical_compare respectively

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator==(span<T, X> lhs, span<U, Y> rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::ptrdiff_t i = 0; i < lhs.size(); i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }

    return true;
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator!=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(lhs == rhs);
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator<(span<T, X> lhs, span<U, Y> rhs)
{
    // No std::min to avoid dragging in <algorithm>
    const std::ptrdiff_t size =
        lhs.size() < rhs.size() ? lhs.size() : rhs.size();

    for (std::ptrdiff_t i = 0; i < size; i++) {
        if (lhs[i] < rhs[i]) {
            return true;
        }
        if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator<=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(rhs < lhs);
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator>(span<T, X> lhs, span<U, Y> rhs)
{
    return rhs < lhs;
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator>=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(lhs < rhs);
}

template <typename ElementType, std::ptrdiff_t Extent>
span<const byte, ((Extent == dynamic_extent)
                      ? dynamic_extent
                      : (static_cast<ptrdiff_t>(sizeof(ElementType)) * Extent))>
as_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
}

template <
    class ElementType, ptrdiff_t Extent,
    typename std::enable_if<!std::is_const<ElementType>::value, int>::type = 0>
span<byte, ((Extent == dynamic_extent)
                ? dynamic_extent
                : (static_cast<ptrdiff_t>(sizeof(ElementType)) * Extent))>
as_writable_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
}

/* Extension: nonmember subview operations */

#ifndef TCB_SPAN_STD_COMPLIANT_MODE

template <std::ptrdiff_t Count, typename T>
TCB_SPAN_CONSTEXPR11 auto first(T& t)
    -> decltype(make_span(t).template first<Count>())
{
    return make_span(t).template first<Count>();
}

template <std::ptrdiff_t Count, typename T>
TCB_SPAN_CONSTEXPR11 auto last(T& t)
    -> decltype(make_span(t).template last<Count>())
{
    return make_span(t).template last<Count>();
}

template <std::ptrdiff_t Offset, std::ptrdiff_t Count = dynamic_extent,
          typename T>
TCB_SPAN_CONSTEXPR11 auto subspan(T& t)
    -> decltype(make_span(t).template subspan<Offset, Count>())
{
    return make_span(t).template subspan<Offset, Count>();
}

template <typename T>
TCB_SPAN_CONSTEXPR11 auto first(T& t, std::ptrdiff_t count)
    -> decltype(make_span(t).first(count))
{
    return make_span(t).first(count);
}

template <typename T>
TCB_SPAN_CONSTEXPR11 auto last(T& t, std::ptrdiff_t count)
    -> decltype(make_span(t).last(count))
{
    return make_span(t).last(count);
}

template <typename T>
TCB_SPAN_CONSTEXPR11 auto subspan(T& t, std::ptrdiff_t offset,
                                  std::ptrdiff_t count = dynamic_extent)
    -> decltype(make_span(t).subspan(offset, count))
{
    return make_span(t).subspan(offset, count);
}

#endif // TCB_SPAN_STD_COMPLIANT_MODE

} // namespace TCB_SPAN_NAMESPACE_NAME

/* Extension: support for C++17 structured bindings */

#ifndef TCB_SPAN_STD_COMPLIANT_MODE

namespace TCB_SPAN_NAMESPACE_NAME {

template <std::ptrdiff_t N, typename E, std::ptrdiff_t S>
constexpr auto get(span<E, S> s) -> decltype(s[N])
{
    return s[N];
}

} // namespace TCB_SPAN_NAMESPACE_NAME

namespace std {

template <typename E, ptrdiff_t S>
class tuple_size<tcb::span<E, S>> : public integral_constant<size_t, S> {};

template <typename E>
class tuple_size<tcb::span<E, tcb::dynamic_extent>>; // not defined

template <size_t N, typename E, ptrdiff_t S>
class tuple_element<N, tcb::span<E, S>> {
public:
    using type = E;
};

} // end namespace std

#endif // TCB_SPAN_STD_COMPLIANT_MODE
//}}}
template <typename ElementType, std::ptrdiff_t Extent = TCB_SPAN_NAMESPACE_NAME::dynamic_extent>
using span = TCB_SPAN_NAMESPACE_NAME::span<ElementType, Extent>;

// include "audio_buffer.h"
struct contiguous_interleaved_t{};
inline constexpr contiguous_interleaved_t contiguous_interleaved;

struct contiguous_deinterleaved_t{};
inline constexpr contiguous_deinterleaved_t contiguous_deinterleaved;

struct ptr_to_ptr_deinterleaved_t{};
inline constexpr ptr_to_ptr_deinterleaved_t ptr_to_ptr_deinterleaved;

//{{{
template <typename _SampleType> class audio_buffer {
public:
  using sample_type = _SampleType;
  using index_type = size_t;

  //{{{
  audio_buffer (sample_type* data, index_type num_frames, index_type num_channels, contiguous_interleaved_t)
    : _num_frames(num_frames),
      _num_channels(num_channels),
      _stride(_num_channels),
      _is_contiguous(true) {
    assert (num_channels <= _max_num_channels);
    for (auto i = 0; i < _num_channels; ++i) {
      _channels[i] = data + i;
    }
  }
  //}}}
  //{{{
  audio_buffer (sample_type* data, index_type num_frames, index_type num_channels, contiguous_deinterleaved_t)
      : _num_frames(num_frames),
        _num_channels(num_channels),
        _stride(1),
        _is_contiguous(true) {
    assert (num_channels <= _max_num_channels);
    for (auto i = 0; i < _num_channels; ++i) {
      _channels[i] = data + (i * _num_frames);
    }
  }
  //}}}
  //{{{
  audio_buffer (sample_type** data, index_type num_frames, index_type num_channels, ptr_to_ptr_deinterleaved_t)
      : _num_frames(num_frames),
        _num_channels(num_channels),
        _stride(1),
        _is_contiguous(false) {
    assert (num_channels <= _max_num_channels);
    copy (data, data + _num_channels, _channels.begin());
  }
  //}}}

  //{{{
  sample_type* data() const noexcept {
    return _is_contiguous ? _channels[0] : nullptr;
  }
  //}}}

  //{{{
  bool is_contiguous() const noexcept {
    return _is_contiguous;
  }
  //}}}
  //{{{
  bool frames_are_contiguous() const noexcept {
    return _stride == _num_channels;
  }
  //}}}
  //{{{
  bool channels_are_contiguous() const noexcept {
    return _stride == 1;
  }
  //}}}

  //{{{
  index_type size_frames() const noexcept {
    return _num_frames;
  }
  //}}}
  //{{{
  index_type size_channels() const noexcept {
    return _num_channels;
  }
  //}}}
  //{{{
  index_type size_samples() const noexcept {
    return _num_channels * _num_frames;
  }
  //}}}

  //{{{
  sample_type& operator() (index_type frame, index_type channel) noexcept {
    return const_cast<sample_type&>(std::as_const(*this).operator()(frame, channel));
  }
  //}}}
  //{{{
  const sample_type& operator() (index_type frame, index_type channel) const noexcept {
    return _channels[channel][frame * _stride];
  }
  //}}}

private:
  bool _is_contiguous = false;
  index_type _num_frames = 0;
  index_type _num_channels = 0;
  index_type _stride = 0;
  constexpr static size_t _max_num_channels = 16;
  std::array<sample_type*, _max_num_channels> _channels = {};
  };
//}}}

// TODO: this is currently macOS specific!
using audio_clock_t = std::chrono::steady_clock;
//{{{
template <typename _SampleType> struct audio_device_io {

  std::optional<audio_buffer<_SampleType>> input_buffer;
  std::optional<std::chrono::time_point<audio_clock_t>> input_time;

  std::optional<audio_buffer<_SampleType>> output_buffer;
  std::optional<std::chrono::time_point<audio_clock_t>> output_time;
  };
//}}}

//{{{
class __wasapi_util {
public:
  //{{{
  static const CLSID& get_MMDeviceEnumerator_classid()
  {
    static const CLSID MMDeviceEnumerator_class_id = __uuidof(MMDeviceEnumerator);
    return MMDeviceEnumerator_class_id;
  }
  //}}}
  //{{{
  static const IID& get_IMMDeviceEnumerator_interface_id()
  {
    static const IID IMMDeviceEnumerator_interface_id = __uuidof(IMMDeviceEnumerator);
    return IMMDeviceEnumerator_interface_id;
  }
  //}}}
  //{{{
  static const IID& get_IAudioClient_interface_id()
  {
    static const IID IAudioClient_interface_id = __uuidof(IAudioClient);
    return IAudioClient_interface_id;
  }
  //}}}
  //{{{
  static const IID& get_IAudioRenderClient_interface_id()
  {
    static const IID IAudioRenderClient_interface_id = __uuidof(IAudioRenderClient);
    return IAudioRenderClient_interface_id;
  }
  //}}}
  //{{{
  static const IID& get_IAudioCaptureClient_interface_id()
  {
    static const IID IAudioCaptureClient_interface_id = __uuidof(IAudioCaptureClient);
    return IAudioCaptureClient_interface_id;
  }
  //}}}

  //{{{
  class com_initializer {
  public:
    com_initializer() : _hr(CoInitialize(nullptr)) { }

    ~com_initializer() { if (SUCCEEDED(_hr)) CoUninitialize(); }

    operator HRESULT() const { return _hr; }

    HRESULT _hr;
    };
  //}}}
  //{{{
  template<typename T> class auto_release {
  public:
    auto_release(T*& value) : _value(value) {}

    ~auto_release() {
      if (_value != nullptr)
        _value->Release();
      }

  private:
    T*& _value;
    };
  //}}}

  //{{{
  static std::string convert_string (const wchar_t* wide_string)
  {
    int required_characters = WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, nullptr, 0, nullptr, nullptr);
    if (required_characters <= 0)
      return {};

    std::string output;
    output.resize(static_cast<size_t>(required_characters));
    WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, output.data(), static_cast<int>(output.size()), nullptr, nullptr);
    return output;
  }
  //}}}
  //{{{
  static std::string convert_string (const std::wstring& input)
  {
    int required_characters = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (required_characters <= 0)
      return {};

    std::string output;
    output.resize(static_cast<size_t>(required_characters));
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), output.data(), static_cast<int>(output.size()), nullptr, nullptr);
    return output;
  }
  //}}}
};
//}}}
//{{{
struct audio_device_exception : public std::runtime_error {
  explicit audio_device_exception (const char* what) : runtime_error(what) { }
  };
//}}}

//{{{
class audio_device {
public:
  audio_device() = delete;
  audio_device(const audio_device&) = delete;
  audio_device& operator=(const audio_device&) = delete;

  //{{{
  audio_device (audio_device&& other) :
    _device(other._device),
    _audio_client(other._audio_client),
    _audio_capture_client(other._audio_capture_client),
    _audio_render_client(other._audio_render_client),
    _event_handle(other._event_handle),
    _device_id(std::move(other._device_id)),
    _running(other._running.load()),
    _name(std::move(other._name)),
    _mix_format(other._mix_format),
    _processing_thread(std::move(other._processing_thread)),
    _buffer_frame_count(other._buffer_frame_count),
    _is_render_device(other._is_render_device),
    _stop_callback(std::move(other._stop_callback)),
    _user_callback(std::move(other._user_callback))
  {
    other._device = nullptr;
    other._audio_client = nullptr;
    other._audio_capture_client = nullptr;
    other._audio_render_client = nullptr;
    other._event_handle = nullptr;
  }
  //}}}
  //{{{
  audio_device& operator = (audio_device&& other) noexcept {

    if (this == &other)
      return *this;

    _device = other._device;
    _audio_client = other._audio_client;
    _audio_capture_client = other._audio_capture_client;
    _audio_render_client = other._audio_render_client;
    _event_handle = other._event_handle;
    _device_id = std::move(other._device_id);
    _running = other._running.load();
    _name = std::move(other._name);
    _mix_format = other._mix_format;
    _processing_thread = std::move(other._processing_thread);
    _buffer_frame_count = other._buffer_frame_count;
    _is_render_device = other._is_render_device;
    _stop_callback = std::move(other._stop_callback);
    _user_callback = std::move(other._user_callback);

    other._device = nullptr;
    other._audio_client = nullptr;
    other._audio_capture_client = nullptr;
    other._audio_render_client = nullptr;
    other._event_handle = nullptr;
  }
  //}}}
  //{{{
  ~audio_device() {

    stop();

    if (_audio_capture_client != nullptr)
      _audio_capture_client->Release();

    if (_audio_render_client != nullptr)
      _audio_render_client->Release();

    if (_audio_client != nullptr)
      _audio_client->Release();

    if (_device != nullptr)
      _device->Release();
    }
  //}}}

  //{{{
  std::string_view name() const noexcept
  {
    return _name;
  }
  //}}}

  using device_id_t = std::wstring;
  //{{{
  device_id_t device_id() const noexcept {
    return _device_id;
    }
  //}}}

  //{{{
  bool is_input() const noexcept
  {
    return _is_render_device == false;
  }
  //}}}
  //{{{
  bool is_output() const noexcept
  {
    return _is_render_device == true;
  }
  //}}}

  //{{{
  int get_num_input_channels() const noexcept {

    if (is_input() == false)
      return 0;

    return _mix_format.Format.nChannels;
    }
  //}}}
  //{{{
  int get_num_output_channels() const noexcept {

    if (is_output() == false)
      return 0;

    return _mix_format.Format.nChannels;
    }
  //}}}

  using sample_rate_t = DWORD;
  //{{{
  sample_rate_t get_sample_rate() const noexcept
  {
    return _mix_format.Format.nSamplesPerSec;
  }
  //}}}
  //{{{
  bool set_sample_rate(sample_rate_t new_sample_rate)
  {
    _mix_format.Format.nSamplesPerSec = new_sample_rate;
    _fixup_mix_format();
    return true;
  }
  //}}}

  using buffer_size_t = UINT32;
  //{{{
  buffer_size_t get_buffer_size_frames() const noexcept
  {
    return _buffer_frame_count;
  }
  //}}}
  //{{{
  bool set_buffer_size_frames (buffer_size_t new_buffer_size)
  {
    _buffer_frame_count = new_buffer_size;
    return true;
  }
  //}}}

  //{{{
  template <typename _SampleType> constexpr bool supports_sample_type() const noexcept {

    return is_same_v<_SampleType, float> || is_same_v<_SampleType, int32_t> || is_same_v<_SampleType, int16_t>;
    }
  //}}}
  //{{{
  template <typename _SampleType> bool set_sample_type() {

    if (_is_connected() && !is_sample_type<_SampleType>())
      throw audio_device_exception("Cannot change sample type after connecting a callback.");

    return _set_sample_type_helper<_SampleType>();
    }
  //}}}
  //{{{
  template <typename _SampleType> bool is_sample_type() const {

    return _mix_format_matches_type<_SampleType>();
    }
  //}}}
  //{{{
  constexpr bool can_connect() const noexcept {
    return true;
    }
  //}}}
  //{{{
  constexpr bool can_process() const noexcept {

    return true;
    }
  //}}}
  //{{{  template float void connect (_CallbackType callback)
  template <typename _CallbackType,
    std::enable_if_t<std::is_nothrow_invocable_v<_CallbackType, audio_device&, audio_device_io<float>&>, int> = 0>
  void connect(_CallbackType callback)
  {
    _set_sample_type_helper<float>();
    _connect_helper(__wasapi_float_callback_t{callback});
  }
  //}}}
  //{{{  template int32_t void connect (_CallbackType callback
  template <typename _CallbackType,
    std::enable_if_t<std::is_nothrow_invocable_v<_CallbackType, audio_device&, audio_device_io<int32_t>&>, int> = 0>
  void connect(_CallbackType callback)
  {
    _set_sample_type_helper<int32_t>();
    _connect_helper(__wasapi_int32_callback_t{callback});
  }
  //}}}
  //{{{  template int16_t void connect (_CallbackType callback
  template <typename _CallbackType,
    std::enable_if_t<std::is_nothrow_invocable_v<_CallbackType, audio_device&, audio_device_io<int16_t>&>, int> = 0>
  void connect(_CallbackType callback)
  {
    _set_sample_type_helper<int16_t>();
    _connect_helper(__wasapi_int16_callback_t{ callback });
  }
  //}}}

  // TODO: remove std::function as soon as C++20 default-ctable lambda and lambda in unevaluated contexts become available
  using no_op_t = std::function<void(audio_device&)>;
  //{{{  template bool start (
  template <typename _StartCallbackType = no_op_t, typename _StopCallbackType = no_op_t,
    // TODO: is_nothrow_invocable_t does not compile, temporarily replaced with is_invocable_t
    typename = std::enable_if_t<std::is_invocable_v<_StartCallbackType, audio_device&> && std::is_invocable_v<_StopCallbackType, audio_device&>>>
  bool start(
    _StartCallbackType&& start_callback = [](audio_device&) noexcept {},
    _StopCallbackType&& stop_callback = [](audio_device&) noexcept {})
  {
    if (_audio_client == nullptr)
      return false;

    if (!_running)
    {
      _event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (_event_handle == nullptr)
        return false;

      REFERENCE_TIME periodicity = 0;

      const REFERENCE_TIME ref_times_per_second = 10'000'000;
      REFERENCE_TIME buffer_duration = (ref_times_per_second * _buffer_frame_count) / _mix_format.Format.nSamplesPerSec;
      HRESULT hr = _audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        periodicity,
        &_mix_format.Format,
        nullptr);

      // TODO: Deal with AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED return code by resetting the buffer_duration and retrying:
      // https://docs.microsoft.com/en-us/windows/desktop/api/audioclient/nf-audioclient-iaudioclient-initialize
      if (FAILED(hr))
        return false;

      /*HRESULT render_hr =*/ _audio_client->GetService(__wasapi_util::get_IAudioRenderClient_interface_id(), reinterpret_cast<void**>(&_audio_render_client));
      /*HRESULT capture_hr =*/ _audio_client->GetService(__wasapi_util::get_IAudioCaptureClient_interface_id(), reinterpret_cast<void**>(&_audio_capture_client));

      // TODO: Make sure to clean up more gracefully from errors
      hr = _audio_client->GetBufferSize(&_buffer_frame_count);
      if (FAILED(hr))
        return false;

      hr = _audio_client->SetEventHandle(_event_handle);
      if (FAILED(hr))
        return false;

      hr = _audio_client->Start();
      if (FAILED(hr))
        return false;

      _running = true;

      if (!_user_callback.valueless_by_exception())
      {
        _processing_thread = std::thread{ [this]()
        {
          SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

          while (_running)
          {
            visit([this](auto&& callback)
              {
                if (callback)
                {
                  process(callback);
                }
              },
              _user_callback);
            wait();
          }
        } };
      }

      start_callback(*this);
      _stop_callback = stop_callback;
    }

    return true;
  }
  //}}}
  //{{{
  bool stop()
  {
    if (_running)
    {
      _running = false;

      if (_processing_thread.joinable())
        _processing_thread.join();

      if (_audio_client != nullptr)
        _audio_client->Stop();
      if (_event_handle != nullptr)
      {
        CloseHandle(_event_handle);
      }
      _stop_callback(*this);
    }

    return true;
  }
  //}}}
  //{{{
  bool is_running() const noexcept
  {
    return _running;
  }
  //}}}
  //{{{
  void wait() const
  {
    WaitForSingleObject(_event_handle, INFINITE);
  }
  //}}}

  //{{{  template void float process (const _CallbackType& callback)
  template <typename _CallbackType,
    std::enable_if_t<std::is_invocable_v<_CallbackType, audio_device&, audio_device_io<float>&>, int> = 0>
  void process(const _CallbackType& callback)
  {
    if (!_mix_format_matches_type<float>())
      throw audio_device_exception("Attempting to process a callback for a sample type that does not match the configured sample type.");

    _process_helper<float>(callback);
  }
  //}}}
  //{{{  template void int32 process (const _CallbackType& callback
  template <typename _CallbackType,
    std::enable_if_t<std::is_invocable_v<_CallbackType, audio_device&, audio_device_io<int32_t>&>, int> = 0>
    void process(const _CallbackType& callback)
  {
    if (!_mix_format_matches_type<int32_t>())
      throw audio_device_exception("Attempting to process a callback for a sample type that does not match the configured sample type.");

    _process_helper<int32_t>(callback);
  }
  //}}}
  //{{{  template void int16_t process (const _CallbackType& callback
  template <typename _CallbackType,
    std::enable_if_t<std::is_invocable_v<_CallbackType, audio_device&, audio_device_io<int16_t>&>, int> = 0>
    void process(const _CallbackType& callback)
  {
    if (!_mix_format_matches_type<int16_t>())
      throw audio_device_exception("Attempting to process a callback for a sample type that does not match the configured sample type.");

    _process_helper<int16_t>(callback);
  }
  //}}}
  //{{{
  bool has_unprocessed_io() const noexcept {

    if (_audio_client == nullptr)
      return false;

    if (!_running)
      return false;

    UINT32 current_padding = 0;
    _audio_client->GetCurrentPadding(&current_padding);

    auto num_frames_available = _buffer_frame_count - current_padding;
    return num_frames_available > 0;
    }
  //}}}

private:
  friend class __audio_device_enumerator;
  //{{{
  audio_device (IMMDevice* device, bool is_render_device) :
    _device(device),
    _is_render_device(is_render_device)
  {
    // TODO: Handle errors better.  Maybe by throwing exceptions?
    if (_device == nullptr)
      throw audio_device_exception("IMMDevice is null.");

    _init_device_id_and_name();
    if (_device_id.empty())
      throw audio_device_exception("Could not get device id.");

    if (_name.empty())
      throw audio_device_exception("Could not get device name.");

    _init_audio_client();
    if (_audio_client == nullptr)
      return;

    _init_mix_format();
  }
  //}}}
  //{{{
  void _init_device_id_and_name() {

    LPWSTR device_id = nullptr;
    HRESULT hr = _device->GetId(&device_id);
    if (SUCCEEDED(hr))
    {
      _device_id = device_id;
      CoTaskMemFree(device_id);
    }

    IPropertyStore* property_store = nullptr;
    __wasapi_util::auto_release auto_release_property_store{ property_store };

    hr = _device->OpenPropertyStore(STGM_READ, &property_store);
    if (SUCCEEDED(hr))
    {
      PROPVARIANT property_variant;
      PropVariantInit(&property_variant);

      auto try_acquire_name = [&](const auto& property_name)
      {
        hr = property_store->GetValue(property_name, &property_variant);
        if(SUCCEEDED(hr))
        {
          _name = __wasapi_util::convert_string(property_variant.pwszVal);
          return true;
        }

        return false;
      };

      try_acquire_name(PKEY_Device_FriendlyName)
        || try_acquire_name(PKEY_DeviceInterface_FriendlyName)
        || try_acquire_name(PKEY_Device_DeviceDesc);

      PropVariantClear(&property_variant);
    }
  }
  //}}}
  //{{{
  void _init_audio_client() {

    HRESULT hr = _device->Activate(__wasapi_util::get_IAudioClient_interface_id(), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&_audio_client));
    if (FAILED(hr))
      return;
    }
  //}}}
  //{{{
  void _init_mix_format() {

    WAVEFORMATEX* device_mix_format;
    HRESULT hr = _audio_client->GetMixFormat(&device_mix_format);
    if (FAILED(hr))
      return;

    auto* device_mix_format_ex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(device_mix_format);
    _mix_format = *device_mix_format_ex;

    CoTaskMemFree(device_mix_format);
    }
  //}}}
  //{{{
  void _fixup_mix_format() {
    _mix_format.Format.nBlockAlign = _mix_format.Format.nChannels * _mix_format.Format.wBitsPerSample / 8;
    _mix_format.Format.nAvgBytesPerSec = _mix_format.Format.nSamplesPerSec * _mix_format.Format.wBitsPerSample * _mix_format.Format.nChannels / 8;
    }
  //}}}
  //{{{
  template<typename _CallbackType> void _connect_helper (_CallbackType callback) {

    if (_running)
      throw audio_device_exception("Cannot connect to running audio_device.");

    _user_callback = move(callback);
    }
  //}}}
  //{{{
  template<typename _SampleType> bool _mix_format_matches_type() const noexcept {

    if constexpr (std::is_same_v<_SampleType, float>)
      return _mix_format.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    else if constexpr (std::is_same_v<_SampleType, int32_t>)
      return _mix_format.SubFormat == KSDATAFORMAT_SUBTYPE_PCM
              && _mix_format.Format.wBitsPerSample == sizeof(int32_t) * 8;
    else if constexpr (std::is_same_v<_SampleType, int16_t>)
      return _mix_format.SubFormat == KSDATAFORMAT_SUBTYPE_PCM
              && _mix_format.Format.wBitsPerSample == sizeof(int16_t) * 8;
    else
      return false;
    }
  //}}}
  //{{{
  template<typename _SampleType, typename _CallbackType> void _process_helper(const _CallbackType& callback) {

    if (_audio_client == nullptr)
      return;

    if (!_mix_format_matches_type<_SampleType>())
      return;

    if (is_output()) {
      UINT32 current_padding = 0;
      _audio_client->GetCurrentPadding(&current_padding);

      auto num_frames_available = _buffer_frame_count - current_padding;
      if (num_frames_available == 0)
        return;

      BYTE* data = nullptr;
      _audio_render_client->GetBuffer(num_frames_available, &data);
      if (data == nullptr)
        return;

      audio_device_io<_SampleType> device_io;
      device_io.output_buffer = { reinterpret_cast<_SampleType*>(data), num_frames_available, _mix_format.Format.nChannels, contiguous_interleaved };
      callback(*this, device_io);

      _audio_render_client->ReleaseBuffer(num_frames_available, 0);
      }

    else if (is_input()) {
      UINT32 next_packet_size = 0;
      _audio_capture_client->GetNextPacketSize(&next_packet_size);
      if (next_packet_size == 0)
        return;

      // TODO: Support device position.
      DWORD flags = 0;
      BYTE* data = nullptr;
      _audio_capture_client->GetBuffer(&data, &next_packet_size, &flags, nullptr, nullptr);
      if (data == nullptr)
        return;

      audio_device_io<_SampleType> device_io;
      device_io.input_buffer = { reinterpret_cast<_SampleType*>(data), next_packet_size, _mix_format.Format.nChannels, contiguous_interleaved };
      callback(*this, device_io);

      _audio_capture_client->ReleaseBuffer(next_packet_size);
      }
    }
  //}}}
  //{{{
  template <typename _SampleType> bool _set_sample_type_helper() {

    if constexpr (std::is_same_v<_SampleType, float>)
      _mix_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    else if constexpr (std::is_same_v<_SampleType, int32_t>)
      _mix_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    else if constexpr (std::is_same_v<_SampleType, int16_t>)
      _mix_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    else
      return false;

    _mix_format.Format.wBitsPerSample = sizeof(_SampleType) * 8;
    _mix_format.Samples.wValidBitsPerSample = _mix_format.Format.wBitsPerSample;
    _fixup_mix_format();

    return true;
    }
  //}}}
  //{{{
  bool _is_connected() const noexcept {

    if (_user_callback.valueless_by_exception())
      return false;

    return visit([](auto&& callback) { return static_cast<bool>(callback); }, _user_callback);
    }
  //}}}

  IMMDevice* _device = nullptr;
  IAudioClient* _audio_client = nullptr;
  IAudioCaptureClient* _audio_capture_client = nullptr;
  IAudioRenderClient* _audio_render_client = nullptr;
  HANDLE _event_handle;
  std::wstring _device_id;
  std::atomic<bool> _running = false;
  std::string _name;

  WAVEFORMATEXTENSIBLE _mix_format;
  std::thread _processing_thread;
  UINT32 _buffer_frame_count = 0;
  bool _is_render_device = true;

  using __stop_callback_t = std::function<void(audio_device&)>;
  __stop_callback_t _stop_callback;

  using __wasapi_float_callback_t = std::function<void(audio_device&, audio_device_io<float>&)>;
  using __wasapi_int32_callback_t = std::function<void(audio_device&, audio_device_io<int32_t>&)>;
  using __wasapi_int16_callback_t = std::function<void(audio_device&, audio_device_io<int16_t>&)>;
  std::variant<__wasapi_float_callback_t, __wasapi_int32_callback_t, __wasapi_int16_callback_t> _user_callback;

  __wasapi_util::com_initializer _com_initializer;
  };
//}}}
//{{{
enum class audio_device_list_event {
  device_list_changed,
  default_input_device_changed,
  default_output_device_changed,
  };
//}}}
template <typename F, typename = std::enable_if_t<std::is_invocable_v<F>>> void set_audio_device_list_callback (audio_device_list_event, F&&);

class audio_device_list : public std::forward_list<audio_device> {};
//{{{
class __audio_device_monitor {
public:
  //{{{
  static __audio_device_monitor& instance()
  {
    static __audio_device_monitor singleton;
    return singleton;
  }
  //}}}

  //{{{
  template <typename F> void register_callback(audio_device_list_event event, F&& callback)
  {
    _callback_monitors[static_cast<int>(event)].reset(new WASAPINotificationClient{_enumerator, event, std::move(callback)});
  }
  //}}}
  //{{{
  template <> void register_callback(audio_device_list_event event, nullptr_t&&)
  {
    _callback_monitors[static_cast<int>(event)].reset();
  }
  //}}}

private:
  //{{{
  __audio_device_monitor() {

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&_enumerator);
    if (FAILED(hr))
      throw audio_device_exception("Could not create device enumerator");
    }
  //}}}
  //{{{
  ~__audio_device_monitor() {

    if (_enumerator == nullptr)
      return;

    for (auto& callback_monitor : _callback_monitors)
      callback_monitor.reset();

    _enumerator->Release();
    }
  //}}}

  //{{{
  class WASAPINotificationClient : public IMMNotificationClient {
  public:
    //{{{
    WASAPINotificationClient(IMMDeviceEnumerator* enumerator, audio_device_list_event event, std::function<void()> callback) :
        _enumerator(enumerator), _event(event), _callback(std::move(callback)) {

      if (_enumerator == nullptr)
        throw audio_device_exception("Attempting to create a notification client for a null enumerator");

      _enumerator->RegisterEndpointNotificationCallback(this);
      }
    //}}}
    //{{{
    virtual ~WASAPINotificationClient()
    {
      _enumerator->UnregisterEndpointNotificationCallback(this);
    }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, [[maybe_unused]] LPCWSTR device_id) {

      if (role != ERole::eConsole)
        return S_OK;

      if (flow == EDataFlow::eRender) {
        if (_event != audio_device_list_event::default_output_device_changed)
          return S_OK;
        }
      else if (flow == EDataFlow::eCapture) {
        if (_event != audio_device_list_event::default_input_device_changed)
          return S_OK;
        }

      _callback();
      return S_OK;
      }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceAdded([[maybe_unused]] LPCWSTR device_id) {

      if (_event != audio_device_list_event::device_list_changed)
        return S_OK;

      _callback();
      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved([[maybe_unused]] LPCWSTR device_id) {

      if (_event != audio_device_list_event::device_list_changed)
        return S_OK;

      _callback();
      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged([[maybe_unused]] LPCWSTR device_id, [[maybe_unused]] DWORD new_state) {

      if (_event != audio_device_list_event::device_list_changed)
        return S_OK;

      _callback();
      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged([[maybe_unused]] LPCWSTR device_id, [[maybe_unused]] const PROPERTYKEY key) {

      return S_OK;
      }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **requested_interface) {

      if (IID_IUnknown == riid)
        *requested_interface = (IUnknown*)this;
      else if (__uuidof(IMMNotificationClient) == riid)
        *requested_interface = (IMMNotificationClient*)this;
      else {
        *requested_interface = nullptr;
        return E_NOINTERFACE;
        }

      return S_OK;
      }
    //}}}
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    ULONG STDMETHODCALLTYPE Release() { return 0; }

  private:
    __wasapi_util::com_initializer _com_initializer;
    IMMDeviceEnumerator* _enumerator;
    audio_device_list_event _event;
    std::function<void()> _callback;
    };
  //}}}

  __wasapi_util::com_initializer _com_initializer;
  IMMDeviceEnumerator* _enumerator = nullptr;
  std::array<std::unique_ptr<WASAPINotificationClient>, 3> _callback_monitors;
  };
//}}}
//{{{
class __audio_device_enumerator {
public:
  //{{{
  static std::optional<audio_device> get_default_output_device()
  {
    const bool is_output_device = true;
    return get_default_device(is_output_device);
  };
  //}}}
  //{{{
  static std::optional<audio_device> get_default_input_device()
  {
    const bool is_output_device = false;
    return get_default_device(is_output_device);
  };
  //}}}
  //{{{
  static auto get_input_device_list()
  {
    return get_device_list(false);
  }
  //}}}
  //{{{
  static auto get_output_device_list()
  {
    return get_device_list(true);
  }
  //}}}

private:
  __audio_device_enumerator() = delete;

  //{{{
  static std::optional<audio_device> get_default_device(bool output_device) {

    __wasapi_util::com_initializer com_initializer;

    IMMDeviceEnumerator* enumerator = nullptr;
    __wasapi_util::auto_release enumerator_release{ enumerator };

    HRESULT hr = CoCreateInstance(
      __wasapi_util::get_MMDeviceEnumerator_classid(), nullptr,
      CLSCTX_ALL, __wasapi_util::get_IMMDeviceEnumerator_interface_id(),
      reinterpret_cast<void**>(&enumerator));

    if (FAILED(hr))
      return std::nullopt;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(output_device ? eRender : eCapture, eConsole, &device);
    if (FAILED(hr))
      return std::nullopt;

    try {
      return audio_device{ device, output_device };
      }
    catch (const audio_device_exception&) {
      return std::nullopt;
      }
    }
  //}}}
  //{{{
  static std::vector<IMMDevice*> get_devices(bool output_devices) {

    __wasapi_util::com_initializer com_initializer;

    IMMDeviceEnumerator* enumerator = nullptr;
    __wasapi_util::auto_release enumerator_release{ enumerator };
    HRESULT hr = CoCreateInstance(
      __wasapi_util::get_MMDeviceEnumerator_classid(), nullptr,
      CLSCTX_ALL, __wasapi_util::get_IMMDeviceEnumerator_interface_id(),
      reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr))
      return {};

    IMMDeviceCollection* device_collection = nullptr;
    __wasapi_util::auto_release collection_release{ device_collection };

    EDataFlow selected_data_flow = output_devices ? eRender : eCapture;
    hr = enumerator->EnumAudioEndpoints(selected_data_flow, DEVICE_STATE_ACTIVE, &device_collection);
    if (FAILED(hr))
      return {};

    UINT device_count = 0;
    hr = device_collection->GetCount(&device_count);
    if (FAILED (hr))
      return {};

    std::vector<IMMDevice*> devices;
    for (UINT i = 0; i < device_count; i++) {
      IMMDevice* device = nullptr;
      hr = device_collection->Item(i, &device);
      if (FAILED(hr)) {
        if (device != nullptr)
          device->Release();
        continue;
        }

      if (device != nullptr)
        devices.push_back(device);
      }

    return devices;
    }
  //}}}
  //{{{
  static audio_device_list get_device_list(bool output_devices) {

    __wasapi_util::com_initializer com_initializer;

    audio_device_list devices;
    const auto mmdevices = get_devices(output_devices);

    for (auto* mmdevice : mmdevices) {
      if (mmdevice == nullptr)
        continue;

      try {
        devices.push_front(audio_device{ mmdevice, output_devices });
        }
      catch (const audio_device_exception&) {
        // TODO: Should I do anything with this exception?
        // My impulse is to leave it alone.  The result of this function
        // should be an array of properly-constructed devices.  If we
        // couldn't create a device, then we shouldn't return it from
        // this function.
        }
      }

    return devices;
    }
  //}}}
  };
//}}}

//{{{
std::optional<audio_device> get_default_audio_input_device() {
  return __audio_device_enumerator::get_default_input_device();
  }
//}}}
//{{{
std::optional<audio_device> get_default_audio_output_device() {
  return __audio_device_enumerator::get_default_output_device();
  }
//}}}
//{{{
audio_device_list get_audio_input_device_list() {
  return __audio_device_enumerator::get_input_device_list();
  }
//}}}
//{{{
audio_device_list get_audio_output_device_list() {
  return __audio_device_enumerator::get_output_device_list();
  }
//}}}

//{{{
template <typename F, typename /* = enable_if_t<is_invocable_v<F>> */> void set_audio_device_list_callback (audio_device_list_event event, F&& callback) {
  __audio_device_monitor::instance().register_callback (event, std::move (callback));
  }
//}}}
