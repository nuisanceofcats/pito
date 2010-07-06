#include <chilon/conf/cmd/command_line.hpp>
#include <pito/interceptor/application.hpp>
#include <pito/config.hpp>

#include <dlfcn.h>

#include <iostream>

/**
 * @file This is a utility to run a process from the system path with
 *       a specified preload library. e.g.
 *       % pito log ls -R
 *       This is equivalent to executing the following command:
 *          LD_PRELOAD=<pito install location>/libpito_log.so ls -R
 *
 *       % pito -l/usr/local/mylibdir log ls -R
 *          LD_PRELOAD=/usr/local/mylibdir/libpito_log.so ls -R
 */

#define PITO_PROGRAM_VERSION "0.9.1"

namespace pito {

namespace jail = interceptor::jail;
namespace cmd_line = chilon::conf::cmd;
using chilon::conf::value;

bool verbose = false;

inline int main(int argc, char *argv[]) {
    {
        using cmd_line::options_description;
        // TODO: make all arguments from second positional and inclusive the
        //       new argv/argc
        options_description options;

        bool showHelp = false;
        bool silent = false;
        options.add_options()
            ("v,verbose", verbose, "increase verbosity")
            .help("pito " PITO_PROGRAM_VERSION "\nusage: pito [arguments] "
                  "<wrapper library name> <program> [program arguments]")
            ("s,silent", silent, "don't say anything")
            ("l,library-dir", value(jail::preload), "pito library directory")
            ;

        size_t arg_index;
        try {
            arg_index = silent ?
                cmd_line::parser(
                    argc, argv, options).until_positional().index() :
                cmd_line::parser(
                    argc, argv, options).until_positional(std::cerr).index();

            if (2 > argc - arg_index) {
                if (! silent) {
                    if (1 == argc - arg_index)
                        std::cerr << "missing <program> argument" << std::endl;
                    else
                        std::cerr << "missing <wrapper library name> and <program> arguments" << std::endl;
                    std::cerr << options << std::endl;
                }
                return 1;
            }
        }
        catch (cmd_line::invalid_arguments& ) {
            return 1;
        }

        std::string libPath = "libpito_";
        libPath.append(argv[arg_index]);

        interceptor::search_for_preload_library(libPath, jail::preload, jail::preload);

        if (jail::preload.empty()) {
            if (! silent) std::cerr << "library " << libPath << " could not be found at"
                                    " install location or in $" PITO_LD_LIBRARY_PATH << std::endl;
        }
        else {
            if (verbose)
                std::cerr << "load interceptor library ("
                          << jail::preload << ")\n";
            // TODO: initialise environment
            auto lib = dlopen(jail::preload.c_str(), RTLD_LAZY);
            if (! lib) {
                std::cerr << "could not dlopen library\n";
                return 1;
            }

            auto lib_name = std::string(argv[arg_index]) + "_init";
            auto init = dlsym(lib, lib_name.c_str());
            if (init) {
                if (verbose)
                    std::cerr << "init interceptor library ("
                              << jail::preload << ")\n";

                typedef int (*init_ptr)(int, int, char *[]);
                arg_index =
                    reinterpret_cast<init_ptr>(init)(arg_index, argc, argv);
            }
            else ++arg_index;

            jail::enforce_environment();
            execvp(argv[arg_index], argv + arg_index);
        }
        return 1;
    }
}

}

int main(int argc, char *argv[]) {
    return pito::main(argc, argv);
}
