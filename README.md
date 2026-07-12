# stage0-macos

**work in progress** boostrap binary seed for MacOS, the goal is bootstrap a compiler from scratch, without relying on tools which are not installed by default in a fresh MacOS System (not even xcode command line tools).

## Project Structure
- `hex0.sh`: Hex Monitor written in shell script, hex0 file in, raw bytes out, for more details see: https://bootstrapping.miraheze.org/wiki/Stage0
- `hello_macos-x86_64.hex0`: A minimal hello_world mach-o executable, written in [Hex0 format](https://bootstrapping.miraheze.org/wiki/Hex0).

For unix/linux systems, see [stage0-posix](https://github.com/oriansj/stage0-posix).

## Example
Compile the `hello_macos-x86_64.hex0` example, works in Apple Silicon with Rosetta or any Intel Mac.
```shell
# Make sure the hex0.sh is executable
chmod +x ./hex0.sh

# Assembly the example programn
./hex0.sh ./hello_macos-x86_64.hex0 > hello_world

# Make it executable
chmod +x hello_world

# Works in Apple Silicon with Rosetta or any Intel Mac.
./hello_world
```
TODO: Will push a Aarch64 hex0 example once a figure out how to create a portable executable without depending on apples dyld.

## Mach2Hex0
Tool for convert a macos executable to Hex0, Mach-O specific headers are defined in `mach2hex0.h`.
Dependencies: Just a C compiler with libc.
OBS: only tested on MacOS and Debian.
```shell
# Compile mach2hex0
cc -std=c99 -I. -o ./mach2hex0 ./mach2hex0.c

# print the mach-o header of mach2hex0 itself.
./mach2hex0 '<mach-o executable path here>'
```
