#ifndef PITO_SANDBOX_HELPER_HPP
#define PITO_SANDBOX_HELPER_HPP

#include <pito/system_call.hpp>
#include <pito/lib/c_traits.hpp>
#include <pito/config.hpp>

#include <chilon/iterator_range.hpp>
#include <chilon/print.hpp>
#include <chilon/argument.hpp>
#include <chilon/realpath.hpp>
#include <chilon/meta/contains.hpp>
#include <chilon/meta/find_int.hpp>
#include <chilon/meta/same.hpp>
#include <chilon/meta/at.hpp>

#include <vector>
#include <unordered_map>
#include <cmath>

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
    typedef PITO_RETURN(Tag) return_type;

  protected:
    enum {
        FileMustExist =
            meta::contains<file_must_exist, Options...>::value
    };

    enum {
        DirFdIdx = meta::find_int<dir_fd, -1, Options...>::value
    };

    enum {
        PathIdx = meta::find_int<path_index, DirFdIdx + 1, Options...>::value
    };

    system_call<Tag>& mixin() {
        return static_cast< system_call<Tag> & >(*this);
    }

    template <class... Args>
    return_type pretend(Args... args) const { return 0; }

    return_type blacklist() { return -1; }

    // executes the system call depending on the path argument
    template <bool FileMustExist_, class... Args>
    write_mode path_type(chilon::realpath_type& realpath, Args... args) {
        auto path_arg = chilon::argument<PathIdx>(args...);

        if (FileMustExist_ ?
                ! ::realpath(path_arg, realpath) :
                ! chilon::realpath(path_arg, realpath))
        {
            chilon::println(color::red,
                this->name(), ": error resolving path ", path_arg);
            return WRITE_MODE_UNKNOWN;
        }

        return path_mode(realpath);
    }

    // test path, with default FileMustExist option
    template <class... Args>
    write_mode path_type(chilon::realpath_type& realpath, Args... args) {
        return path_type<FileMustExist>(realpath, args...);
    }


    write_mode path_mode(chilon::realpath_type const& realpath) const {
        for (auto it = ctxt.paths.begin(); it != ctxt.paths.end(); ++it) {
            if (std::equal(it->begin() + 1, it->end(), realpath)) {
                char const last = realpath[it->size() - 1];
                if ('/' == last || '\0' == last) return it->front();
            }
        }

        return ctxt.default_mode;
    }

    template <bool FileMustExist_, class... Args>
    return_type run(Args... args) {
        chilon::realpath_type realpath;

        return handle_mode(
            path_type<FileMustExist_>(realpath, args...), realpath, args...);
    }

    template <class... Args>
    return_type handle_mode(
        write_mode const             write_type,
        chilon::realpath_type const& realpath,
        Args...                      args)
    {
        switch (write_type) {
            case WRITE_MODE_PRETEND:
                chilon::println(color::red,
                    this->name(), ": ", realpath, " PRETEND");

                return this->mixin().pretend(args...);

            case WRITE_MODE_WHITELIST:
                return this->system(args...);

            default:
                errno = EACCES;
                chilon::println(color::red,
                    this->name(), ": ", realpath, " DENIED");

                return this->mixin().blacklist();
        }
    }

    template <class... Args>
    return_type handle_path(
        chilon::realpath_type const& realpath,
        Args...                      args)
    { return handle_mode(path_mode(realpath), realpath, args...); }


    bool get_fd_path(realpath_type& realpath, int fd) const {
        // 128-bit.. nah
        char proc_path[sizeof(PITO_PROC_FD "/") + PITO_MAX_FD_LENGTH];
        std::copy(PITO_PROC_FD "/", chilon::end(PITO_PROC_FD "/"), proc_path);

        int const fd_length = static_cast<int>(std::log10(fd));
        if (fd_length > PITO_MAX_FD_LENGTH)
            return false;

        char *proc_ptr = proc_path + sizeof(PITO_PROC_FD "/") + fd_length;

        for (*proc_ptr = '\0'; fd > 0; fd /= 10)
            *(--proc_ptr) = '0' + (fd % 10);

        return ::realpath(proc_path, realpath);
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

// if FileMustExist is true, it may still be possible that the file may
// not exist, depending on the arguments to the open call
template <class Tag, class... Opts>
struct sandbox_call_open : sandbox_call<Tag, Opts...> {
    typedef sandbox_call<Tag, Opts...> base;
    typedef PITO_RETURN(Tag)           return_type;

    template <class Path, class... Args>
    return_type pretend(Path, Args... args) {
        return this->system("/dev/null", args...);
    }

    template <class Path, class... Args>
    return_type pretend(int const dirfd, Path, Args... args) {
        return this->system(dirfd, "/dev/null", args...);
    }

    template <class... Args>
    return_type operator()(Args... args) {
        enum {
            FlagIdx = meta::same<
                typename meta::head<Args...>::type, int>::value + 1
        };

        enum {
            FileMustExist =
                base::FileMustExist && FlagIdx + 1 >= sizeof...(args)
        };

        auto const oflag = argument<FlagIdx>(args...);

        // can't test & with O_RDONLY because O_RDONLY can be 0
        if (FileMustExist && ! (oflag & (O_WRONLY | O_RDWR)))
            return this->system(args...);
        else
            return this->template run<FileMustExist>(args...);
    }
};

template <class Tag>
struct sandbox_fd_call : sandbox_call<Tag> {
    template <class... Args>
    PITO_RETURN(Tag) operator()(int fd, Args... args) {
        realpath_type realpath;
        if (! this->get_fd_path(realpath, fd))
            return this->blacklist();
        else
            return this->handle_path(realpath, fd, args...);
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
                return this->template run<false>(path, mode, stream...);
            else
                return this->system(path, mode, stream...);
        }
        else
            return this->run(path, mode, stream...);
    }
};

} }

#endif
