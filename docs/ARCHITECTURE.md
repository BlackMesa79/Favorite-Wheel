# 工程说明

> 2026-10-28 接手补充：当前（0.9.0）的模块索引、线程与锁顺序、SKSE 序列化记录总表、INI 配置总表、构建/测试/打包/部署流程和用户已确认的决策，见本文末尾的“2026-10-28 接手补充（0.9.0 现状）”一节，以该节为准。下面按版本倒序排列的段落是历史记录，其中的版本号与“当前”说法可能已经过时。文档索引见 [docs/README.md](README.md)。

2026-10-10当前0.5.2补齐8种自带语言：英/简中/法/巴西葡语/俄/日/韩/德。UIResources启动解析Windows字体目录与Font=auto，WheelFonts在字库变化时用stbtt检查字符覆盖、仅合并缺失字形并缓存字体文件/合并ranges（直到atlas清理才释放）；不在稳定帧查询字体文件或重建atlas。见LOCALIZATION.md、LOCALIZATION_EXPANSION_0.5.2.md。1080Ti负载反馈排查见GPU_LOAD_REPORT.md，未据此修改Present或强加限帧。

2026-10-10当前0.5.2追加设置修正：实际手柄选杆/分流/重置读取已应用值，事务预览不切换物理操作方案；HideEmptyCategories默认1，可切为完整八类，复用现有目录掩码；新安装/默认值位置X72/Y46，保留已有坐标。见SETTINGS_REFINEMENTS_0.5.2.md。

当前0.5.2追加可选ControllerSticks输入分流与LookStickGate回中交接；GamepadMoveWhileOpen默认0，左杆在实时轮盘自由移动、右杆选择/窗口光标，模态停步。见CONTROLLER_SPLIT_STICKS.md。

当前开发0.5.2（最近正式0.5.1）：InventoryPages生成可见分类掩码，导航/标题使用同一集合；换装保留共有实例，主体优先穿戴目标，再验证/清理旧配件，最多两次装备调用与软3ms预算。详见EMPTY_CATEGORIES_AND_OUTFITS_0.5.2.md；下面同编号GOG测试为历史，不代表本轮内容。

2026-10-09：用户确认此前测试改动正常并授权发布0.5.1，合并上下文入口/GOG/连续装备与手柄方案/设置持久化；下文待测描述为历史记录。发布依据与编号规则见RELEASE_0.5.1.md。

0.5.4将KeepOpen默认开启，已有明确0保留；修复设置路径/CWD与虚拟文件系统临时替换隐患，增加独立进程重读和写入诊断。设置不属于存档、不会随读档复位，反馈者环境未知尚待复测。

0.5.3增加可选装备后保留轮盘（消耗品仍关闭）、手柄LB/RB与LT/RT类别/装备互换、键盘与手柄独立设置页。执行仍由原任务/PlayerUpdate线程负责，保留显示时短暂释放自己的暂停，串行执行并合并刷新；详见CONTINUOUS_USE_0.5.3.md。

当前正式版本：0.5.1。用户确认合并测试正常，以下0.5.1–0.5.4属于发布前历史测试编号；未来从正式版递增。运行时精确支持1.5.97 / 1.6.640 / 1.6.1170 / GOG 1.6.1179 / 1.7.99 / 1.7.104；1.5.97与1.6.1170已有早期版本用户实测，0.4.1–0.4.8合并项目与0.4.9输入隔离修复收到正常反馈，GOG1179/640/1.7仍无实机确认。依赖为CommonLibSSE-NG v11.0.0固定提交，历史目录名仍为extern/CommonLibVR。下文保留早期实现依据，涉及旧快照/首版运行时的说明不是当前依赖声明；来源见COMMONLIB_1.7_REVIEW.md。

0.5.1入口按物理设备/码检查Gameplay映射的有效控制组；空闲时禁用则完整透传，排队开盘复查，已经捕获输入的周期保护仍保留。没有绑定的独立自定义入口允许；不硬编码容器/QuickLoot菜单或API。读档输出实际键位，默认继续跟随Favorites。见ENTRY_CONTEXT_0.5.1.md。

0.4.9对InputGate::Suppress从临时派发链移除按钮，避免零值按钮触发声音能力/第三方菜单；保留Release/Resume/Pass与IME字符，派发返回恢复引擎链接。详见INPUT_ISOLATION_0.4.9.md。

0.4.8以游戏SpellType/TESShout在原收藏遍历内区分Spells/Shouts/Powers，过滤被动/内部效果；八类目录边界及标题/图标数组有长度保护。无新扫描/hook/存档格式，新增spells/shouts/powers译文，详见MAGIC_CATEGORIES_0.4.8.md。

