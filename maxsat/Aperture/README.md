# Aperture

Aperture is an anytime, complete and incremental MaxSAT solver. It provides an Aplication Programming Interface (API) as well as a command-line interface. It is implemented from scratch in C++ 20 and is available under the MIT License.

This is the version of Aperture submitted to the MaxSAT Evaluation 2026 incremental track.

# Building and Running

Run `make -j` to build the statically compiled executable `aperture_static` in the `build` directory. To run the command-line interface, execute `./build/aperture_static <options> <input_file> <sub_commands>`. For more details on the available options and subcommands, run `./build/aperture_static --help`. The input file should be in the standard WCNF format. Note that you can change the order of the options and the input file, but the subcommands should be at the end of the command.
