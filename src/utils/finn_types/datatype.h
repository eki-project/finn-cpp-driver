#ifndef _UTILS_FINN_TYPES_DATATYPE_H_
#define _UTILS_FINN_TYPES_DATATYPE_H_

class Datatype {
   public:
  constexpr virtual bool sign() = 0;
};

template<size_t B>
class DatatypeInt : Datatype {
   public:
  constexpr bool sign() override { return false; }
};

#endif  //_UTILS_FINN_TYPES_DATATYPE_H_