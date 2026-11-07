# 0.7.0：轮盘不可见与 LB + Y 反馈调查

初步调查日期：2026-10-14。下文保留初步证据；2026-10-15 已按用户授权完成测试修复，见文末。正式发布包和玩家配置未改动。

**最新状态：两版菜单绘制实验均未通过本地显示实测，现已撤回，渲染器恢复为正式 0.7.0 原始代码。COC 和手柄修复保留。本文后面的菜单目标方案属于历史记录；当前交付见 [RESTORE_STABLE_RENDERER_0.7.0.md](RESTORE_STABLE_RENDERER_0.7.0.md)。**

后续澄清：用户确认这份原始不可见日志来自闭源帧生成器加 ENB，并非已确认的 TRP 环境。另有独立 TRP 用户报告。公开爱发电链接未返回可读详情，因此未确定闭源模组的版本或接口。另一个从标题画面 `coc` 后的初始化问题及新测试包见 [COC_STARTUP_0.7.0.md](COC_STARTUP_0.7.0.md)。

后续测试发现本地也有“暂停但不显示”，上一轮对当前绑定 UI 目标的假设不可靠；v2 已改为显式请求引擎 UI 目标后读取实际绑定。下文保留历史记录，最新修正和验证见 [NATIVE_UI_TARGET_REGRESSION_0.7.0.md](NATIVE_UI_TARGET_REGRESSION_0.7.0.md)。

## 提供的日志

来源：`G:/Downloads/Browser/FavoriteWheel.log`，FavoriteWheel 0.7.0，Skyrim SE 1.5.97。

- D3D11 渲染器、材质和输入 Hook 均初始化成功。
- 22:13:32.939 正常打开轮盘；22:13:32.965 选择 `engine UI framebuffer 1280x720 format=28`。
- 字体成功使用 `C:/Windows/Fonts/msyh.ttc`，非应急像素字体。
- 22:13:33.175 开盘动画记录 5,526 顶点、16,740 索引；随后仍有正常开关和物品使用记录。
- 没有渲染初始化失败或禁用渲染的错误。第一次字体构建约 173 ms 能解释首开短暂延迟，不能解释持续不可见。

这证明输入和生成绘制数据正常，不能证明绘制目标最终被显示。当前日志没有渲染模组清单、交换链与 framebuffer 的资源身份、绑定后被第三方重定向的实际 RTV，以及后续合成阶段。因此无法仅凭此日志断定是 TRP，也不能把它当作手柄用户的日志。

## Theo's Render Pipeline

官方源码：https://github.com/theosw/theosrenderpipeline 。本地静态调查快照 `d6e8172ff442385c15c2767757ddcd968c8ba6e6`，README 0.4.1。

原版收藏菜单走游戏 Scaleform 菜单流程。FavoriteWheel 是独立 ImGui/D3D11，在交换链 Present Hook 内、调用原 Present 之前绘制，并无条件优先选择引擎 `kFRAMEBUFFER`。渲染模组可以让该目标与最终显示资源分离；选择引擎字段本身并不能保证跟随所有模组的当前 UI 层。

TRP 的 `GameSwapChain.cpp`、`UpscalerDeviceHooks.cpp`、`FrameGen/SourceFrameCoordinator.h` 显示：它有游戏侧代理交换链，Present 内还会结束 UI pass、准备画面并合成独立 UI。`UpscalerHooks.cpp` 的 OM 重定向只识别特定游戏侧、ENB 和原生 UI 资源，并受当前 pass 状态限制。该结构存在与轮盘当前目标选择和绘制阶段不匹配的风险，符合“能交互但看不到”的症状；用户侧准确路径尚未验证。

`compatibility/ImGuiCompat/ImGuiIntegration.cpp` 的八个已适配生产者不包含 FavoriteWheel。不能认为“TRP 支持 ImGui”就自动支持本模组。`SolFG_GetLateOverlayBridge` 在该快照中保留 ABI 但 query/begin 返回 0；新增 StartupOverlayBridge 有阶段门控。仅探测导出或照搬 startup 接口不能保证游戏中的轮盘适配。

后续适配应明确当前 UI 目标及其合成时机，在适合的 UI pass 绘制，处理未安装 TRP、不同版本以及 CS 路径。需收集 TRP 版本、其日志及其他渲染模组信息，并做同场景启用/停用 TRP 的对照，再验证原版、CS 和 TRP。当前没有安装或执行外部 DLL，也没有声称游戏内兼容已验证。

## LB + Y

