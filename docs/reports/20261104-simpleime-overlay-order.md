# SimpleIME 候选框层级修复

## 用户验收与新反馈
用户确认书籍分类正常、中文已能输入。新反馈为输入提示和候选框被命名面板覆盖，重命名时候选框可能完全不可见。

## 证据与修改
- 截图可见候选框在面板左侧露出一部分，重叠区域被面板遮挡。
- 当前 SimpleIME.log 多次记录 Candidate window became visible，最近两组候选框坐标为 (764,706)、(508,600)。支持遮挡判断，但无法仅凭日志逐一对应用户每次操作。
- SimpleIME 的 ImeMenu 在 depthPriority 13 的 PostDisplay 绘制。轮盘原来在 IDXGISwapChain::Present 前绘制，晚于菜单阶段，覆盖了输入法。
- WheelMenu 添加 kCustomRendering，命名框且 SimpleIME 已加载时，在 depthPriority 3 的 PostDisplay 绘制轮盘，随后由输入法绘制候选框。
- 本帧已由菜单绘制时，Present 跳过重复绘制；普通轮盘及无 SimpleIME 环境保留原有路径。
- 保留 ImGui 上下文与 D3D11 输出目标恢复、异常处理。未修改 SimpleIME 或用户配置。

## 验证与边界
- FavoriteWheel 构建通过并自动部署，git diff --check 通过。
- DLL 构建和部署 SHA256：7b2a71b42a6e6b5c2dbf554755b9f75679820a2aea10279b8f86e90be9590c9e
- 主配置 SHA256：1cbc56ee9ffb670fb1107823b42494bf45d63c412450e6081dd90c8a19f205f6
- 未接管键鼠，未执行游戏内视觉验证；构建成功不能证明最终合成层级通过。
- 待游戏确认新建、重命名候选框能完整显示，取消后重新进入仍正常。若特定渲染模组改变菜单合成顺序，还需依据实际结果进一步调整。

## 用户游戏内验收补充

2026-11-04，用户确认本次测试无问题，候选框层级修复验收通过。结合前次反馈，书籍独立分类、中文输入及候选框遮挡问题均已通过用户实测。此确认不扩展为技能书、法术书和第三方 OnRead 菜单的逐项验收。