0.4.7按物理边沿追踪手柄十字键翻页、零值手柄release清除吞键，入口与UI语义分开；单页不清光标，翻页诊断见CONTROLLER_PAGING_0.4.7.md。没有新增地址/hook/绘制/配置。

0.4.6将装备预设UI标记改为WearingPieces（全部保存实例已穿戴），额外衣物不影响标记；Apply卸装分支及Tick完成检查仍使用Wearing（完整集合一致）。手动与轮盘穿戴共用库存状态，无last-applied标记/新状态/扫描；见OUTFIT_WEAR_BADGE_0.4.6.md。

0.4.5修正无UID的相同强化/附魔/改名副本误判，保存前验证、不可用配件详情与状态变化日志见OUTFIT_MATCHING_0.4.5.md；格式仍FWO 1。

0.4.4轮盘不再注册kUsesMenuContext/kMenuMode，以独立输入门控控制叠加层。移动方向保持开盘前状态直到松键/摇杆回中，新增方向输入只操作UI，实际关掉两个菜单后仍按住的移动键补一次down。详细规则与待测边界见MOVEMENT_INPUT.md；0.4.3时间生命周期保护继续保留。

0.4.2新增TimeControl/TimeLease相对倍率会话、固定标志的独立暂停守卫菜单、三标签设置与事件dirty驱动的实时库存刷新。默认暂停不变。生命周期/所有权与待测边界见TIME_MODE.md。

0.4.1新增默认关闭的全部背包模式。轻量目录按分类连续范围缓存，View只发布当前页最多10项；单项详情80ms悬停后请求并缓存。直接打开功能轮盘跳过物品采集；执行/详情查找重新验证目标实例并借用原生entry，不克隆全库存。生命周期与性能边界见INVENTORY_MODE.md。

0.3.14的UIResources在启动读取Windows显示语言，按Language=auto或显式语言代码解析完整地区/通用语言/英文回退；文本、字体和字形预热统一使用解析后的语言。auto模式不覆盖为实际语言ID，旧配置保留手动选择。翻译接口和文件规范见LOCALIZATION.md。

- `main.cpp`：SKSE 生命周期、运行时检查（首版 1.6.1170）和初始化。
- `ActorRuntime.h`：复用面部光照已验证的显式 Actor 虚表调用适配。此 CommonLibVR 快照在平面版也声明了额外的 TESObjectREFR Unk_8C，不能直接调用后续的 IsDead / DrinkPotion；使用库实现注明的 SE/AE 0x99 / 0x10F。新增同一布局的模拟 Actor 回归测试，保留依赖库本身不变。
- `Settings.cpp`：互斥保护的配置值快照；设置页事务支持预览、撤销和应用，配置路径固定到本体exe目录；在系统临时目录准备并验证INI，通过目标INI原位句柄写入/flush/读回验证，保留未知键；避免VFS临时文件rename。详情见SETTINGS_PERSISTENCE_0.5.4.md。不接 SKSE Menu Framework。
- `UIResources.cpp`：启动时读取 UTF-8 语言和主题文件，随后保持只读；文本缺失回退内置英文，未知主题回退经典主题。`UILayout.h` 共享绘制和输入的按钮坐标。
- `Outfits.cpp` / `OutfitModel.h`：独立预设实例模型、库存匹配、装备事务、受限文本导入导出与 SKSE 序列化。ActionKind 区分物品和功能操作，功能视图与收藏分类/页码独立。
- `Favorites.cpp`：在游戏线程遍历原版收藏；装备实例由 FormID、ExtraDataList 地址及唯一 ID/附魔/强化信息标识。渲染层只拿值拷贝。执行前重新读取库存并核对实例，避免解引用失效指针。
- `Wheel.cpp`：分类、页码、虚拟鼠标、独立暂停菜单和输入 hook。默认读取游戏 Favorites 键位。使用面部光照同款 SKSE 消息初始化方式；输入 dispatch 地址采用用户现有输入法项目使用的 CommonLib 调用位置，保留前一个 hook。
- `InputGate.h`：动作键一次释放及吞到物理松键；移动键独立保留/恢复策略，左摇杆锁定开盘向量并在回中时停止，避免新UI输入改变人物方向。
- `Renderer.cpp`：在游戏 swap chain 的 Present 上绘制，持有独立 ImGui context，不替换 WndProc；恢复原来的 context、渲染管线与输出目标。每帧临时获取游戏 kFRAMEBUFFER.RTV 及其实际资源尺寸，跟随 UI 重定向；目标缺失才使用 backbuffer，不跨帧持有目标引用。
- `Draw.cpp`：值快照 -> 十格轮盘；图标通过几何线条原创绘制。无游戏对象访问。
- `WheelFonts.cpp`：与绘制共享布局缩放，在 NewFrame 前为实际整数像素字号构建字形；界面文字和当前收藏名称按需加入，多个字号共享字体文件内存。字体与绘制使用同一份 View 快照。
- `ActionPolicy.h` / `Wheel.cpp`：输入帧驱动待执行动作，每次最多一个 SKSE 检查任务，等待实际关菜单及解除暂停；不在任务队列中递归重试，超过两秒或世代/游戏状态变化即取消。消耗品仅调用一次 EquipObject，交给游戏及动画插件管理最终使用。
- `tests/WheelPreview.cpp`：使用相同 Draw.cpp 和 D3D11 WARP 离屏输出 BMP；合成库存数据只用于布局 QA。

