#ifndef _UTILS_FINN_TYPES_DATATYPE_H_
#define _UTILS_FINN_TYPES_DATATYPE_H_

#include <concepts>
#include <utility>

template<class T>
concept Integral = std::is_integral<T>::value;
template<class T>
concept SignedIntegral = Integral<T> && std::is_signed<T>::value;
template<class T>
concept UnsignedIntegral = Integral<T> && !SignedIntegral<T>;

class Datatype {
   public:
  constexpr virtual bool sign() const = 0;

  constexpr virtual std::size_t bitwidth() const = 0;

  constexpr virtual long long min() const = 0;
  constexpr virtual long long max() const = 0;

  template<Integral T>
  bool allowed(const T& val) const {
    return (val > min()) && (val < max());
  };

  constexpr virtual long long getNumPossibleValues() const {
    return (min() < 0) ? -min() + max() + 1 : min() + max() + 1;
  }

  virtual ~Datatype() = default;

   protected:
  Datatype() = default;
  Datatype(Datatype&&) = default;
  Datatype(const Datatype&) = default;
  Datatype& operator=(Datatype&&) = default;
  Datatype& operator=(const Datatype&) = default;
};

template<std::size_t B>
class DatatypeInt : public Datatype {
   public:
  constexpr bool sign() const override { return true; }

  constexpr std::size_t bitwidth() const override { return B; }

  constexpr long long min() const override { return -static_cast<long long>(1U << (B - 1)); }
  constexpr long long max() const override { return (1U << B) - 1; }
};

#endif  //_UTILS_FINN_TYPES_DATATYPE_H_