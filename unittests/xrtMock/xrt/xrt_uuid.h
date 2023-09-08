#ifndef xrt_uuid_h_
#define xrt_uuid_h_

#include <uuid/uuid.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>


typedef unsigned char uuid_t[16];


namespace xrt {
    class uuid {
        uuid_t m_uuid;
        inline static unsigned int uuid_constructors_called = 0;

         public:
        /**
         * uuid() - Construct cleared uuid
         */
        uuid() {}

        /**
         * uuid() - Construct uuid from a basic bare uuid
         *
         * @param val
         *  The basic uuid to construct this object from
         *
         * A basic uuid is either a uuid_t on Linux, or a typedef
         * of equivalent basic type of other platforms
         */
        uuid(const uuid_t val) {
            ++uuid_constructors_called;
            std::memcpy(&m_uuid, val, 16);
        }

        /**
         * uuid() - Construct uuid from a string representaition
         *
         * @param uuid_str
         *  A string formatted as a uuid string
         *
         * A uuid string is 36 bytes with '-' at 8, 13, 18, and 23
         */
        // explicit uuid(const std::string& uuid_str) {
        //     if (uuid_str.empty()) {
        //         uuid_clear(m_uuid);
        //         return;
        //     }
        //     uuid_parse(uuid_str.c_str(), m_uuid);
        // }

        /**
         * uuid() - copy constructor
         *
         * @param rhs
         *   Value to be copied
         */
        uuid(const uuid& rhs) {
            ++uuid_constructors_called;
            std::copy(std::begin(rhs.m_uuid), std::end(rhs.m_uuid), std::begin(m_uuid));
        }

        /// @cond
        uuid(uuid&&) = default;
        uuid& operator=(uuid&&) = default;
        /// @endcond

        /**
         * operator=() - assignment
         *
         * @param rhs
         *   Value to be assigned from
         * @return
         *   Reference to this
         */
        uuid& operator=(const uuid& rhs) {
            uuid source(rhs);
            std::swap(*this, source);
            return *this;
        }

        /**
         * get() - Get the underlying basis uuid type value
         *
         * @return
         *  Basic uuid value
         *
         * A basic uuid is either a uuid_t on Linux, or a typedef
         * of equivalent basic type of other platforms
         */
        const uuid_t& get() const { return m_uuid; }

        /**
         * to_string() - Convert to string
         *
         * @return
         *   Lower case string representation of this uuid
         */
        // std::string to_string() const {
        //     char str[40] = {0};
        //     uuid_unparse_lower(m_uuid, str);
        //     return str;
        // }

        /**
         * bool() - Conversion operator
         *
         * @return
         *  True if this uuid is not null
         */
        // operator bool() const { return uuid_is_null(m_uuid) == false; }

        /**
         * operator==() - Compare to basic uuid
         *
         * @param xuid
         *  Basic uuid to compare against
         * @return
         *  True if equal, false otherwise
         *
         * A basic uuid is either a uuid_t on Linux, or a typedef
         * of equivalent basic type of other platforms
         */
        bool operator==(const unsigned char (&xuid)[16]) const { return std::equal(std::begin(m_uuid), std::end(m_uuid), std::begin(xuid)); }

        /**
         * operator!=() - Compare to basic uuid
         *
         * @param xuid
         *  Basic uuid to compare against
         * @return
         *  False if equal, true otherwise
         *
         * A basic uuid is either a uuid_t on Linux, or a typedef
         * of equivalent basic type of other platforms
         */
        bool operator!=(const unsigned char (&xuid)[16]) const { return !std::equal(std::begin(m_uuid), std::end(m_uuid), std::begin(xuid)); }

        /**
         * operator==() - Comparison
         *
         * @param rhs
         *  uuid to compare against
         * @return
         *  True if equal, false otherwise
         */
        bool operator==(const uuid& rhs) const { return std::equal(std::begin(m_uuid), std::end(m_uuid), std::begin(rhs.m_uuid)); }

        /**
         * operator!=() - Comparison
         *
         * @param rhs
         *  uuid to compare against
         * @return
         *  False if equal, true otherwise
         */
        bool operator!=(const uuid& rhs) const { return !std::equal(std::begin(m_uuid), std::end(m_uuid), std::begin(rhs.m_uuid)); }

        /**
         * operator<() - Comparison
         *
         * @param rhs
         *  uuid to compare against
         * @return
         *  True of this is less that argument uuid, false otherwise
         */
        // bool operator<(const uuid& rhs) const { return uuid_compare(m_uuid, rhs.m_uuid) < 0; }
    };

}  // namespace xrt
#endif