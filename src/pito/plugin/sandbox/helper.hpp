#ifndef PITO_SANDBOX_HELPER_HPP
#define PITO_SANDBOX_HELPER_HPP

#include <pito/system_call.hpp>
#include <pito/lib/c_traits.hpp>

#include <chilon/singleton.hpp>
#include <chilon/iterator_range.hpp>

#include <vector>

#define PITO_SANDBOX_DEFAULT  "PITO_SANDBOX_DEFAULT"
#define PITO_SANDBOX_PATHS    "PITO_SANDBOX_PATHS"

namespace pito { namespace sandbox {

enum write_mode {
    WRITE_MODE_PRETEND,
    WRITE_MODE_WHITELIST,
    WRITE_MODE_BLACKLIST
};

struct context {
    context();

    std::vector<range> paths;
    write_mode         mode;
};

extern context& ctxt;

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

#endif
