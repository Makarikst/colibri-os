[English](README.md) | [Русский](README.ru.md) | [中文](README.zh.md)
<h1>Colibri OS</h1>

![版本](https://img.shields.io/badge/version-0.8-blue)
![状态](https://img.shields.io/badge/status-stable-green)
![平台](https://img.shields.io/badge/platform-i386-lightgrey)
![许可证](https://img.shields.io/badge/license-MIT-yellow)

**Colibri OS** 是一个为 **i386** 架构编写的小型业余操作系统，使用 C 和汇编语言开发。它通过 **multiboot** 启动，运行在 **32 位保护模式**下，拥有自己的命令行、内存文件系统和内置命令集。

## 功能特性

- **VGA 文本模式** 80x25，支持 16 种颜色
- **交互式命令行**，提示符为 `colibri:/users/alpha>`
- **内存文件系统**（文件和目录）
- **内置计算器**，支持 `+ - * / % ^`
- **数学函数**：`sqrt`、`pow`、`gcd`、`lcm`、`prime`、`rand`、`hex`、`bin`
- **环境变量**和**命令别名**
- **进程和网络的占位实现**
- **Multiboot 启动** —— 兼容 QEMU、GRUB、VirtualBox

## 快速开始

### 依赖

- `x86_64-elf-gcc` —— 交叉编译器
- `x86_64-elf-ld` —— 链接器
- `qemu-system-i386` —— 模拟器

### 编译与运行

    git clone https://github.com/Makarikst/colibri-os.git
    cd colibri-os
    ./build.sh

编译会生成 **`colibri.cos`** —— ELF 内核镜像。QEMU 会自动运行它。

手动运行：

    qemu-system-i386 -kernel colibri.cos -m 16M -net none -vga std

## 项目结构

    colibri-os/
    ├── build.sh           # 编译与运行脚本
    ├── linker.ld          # 链接脚本（内核加载到 0x100000）
    ├── kernel_entry.S     # 入口点，multiboot 头
    ├── kernel.c           # 内核：VGA、命令行、文件系统、命令
    ├── kmalloc.c/.h       # 堆分配器
    ├── utils.c/.h         # 工具：字符串、数字、RTC、端口
    └── colibri.cos        # 编译后的 ELF 镜像

## 命令行指令

在命令行输入 `help` 查看完整列表。

### 系统

| 命令 | 说明 |
|---------|-------------|
| `help` | 显示所有命令 |
| `ver` | 系统版本 |
| `banner` | 显示启动画面 |
| `clear` | 清屏 |
| `date` | 日期和时间 |
| `mem` | 内存信息 |
| `heap` | 堆统计 |
| `history` | 命令历史 |
| `reboot` | 重启 |
| `shutdown` | 关机 |

### 文件

| 命令 | 说明 |
|---------|-------------|
| `ls [path]` | 列出文件 |
| `pwd` | 当前目录 |
| `cd <path>` | 切换目录 |
| `mkdir X` | 创建目录 |
| `touch X` | 创建文件 |
| `rm X` | 删除 |
| `cat <file>` | 显示文件 |
| `write <f> <t>` | 写入文本 |
| `cp <src> <dst>` | 复制 |
| `mv <src> <dst>` | 移动 |
| `tree` | 文件树 |
| `stat <file>` | 文件信息 |
| `wc <file>` | 行数 / 单词数 / 字节数 |
| `grep <pat> <file>` | 在文件中搜索 |

### 数学

| 命令 | 说明 |
|---------|-------------|
| `calc <expr>` | 计算器：`+ - * / % ^` |
| `sqrt <n>` | 平方根 |
| `pow <a> <b>` | 幂运算 |
| `prime <n>` | 素数检测 |
| `gcd <a> <b>` | 最大公约数 |
| `lcm <a> <b>` | 最小公倍数 |
| `hex <n>` | 转十六进制 |
| `bin <n>` | 转二进制 |

### 环境

| 命令 | 说明 |
|---------|-------------|
| `env` | 显示变量 |
| `setenv K=V` | 设置变量 |
| `alias X=Y` | 创建别名 |
| `unalias X` | 删除别名 |

## 工作原理

### 启动

内核是一个**符合 multiboot 规范的 ELF 文件**。QEMU 或 GRUB 读取前 8 KB 中的 multiboot 头，然后跳转到 `_start`。

    .section .multiboot, "a"
        .long 0x1BADB002    # 魔数
        .long 0x00000003    # 标志
        .long -(MAGIC + FLAGS)

### 内存布局

- 内核：`0x100000`（1 MB）
- 栈：`0x9F000`
- 堆：`0x200000`（2 MB）

### 文件系统

内存文件系统：`FsObject` 数组，每个对象包含类型（`FILE` / `DIR`）、名称、父目录，每个文件最多 4 KB 数据。

## 开发计划

- [x] VGA 输出和启动画面
- [x] 交互式命令行
- [x] 内存文件系统
- [x] 计算器和数学函数
- [x] 环境变量和别名
- [ ] 方向键滚动历史
- [ ] 真正的进程调度
- [ ] 完整键盘驱动
- [ ] ATA PIO 磁盘驱动
- [ ] 网络（NE2000 / RTL8139）
- [ ] 带 GRUB 的可启动 ISO

## 在真实硬件上运行

编译出 `colibri.cos`，把它和 GRUB 一起放到 U 盘上。示例 `grub.cfg`：

    menuentry "Colibri OS" {
        multiboot /boot/colibri.cos
        boot
    }

在 U 盘上安装 GRUB：

    sudo grub-install --target=i386-pc --boot-directory=/mnt/usb/boot /dev/sdX

## 许可证

MIT

## 作者

**Asde LLC** —— 业余项目，2026 年。

问题、bug、建议 —— 请开 [issue](https://github.com/Makarikst/colibri-os/issues)。

---

*Colibri（蜂鸟）是世界上最娇小的鸟类。就像这个操作系统一样。*
