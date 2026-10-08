# 工程说明

当前版本：0.5.0正式版。运行时精确支持1.5.97 / 1.6.640 / 1.6.1170 / 1.7.99 / 1.7.104；1.5.97与1.6.1170已有早期版本用户实测，0.4.1–0.4.8合并项目与0.4.9输入隔离修复收到正常反馈，640/1.7仍无实机确认。依赖为CommonLibSSE-NG v11.0.0固定提交，历史目录名仍为extern/CommonLibVR。下文保留早期实现依据，涉及旧快照/首版运行时的说明不是当前依赖声明；来源见COMMONLIB_1.7_REVIEW.md。

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
- `Settings.cpp`：互斥保护的配置值快照；设置页事务支持预览、撤销和应用，通过临时 INI 完整写入后原子替换保存，保留未知键。不接 SKSE Menu Framework。
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
