/**
 * @file join.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides functionality to join any stl like container into a string for printing using a provided delimiter
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef JOIN_HPP
#define JOIN_HPP

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>

/**
 * @brief Implements an infix iterator
 *
 * @tparam T Datatype of the printed iterable container
 * @tparam charT Character type used
 * @tparam traits type traits of the character type
 */
template<class T, class charT = char, class traits = std::char_traits<charT>>
class infix_ostream_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
    std::basic_ostream<charT, traits>* os;
    charT const* delimiter;
    bool first_elem;

     public:
    /**
     * @brief Character type used
     *
     */
    typedef charT char_type;
    /**
     * @brief Type traits of the character type
     *
     */
    typedef traits traits_type;
    /**
     * @brief Type of output stream
     *
     */
    typedef std::basic_ostream<charT, traits> ostream_type;
    /**
     * @brief Construct a new infix ostream iterator object
     *
     * @param s
     */
    explicit infix_ostream_iterator(ostream_type& s) : os(&s), delimiter(0), first_elem(true) {}
    /**
     * @brief Construct a new infix ostream iterator object
     *
     * @param s
     * @param d
     */
    infix_ostream_iterator(ostream_type& s, charT const* d) : os(&s), delimiter(d), first_elem(true) {}
    /**
     * @brief Copy assignment operator
     *
     * @param item
     * @return infix_ostream_iterator<T, charT, traits>&
     */
    infix_ostream_iterator<T, charT, traits>& operator=(T const& item) {
        // Here's the only real change from ostream_iterator:
        // Normally, the '*os << item;' would come before the 'if'.
        if (!first_elem && delimiter != 0)
            *os << delimiter;
        *os << item;
        first_elem = false;
        return *this;
    }
    /**
     * @brief Return reference to element that is currently pointed to by the iterator
     *
     * @return infix_ostream_iterator<T, charT, traits>&
     */
    infix_ostream_iterator<T, charT, traits>& operator*() { return *this; }
    /**
     * @brief Increment operator
     *
     * @return infix_ostream_iterator<T, charT, traits>&
     */
    infix_ostream_iterator<T, charT, traits>& operator++() { return *this; }
    /**
     * @brief Increment operator for multiple elements at once
     *
     * @return infix_ostream_iterator<T, charT, traits>&
     */
    infix_ostream_iterator<T, charT, traits>& operator++(int) { return *this; }
};

namespace detail {
    // To allow ADL with custom begin/end
    using std::begin;
    using std::end;

    /**
     * @brief Internal implementation of is_iterable
     *
     * @tparam T
     * @return decltype(begin(std::declval<T&>()) != end(std::declval<T&>()),    // begin/end and operator !=
     * ++std::declval<decltype(begin(std::declval<T&>()))&>(),  // operator ++
     * *begin(std::declval<T&>()),                              // operator*
     * std::true_type{})
     */
    template<typename T>
    auto is_iterable_impl(int) -> decltype(begin(std::declval<T&>()) != end(std::declval<T&>()),    // begin/end and operator !=
                                           ++std::declval<decltype(begin(std::declval<T&>()))&>(),  // operator ++
                                           *begin(std::declval<T&>()),                              // operator*
                                           std::true_type{});

    /**
     * @brief Default case for is_iterable, results in std::false_type
     *
     * @tparam T
     * @param ...
     * @return std::false_type
     */
    template<typename T>
    std::false_type is_iterable_impl(...);

}  // namespace detail

/**
 * @brief Implements type trait to test if type T is iterable
 *
 * @tparam T
 */
template<typename T>
using is_iterable = decltype(detail::is_iterable_impl<T>(0));

/**
 * @brief Joins iterable elements into a string using provided delimiter
 *
 * @tparam T Iterable type
 * @param p Iterable STL-like object
 * @param delimiter Symbol used to delimit elements of the vector
 * @return std::enable_if<is_iterable<T>::value, std::string>::type resolves to
 * std::string iff input is iterable and error type else
 */
template<typename T>
auto join(const T& p, const char* delimiter) -> typename std::enable_if<is_iterable<T>::value, std::string>::type {
    std::ostringstream s;
    if constexpr (std::is_same<typename T::value_type, bool>::value) {
        s << std::boolalpha;
    }
    std::copy(p.begin(), p.end(), infix_ostream_iterator<typename T::value_type>(s, delimiter));
    if constexpr (std::is_same<typename T::value_type, bool>::value) {
        s << std::noboolalpha;
    }
    return s.str();
}

#endif  // JOIN_HPP
