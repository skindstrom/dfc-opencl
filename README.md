# DFC
High performance string matching algorithm

## Requirements
- CMake (3.1 or greater)
- OpenCL (1.2 or greater)
- C compiler
- C++ compiler (C++11 or greater)
- git

## Building
```sh
mkdir build
cd build
cmake ..
make
```

## Example
In the build folder:
```sh
./example/example
```

## Testing
In the build folder:
```sh
./tests/tests
```

## Reference
Byungkwon Choi, JongWook Chae, Muhammad Jamshed, KyoungSoo Park, and Dongsu Han, _DFC: Accelerating String Pattern Matching for Network Applications_, NSDI 2016 [PDF](http://ina.kaist.ac.kr/~dongsuh/paper/nsdi16-paper-choi.pdf)

## License
`CC BY-NC-SA` (Attribution-NonCommercial-ShareAlike)

The code in this repository is a heavily refactored version of [nfsp3k/DFC](https://github.com/nfsp3k/DFC)