# DFC
High performance string matching algorithm

## Requirements
- CMake (3.1 or greater)
- OpenCL (1.2 or greater)
- C compiler
- C++ compiler (C++11 or greater)
- git

## Optimization options
Edit the respective fields in `CMakeLists.txt` in the root of the repository.
Some options, such as max pattern length, exists in `src/shared.h`.

## Building
```sh
mkdir build
cd build
cmake ..
make
```

This builds a shared library `libdfc.so` that is used for the benchmarks.

### Release build
Run cmake with the flag `-DCMAKE_BUILD_TYPE=Release`:
```sh
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Debug build
Run cmake with the flag `-DCMAKE_BUILD_TYPE=Debug`:
```sh
cmake .. -DCMAKE_BUILD_TYPE=Debug
```


## Example
In the **build** folder:
```sh
./example/example
```

## Testing
In the **build** folder:
```sh
./tests/tests
```

## Code structure
- `example`: a simple example of how to use the library
- `tests`: an extensive unit test suite to see how DFC is supposed to work
- `src`: source code
  - `dfc.c`: The preprocessing of DFC
  - `search/*`: Files used for matching
    - `search-gpu.c`: Used for GPU and HET matching
    - `search-cpu.c`: Used for CPU matching (and second phase HET)
  - `memory.c`: Handles buffers and some OpenCL logic
  - `shared.h`: Some contants used for both the CPU and GPU version 
    - (not sure if still true, it was when I started)
    - **This file may contain some interesting contants**
  - `shared-functions.h`: Functions used by both the CPU and GPU
  - `contants.h`: Exit codes

## Reference
Byungkwon Choi, JongWook Chae, Muhammad Jamshed, KyoungSoo Park, and Dongsu Han, _DFC: Accelerating String Pattern Matching for Network Applications_, NSDI 2016 [PDF](http://ina.kaist.ac.kr/~dongsuh/paper/nsdi16-paper-choi.pdf)

## License
`CC BY-NC-SA` (Attribution-NonCommercial-ShareAlike)

The code in this repository is a heavily refactored version of [nfsp3k/DFC](https://github.com/nfsp3k/DFC)
