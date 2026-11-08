**简体中文** | [English](README.md)

# KineTexture

[动作通道语义](CHANNEL_SEMANTICS.md) · [CLI 转换工具说明](tools/gkclip_export/README.md) · [Python 实验包说明](python/README.md) · [两骨 IK 示例](examples/two_bone_ik/README.md)

## 这是什么

KineTexture 是一个 C++20 运动学与空间数学核心库（Header-only 纯标头库），提供正向运动学（FK）、两骨逆向运动学（TwoBoneIK）、四元数旋转和动作曲线采样。核心库不依赖游戏引擎、图像渲染或外部模型权重，也不会推断角色配置或自动修正接触。

仓库另有脱机命令行工具 `gkclip2vmd` 与 `gkclip2smpl`，可将 `.gkclip` 导出为 MMD 的 `.vmd` 或 SMPL/SMPL-X 的 `.npz`，供外部工具检视。内置预设是显式选择的近似朝向预览，不是任意源转换器的精确逆变换；源姿态、接触和目标模型画面仍需分别验证。

API 仍处于实验阶段，不提供稳定二进制 ABI 或跨任意版本的兼容承诺。集成时应固定已审阅的提交，同时使用该版本的头文件、CMake 包和文档。

KineTexture 是 SDK 与离线工具，不是 GakuLoader 插件，无需放入游戏目录或 `plugins/`。GakuMotion 使用其构建时配套的核心标头。本文提供源码构建与使用入口；仓库没有在本页提供预编译包下载。

## 准备工作与依赖

### 运行环境
- **命令行工具使用者**：在 Windows x64 上运行本地编译的原生工具，无需配置 Python 或 Unity 环境；Python 标准库仅参与可选开发测试。
- **C++ 开发者**：KineTexture 核心库为 Header-only 设计，集成到自己的工程时仅需引入标头文件，无需链接第三方二进制依赖。

### 源码编译环境要求
- Windows 10 / 11 64 位
- CMake 3.20 或更高版本
- Ninja 构建工具
- Visual Studio 2022+ MSVC C++20 工具链；当前记录的原生验证使用 Windows/MSVC，其他编译器与平台仍需分别验证。
- Python 3 标准库用于可选 CLI 合成测试和源码清单检查，不参与原生工具运行。

### 项目结构概览
```text
KineTexture/
├── src/KineTexture/            # 核心 C++20 标头文件（Header-only）
├── tools/                      # 命令行转换工具源码
│   └── gkclip_export/          # gkclip2vmd 与 gkclip2smpl 实现
├── examples/                   # 使用示例（如两骨 IK 与独立 CMake 接入）
├── python/                     # 供算法研究与数值验证的独立 Python 包
└── CHANNEL_SEMANTICS.md        # 动作通道与坐标系契约文档
```

## 使用方法与命令行操作

### 1. 使用 CLI 工具转换动作文件

按下方源码构建后，可执行程序位于 `build/cli/` 或 `bin/` 目录中。

#### 常用转换命令

```powershell
# 查看工具帮助与可用参数
.\build\cli\gkclip2vmd.exe --help
.\build\cli\gkclip2smpl.exe --help

# 将 .gkclip 转换为 MMD 动作（.vmd）
# 使用近似 MMD 朝向预设，显式允许省略未映射通道
.\build\cli\gkclip2vmd.exe motion.gkclip --preset canonical-mmd --body-only --output review.vmd

# 将 .gkclip 转换为 SMPL 动作（.npz）
.\build\cli\gkclip2smpl.exe motion.gkclip --preset canonical-smpl --body-only --output review_smpl.npz

# 将 .gkclip 转换为 SMPL-X 动作（.npz），并使用标准中性体型预览
.\build\cli\gkclip2smpl.exe motion.gkclip --preset canonical-smplx --body-only --neutral-shape --output review_smplx.npz
```

#### 转换报告说明
每次转换都会在输出文件同目录下生成对应的 `OUTPUT.report.json`，记录映射配置、源和输出时基、有效骨骼掩码，以及省略的骨骼、相机和表情通道。已有输出、报告或 profile 文件会被拒绝覆盖；示例中的输入文件需自行准备。

