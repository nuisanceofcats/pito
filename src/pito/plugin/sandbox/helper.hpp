#ifndef PITO_SANDBOX_HELPER_HPP
#define PITO_SANDBOX_HELPER_HPP

#include <pito/system_call.hpp>
#include <pito/lib/c_traits.hpp>

#include <chilon/iterator_range.hpp>
#include <chilon/print.hpp>
#include <chilon/argument.hpp>
#include <chilon/realpath.hpp>
#include <chilon/meta/contains.hpp>
#include <chilon/meta/find_int.hpp>

#include <vector>
#include <unordered_map>

#include <sys/stat.h>

#define PITO_SANDBOX_DEFAULT  "PITO_SANDBOX_DEFAULT"
#define PITO_SANDBOX_PATHS    "PITO_SANDBOX_PATHS"

namespace pito { namespace sandbox {

namespace meta = chilon::meta;

// options
struct file_must_exist;

template <int i>
struct path_index : std::integral_constant<int, i> {};

template <int i>
struct dir_fd : std::integral_constant<int, i> {};
// end options

template <class Tag>
struct system_call;

namespace color = chilon::color;

typedef char write_mode;

write_mode const WRITE_MODE_UNKNOWN   = 'U';
write_mode const WRITE_MODE_PRETEND   = 'P';
write_mode const WRITE_MODE_WHITELIST = 'W';
write_mode const WRITE_MODE_BLACKLIST = 'B';

struct context {
    context();

    std::vector<range> paths;
    write_mode         default_mode;
};

extern context& ctxt;

template <class Tag, class... Options>
struct sandbox_call : system_call_real<Tag> {

    enum {
        FileMustExist =
            meta::contains<file_must_exist, Options...>::value
    };

    typedef PITO_RETURN(Tag) return_type;

  protected:
    system_call<Tag>& mixin() {
        return static_cast< system_call<Tag> & >(*this);
    }

    template <class... Args>
    return_type pretend(Args... args) const { return 0; }

    return_type blacklist() { return -1; }

    // executes the system call depending on the path argument
    template <bool FileMustExist_, class... Args>
    write_mode path_type(chilon::realpath_type& realpath, Args... args) {
        enum {
            Idx = meta::find_int<path_index, 0, Options...>::value
        };

        auto path_arg = chilon::argument<Idx>(args...);

        if (FileMustExist_ ?
                ! ::realpath(path_arg, realpath) :
                ! chilon::realpath(path_arg, realpath))
        {
            chilon::println(color::red,
                this->name(), ": error resolving path ", path_arg);
            return WRITE_MODE_UNKNOWN;
        }

        for (auto it = ctxt.paths.begin(); it != ctxt.paths.end(); ++it) {
            if (std::equal(it->begin() + 1, it->end(), realpath)) {
                char const last = realpath[it->size() - 1];
                if ('/' == last || '\0' == last) return it->front();
            }
        }

        return ctxt.default_mode;
    }

    // test path, with default FileMustExist option
    template <class... Args>
    write_mode path_type(chilon::realpath_type& realpath, Args... args) {
        return path_type<FileMustExist>(realpath, args...);
    }

    template <bool FileMustExist_, class... Args>
    return_type run(Args... args) {
        chilon::realpath_type realpath;
        auto const write_type =
            path_type<FileMustExist_>(realpath, args...);

        switch (write_type) {
            case WRITE_MODE_WHITELIST:
                return this->system(args...);

            case WRITE_MODE_BLACKLIST:
                errno = EACCES;
                chilon::println(color::red,
                    this->name(), ": ", realpath, " DENIED");

                return this->mixin().blacklist();

            case WRITE_MODE_PRETEND:
                chilon::println(color::red,
                    this->name(), ": ", realpath, " PRETEND");

                return this->mixin().pretend(args...);
        }
    }

    template <class... Args>
    return_type run(Args... args) {
        return run<FileMustExist>(args...);
    }

  public:
    template <class... Args>
    return_type operator()(Args... args) {
        return run(args...);
    }
};

template <class Tag, int fdIndex = 0, int pathIndex = -1>
struct sandbox_fd_call : system_call_real<Tag> {
    // todo:

    typedef PITO_RETURN(Tag) return_type;

    template <class... Args>
    return_type operator()(Args... args) {
        // todo: remember to check for AT_FDCWD
        return this->system(args...);
    }
};

// if CreateFile is false, it can still possibly create the file depending
// on the arguments
template <class Tag, bool CreateFile = false>
struct sandbox_call_open : sandbox_call<Tag> {
    typedef sandbox_call<Tag> base;
    typedef PITO_RETURN(Tag)  return_type;

    template <class Path, class... Args>
    return_type pretend(Path, Args... args) {
        return this->system("/dev/null", args...);
    }

    template <class Arg2, class... ModeArg>
    return_type operator()(const char *path, Arg2 oflag, ModeArg... mode) {
        // can't test & with O_RDONLY because O_RDONLY can be 0
        if (! CreateFile && ! sizeof...(mode) &&
            ! (oflag & (O_WRONLY | O_RDWR)))
                return this->system(path, oflag, mode...);

        return this->template run<
            ! CreateFile && ! sizeof...(mode)>(path, oflag, mode...);
    }
};

template <class Tag>
struct sandbox_call_fopen : sandbox_call_open<Tag> {
    typedef PITO_RETURN(Tag)  return_type;

    return_type blacklist() { return 0; }

    template <class Path, class... Args>
    return_type pretend(Path, Args... args) {
        return this->system("/dev/null", args...);
    }

    template <class... Stream>
    return_type operator()(const char *path,
                           const char *mode,
                           Stream...   stream)
    {
        if (! *mode)
            return this->system(path, mode, stream...);

        if ('r' == *mode) {
            if ('+' == *(mode + 1))
                return this->template run<true>(path, mode, stream...);
            else
                return this->system(path, mode, stream...);
        }
        else
            return this->run(path, mode, stream...);
    }
};

} }

#endif
