# Curvable

[English](README.md) | **简体中文**

Curvable 是一款自带钢琴卷帘的乐器插件，卷帘里的每个音符都有自己的音高曲线。你可以逐个音符地画出滑音、下方滑入、尾音下滑、颤音和 Trill，和弦里的每个音也可以各自弯向不同的方向。

Curvable 本身不发声。它托管你已有的音源，并按需要复制出多个副本，每个弯音走向不同的音符使用一个副本。这样即使音源不支持 MPE，每个音符也能有自己的弯音。支持 MPE 的音源也可以只用一个实例。

Curvable 主要为 Windows 上的 FL Studio 设计：FL Studio 的钢琴卷帘无法让和弦里的音符各自弯音。

## 功能

- **逐音符的音高曲线。** 添加节点，拖到任意音高，并把每一段从直线调成不同程度的弯曲。节点吸附到半音和网格，按住 Alt 可自由移动。
- **颤音与 Trill。** 可以作用于整个音符，也可以作用于选定的段落，每一段的设置各自独立。深度、速度和渐入长度直接在音符上用手柄调整，速度可以用 Hz，也可以跟随曲速。
- **常用弯音。** 下方滑入、越过回落、推弦再放回、尾音下滑、尾音上挑，都在右键菜单里。
- **曲线预设。** 保存一条曲线，在任何工程里套用到任意多个音符，并按每个音符的长度伸缩。
- **滑音（Glide）。** 把一串音符合并成一个连奏长音，在音高之间滑动；和弦之间也能逐个声部地滑音。
- **切割工具。** 切开音符而不改变听感。
- **任何音源都能独立弯音。** 最多 16 个音符可以同时各自弯音。空闲的音源副本会自动暂停以节省 CPU。
- **MPE 模式**（适用于支持 MPE 的音源）：只用一个音源实例，每个弯音的音符占一个 MIDI 通道。
- **MIDI 导入与导出。** 把 MIDI 文件拖进卷帘，其中的弯音会变成曲线；把音符连同弯音拖到 FL Studio 或其他 DAW，可选每个声部一个通道或 MPE 格式。
- **自动化槽。** 把托管音源的最多 16 个旋钮关联到 FL Studio 可以自动化的参数上，所有音源副本同步跟随。
- **界面支持英语、简体中文、日语、韩语和西班牙语**，有浅色和深色两种主题。

## 系统要求

- Windows 10 或 11（64 位）
- 能加载 VST3 乐器的宿主。Curvable 在 FL Studio 中开发和测试。
- 一个供 Curvable 托管的 VST3 音源

另提供 macOS 版本（VST3 和 AU，macOS 11 或更新）。它们通过了自动化验证，但还没有在实机上测试过。

## 安装