#### 自定义骨骼映射配置（Profile）
如果需要针对自定义模型调整骨骼层级或旋转基底，可以导出并修改配置文件：

```powershell
# 导出近似预设为可编辑的配置文件
.\build\cli\gkclip2vmd.exe --preset canonical-mmd --write-profile review.profile

# 使用自定义配置文件执行转换
.\build\cli\gkclip2vmd.exe motion.gkclip --profile review.profile --body-only --output review_custom.vmd
```

### 2. 在 C++ 项目中引入 SDK

在独立的 CMake 项目中，可以通过 `find_package` 直接引入安装好的 KineTexture：

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(KineTextureExample LANGUAGES CXX)

find_package(KineTexture CONFIG REQUIRED)

add_executable(kinetexture_example main.cpp)
target_link_libraries(kinetexture_example PRIVATE KineTexture::Core)
```

`main.cpp`:
```cpp
#include <KineTexture/KineTexture.hpp>
#include <cmath>

int main()
{
    // 创建一个绕 Z 轴旋转 90 度的四元数
    const auto q = KineTexture::Quaternion::FromEulerDegreesXYZ(0, 0, 90);
    // 对 X 轴单位向量进行旋转，理论结果为 (0, 1, 0)
    const auto direction = q.Rotate({1, 0, 0});
    return (std::abs(direction.x) < 1e-5f && std::abs(direction.y - 1) < 1e-5f) ? 0 : 1;
}
```

## 常见问题与排查

1. **转换时提示存在未识别通道而报错**：
   - 默认模式下，若源动作中存在目标格式不支持的表情通道、相机通道或非标准骨骼旋转，转换器会报错终止。
   - `--body-only` 显式允许省略未映射的旋转、相机和表情通道，并在 `.report.json` 中记录；它不会修复其他格式或映射错误，也不会补出缺失的实测姿态。
2. **导出的 MMD 动作在部分模型上姿态异常**：
   - `canonical-mmd` 声明了包含“上半身3”的 52 骨骼层级。目标模型使用其他层级（包括常见的 51 骨骼变体）时，需要匹配其父子关系和骨骼参考系；忽略中间骨骼也会改变后代朝向。该预设不保证源姿态或接触一致。
3. **VMD 插值曲线更新（2026-11-04）**：
   - 早期导出的 VMD 文件在部分第三方 MMD 查看器中可能出现关键帧之间位移突变的问题。当前版本导出器已将全部四组冗余插值控制块填入线性参数，修复了 Z 轴平移与旋转插值曲线，建议使用新版 CLI 重新导出。
4. **数学计算契约**：
   - 四元数采用 Hamilton 约定，以 xyzw 顺序存储，乘法顺序为 `parent * local`。
   - 长度单位默认为米（m），时间戳单位为秒（s）。

## 源码构建

在 Visual Studio x64 Native Tools 终端或配置好 C++ 环境的 PowerShell 中运行以下命令：

### 编译核心库测试与安装 SDK
```powershell
$installPrefix = Join-Path (Get-Location) 'build/install'
# 配置构建工程（关闭 CLI 工具，仅构建核心库测试）
cmake -S . -B build/core -G Ninja -DCMAKE_BUILD_TYPE=Release -DKINETEXTURE_BUILD_GKCLIP_TOOLS=OFF
# 编译
cmake --build build/core
# 运行单元测试
ctest --test-dir build/core --output-on-failure
# 安装 SDK 至指定目录
cmake --install build/core --prefix "$installPrefix"
```

### 编译 CLI 转换工具（`gkclip2vmd` 与 `gkclip2smpl`）
```powershell
# 配置并编译 CLI 工具
cmake -S . -B build/cli -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cli
# 运行 CLI 验证测试
ctest --test-dir build/cli --output-on-failure
```

编译生成的可执行程序位于 `build/cli/` 目录下。

### 许可证
KineTexture 源码采用 [MIT 许可证](LICENSE)。
