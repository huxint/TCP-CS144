Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: https://cs144.stanford.edu

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

Lab 0–7 中文图解：[从字节流到完整网络](writeups/README.md)。用具体例子讲清每个组件，再对照源码和测试。

先看 [TCP 双向架构图](writeups/assets/tcp-architecture.svg)，或浏览 [全部手绘图解与 Excalidraw 源文件](writeups/assets/README.md)。