本模组使用 ButtonEvent 保留手柄按下/松开状态，不调用 Papyrus IsKeyPressed。当前可以表示 LB + Y。收藏和功能轮盘共享主键，功能轮盘修饰键匹配时优先。

默认 `GamepadActionModifier=274`（LB）。若用户把收藏设为 Y + LB，同时保留功能 LB，两者完全重合，将进入功能轮盘，而非收藏轮盘。这是明确的策略结果，但还不能解释用户“完全不开”的所有情况。

建议通过 FavoriteWheel 自己的设置绑定，保留游戏原有 Jump/Favorites 映射；示例 INI 的 `[Controls]` 项目：

```ini
[Controls]
GamepadHotkey=279
GamepadModifier=274
GamepadActionModifier=275
```

279=Y，274=LB，275=RB：LB + Y 收藏；RB + Y 功能；轮盘关闭且没有修饰键时 Y 不满足入口条件，正常交还游戏。不要把功能修饰键设为 -1 来“禁用”：-1 表示无修饰键，会优先匹配功能入口。

用户如已在游戏中把 Favorites 改到 Y，应恢复原映射，否则跳跃被替换是游戏 ControlMap 的结果。LB 的轮盘内分类用途只在开盘后生效，不会仅因使用默认分类按钮就阻止闭盘入口。

若独立配置后仍不能打开，需要他自己的 INI 与对应 `Controller modifier` / `Controller entrance` 日志，检查 `key=279`、修饰键状态、`mode` 和 `blocked`，再判断其他映射模组是否移除或重写输入事件。

## 2026-10-15 测试修复

用户补充了 Kontrol 自定义映射的反馈，并授权先处理已知问题。

- 新增独立 `FavoriteWheelRenderHost` 原生菜单，只在 `PostDisplay` 的 UI 阶段绘制。它没有输入上下文、暂停或光标控制，也不持有主轮盘的输入生命周期；关闭输入菜单后仍可绘制收起动画。Present Hook 仅初始化设备和重置每帧绘制标记，不再向已经合成的旧 UI 纹理绘制。
- 从当前 D3D11 输出绑定获取 UI 目标及真实纹理尺寸，跟随原版、CS/TRP 的 UI 重定向，不按某个渲染模组名称硬编码。无有效 UI 目标则报告错误、取消轮盘并停止接管入口。新日志标记 `route=menu-postdisplay-v1`。
- 读取游戏 `Favorites` 的 `UserEventMapping.modifier`（偏移 0xA）。跟随游戏主键时，入口同时要求原映射的修饰键。缺少 LB 的普通 Y 不匹配入口，也不进入“替换原版入口”的吞事件分支。
- 同一收藏/功能手柄修饰键冲突时优先收藏，修复继承 Kontrol LB+Y 后默认功能 LB 抢走入口的问题。原版 Up 收藏、LB+Up 功能保持原行为。另配独立的功能修饰键仍可进入功能轮盘。明确配置轮盘主键时不继承游戏修饰键；继承原生组合时功能入口也保留原生组合要求。
- 输入日志添加 `nativeRequired`、`followNative` 和 `binding=native-chord-v1`。未改 Kontrol/controlmap.txt，也未重新分配 Jump、Wait、Tween 或玩家的其他按钮。

本地验证：编译、原生字段偏移验证、手柄回归测试和 WARP 目标/合成测试。WARP 验证当前绑定目标的资源与尺寸，以及 UI 绘制先于合成时可进入显示结果；它不能证明游戏实际调用了菜单回调或替代 TRP/Kontrol 环境实测。

最终检查：17 个测试套件全部通过，运行时测试使用七份真实 Address Library 数据库。测试 DLL SHA256 为 `B1AD9C1769B8AA1911B4DB93DC1EE1EAA19EAE7CC55D27EAB32990D07FE0F85A`。已同步到本地 MO2 安装目录，只替换 DLL；另外 98 份现有文件的哈希保持不变。自动部署配置已恢复。独立测试安装包保留原版 0.7.0 全部资源，附英文测试说明且不含主 INI；完整测试源码包逐文件校验。原版 0.7.0 两个正式 ZIP 保持原哈希。

该版本供 0.7.0 用户替换 DLL 测试，游戏内版本元数据保持 0.7.0；以新增日志标记与包内 SHA256 区分。报告配置建议是初步调查时的手动绕行方案，修复后的默认“跟随游戏”可直接使用原有组合。若同一 LB 已包含在原生收藏组合中，功能入口需要在设置中选择另一个修饰键，不强制占用 RB 或其他按钮。
