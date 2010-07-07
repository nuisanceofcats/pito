#ifndef PITO_SANDBOX_HELPER_HPP
#define PITO_SANDBOX_HELPER_HPP

#include <pito/system_call.hpp>
#include <pito/lib/c_traits.hpp>

#include <iostream>

namespace pito { namespace sandbox {

using namespace system_call_tag;

template <class Tag>
struct system_call;

template <class Tag>
struct system_call : detail::system_call<Tag> {
    template <class... Args>
    PITO_RETURN(Tag) operator()(Args... args) {
        return PITO_SUPER(Tag, args...);
    }
};

template <>
struct system_call<creat> : detail::system_call<creat> {
    PITO_RETURN(creat) operator()(const char *file, mode_t mode) {
        return PITO_SUPER(system_call_tag::creat, file, mode);
    }
};

} }

extern "C" {

int sandbox_init(int offset, int argc, char *argv[]) {
    std::cout << "sandbox init" << std::endl;
    return offset + 1;
}

}

#endif