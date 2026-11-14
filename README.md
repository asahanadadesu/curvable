# Curvable

一款用于音乐制作的音频插件：在插件内的卷帘中为每个音符单独绘制音高曲线（slide、vibrato 等），和弦中的每个音符都能走自己的路线。

Curvable 不内置音源，而是托管你已有的音源。即使音源不支持 MPE，也能通过多实例的方式实现逐音符弯音。主要面向 FL Studio。

> 开发中，尚无可用版本。

## 目录结构

```
Curvable/
├── src/        插件源代码
├── tests/      测试
├── assets/     界面资源（图片、字体等）
├── docs/       公开文档
├── scripts/    构建 / 打包脚本
└── libs/       第三方依赖（JUCE 以 git submodule 引入）
```

## 构建

需要：CMake ≥ 3.25、C++20 编译器（Windows 上为 Visual Studio 2022 或更新版本；macOS 上为 Xcode）。

```bash
git clone --recursive <repo-url>
cd Curvable
cmake -B build -S .
cmake --build build --config Release
ctest --test-dir build -C Release
```

产物位于 `build/Curvable_artefacts/Release/`（VST3、Standalone，macOS 上另有 AU）。

### Windows 开发调试

以管理员身份运行一次以下脚本，它会在系统 VST3 目录（`C:\Program Files\Common Files\VST3`）中创建一个指向编译产物的链接。之后每次重新编译，宿主加载的就是最新版本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\link-dev-vst3.ps1
```

注意：宿主加载插件期间，插件文件会被占用，此时重新编译会失败。请先在宿主中移除插件，或关闭工程。

## 许可证

[GNU AGPLv3](LICENSE)。本项目基于 [JUCE](https://juce.com/)，使用其 AGPLv3 开源授权。
