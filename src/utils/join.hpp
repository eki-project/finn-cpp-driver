#ifndef JOIN_HPP
#define JOIN_HPP

#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>

template<class T, class charT = char, class traits = std::char_traits<charT>>
class infix_ostream_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
    std::basic_ostream<charT, traits>* os;
    charT const* delimiter;
    bool first_elem;

     public:
    typedef charT char_type;
    typedef traits traits_type;
    typedef std::basic_ostream<charT, traits> ostream_type;
    explicit infix_ostream_iterator(ostream_type& s) : os(&s), delimiter(0), first_elem(true) {}
    infix_ostream_iterator(ostream_type& s, charT const* d) : os(&s), delimiter(d), first_elem(true) {}
    infix_ostream_iterator<T, charT, traits>& operator=(T const& item) {
        // Here's the only real change from ostream_iterator:
        // Normally, the '*os << item;' would come before the 'if'.
        if (!first_elem && delimiter != 0)
            *os << delimiter;
        *os << item;
        first_elem = false;
        return *this;
    }
    infix_ostream_iterator<T, charT, traits>& operator*() { return *this; }
    infix_ostream_iterator<T, charT, traits>& operator++() { return *this; }
    infix_ostream_iterator<T, charT, traits>& operator++(int) { return *this; }
};

namespace detail {
    // To allow ADL with custom begin/end
    using std::begin;
    using std::end;

    template<typename T>
    auto is_iterable_impl(int) -> decltype(begin(std::declval<T&>()) != end(std::declval<T&>()),    // begin/end and operator !=
                                           ++std::declval<decltype(begin(std::declval<T&>()))&>(),  // operator ++
                                           *begin(std::declval<T&>()),                              // operator*
                                           std::true_type{});

    template<typename T>
    std::false_type is_iterable_impl(...);

}  // namespace detail

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
    std::copy(p.begin(), p.end(), infix_ostream_iterator<typename T::value_type>(s, delimiter));
    return s.str();
}

#endif  // JOIN_HPP
