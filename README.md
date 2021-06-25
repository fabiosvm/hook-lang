
[![CI](https://github.com/fabiosvm/hook-lang/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/fabiosvm/hook-lang/actions/workflows/ci.yml)

# Hook 

The Hook Programming Language

## Building

```
git clone https://github.com/fabiosvm/hook-lang.git
cd hook-lang
cmake -B build
cmake --build build
```

### Building a release

```
cmake --build build --config Release
```

## Running an example

```
cd bin
hook < ../examples/sum.hook
```

### Displaying the bytecode

```
hook --disasm < ../examples/sum.hook
```