在 [Releases 页面](https://github.com/asahanadadesu/curvable/releases) 下载最新版本。

**Windows：** 运行 `Curvable-<版本>-Windows-Setup.exe`，插件会安装到标准的 VST3 文件夹（`C:\Program Files\Common Files\VST3`）。安装程序暂时没有代码签名，Windows SmartScreen 可能会弹出警告，点 **更多信息**，再点 **仍要运行** 即可。如果想手动安装，解压 `Curvable-<版本>-Windows-VST3.zip`，把 `Curvable.vst3` 复制到 VST3 文件夹。

**macOS：** 把 VST3 或 AU 版本解压到 `~/Library/Audio/Plug-Ins/VST3` 或 `~/Library/Audio/Plug-Ins/Components`。这些版本没有经过公证，macOS 会阻止加载，需要先去掉隔离标记，例如：

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Curvable.vst3
```

然后在宿主里重新扫描插件。在 FL Studio 中，打开 **Options > Manage plugins**，点 **Find installed plugins**。

## 快速上手

1. 在 FL Studio 的通道机架（Channel rack）中，把 **Curvable** 作为乐器添加进来。
2. 点右上角的 **加载音源**，选择一个音源。FL Studio 已经扫描过的音源会直接出现在列表里；也可以把 `.vst3` 文件或 FL Studio 浏览器里的插件拖到 Curvable 上。
3. 用 **绘制** 工具在 Curvable 的卷帘里画音符。
4. 切换到 **曲线** 工具，单击音符添加弯音节点，再上下拖动。右键音符可以添加颤音、Trill、常用弯音和预设。
5. 把底栏的 **弯音** 设成音源的弯音范围，并在音源里设成相同的值。然后在 FL Studio 里播放。

在 Curvable 中按 **F1** 或点 **?**，可以查看全部鼠标和键盘操作。

## 文档

- [用户手册](docs/user-guide.zh-CN.md)：全部功能、设置和常见问题
- [更新记录](CHANGELOG.md)（英文）

## 注意事项

- Curvable 会运行多个音源副本，音源自带的效果（混响、延迟等）会在每个副本上各加一次。请在音源里关掉这些效果，改用宿主的效果器。
- CPU 和内存占用随副本数量增加。采样类音源每多一个副本，就多占一份内存。
- 自由运行的 LFO 和随机的振荡器相位，可能让各个副本听起来略有不同。如果音源支持，请打开重触发（retrigger）。
- Curvable 的弯音范围必须和音源一致。如果弯音听起来过大或过小，请先检查这一项。

## 许可证

Curvable 是自由软件，采用 [GNU Affero 通用公共许可证 v3.0](LICENSE)。它基于 [JUCE](https://juce.com) 开发，使用 [Catch2](https://github.com/catchorg/Catch2) 测试。

---

## 开发者须知

### 编译

需要 CMake 3.25 或更新版本，以及支持 C++20 的编译器（Windows 上为 Visual Studio 2022 或更新版本，macOS 上为 Xcode）。

```bash
git clone --recursive https://github.com/asahanadadesu/curvable.git
cd curvable
cmake -B build -S .
cmake --build build --config Release
ctest --test-dir build -C Release
```

插件输出到 `build/Curvable_artefacts/Release/`（VST3 和 Standalone；macOS 上另有 AU）。

在 Windows 上，以管理员身份运行一次 `scripts/link-dev-vst3.ps1`，它会把系统 VST3 文件夹链接到编译输出，宿主加载的就始终是最新编译的版本。安装正式版之前，请用 `-Remove` 参数删除这个链接，因为安装程序会替换该文件夹里的 `Curvable.vst3`。

### 测试

- `tests/` 是单元测试（Catch2），覆盖曲线模型、编辑操作、声部分配、回放、保存与读取、MIDI 导入导出和翻译。
- `tests/smoke/` 是加载真实 VST3 音源的托管测试：`curvable_hosting_smoke <.vst3 路径>`。
- CI 在每次推送时于 Windows 上运行单元测试和 [pluginval](https://github.com/Tracktion/pluginval)，macOS 每周以及发布时运行。

### 目录结构

```
src/
  model/     音符、音高曲线、编辑操作、工程的保存
  engine/    调度、声部分配、弯音回放
  hosting/   托管音源及其副本
  io/        MIDI 导入导出
  i18n/      界面翻译
  plugin/    插件的处理器和界面
  ui/        钢琴卷帘及其他界面组件
tests/       单元测试和托管测试
scripts/     安装程序脚本和开发辅助脚本
libs/        JUCE 和 Catch2（git 子模块）
```

### 翻译

代码里的界面文字用英文书写，并用 `tr`、`trf` 或 `TRK` 标记，译文在 `src/i18n/Translations.cpp` 中。只要有标记的文字缺少译文，单元测试就会失败，因此新加的文字需要在表里补齐所有语言。

### 发布

1. 更新 `CMakeLists.txt` 中的版本号，并在 `CHANGELOG.md` 中为该版本添加一节。
2. 推送 `v0.1.0` 这样的标签。CI 会在 Windows 和 macOS 上编译和验证，然后发布 GitHub Release，附带 Windows 安装程序和插件压缩包。发布说明取自更新记录。
