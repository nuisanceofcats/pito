#ifndef _PITO_INTERCEPTOR_LIB_TRAITS_
#define _PITO_INTERCEPTOR_LIB_TRAITS_

#include <rbutil/type/function.hpp>

namespace pito { namespace interceptor { namespace system_call {

template <class Tag>
struct traits;

} } }

// TODO: make the library tag a trait
#define PITO_SYSTEM_CALL_TRAIT(name_, lib_, signature_) \
    struct name_ {}; \
    template <> struct traits<name_> : rb::util::type::function<signature_> { \
        static char const       name[]; \
        typedef library::lib_   library; \
    }; \
    char const traits<name_>::name[] = #name_;

#endif
