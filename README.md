# stage0-macos

**WIP** boostrap binary seed for MacOS, the goal is bootstrap a compiler from scratch, without relying on tools which doesn't come by default in a fresh MacOS Install.

## Project Structure
- `hex0.sh`: Hex Monitor written in shell script, hex0 file in, raw bytes out, for more details see: https://bootstrapping.miraheze.org/wiki/Stage0
- `hello_macos-x86_64.hex0`: A minimal hello_world mach-o executable, written in [Hex0 format](https://bootstrapping.miraheze.org/wiki/Hex0).

## Example
Compile the `hello_macos-x86_64.hex0` example:
```shell
# Make sure the hex0.sh is executable
chmod +x ./hex0.sh

# Assembly the example programn
./hex0.sh ./hello_macos-x86_64.hex0 > hello_world

# Make it executable
chmod +x hello_world

# Works in Apple Sillicon with Rosetta or Intel Mac.
./hello_world
```

