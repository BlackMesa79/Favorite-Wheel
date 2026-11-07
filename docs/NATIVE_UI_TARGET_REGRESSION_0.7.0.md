# 菜单 UI 测试版暂停后不可见：目标选择修正

日期：2026-10-15。用户确认使用本地原有环境测试：标题画面 `coc qasmoke` 进入后按 Q，轮盘暂停时间，但界面不可见。

**后续实测：v2 仍不可见，本文所述修正未解决现场，已撤回菜单绘制实验。当前恢复正式 0.7.0 渲染器，保留 COC 和手柄修复，详见 [RESTORE_STABLE_RENDERER_0.7.0.md](RESTORE_STABLE_RENDERER_0.7.0.md)。下文为历史调查记录。**

## 证据与错误假设

本地可读日志为 `D:/MyFiles/Documents/My Games/Skyrim Special Edition/SKSE/FavoriteWheel.log`，最后写入 2026-10-15 13:02:35。该日志包含前一轮 `menu-postdisplay-v1` 的普通读档记录，不能把它当作新 COC 兜底路径的运行证明。

但其中的多次开关有一致现象：暂停开盘时目标为 2560×1440、format=24（R10G10B10A2_UNORM）；取消暂停并收起时切为 format=28（R8G8B8A8_UNORM）。菜单回调已执行且有非零顶点，单纯“回调被调用”或“已绑定一个 RTV”不能证明该 RTV 是参与最终 UI 合成的目标。

前一轮把菜单回调入口的当前 D3D11 输出目标直接视作 UI 层，假设不成立。回调可能继承场景/HDR 输出。最初的 WARP 测试预先绑定了正确 UI 层，没有覆盖这种错误入口状态。

官方 CS 源码的 `HDRDisplay::SetUIBuffer()` 与 `Upscaling::MenuManagerDrawInterfaceStartHook` 展示了对 `kFRAMEBUFFER.RTV` 的明确重定向；不同 HDR/帧生成/暂停路径可以选择不同 UI 或后备输出。参考：

- https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/HDRDisplay.cpp
- https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/Upscaling.cpp

调查下载的是公开 dev 源码，不代表本地安装版本完全相同。现有日志和源码检查定位了目标选择缺陷，最终是否解决本次现场仍需用户复测。

## 修正

保留菜单阶段绘制和 COC 初始化修复。现在先保存全部输出目标及 depth，显式请求引擎的 `kFRAMEBUFFER.RTV`，再读取经过第三方 OM 拦截后的实际绑定资源与尺寸。这样既不把入口碰巧绑定的场景目标当作 UI，也不忽略渲染模组在绑定时的重定向。

`UIRenderPass` 在普通完成和异常退出时恢复原绑定。缺少明确 UI 目标则不擅自画入场景资源。日志改为 `route=menu-framebuffer-v2`，同时记录实际 `format` 与 `entry_format`。这没有宣称特定格式一定正确；格式用于定位绑定变化，选择依据是引擎 UI 请求与请求后的实际绑定。

## 验证与交付

18 个本地套件通过。新增 WARP 场景以 R10G10B10A2 场景目标、第二个 MRT 和 depth 开始，验证切换到明确 UI 资源；还模拟渲染器对请求的重定向，验证读取重定向后的资源/尺寸，检查多目标和 depth 的恢复、异常退出与缺失 UI 目标。

新 DLL 已同步到本地 MO2，SHA256 `3D25CE9083FC0BDA97A5DCC16EBB751663D736DC379D28A358EA8DB78159717D`，另外 98 个现有文件哈希不变。独立测试包使用 `render-input-coc-test-v2` 名称，正式包与前两个测试包均保留。完整测试源码包同时更新相关源码和测试。

未由代理启动游戏验证本次显示结果。请在本地分别检查 COC、普通读档、Pause/Slow/Live 及开关动画；若仍不可见，提供 v2 日志，继续核对绘制发生的阶段与 UI 合成先后。
