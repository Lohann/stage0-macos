# stage0-macos

**work in progress** boostrap binary seed for MacOS, the goal is bootstrap a compiler from scratch, without relying on tools which are not installed by default in a fresh MacOS System (not even xcode command line tools).

## Project Structure
- `hex0.sh`: Hex Monitor written in shell script, hex0 file in, raw bytes out, for more details see: https://bootstrapping.miraheze.org/wiki/Stage0
- `hello_macos-x86_64.hex0`: A minimal hello_world mach-o x86_64 executable, written in [Hex0 format](https://bootstrapping.miraheze.org/wiki/Hex0).
- `hello_macos-arm64.hex0`: A minimal hello_world mach-o ARM64 executable, written in [Hex0 format](https://bootstrapping.miraheze.org/wiki/Hex0).

For unix/linux systems, see [stage0-posix](https://github.com/oriansj/stage0-posix).

OBS: MacOS Kernel disallow ARM64 static binaries:
https://github.com/apple-oss-distributions/xnu/blob/ac9718fb1af618d5ce8678d0dc6e8a58f252216f/bsd/kern/mach_loader.c#L1342-L1347

## Example
Compile the `hello_macos-x86_64.hex0` or `hello_macos-arm64.hex0`.
```shell
# Make sure the hex0.sh is executable
chmod +x ./hex0.sh

# Assembly the example programn
./hex0.sh ./hello_macos-x86_64.hex0 > hello_world_x86
./hex0.sh ./hello_macos-arm64.hex0 > hello_world_arm

# Make it executable
chmod +x hello_world_x86
chmod +x hello_world_arm

# Works in Apple Silicon with Rosetta or any Intel Mac.
./hello_world_x86

# Works in Apple Silicon Macs.
./hello_world_arm
```

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