## 线程和数据边界

库存访问与装备行为在游戏线程进行。UI 快照由互斥锁保护，Present 只读取字符串/数值副本；队列中的装备命令捕获存档世代，读档后失效。收藏仍以游戏收藏为数据源；0.2.0 起额外的装备预设由 SKSE co-save 保存，记录类型/插件 ID 为 0x46574F50，记录版本为 1。

设置编辑在输入路径进行，渲染层使用 View 携带的同一份配置快照生成字形并绘制。进入设置后按键分流到设置/录入逻辑，所有设置点击仍经 InputGate 吞到物理释放。关闭、读档、失焦回滚未应用配置。物品选择使用单位圆限位光标，设置使用自由光标；共享 ViewScale/ViewCentre 计算尺寸与偏左中心。

0.1.6 删除 BackdropBlur 源文件和所有背景效果。原路径使用 swapChain->GetBuffer()；用户的 CommunityShaders.log 显示启用了 CS 1.6 的 DX12SwapChain，该路径将场景与 UI 分开合成。轮盘提前画进场景后，后续 HUD 合成可覆盖它。现在在原有 Present 前置回调中绘制到游戏 UI framebuffer，使用 RTV->GetResource 获取实际目标尺寸（不能使用可能仍指向场景的 texture 字段）。输出合并器和 ImGui 管线状态照常恢复。

