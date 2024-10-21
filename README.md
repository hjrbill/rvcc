## rvcc

本课程基于 Rui 的[chibicc](https://github.com/rui314/chibicc)，@sunshaoce 和@ksco 将其由原来的 X86 架构改写为 RISC-V 64 架构，同时加入了大量的中文注释，并且配有 316 节对应于每一个 commit 的课程，帮助读者可以层层推进、逐步深入的学习编译器的构造。

### rvcc 课程资料

课程用到的资料（环境构建，Q&A 等）都在[rvcc-course](https://github.com/sunshaoce/rvcc-course)。

如需发起 Issue 或者 PR，或者是其他问题，也请访问：https://github.com/sunshaoce/rvcc-course。

### 构建

项目的构建命令为：`make`。

（可选）项目使用 CMake 的构建命令为：

```shell
cmake -Bbuild .
cd build/
make
```

### RISC-V 介绍

RISC-V 是一个开源的精简指令集，相较于常见的 X86、ARM 架构，其简单易学，并且发展迅猛。现在已经出现了支持 RISC-V 的各类设备，未来还将出现 RISC-V 架构的笔记本电脑，可谓是前景一片光明。

### chibicc

[chibicc](https://github.com/rui314/chibicc)是 Rui 开发的一个 X86 架构的迷你编译器。Rui 同时也是 8cc、9cc、mold、lld 等著名项目的主要开发者，chibicc 是他最新的编译器项目。chibicc 项目以 commit 为阶段，借助于 316 个 commits 实现了一个能够编译 Git 等项目的 C 编译器，同时层层深入的课程，也大大降低了难度，帮助更多人来上手 chibicc。
