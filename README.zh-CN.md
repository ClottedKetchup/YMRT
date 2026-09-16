# YMRT

[English](README.md) | **中文**

基于 CUDA 的简单 IPR 路径追踪器。

## 构建

### 环境要求

测试环境为 Windows 操作系统，已验证以下配置:

- RTX 3060 Laptop — CUDA 11.8.89，MSVC 19.29.30154(VS2019)
- RTX 5080 — CUDA 13.2.51，MSVC 19.44.35225.0(VS2022)

其他要求:

- 支持 OpenGL 4.5 的显卡
- CMake 3.20 或更高版本

所有第三方库([glad](https://github.com/Dav1dde/glad)、[glfw](https://github.com/glfw/glfw)、[assimp 5.1.0](https://github.com/assimp/assimp)、[imgui](https://github.com/ocornut/imgui)、[glm](https://github.com/g-truc/glm)、[stb](https://github.com/nothings/stb))均已内置于 `external/` 目录，无需手动配置依赖。

### 构建步骤

#### 命令行

1. 在代码目录打开终端。

2. 创建 build 文件夹:

```bash
mkdir build
```

3. 配置工程(使用 Visual Studio 生成器):

```bash
cmake -S . -B build
```

4. 命令行编译:

```bash
MSBuild build/YMRT.sln -p:Configuration=Release -p:Platform=x64 -m
```

可执行文件输出于 `build/Release/YMRT.exe`，构建过程会将 `shaders/`、`model/`、`test_assests/` 拷贝到可执行文件旁边。

#### 图形界面(Visual Studio)

1. 打开 CMake GUI，将「源代码目录」设为代码目录，将「构建目录」设为代码目录下的 `build` 文件夹。

2. 点击 Configure，选择对应的 Visual Studio 版本与 `x64` 平台;配置完成后点击 Generate。

3. 用 Visual Studio 打开生成的 `build/YMRT.sln`。

4. 编译解决方案(Release 配置)。

5. 将 `YMRT` 设为启动项目，然后运行。

> [!WARNING]
> 修改 `.h` 或 `.cuh` 文件时，CUDA 源文件可能不会自动重新编译，需要手动重新编译。

## 特性

- **wavefront 路径追踪**
- **PBR 流程中常见的 BSDF**
- **体积渲染**
- **纹理系统**——图像纹理与嵌套纹理
- **低差异序列采样**
- **交互式编辑器**——对象属性可以实时编辑
- 可以使用内部格式临时保存场景

**暂未实现**:使用 HDR 贴图的环境光照，以及使用 LightBVH 的多光源采样。

## 界面布局

![程序界面布局](layout.png)

## 测试

![泳池场景](pool_test.png)

场景:`src/model/pool_test.ymtmp`

![体积雾中的 Sponza 场景](Sponza_fog_2.png)

场景:`src/model/Sponza_Test.ymtmp`

## 使用

详见 [usage.zh-CN.md](usage.zh-CN.md)。

## 素材来源

`src/model` 下的测试场景，除 `CornellBox_Test.ymtmp` 和 `pool_test.ymtmp` 之外均来自 <https://casual-effects.com/g3d/data10/index.html>;`pool_test.ymtmp` 所用贴图来自 Quixel Bridge;`src/test_assests` 中的贴图来自 [Poly Haven](https://polyhaven.com/)。如不慎侵犯了您的著作权，请联系我删除相关模型。
