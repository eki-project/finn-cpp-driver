#ifndef _UTILS_FINN_TYPES_DATATYPE_H_
#define _UTILS_FINN_TYPES_DATATYPE_H_

#include <concepts>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

template<class T>
concept Integral = std::is_integral<T>::value;
template<class T>
concept SignedIntegral = Integral<T> && std::is_signed<T>::value;
template<class T>
concept UnsignedIntegral = Integral<T> && !SignedIntegral<T>;

// forward declare Datatype for concept
template<typename T>
class Datatype;

template<typename T, typename D = T>
concept IsDatatype = std::derived_from<T, Datatype<D>>;


template<typename D>
class Datatype {
     public:
    constexpr virtual bool sign() const = 0;

    constexpr virtual std::size_t bitwidth() const = 0;

    constexpr virtual double min() const = 0;
    constexpr virtual double max() const = 0;

    template<Integral T>
    bool allowed(const T& val) const {
        return static_cast<D*>(this)->template allowedImpl<T>(val);
    };

    constexpr virtual double getNumPossibleValues() const { return (min() < 0) ? -min() + max() + 1 : min() + max() + 1; }

    constexpr virtual bool isInteger() const = 0;
    constexpr virtual bool isFixedPoint() const = 0;

    template<typename D2>
    constexpr friend bool operator==([[maybe_unused]] const Datatype<D>& lhs, [[maybe_unused]] const Datatype<D2>& rhs) {
        return std::is_same_v<D, D2>;
    }

    template<typename D2>
    constexpr friend bool operator!=(const Datatype<D>& lhs, const Datatype<D2>& rhs) {
        return !(lhs == rhs);
    }

    virtual ~Datatype() = default;

     protected:
    Datatype(Datatype&&) noexcept = default;
    Datatype(const Datatype&) = default;
    Datatype& operator=(Datatype&&) noexcept = default;
    Datatype& operator=(const Datatype&) = default;

     private:
    // Some somewhat hacky code to make sure that CRTP is implemented correctly by all Derived classes -> creates error if for class A : public Base<B> A!=B
    Datatype() = default;
    friend D;

    template<typename T>
    bool static allowedImpl([[maybe_unused]] const T& val) {
        return false;
    };
};


class DatatypeFloat : public Datatype<DatatypeFloat> {
     public:
    constexpr bool sign() const override { return true; }

    constexpr std::size_t bitwidth() const override { return 32; }

    constexpr double min() const override { return static_cast<double>(std::numeric_limits<float>::lowest()); }
    constexpr double max() const override { return static_cast<double>(std::numeric_limits<float>::max()); }

    constexpr bool isInteger() const override { return false; }

    constexpr bool isFixedPoint() const override { return false; }

     private:
    // cppcheck-supress unusedPrivateFunction
    template<typename T>
    [[maybe_unused]] bool allowedImpl(const T& val) const {
        return (val >= min()) && (val <= max());
    };
};

template<std::size_t B>
class DatatypeInt : public Datatype<DatatypeInt<B>> {
     public:
    constexpr bool sign() const override { return true; }

    constexpr std::size_t bitwidth() const override { return B; }

    constexpr double min() const override { return -static_cast<double>(1U << (B - 1)); }
    constexpr double max() const override { return (1U << (B - 1)) - 1; }

    constexpr bool isInteger() const override { return true; }

    constexpr bool isFixedPoint() const override { return false; }

    virtual std::string getHlsType() {
        std::stringstream strs;
        strs << "ap_int<" << bitwidth() << ">";
        return strs.str();
    }

     private:
    template<typename T>
    // cppcheck-supress unusedPrivateFunction
    [[maybe_unused]] bool allowedImpl(const T& val) const {
        return (val >= min()) && (val <= max());
    };
};

template<std::size_t B, std::size_t I>
class DatatypeFixed : public Datatype<DatatypeFixed<B, I>> {
     public:
    constexpr bool sign() const override { return true; }

    constexpr std::size_t bitwidth() const override { return B; }

    constexpr std::size_t intBits() const { return I; }
    constexpr std::size_t fracBits() const { return B - I; }

    constexpr double scaleFactor() const { return 1.0 / (1U << I); }

    constexpr double min() const override { return -static_cast<double>(1U << (B - 1)) * scaleFactor(); }
    constexpr double max() const override { return ((1U << (B - 1)) - 1) * scaleFactor(); }

    constexpr bool isInteger() const override { return false; }

    constexpr bool isFixedPoint() const override { return true; }

     private:
    template<typename T>
    // cppcheck-supress unusedPrivateFunction
    [[maybe_unused]] bool allowedImpl(const T& val) const {
        T intEquivalent = val * (1U << I);
        return (intEquivalent >= min()) && (intEquivalent <= max());
    };
};

template<std::size_t B>
class DatatypeUInt : public Datatype<DatatypeUInt<B>> {
     public:
    constexpr bool sign() const override { return false; }

    constexpr std::size_t bitwidth() const override { return B; }

    constexpr double min() const override { return 0; }
    constexpr double max() const override { return (1U << B) - 1; }

    constexpr bool isInteger() const override { return true; }

    constexpr bool isFixedPoint() const override { return false; }

    virtual std::string getHlsType() {
        std::stringstream strs;
        strs << "ap_uint<" << bitwidth() << ">";
        return strs.str();
    }

     private:
    template<typename T>
    // cppcheck-supress unusedPrivateFunction
    [[maybe_unused]] bool allowedImpl(const T& val) const {
        return (val >= min()) && (val <= max());
    };
};

using DatatypeBinary = DatatypeUInt<1>;

class DatatypeBipolar : public Datatype<DatatypeBipolar> {
    constexpr bool sign() const override { return true; }

    constexpr std::size_t bitwidth() const override { return 1; }

    constexpr double min() const override { return -1; }
    constexpr double max() const override { return 1; }

    constexpr bool isInteger() const override { return true; }

    constexpr bool isFixedPoint() const override { return false; }

    constexpr double getNumPossibleValues() const override { return 2; }

     private:
    template<typename T>
    // cppcheck-supress unusedPrivateFunction
    [[maybe_unused]] static bool allowedImpl(const T& val) {
        return (val == -1 || val == 1);
    };
};

class DatatypeTernary : public Datatype<DatatypeTernary> {
    constexpr bool sign() const override { return true; }

    constexpr std::size_t bitwidth() const override { return 2; }

    constexpr double min() const override { return -1; }
    constexpr double max() const override { return 1; }

    constexpr bool isInteger() const override { return true; }

    constexpr bool isFixedPoint() const override { return false; }

    constexpr double getNumPossibleValues() const override { return 3; }

     private:
    template<typename T>
    // cppcheck-supress unusedPrivateFunction
    [[maybe_unused]] static bool allowedImpl(const T& val) {
        return (val == -1 || val == 1 || val == 0);
    };
};


#endif  //_UTILS_FINN_TYPES_DATATYPE_H_