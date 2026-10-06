# 面部光照功能轮盘（0.3.13）

接入日期：2026-10-02。已实现 Face Lighting 公开 API V1 的可选客户端，不改面光工程实现、不直接写面光 INI、不模拟热键。

## 操作

Shift+Q → 上次功能分类（首次默认装备预设），A/D 切换到面部光照：玩家主开关、指向角色开关、随从总开关、随从列表。选择开关关闭轮盘后执行；列表导航不关轮盘。W/S/滚轮在当前分类翻页；随从子列表 Esc 返回面光，顶层 Esc 关闭；R 切换收藏。装备页只有装备预设，面光页只有面光功能。

打开轮盘前把准星指向 NPC。即使先 Q 再 R，仍使用本次打开前捕获的角色，不重新读取暂停中的准星。未加载、已死亡/不安全、句柄无效或已离队的随从行不可执行。组关闭不隐式启用组；禁用个人偏好仍允许。V1 不能枚举指定 NPC 全名单或修改指定组总开关，因此不加入这些项目。

## 契约与实现

- `include/FaceLightingAPI.h` 是从面光项目复制的公开 SDK 原样头文件；固定 POD，8 字节默认对齐。
- `FaceLightClient.cpp` 在 PostPostLoad 仅协商接口，GetModuleHandle/GetProcAddress；不 LoadLibrary。
- 首次 NewGame/PostLoadGame、所有 DataLoaded 回调结束后，链入 PlayerCharacter::Update（0xAD），保留原调用。输入只提交 PendingOpen；更新回调先执行前一个 hook（含面光线程登记），再检查 serial/epoch/截止时间/阻塞状态并 Capture/开轮盘。缓存是值，无跨 DLL 引擎指针。
- `FaceLightModel.h` 为快照/请求策略。FollowerPage 先查询数量/revision，再按 64 行默认初始化缓冲区分页；任一失败丢弃整个列表，4096 行本地上限，避免无界分配。
- 功能页只展示本次快照，绘制与分页不调用接口。关闭后用 PendingAction 原来的 2 秒期限、世代、解除暂停检查在玩家更新回调执行一次 Execute。ActionExecutor 将面光与原有收藏/换装任务分流，SKSE AddTask 不消费面光命令。
- SetPlayer/SetFollowerGroup 使用 Context 的 revision，SetActor 使用 ActorState 的 token/revision；revision 只判断相等，不排序或持久化。
- 0.3.13起，未检测到兼容V1接口（未安装、缺导出或接口不兼容）时隐藏面光分类；检测到接口但暂未就绪/线程拒绝时保留分类并分开提示，日志记录 thread/操作/session/revision/result。StaleRevision/StaleSession 由用户重新打开选择，不自动重试。
- 名字与状态传给 View，打开时预热姓名字形。普通 NPC 与随从个人偏好保存由面光承担，用户需保存游戏；玩家/随从组由 API 保存配置。

## 显示语义与限制

“偏好：开启”表示保存开关，不承诺实际照亮。中心说明展示潜行/视角/环境/对话抑制、未加载、节点缺失或未分配情况，不用 Registered 判断成功。场景更新仍遵循面光插件现有规则。

接口来源：用户项目 docs/public-api.md。安装目录本地 DLL 与面光构建 hash 相同且含 FaceLighting_GetAPI 导出；其版本号仍是 0.9.3，不应使用旧公开同版本包替代。

本地完成 API 布局/版本/回调门槛、随从分页、半途 stale 丢弃、失败不使用输出、显式开关、来源组保护的模拟测试，及 720p/1440p 离屏预览；未运行游戏进行真实照明、主线程、暂停恢复、token 失效联测。测试清单见 TESTING.md。

以下为历史评估，部分“未实现”描述仅适用于旧版。

# 面部光照功能轮盘接入评估

结论：可以另设功能轮盘，复用本项目的绘制、语言、主题和输入基础。0.1.4 只实现收藏轮盘的设置与资源系统；没有修改面部光照项目，没有安装功能轮盘或运行时联动。

已检查本机面部光照工程的 Settings.h、FaceLight.h、SelectedNPCs.h、Followers.h、Hotkeys.cpp 及相应实现：

| 功能 | 现有内部能力 |
| --- | --- |
| 玩家面光 | Settings::SetPlayerEnabled / Settings::Save，FaceLight::RequestUpdate |
| 准星 NPC | SelectedNPCs::CaptureTargets、AddTarget、ToggleCrosshairTarget |
| 已管理 NPC | SelectedNPCs::Snapshot、SetEnabled、Remove |
| 随从 | Followers::Snapshot、SetEnabled |
| 分类总开关 | Settings::Values.selected.enabled / follower.enabled |

上述是 DLL 内部接口，当前没有发现供外部轮盘使用的导出接口或插件消息协议。

建议的交互：独立可配置快捷键打开功能轮盘，首页提供玩家面光、目标 NPC、NPC 名单、随从名单及分类总开关；名单进入子轮盘，显示姓名、已启用/已关闭、已加载/未加载。收藏 Q 轮盘仍保持物品使用职责。精细数值调节继续放在设置页，避免将几十个参数塞进圆环。

建议在面部光照侧提供带版本号的 C ABI（或 SKSE 消息交换的函数表），内容为能力查询、状态快照和命令提交。跨 DLL 只传固定宽度字段、调用方分配的 UTF-8 缓冲区和稳定 ID，不跨边界传 STL 容器或裸 Actor 指针。面部光照负责验证、场景更新和保存；轮盘不直接修改另一模组的 INI。

需要单独解决：

- 暂停前锁定准星目标的 ActorHandle/会话令牌。现有 ToggleCrosshairTarget 要求游戏未暂停并重新读取实时准星，不能直接在轮盘打开后调用，否则可能无目标或作用于错误对象。
- 在执行时重新验证目标和存档世代；读档/卸载/目标失效拒绝命令。
- 开关展示应区别“用户启用”与“当前实际亮起”。面光可能受潜行、第一人称、环境光或对话规则影响，不能把启用状态等同于实际发光。
- 随从和 NPC 的偏好继续由面部光照持有和序列化，执行后返回结果，不能在 UI 中提前假定成功。
- 未安装面部光照或接口版本不匹配时禁用该功能轮盘入口，并给出原因，收藏轮盘保持可用。

建议先接玩家开关和一个明确锁定的目标 NPC，验证状态与读档边界，再加入 NPC/随从子轮盘。不要通过模拟 L / Shift+L 联动：现有热键有暂停限制，且无法可靠返回状态与错误。

## 0.3.0 实机问题与 0.3.1 验证边界

日志显示 API V1 available capabilities=F，随后 snapshot result=3（WrongThread）。面光的线程判断由自己的玩家更新 hook 登记；SKSE AddTask 实际在不同工作线程运行。0.3.1 按该线程契约修复轮盘调用点，没有放宽面光的线程检查或修改其 DLL。

本地新增执行器分流、关闭/暂停/世代/超时、分类循环与子列表导航、WrongThread 传播测试，均通过。2560×1440 装备/面光/随从布局离屏通过。尚未在游戏里观察到修复后的 result=0，需要用户复测；后装且不保留调用链的第三方 Update hook 仍可能影响入口，诊断日志保留。

## 2026-10-03 接续

用户已实测确认 0.3.1 面光查询/操作和功能分类基本正常。联动按 API V1 协商，不匹配模组发布版本；当前公开 SDK 与客户端头文件仍完全一致。0.3.2 新增 1.5.97 运行时测试目标，面光仍需自身能加载于该运行时。ABI 与线程等契约边界见 RUNTIME_COMPATIBILITY.md。
