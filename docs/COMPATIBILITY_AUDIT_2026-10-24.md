# 上采样、兽化与乐器堆叠反馈检查

日期：2026-10-24。范围为当前工作区代码；工作区包含尚未提交的 0.8.0 与其他改动，不能假定反馈者使用相同 DLL。本次是调查，不是兼容补丁交付。未修改生产逻辑、部署 DLL、启动游戏或接管键鼠。

## 结论与证据等级

| 反馈 | 检查结论 | 尚缺证据 |
| --- | --- | --- |
| PureDark / TRP 下不可见 | 当前 Present + 引擎 framebuffer 路线没有通用的第三方合成阶段契约，存在目标或时序不匹配风险；此前修复实验已撤回 | 报告者的准确渲染器版本、配置、轮盘日志和游戏内对照 |
| 吸血鬼领主不开盘 | 代码明确禁止兽化开盘，交回原版；属于当前功能限制 | 如果原版菜单也打不开，需要该环境的输入与菜单证据 |
| 变身相关闪退 | 尚未确认根因；不可见或兽化拦截本身不能证明崩溃原因 | Crash Logger 完整日志、轮盘日志、触发步骤及版本 |
| 鼓、笛、鲁特琴逐个占格 | 非武器分支确实逐 ExtraDataList 生成格子；只合并无 extra 的余量，符合反馈 | 实际乐器 extra 类型，判断哪些实例可以安全合并 |
| GOG 1.6.1179 | 在支持列表内；布局与 actor 派发测试通过，没有发现此堆叠问题独有的 GOG 分支 | 不能以模拟布局测试替代 GOG 游戏内复现 |

## 渲染器

`src/Renderer.cpp` 在 DataLoaded 保存一个交换链地址，替换 vtable[8]，在旧 Present 之前绘制。只有传入交换链等于保存地址才绘制。目标优先为 `kFRAMEBUFFER.RTV`；仅在该指针为空时退回交换链 buffer。

因此，非空但不再参与最终合成的 framebuffer 不会触发 fallback；代理/替换交换链以及第三方后续合成也不在当前处理契约内。这些是可定位的兼容风险，不是已证明每个报告者都命中的根因。仅盲目切换到 backbuffer 也可能破坏已工作的 CS UI 层。

已重新读取 TRP 公开源码：提交 `ba3b0e8474edeeab9c6479a9c1cc5a9c53ba9e9e`，2026-10-23 合入 Release 0.5.3。

- `compatibility/ImGuiCompat/ImGuiIntegration.cpp` 的八个生产者不包括 FavoriteWheel。
- `src/FrameGen/GameSwapChain.cpp` 的 Present 在内部 Present 前调用 `BeforeGameSwapChainPresent`。
- `src/UpscalerDeviceHooks.cpp` 中该流程按路径调用 `PrepareCommunityFrameForPresent`，或 `PrepareSourceFrameForPresent`、`OverlayUI::OnPresent`、`FinishNativeUIPassForPresent`。
- 不能把以前 0.4.1 快照中 bridge 的状态当成所有新版的事实；本次没有确认新版 bridge 可供轮盘直接使用。

来源：https://github.com/theosw/theosrenderpipeline/tree/ba3b0e8474edeeab9c6479a9c1cc5a9c53ba9e9e

另读 PureDark 公开 Skyrim-Upscaler 提交 `fa057bb088cf399e1112c1eaba714590c881e462` 的 `src/UpscalerHooks.cpp`：也钩住交换链 vtable[8]，在旧 Present 前绘制设置 UI。多个 Present hook 本身不是冲突证明，正确链式调用可以共存。该公开实现不能代表所有付费 ENB/帧生成分支，不能据此声称已复现其报告。

来源：https://github.com/PureDark/Skyrim-Upscaler/blob/fa057bb088cf399e1112c1eaba714590c881e462/src/UpscalerHooks.cpp

下载的文本快照在 `build/compat-audit-2026-10-24/`。历史实测失败与撤回记录见 `RESTORE_STABLE_RENDERER_0.7.0.md`。建议下一步先补充一次性目标身份、交换链身份与 hook 所属模块诊断，再针对明确版本确定合成前后的接入点；不要再次直接用未经游戏验证的菜单回调替换默认路径。

## 吸血鬼领主与闪退

`src/Wheel.cpp` 的 `ValidPlayer` 和 `OpenBlockReason` 均检查 `MenuControls::InBeastForm()`；阻止原因明确为 `beast form: yielding to vanilla`。普通人形吸血鬼不因 race playable 标志被禁止。

空闲时入口被阻止不会主动吞掉原版收藏事件；替换入口分支也要求无开盘阻止原因。已开盘后检测到不合格玩家会取消；待执行动作通过 `TakeAction` 再检查 `ValidPlayer`，Combat/Outfit Tick 同样检查。因而不能通过简单删除兽化门槛来解决：现有目录会包含人形装备、战斗预设与通用动作，需要另行设计兽化技能来源和允许执行的动作。

补充 RuntimeLayoutTests，验证全部七个支持的 flat runtime（含 GOG 1179）读取 0x81 兽化标志，且相邻 isProcessing/remapMode 不会误判。它验证 accessor 布局，不证明原版或其他模组在变身过程中写入该状态的时机。

目前没有本次反馈对应的崩溃日志。Renderer 的 `catch(std::exception)` 也不是 Windows 访问违规的通用保护，不能因为没有“rendering disabled”日志就排除 native crash。应区分变身时、变身后按收藏键时、选择技能时及还原人形时崩溃，并比较禁用轮盘后的同一场景。

## 乐器与杂物

`src/Favorites.cpp::AppendInventory` 只对武器使用逻辑堆叠。非武器两次遍历 extraLists：第一遍选择能接收 spare 的 anchor；第二遍仍对每个 extra 调用 append。若 N 个乐器各有一个带收藏标记的 extra，spare=0，就会生成 N 格；若矿石只有一个 extra 加上无 extra 的余量，则是一格。这足以解释两类物品表现不同，但尚未证明实际存档的 extra 正是此结构。

`Plain` 只控制余量归属，不会去重多个已存在的 extra。物品显示名相同也不能证明可以合并：所有权、任务/唯一标识、自定义名字、快捷键和其他模组元数据都可能不同。不能把修复写成按名字或 FormID 无条件 unique，否则可能丢数量、改错快捷键或选中错误实例。

建议实现杂物的显示堆叠策略，保留实体成员用于重新解析与快捷键操作；同一 base form 下仅合并经过定义的等价实例，累计数量并保护不同标记。先记录报告者三种乐器的 extra 类型和数量，再决定允许归并哪些差异。现有武器堆叠字段不能未经动作与选择语义检查就直接复用。

当前 Supported 不包括普通杂物，因此这份反馈首先是显示/分页问题，不应顺带给乐器添加通用 EquipObject 执行能力。GOG 版本不改变上述非武器枚举逻辑。

## 验证

后台构建并运行 InventoryTests、WheelLogicTests、RuntimeLayoutTests、ActorRuntimeTests；四项通过。RuntimeLayoutTests 本次未传外部 Address Library 数据库：通过的是模拟运行时布局、测试数据库格式与元数据检查，不是重新验证七份真实数据库。

现有 InventoryTests 覆盖数量预算、收藏标记、分页与选择，不覆盖真实乐器 ExtraDataList 去重；测试全绿不代表该报告不存在。未进行 GPU 渲染或游戏内兼容性验证。