参考官方 [HDRDisplay.cpp](https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/HDRDisplay.cpp) 的 SetUIBuffer / DrawImGuiForPresent，以及 [DX12SwapChain.cpp](https://github.com/community-shaders/skyrim-community-shaders/blob/dev/src/Features/Upscaling/DX12SwapChain.cpp) 的 GetBuffer / Present。只参考渲染契约，不复制 CS 实现，也不依赖它的私有地址、结构或额外 DLL。研究源码只位于 build/research，不包含在安装或源码发布包中。官方 dev 源码可能不同于用户的实际二进制，最终兼容性仍由实机验证。

移除模糊消除了该效果每帧的场景复制和三次计算着色器调度，但本地没有游戏 GPU 帧时间数据；不能据此断言用户掉帧的唯一原因。

本版不声称兼容所有原生 hook 组合。其他替换收藏菜单、帧生成、D3D 包装器、无暂停菜单的组合需游戏内验证。失去 renderer 时不接管打开键；不支持的运行时直接拒绝加载。

## 依赖来源

- CommonLibSSE-NG v11.0.0：上游提交 `94faaed0c60eddd8347767f2d4d29a97c93bde8c`，使用固定ZIP及SHA-256校验；沿用目录名extern/CommonLibVR，未修改上游源码。原面光工程4.39.3快照仅为历史依赖，0.3.12起已替换。
- Dear ImGui：官方仓库 tag `v1.91.9b`，commit `f5befd2d29e66809cd1110a152e375a7f1981f06`。
- 本次未下载、复制或编译 Grid Inventory 的源码和资源。

依赖放在 extern，本机缓存和构建产物不进入 Git。`bootstrap.ps1` 默认下载固定上游源，也可从同提交的CommonLib源目录准备依赖；完整源码 ZIP 包含本次实际依赖，不依赖用户其他项目的绝对路径。

## 0.1.7 过渡与布局

轮盘使用独立 wheelScale 与百分比 X/Y，设置页保留自己的固定中心和旧 scale。共享 ViewScale/ViewCentre 统一尺寸及边界。遮罩在 DrawWheel 最先绘制，为一个全屏黑色矩形；随后仅对本轮 UI 顶点 alpha 应用淡入淡出。Transition 是渲染线程局部状态，缓存纯值 View 供最多约 100ms 的关闭过渡，菜单和物品动作不等待动画。无可见 UI 时不执行 Render，避免持续复制快照。正常关闭可播放 RE::PlaySound；生命周期强制关闭清除淡出并保持静音。

0.2.0 注册 SKSE save/load/revert 回调；切换角色或读档时丢弃旧内存预设，从对应 co-save 恢复。模型序列化不保存指针。UI 管理使用值快照，正常换装复用已有的关菜单后动作队列与世代检查。输入名称只响应管理页，字符事件被吞掉；Ctrl+V 仅在用户主动粘贴时读取 Unicode 剪贴板。

## 0.2.7 名称编辑

名称框采用自己的 UTF-8 编辑模型 NameEditor，而非 ImGui::InputText；因此不共享其他 DLL 的 ImGui context。Draw 只从值快照绘制光标/选区并发布名称命中坐标，输入线程进行编辑。InputObserver 接收 hook 链之后实际交给游戏的 CharEvent，再吞掉已接收字符，兼容 Text Bridge 在任一 hook 顺序下注入字符。键盘回退翻译只在没有 CharEvent 且桥接未激活时进行。TextBridgeClient 可选查询 C ABI 状态/快捷键，不跨 DLL 共享字符串或指针；桥接组合/待提交期间禁用轮盘名称快捷命令，离开名称页请求 reset。名字仍按 128 字节上限保存，不改变预设格式。

## 0.2.1 分帧换装

Outfits::Apply 完成一次预检查并创建仅含 Piece 值的 Job。原有 input-frame 驱动的 SKSE 队列调用 Tick，间隔至少 16ms；一次提交后用 DecideStep 在后续 tick 观察状态。每个操作最多提交一次，750ms 超时停止，保留逐件重解析及原来的完整卸装顺序。Busy 为原子可见状态，禁止重入；epoch 和 CancelJob 在读档等状态变化时取消。每步引擎调用耗时及慢库存查询写入日志，以区分模组/引擎单件成本与轮盘自身成本。

## 0.3.1 面光执行线程与功能分类

面光 API 认可的线程由其玩家 Update hook 记录。实机确认 SKSE AddTask 的工作线程不一定符合此契约（GetContext WrongThread）。轮盘在首次新游戏/读档通知、所有 DataLoaded hook 安装完毕后链入同一个 PlayerCharacter Update 0xAD，先调用前一个 hook，再处理 PendingOpen 捕获和面光 PendingAction。打开请求保留 serial、epoch 和期限；执行仍等独立暂停菜单隐藏和解除暂停。普通收藏/装备预设保留原来的 SKSE 任务队列，ActionExecutor 策略禁止两条执行路径相互消费命令。

FunctionNavigation.h 定义装备/面光类型及随从子列表。A/D 循环顶层类型，W/S/滚轮只翻当前内容；随从子列表 Esc 回面光，顶层 Esc 关闭。Present、类型切换和名单分页只读取值缓存。API 缺失、NotReady、WrongThread 独立提示，快照/命令日志包含线程 ID。该契约在已安装面光源码上确认，本地策略和布局测试通过，游戏内成功结果仍待用户验证。

## 0.3.2 双运行时测试构建

RuntimeSupport.h 仅接受 1.5.97/1.6.1170，不放开所有 SE/AE。CommonLib 构建同时启用 SE/AE，输入 RelocationID 自动分流，renderer/control/player 等保留运行时访问器。RuntimeLayoutTests 使用 REL::Module::mock 切换实际运行时选择，独立原生字节哨兵和错误版本偏移检查 UI/输入/ActorState/玩家结构，并可读取安装的 Address Library 验证关键 ID。ActorRuntimeTests 在两版逐次验证虚调用。没有更换依赖库或修改别的模组/游戏文件；1.5.97 的游戏内 hook 签名与功能结果仍待验证。

## 2026-10-28 接手补充（0.9.0 现状）

本节由接手审查（task 20261028-takeover-review）补充。所有结论以 0.9.0 基线代码 `069d0c3` 为准，并标注 `文件:行号`。当前交接内容与“不要回归的修复”清单见 `HANDOFF_CURRENT.md` 第 5 节；本次审查发现的问题见 `docs/reports/20261028-takeover-review-audit.md`。代码行号会随后续修改漂移，引用时以符号名为准。

### 1. 模块与文件索引

| 层 | 文件 | 职责 |
|---|---|---|
| 入口/生命周期 | `src/main.cpp:7-46` | `SKSEPluginLoad`：读取设置与资源 → 运行时白名单（`include/RuntimeSupport.h:10-15`）→ 注册 SKSE 消息与序列化回调。`kPostPostLoad` 时发现面光 API；`kDataLoaded` 时安装渲染器与轮盘；`kPreLoadGame`/`kNewGame`/`kPostLoadGame` 时调用 `SetGameActive` |
| | `include/GameSession.h` | 会话状态（AwaitingWorld/Inactive/Active）。`coc` 等不发 SKSE 会话消息的启动方式，由 `ProbeInitialSession`（`src/Wheel.cpp:826-853`）补齐 |
| | `include/QueuedTask.h` | SKSE 任务单槽排队：`RunQueuedTask`/`ScheduleQueuedTask` 保证出现异常时也释放槽位；`DirectoryRequestNeeded`（HANDOFF §5.2） |
| 控制器（超大文件） | `src/Wheel.cpp` | 视图状态 `View`（`include/Wheel.h:12-45`）、打开/关闭、分类与分页、排序与拖拽（`:209-363`）、目录与详情任务（`:364-400`、`:682-726`）、动作调度（`:727-812`）、设置与对话框（`:540-562`、`:941-1114`）、输入 hook（`:1115-1447`）、两个自定义菜单与事件接收器（`:1449-1543`）、安装（`:1598-1627`） |
| 输入策略（纯头文件，可单测） | `include/InputGate.h`、`InputBindings.h`、`ControllerSticks.h`、`InputDispatchChain.h`、`OpenPolicy.h`、`QuickSlots.h`、`SortDrag.h` | 物理键吞到松开为止、移动键的保留与恢复、组合键、手柄翻页边沿、从派发链摘除被吞事件、入口控制组、阻断菜单名单、1–8 快捷槽、拖拽阈值 |
| 视图/导航策略 | `include/CategoryNavigation.h`、`FunctionNavigation.h`、`InventoryPages.h`、`NavigationPresentation.h`、`UILayout.h`、`LayoutSkin.h`、`Transition.h`、`WheelLogic.h` | 9 个物品分类的掩码、功能轮盘的各分区、按分类连续分页、扇区命中、皮肤几何、开合动画 |
| 物品与装备（游戏线程） | `src/Favorites.cpp` | `CollectDirectory`（`:270-384`）在游戏线程遍历库存并生成值快照；`ResolveItem`（`:241-267`）在执行前重新校验实例；`UseFavorite`（`:478-630`）；原生 1–8 快捷槽（`:397-477`） |
| | `include/EquipPolicy.h`、`WeaponStack.h`、`InventoryPolicy.h`、`MiscDisplayStack.h`、`MagicCategories.h`、`AlchemyCategories.h`、`ActionPolicy.h` | 左右手计划、普通武器池、预算、杂物显示堆叠、魔法/药剂分类、动作执行器与保留策略 |
| | `include/FormMode.h`、`FormRuntime.h` | 人类/吸血鬼领主/不支持的兽形；领主形态只允许列表内的法术 |
| | `include/ActorRuntime.h` | `IsDead`/`DrinkPotion` 的显式虚表调用（SE/AE 0x99/0x10F） |
| | `src/ItemInfoCapture.cpp`、`src/ItemInfo.cpp`、`include/ItemInfo.h` | 详情卡数值与效果；数值排序复用这里的取值 |
| 排序（0.9.0） | `include/InventoryOrderModel.h`、`include/InventoryOrder.h`、`src/InventoryOrder.cpp` | 18 组（9 类 × 收藏/全部背包）的模式与自定义顺序、身份 `Key`、`FWS 1` 编解码、读档时解析 FormID |
| 换装预设 | `src/Outfits.cpp`、`include/OutfitModel.h`、`OutfitPlan.h`、`OutfitStep.h` | 捕获、匹配、分帧替换（`Apply`/`Tick` `:274-354`）、导入导出；**同时负责全部 SKSE 序列化回调的注册与分发**（`:111-146`） |
| 战斗预设（0.7.0） | `src/CombatPresets.cpp`、`include/CombatPresetModel.h` | 左右手捕获和逐步恢复（`Apply`/`Tick` `:310-399`），法术通过 VM `Actor.UnequipSpell` 清除；1–8 快捷槽绑定使用独立记录 |
| 战斗循环（0.8.0） | `src/CombatCycles.cpp`、`include/CombatCycleModel.h` | 8 组有序战斗预设，组合键触发下一套；`Apply` 回调中提交游标 |
| 面部光照（可选） | `src/FaceLightClient.cpp`、`include/FaceLightModel.h`、`FaceLightingAPI.h` | 在 `kPostPostLoad` 通过 `GetProcAddress` 获取 V1；只在 PlayerUpdate 线程上 `Capture`/`Execute` |
| 时间 | `src/TimeControl.cpp`、`include/TimePolicy.h` | 相对减速租约、50% 物理下限、外部修改倍率时让出所有权；写入 BSTimer 当前值与目标值（`src/TimeControl.cpp:11-20`） |
| 设置与资源 | `src/Settings.cpp`、`include/SettingsIni.h`、`src/UIResources.cpp`、`include/UIResources.h` | 事务式设置（预览/撤销/应用）、按文件字节写 INI 并校验；语言/主题 INI、系统字体回退、`Tr()` |
| 渲染 | `src/Renderer.cpp` | swap chain vtable[8] Present hook（`:207-217`）、独立 ImGui context、每帧获取 `kFRAMEBUFFER` RTV（`:121-135`）、初始化时预热字体（`:65-88`） |
| | `src/WheelFonts.cpp` | 按实际像素字号烘焙 atlas，缺字合并系统回退字体（`PrepareFonts` `:71-159`） |
| | `src/Draw.cpp`、`include/WheelIcons.h`、`WheelSectorMesh.h`、`src/SkinTextures.cpp` | 根据值快照绘制轮盘、面板、详情卡和拖拽动画；矢量图标；PNG 材质（只在启动时加载） |
| 名称输入 | `include/NameEditor.h`、`NameHitMap.h`、`TextBridgeClient.h` | UTF-8 名称编辑器；可选的 Skyrim Text Bridge C ABI |

### 2. 原生挂钩与注册

| 位置 | 机制 | 说明 |
|---|---|---|
| `src/Wheel.cpp:1602-1617` | 在 `RelocationID(67315,68617)+0x7B` 处做 trampoline `write_call<5>` | 输入派发；先校验原字节是 `E8`，否则拒绝安装并保留原版收藏菜单 |
| `src/Wheel.cpp:813-821` | `PlayerCharacter` vtable 槽 `0xAD` | 首次 NewGame/PostLoadGame 时才链入，确保排在面光等插件的 DataLoaded hook 之后 |
| `src/Renderer.cpp:207-217` | swap chain vtable[8] | 只处理目标 swap chain；初始化失败后不再重试 |
| `src/Wheel.cpp:1607-1621` | `UI::Register` 两个菜单；MenuOpenClose、TESContainerChanged、TESEquip、InputEvent 事件接收器 | `FavoriteWheelMenu` 是输入门控的叠加层；`FavoriteWheelPauseMenu` 是固定标志的暂停守卫 |
| `src/TimeControl.cpp:17-18` | `RELOCATION_ID(511882,388442)`、`(511883,388443)` | 当前与目标全局时间倍率 |
| 运行时白名单 | `include/RuntimeSupport.h:10-15` | 1.5.97、1.6.640、GOG 1.6.659、1.6.1170、GOG 1.6.1179、1.7.99、1.7.104；不支持 VR（`xmake.lua:7`） |

### 3. 线程模型与锁顺序

- 线程契约来自代码注释：输入派发、UI 消息、SKSE 任务、PlayerUpdate 回调可能交错（`src/Wheel.cpp:37-40`）；Present 只读值快照（`src/Renderer.cpp:98-118`）；面光 API 只能在 PlayerUpdate 链中调用（`:799-812`）；Papyrus VM 回调只设置原子量或 `AddTask`（`src/Favorites.cpp:148-162`、`src/CombatPresets.cpp:170-175`）；库存事件接收器只做原子递增（`src/Wheel.cpp:1517-1526`）；序列化回调在存读档时由 SKSE 调用。各回调实际运行在哪个 OS 线程，未在所有运行时和帧生成组合下实测，不能假设一定是同一线程。
- 主要锁：`lifecycleMutex`（**递归**，串行化开关、时间同步与动作提交）、`viewMutex`（**非递归**，保护 `view` 与目录）、`actionMutex`、`openMutex`、`pauseMutex`（`src/Wheel.cpp:36-114`）、`settingsMutex`（`src/Settings.cpp:14`）、`Ordering::mutex`、`Outfits::mutex`、`Combat::mutex`、`Cycles::mutex`、`TimeControl::mutex`、`FaceLight::mutex`、`nameHitMutex`。
- 已观察到的获取顺序（只允许按这个方向嵌套）：
  - `lifecycleMutex` → `openMutex` / `actionMutex` / `viewMutex` / `pauseMutex`
  - `actionMutex` → `viewMutex`（`TakeAction` 调用 `ValidPlayer()`→`IsOpen()`，`src/Wheel.cpp:727-750`）
  - `viewMutex` → `settingsMutex`（`Config()`）、`Ordering::mutex`（`FilterItems`/`ResortInventory`）
  - `Outfits::mutex` → `Combat::mutex` / `Cycles::mutex` / `Ordering::mutex`（存读档与 Revert，`src/Outfits.cpp:111-140`）
  - `Combat::mutex`、`Cycles::mutex`、`TimeControl::mutex`、`FaceLight::mutex`、`settingsMutex` 是叶子锁；`Combat::Remove` 先释放自己的锁再调用 `Cycles::PresetRemoved`（`src/CombatPresets.cpp:262-265`）
- **禁止**：持有 `viewMutex` 时调用 `IsOpen()`、`Snapshot()`、`ValidPlayer()`、`Cancel()`、`RequestDirectory(false)` 或任何会再取得 `viewMutex` 的函数（HANDOFF §5.1：`EndSortDrag` 曾因此抛出 `resource deadlock would occur`）。
- 作废机制：`epoch`（读档/会话变化）、`openSerial`（每次开盘或关闭）、`directoryRevision`（目录内容替换）。排队任务捕获这些值，执行时比对，不一致就丢弃（例如 `src/Wheel.cpp:364-388`、`:682-726`）。

### 4. SKSE 序列化记录总表

所有记录共用插件唯一 ID `0x46574F50`（`SetUniqueID`，`src/Outfits.cpp:142-146`），由 `Outfits::Save/Load/Revert` 统一分发（`:111-140`）。都是文本格式，读档时用 `ResolveFormID` 修正运行时 FormID（plugin 为空表示动态或运行时 ID）。

| 记录类型 | 四字符 | 版本 | 文本头 | 上限 | 定义 / 编解码 | 读档 |
|---|---|---|---|---|---|---|
| `0x46574F50` | FWOP | 1 | `FWO 1` | 1 MiB，100 个预设，每个最多 64 件 | `src/Outfits.cpp:20`；`include/OutfitModel.h:55-87` | `src/Outfits.cpp:126-136` |
| `0x46574350` | FWCP | 1 | `FWC 1` | 1 MiB，100 个预设 | `include/CombatPresets.h:6`；`include/CombatPresetModel.h:71-106` | `src/CombatPresets.cpp:291-298` |
| `0x4657434B` | FWCK | 1 | `FWQ 1` | 1 KiB，8 个绑定 | `include/CombatPresets.h:7`；`include/CombatPresetModel.h:111-129` | `src/CombatPresets.cpp:299-306` |
| `0x46574359` | FWCY | 1 | `FWCY 1` | 64 KiB，8 组 × 64 个成员 | `include/CombatCycles.h:5`；`include/CombatCycleModel.h:48-68` | `src/CombatCycles.cpp:124-134`（读完全部记录后调用 `FinalizeLoad`，剔除已不存在的预设） |
| `0x46575352` | FWSR | 1 | `FWS 1` | 16 MiB，18 组 × 10,000 键 | `include/InventoryOrder.h:6`；`include/InventoryOrderModel.h:81-106` | `src/InventoryOrder.cpp:37-47` |

- 排序模式编号固定为 Default=0 … ValueDescending=10（`include/InventoryOrderModel.h:14`），不得重新编号（HANDOFF §5.4）。
- 版本不匹配或解码失败的记录会被跳过，下次存档时不会写回（审查 A-15）。0.9.0 新增的排序模式和 `health=0` 换装无法被旧版本完整读取。
- 换装、战斗、排序记录固定使用 `std::locale::classic()`；循环记录尚未固定（审查 A-03）。

### 5. INI 配置总表

主配置路径固定为 `<SkyrimSE.exe 所在目录>/Data/SKSE/Plugins/FavoriteWheel.ini`（`src/Settings.cpp:22-31`）。启动时只读；缺失的键使用 `include/Settings.h:4-33` 中的默认值；只有“应用”时才创建或写入（`src/Settings.cpp:173-268`）。读取见 `:90-132`，取值约束见 `Clamp` `:62-80`。

| 节 | 键 | 字段 | 默认 | 取值 / 说明 |
|---|---|---|---|---|
| General | Enabled | enabled | 1 | 0 时 DataLoaded 不安装任何 hook |
| General | AllInventory | allInventory | 0 | 1 = 全部背包 |
| General | HideEmptyCategories | hideEmptyCategories | 1 | |
| General | RememberLastWheel | rememberLastWheel | 0 | 只在本次会话内记忆，读档时清除 |
| General | KeepOpen | keepOpen | 1 | 消耗品始终关闭轮盘 |
| General | TimeMode | timeMode | 0 | 0 暂停 / 1 减速 / 2 正常 |
| General | SlowTimePercent | slowPercent | 50 | 50–100 |
| General | Language | language | auto | 语言文件 ID；兼容旧键 `Chinese` |
| Display | Theme | theme | classic | 主题文件名（不含扩展名） |
| Display | ShowHints | showHints | 1 | |
| Display | WheelScalePercent | wheelScale | 100 | 60–150 |
| Display | ScalePercent | scale | 100 | 60–150（设置面板） |
| Display | PositionXPercent / PositionYPercent | positionX/Y | 72 / 46 | 0–100 |
| Display | OverlayOpacityPercent | overlayOpacity | 35 | 0–80 |
| Display | Font | font | `C:/Windows/Fonts/msyh.ttc` | 语言或主题指定字体优先 |
| Effects | Sounds / Animations | sounds/animations | 1 / 1 | |
| Controls | Hotkey / HotkeyModifier | hotkey/hotkeyModifier | -1 / 0 | -1 跟随游戏的 Favorites 键；修饰键 Shift=1、Ctrl=2、Alt=4 |
| Controls | ActionHotkey / ActionModifier | actionHotkey/actionModifier | -1 / 1 | -1 跟随收藏入口键 |
| Controls | SwitchWheelKey | switchKey | 19（R） | 2–255，不能是 60（F2） |
| Controls | GamepadHotkey / GamepadModifier / GamepadActionModifier | … | -1 / -1 / 274（LB） | 266–281，或 -1 |
| Controls | GamepadCategoryButtons | gamepadCategoryButtons | 0 | 0 LB/RB 切分类，1 LT/RT 切分类 |
| Controls | GamepadMoveWhileOpen | gamepadMoveWhileOpen | 0 | 只在“应用”成功后切换操作方案 |
| Controls | SwapMouseHands | swapMouseHands | 0 | |
| Controls | SensitivityPercent | sensitivity | 100 | 20–300 |

资源目录为 `Data/SKSE/Plugins/FavoriteWheel/{Languages,Themes,Themes/Textures}`（默认根目录见 `include/UIResources.h:27`，相对于进程 CWD，见审查 A-08）。语言文件规范见 `docs/LOCALIZATION.md`，主题键见 `docs/THEME_AUTHORING_0.6.0.md`，主题读取见 `src/UIResources.cpp:338-385`。

### 6. 构建、测试、打包、部署

- 依赖：`scripts/bootstrap.ps1` 下载固定提交的 CommonLibSSE-NG（校验 SHA256）和 ImGui v1.91.9b 到 `extern/`（已被 git 忽略）。
- 构建：`xmake f -m release -y`，然后 `xmake build -j 2 FavoriteWheel`。`FavoriteWheel` 的 `after_build`（`xmake.lua:33-134`）**会自动部署**到 `deploy_dir`（默认 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`，`xmake.lua:12-16`）：复制 DLL；归档并迁移旧主题；只补充缺失的语言或主题文件和键，迁移已知旧提示（`scripts/language_migrations.lua`）；复制 docs 和许可证。不会复制主 INI。部署后要核对 DLL 的 SHA256（AGENTS.md）。
- 测试：22 个 `set_default(false)` 的独立目标（`xmake.lua:136-282`），逐个执行 `xmake build <Target>` 后运行 `build/windows/x64/release/<Target>.exe`。只依赖 `include/` 和少量 `src/` 的纯逻辑测试**不会触发部署**。`RuntimeLayoutTests` 会检查已构建 DLL 的导出版本；`SettingsVfsTests` 需要 MO2 usvfs；`WheelPreview` 用 WARP 离屏绘图。语言迁移测试：`xmake lua scripts/test_language_migrations.lua`。
- 打包：`scripts/package.ps1`（PowerShell 7），从 `xmake.lua` 读取版本，生成 `dist/FavoriteWheel-<ver>.zip` 和源码包，并校验 ZIP 内容。目前**不校验 DLL 是否对应当前源码**（审查 A-02）。
- 版本号目前要手动同步 4 处：`xmake.lua:2`、`src/main.cpp:35,44`、`tests/RuntimeLayoutTests.cpp:121`（审查 A-11）。

### 7. 用户已确认的决策（截至 0.9.0，长期有效）

- 构建后默认自动部署到 MO2 开发目录；保留用户主 INI 和自定义资源；源码里的示例 INI 不进安装包。
- 不接管键鼠、不启动游戏；需要游戏内验证时交给用户。“部署成功”不等于“游戏验证通过”。
- 额外功能小轮盘与页脚按钮方案已撤回，F2 直接打开设置；吸血鬼领主能力音效实验已撤回，不得恢复。
- 排序：T / Shift+T / F2；只对人类形态的收藏或全部背包生效，共 18 组；随角色存档保存；名称排序是字节序，不做拼音排序。
- 记住上次轮盘：默认关闭，只在本次会话内有效。
- 换装 `health=0` 精确保留；负数和非有限值仍然拒绝。
- PureDark Upscaler / Theo's Render Pipeline 的可见性问题尚未解决，不得在发布材料中声称已修复。
- 0.9.0 只完成了本地发布准备，没有上传到线上。

### 8. 文档组织

`docs/README.md` 是文档索引：按专题和版本归类，并标出已被取代的文档。新的修复计划放在 `docs/plans/<task-id>.md`（模板 `docs/plans/_template.md`），审查和测试报告放在 `docs/reports/`。
