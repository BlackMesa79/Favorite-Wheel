# 接手审查报告：FavoriteWheel 0.9.0（2026-10-28）

- Task ID：20261028-takeover-review
- 基线：分支 `wf/20261028-takeover-review`，提交 `069d0c3`（0.9.0 全部成果）
- 范围：`src/`、`include/`、`tests/`、`xmake.lua`、`scripts/`；对照 `HANDOFF_CURRENT.md` 第 5 节和两份历史审查（`docs/CODE_REVIEW_2026-10-24.md`、`docs/CODE_REVIEW_2026-10-26.md`）
- 方法：逐文件静态阅读，加上本地纯逻辑测试。没有构建 `FavoriteWheel` 目标，没有部署，没有启动游戏，也没有修改任何代码或构建文件。
- 结论：**没有发现 BLOCKER**；2 个 MAJOR、13 个 MINOR、7 个 OPTIMIZATION。整体结构（值快照、执行前重新校验实例、世代/序列号作废、独立暂停守卫、事务式设置、序列化限额）可以继续沿用，不建议重写。

## 0. 总表

| 编号 | 严重度 | 标题 | 位置 | 置信度 | 改变用户可见行为/存档格式 |
|---|---|---|---|---|---|
| A-01 | MAJOR | 主题 ID 按 ANSI 代码页生成：中文系统下非 ASCII 主题无法保存设置，部分系统启动时可能抛异常 | `src/UIResources.cpp:340`、`:298` | 高（保存失败）／中（启动异常，待验证） | 修复后非 ASCII 主题可用；格式不变 |
| A-02 | MAJOR | 打包脚本不校验 DLL 版本与新鲜度（2026-10-24 审查 P1，仍未修复） | `scripts/package.ps1:9-15` | 高 | 否 |
| A-03 | MINOR | 战斗循环记录编解码没有固定 `classic` locale，与排序记录的修复不一致 | `include/CombatCycleModel.h:48-68` | 高（代码）／低（触发概率） | 否（格式不变） |
| A-04 | MINOR | 排序记录读档解析 FormID 后不去重，下次读档可能整条拒收 | `src/InventoryOrder.cpp:37-47`、`include/InventoryOrderModel.h:101` | 中（待验证触发条件） | 否（格式不变） |
| A-05 | MINOR | 战斗预设仍拒收 `health<=0`，与 0.9.0 换装零值修复不一致 | `include/CombatPresetModel.h:65` | 高（代码）／待验证（实物） | **是：存档兼容**（旧版会拒收） |
| A-06 | MINOR | 3 个翻译键不在内置英文表中：不预热字形，缺少语言文件时显示原始键名 | `src/UIResources.cpp:13-223`、`:418-426` | 高 | 是（修复后显示正确） |
| A-07 | MINOR | 战斗快捷槽绑定只清理 `inventoryPages.items` 的角标，重新排序后旧角标复现 | `src/Wheel.cpp:637-645` | 高 | 是（修复显示错误） |
| A-08 | MINOR | 语言/主题/换装导入导出根目录依赖进程 CWD（2026-10-24 审查 #4，仍未修复） | `include/UIResources.h:27`、`src/Outfits.cpp:21` | 中 | 否（CWD 正常时） |
| A-09 | MINOR | 游戏中按数字键、Q、十字键都会逐次写日志（需用户决策） | `src/Wheel.cpp:1208-1226`、`:1362-1367` | 高 | 日志量 |
| A-10 | MINOR | 设置保存“重开校验失败”时，文件已写入新值而内存仍回滚 | `src/Settings.cpp:258-265` | 高（代码）／低（触发） | 暂不建议修 |
| A-11 | MINOR | 版本号硬编码在 4 处 | `xmake.lua:2`、`src/main.cpp:35,44`、`tests/RuntimeLayoutTests.cpp:121` | 高 | 否 |
| A-12 | MINOR | 死代码：`CollectFavorites`、`FavoritesKeyMatches`、`DecideAction`、`GamepadOpening` 只被测试或无人使用 | 见正文 | 高 | 否 |
| A-13 | MINOR | 换装/战斗/收藏之间的重复辅助函数（`Ref`、`Describe`、UnequipSpell 派发、双手武器判定、HUD 提示） | 见正文 | 高 | 否 |
| A-14 | MINOR | 自定义排序每组 10,000 键上限：达到后新物品无法手动排列，且没有提示（需用户决策） | `include/InventoryOrderModel.h:30,37,69-80` | 高（代码）／低（触发） | 是 |
| A-15 | MINOR | 未知/更新版本的记录读档时被丢弃，下次存档时被覆盖（降级会丢数据，需用户决策） | `src/Outfits.cpp:126` 等 | 高 | 存档行为 |
| A-16 | OPTIMIZATION | `Tr()` 每次重新解析语言；`UIGlyphs()` 每个可见帧遍历全部译文（2026-10-24 审查 #3，仍未修复） | `src/UIResources.cpp:389-426`、`src/WheelFonts.cpp:82-92` | 高 | 否 |
| A-17 | OPTIMIZATION | 输入派发里每个按钮事件都 `Snapshot()`，关闭状态下每帧仍复制快照；`Config()` 被高频复制（2026-10-24 审查 #2，仍未修复） | `src/Wheel.cpp:1228,1437,685,1546` | 高 | 否 |
| A-18 | OPTIMIZATION | `Fit`/`Wrapped` 逐字符重测，二次复杂度（2026-10-24 审查 #6） | `src/Draw.cpp:89-115` | 高 | 否 |
| A-19 | OPTIMIZATION | 自定义排序在每次比较时拼接身份字符串，每次 `Apply` 都执行 `Observe`（handoff §6） | `include/InventoryOrderModel.h:31-68` | 高 | 否 |
| A-20 | OPTIMIZATION | 每个输入批次都执行 `GetModuleHandleW`/`GetProcAddress` 查询 Text Bridge | `include/TextBridgeClient.h:7-14`、`src/Wheel.cpp:1123` | 高 | 否 |
| A-21 | OPTIMIZATION | 测试基础设施：没有统一 runner；RuntimeLayoutTests 依赖会触发部署的 DLL 构建；残留无目标的 `UIRenderTargetTests.exe` | `xmake.lua:136-282` | 高 | 否 |
| A-22 | OPTIMIZATION | `Wheel.cpp`（1629 行／115 KB）、`Draw.cpp`（1227 行／77 KB）职责过多，建议拆分 | 见正文 | — | 否 |

## 1. MAJOR

### A-01 主题 ID 使用 ANSI 窄字符串

- 位置：`src/UIResources.cpp:340` 写成 `entry.id=path.stem().string();`。语言文件在 `:327-330` 用的是 `u8string()`，两者不一致。`Files()` 在 `:298` 调用会抛异常的 `directory_entry::is_regular_file()`。调用链：`src/main.cpp:29` → `LoadResources`，外层没有 try。
- 失败场景 1（高置信度）：中文/日文 Windows（ANSI 代码页为 936/932）下，玩家把主题文件命名为 `金色.ini`。主题 ID 会变成 GBK 字节，选中后 `SaveSettings` 调用 `SettingsIni::Wide(…, CP_UTF8)`，后者使用 `MB_ERR_INVALID_CHARS`（`include/SettingsIni.h:18-24`），转换失败，于是 `Report("serialize")`，界面显示“无法保存设置”。即使手动在 INI 里写 `Theme=金色`（UTF-8），也匹配不到这个 ID，会回退到经典主题。
- 失败场景 2（中等置信度，待验证）：如果文件名里有当前代码页无法表示的字符（例如英文系统 1252 代码页遇到中文文件名），MSVC 的 `path::string()` 可能抛出 `std::system_error`；同样，权限异常或损坏的目录项也会让 `is_regular_file()` 抛出 `filesystem_error`。这两种异常都发生在 `SKSEPluginLoad` 内，后果是插件加载失败或进程终止。
- 修复建议：主题 ID 改用 `u8string()`；`Files()` 改用 `is_regular_file(error_code)`；`LoadResources` 内部捕获异常并回退到内置英文和经典主题，同时写日志。
- 风险：只影响非 ASCII 主题文件名，ASCII 主题 ID 的字节不变，存档和 INI 格式不变。
- 回归核对：与 HANDOFF §5 无关。

### A-02 打包不校验 DLL 是否对应当前源码

- 位置：`scripts/package.ps1:5-15`。脚本从 `xmake.lua` 读取版本号，只检查 `build/windows/x64/release/FavoriteWheel.dll` 是否存在就直接复制。
- 失败场景：升级版本号或修改源码后忘记重新构建，脚本照样能生成名为新版本、实际装着旧 DLL 的安装包。2026-10-24 审查已经出现过源码 0.7.3、DLL 0.7.2 的实例。
- 修复建议：打包前比较 DLL 的 `FileVersion` 与 `xmake.lua` 版本；如果 `src/`、`include/`、`xmake.lua` 中有文件比 DLL 新，就拒绝打包（提供 `-AllowStaleDll` 显式绕过）；在输出里打印 DLL 的 SHA256。
- 风险：只影响发布流程，不改变插件行为。

## 2. MINOR

### A-03 战斗循环记录没有固定数字格式

- 位置：`include/CombatCycleModel.h:48-56`（Encode）、`:57-68`（Decode），两处都没有 `imbue(std::locale::classic())`。排序记录（`InventoryOrderModel.h:82,90`）、换装记录、战斗预设记录都已经固定。
- 失败场景：其他组件修改了全局 C++ locale（例如带千位分组），而成员中的预设 ID ≥1000 时（`Valid` 允许到 1,000,000），写出的文本会是 `1.234` 这样的形式，读档时 `Valid` 失败，8 个循环组全部丢失。预设 ID 递增，需要累计创建上千次才会达到，触发概率低。
- 修复：读写两端都加 `imbue(classic)`，记录格式 `FWCY 1` 不变。可参考 `tests/InventoryOrderTests.cpp:53-58` 的分组 locale 测试写法。
- 回归核对：与 HANDOFF §5.3 同一思路，不涉及排序记录。

### A-04 排序记录读档后不去重

- 位置：`src/InventoryOrder.cpp:37-47`。读档时逐键执行 `ResolveFormID` 并剔除失败项，但不检查重写后的 FormID 是否重复。`Decode` 遇到重复身份时直接返回 false（`include/InventoryOrderModel.h:101`）。
- 失败场景（待验证）：插件合并、改名或 ESL 重新编号，使两个旧 FormID 解析成同一个新 FormID，且附魔、health、UID、名称也都相同。本次读档仍能用，但存档会把重复键写进去，下次读档 `Decode` 整条拒收，18 组排序和模式全部丢失。
- 修复：解析后按 `Token` 去重，保留首次出现；`Decode` 的严格校验保持不变（防止畸形记录）。
- 回归核对：记录 ID `0x46575352`、版本 1、文本头 `FWS 1`、模式编号都不变（HANDOFF §5.4）；身份字段与上限不变（§5.5）。

### A-05 战斗预设拒收零值 health

- 位置：`include/CombatPresetModel.h:65` 的 `x.health<=0` 会返回无效；换装记录已经在 `include/OutfitModel.h:49-50` 接受 0。
- 失败场景：零值 `ExtraHealth` 的盾牌、火把或武器（0.9.0 曾在戒指和长袍上实际观察到零值）在 `Combat::Capture` 时，`Encode→Decode` 自检失败（`src/CombatPresets.cpp:226`），提示“无法捕获”。不会丢失已有数据。
- 修复：改为 `x.health<0`，与换装一致。**需要用户确认**：含零值的新战斗预设无法被 ≤0.9.0 读取（整条 `FWC` 记录会被拒收），降级会丢失全部战斗预设。
- 置信度：代码层面高；零值手持装备是否真实存在，待用户反馈。

### A-06 翻译键缺少内置英文

- 位置：`compactUseSwapped`、`swapMouseHands`、`swapMouseHandsHelp` 存在于全部 8 个语言 INI，但不在内置 `english` 表（`src/UIResources.cpp:13-223`）中。`UIGlyphs()`（`:418-426`）只遍历内置表；使用处见 `src/Draw.cpp:658,701,1163`。
- 失败场景：① 这 3 段译文的字形不会被预热。实测 `ja.ini` 的 `swapMouseHands=マウスの装備する手を交換` 中有字符没被其他文案覆盖，设置的“键盘”页可能显示缺字。② 如果语言文件缺失或损坏，界面会直接显示 `swapMouseHandsHelp` 这样的键名。
- 修复：补进内置英文表；增加测试：`assets/Languages/en.ini` 的所有键都必须在内置表中（反向检查已存在于 `tests/SettingsTests.cpp:245`）。

### A-07 战斗快捷槽绑定后，未排序目录残留旧角标

- 位置：`src/Wheel.cpp:637-645`（战斗分支只清理 `inventoryPages.items`），收藏分支在 `:651-653` 两个目录都更新。
- 失败场景：先打开收藏轮盘（目录已加载）→ R 切到功能轮盘 → 悬停战斗预设按 3 → 回到收藏页按 T 切换排序。`ResortInventory` 从 `unsortedItems` 重建，原来占用 3 号槽的物品重新显示“3”角标，直到目录刷新或重新打开。纯显示问题。
- 修复：两个目录一起清理，最好抽成可单测的 inline 辅助函数。

### A-08 资源与导入导出路径依赖 CWD

- 位置：`include/UIResources.h:27` 的默认根目录是 `"Data/SKSE/Plugins/FavoriteWheel"`；`src/Outfits.cpp:21` 的 `folder` 也是相对路径。主 INI 已经锚定在 exe 目录（`src/Settings.cpp:22-31`）。
- 失败场景：启动器或其他组件改变了 CWD，结果语言/主题读不到（回退英文和经典主题），换装导出写到别处、导入找不到文件。是否真实触发待验证。
- 修复：提供以 exe 目录为基准的统一 Data 路径，资源和导入导出都改用它；继续保留 VFS 下的写入方式。

### A-09 游戏中逐次按键日志（需用户决策）

- 位置：`src/Wheel.cpp:1208-1215`（键盘 Q 或入口键每次按下都写 `Favorites key`）、`:1216-1226`（十字键上或手柄入口键每次都写 `Controller entrance`）、`:1362-1367`（数字/小键盘每次按下都写 `Numeric shortcut input`）。
- 影响：大量使用快捷键的玩家日志增长快。不过这些日志在 README 和历史排障中被明确要求用户提供，删减会削弱远程诊断能力，因此**不列入修复计划**，等用户决定是保留、限频，还是只在轮盘相关时记录。

### A-10 设置保存重开校验失败后，内存与文件不一致（暂不建议修）

- 位置：`src/Settings.cpp:258-265`。原位写入和句柄读回都成功，但关闭后重新打开读到的字节不同时，函数返回 false，而文件已经是新内容；随后取消会把内存回滚到旧值，下次启动却读到新值。
- 判断：这是 VFS 映射异常时的诊断路径，曾有回归历史（`docs/CONFIG_SAVE_0.6.0_FOLLOWUP.md`）。在没有现场证据前不建议改动，只记录在案。

### A-11 版本号多处硬编码

- 位置：`xmake.lua:2`、`src/main.cpp:35`、`:44`、`tests/RuntimeLayoutTests.cpp:121`。每次发版要同步改 4 处，漏改会产生错误的日志或测试结果。
- 修复：由 xmake 注入版本宏，源码和测试统一引用。

### A-12 死代码

- `CollectFavorites`（`include/Favorites.h:52`、`src/Favorites.cpp:386`）在 `src/` 中没有调用者。
- `FavoritesKeyMatches`（`include/OpenPolicy.h:30-36`，`userEvent` 参数被显式忽略）、`DecideAction`（`include/ActionPolicy.h:44-48`）、`GamepadOpening`（`include/InputBindings.h:85`）只被测试引用。
- 建议：删除，或标注为“仅保留历史测试”；同时删除对应测试断言。风险低。

### A-13 重复辅助函数

- `Ref`/`Describe`：`src/Outfits.cpp:24-44` 与 `src/CombatPresets.cpp:31-68` 基本相同。
- Actor.UnequipSpell 的 VM 派发：`src/Favorites.cpp:148-175` 与 `src/CombatPresets.cpp:170-190`。
- 双手武器判定：`src/Favorites.cpp:178`、`:550` 与 `src/CombatPresets.cpp:41-45`。
- HUD 提示：`Tell`/`Notify` 在 4 个文件中各写了一份。
- 风险：属于纯重构，但涉及装备语义，必须逐字保持原逻辑；优先级低。

### A-14 自定义排序 10,000 键上限（需用户决策）

- 位置：`include/InventoryOrderModel.h:30,37`。`Observe` 达到 `maxKeys` 后不再记录新身份；`Move`/`InsertAt`（`:69-80`）要求两端都在 `order` 中，否则返回 false。
- 失败场景：全部背包、自定义模式下长期游玩，某类累计 10,000 个不同身份（缺失项目按设计永久保留）后，新获得的物品排在末尾，无法前移、后移或拖拽，界面没有任何提示。
- 选项：保持现状；或在达到上限时淘汰“长期未见”的身份（这会改变 HANDOFF §5.5 中“缺失项目保留”的语义），需要用户决定。

### A-15 未知或更新版本的记录会被丢弃（需用户决策）

- 位置：`src/Outfits.cpp:126`、`src/CombatPresets.cpp:291-292`、`:299-300`、`src/CombatCycles.cpp:124-125`、`src/InventoryOrder.cpp:37-38`。版本不符时直接跳过，下次存档只写当前内存数据。
- 影响：用新版本存档后再用旧版本加载并存档，新版本的数据会被永久覆盖。发布说明已经提示过降级限制。可选改进是对无法识别的记录原样透传；这会改变存档行为，需要用户决定。

## 3. OPTIMIZATION

### A-16 每帧重复做翻译与字形准备

- `Tr()`（`src/UIResources.cpp:409-416`）每次调用都通过 `ActiveLanguage→ResolveLanguage`（`:389-400`）对每种语言执行 `NormalizeLanguage`，并且按值返回字符串。
- `PrepareFonts` 每帧调用 `UIGlyphs()`（`src/WheelFonts.cpp:88`）。后者对约 320 个键逐一调用 `Tr`，拼接出数 KB 到数十 KB 的文本，再调用 3 次 `GetKeyNameTextW`；此外每帧还要复制 8 KB 的 `UsedChars` 并逐项比较（`:82-92`）。
- 收益：轮盘打开期间每帧可省下数百次字符串操作，具体收益待用 `Wheel transition … cpu_mean_ms` 日志（`src/Renderer.cpp:34`）测量。
- 建议：按 `config.language` 缓存解析结果，`LoadResources` 时清空；按（语言、3 个键位）缓存 UI 字形串；`PrepareFonts` 只在键变化时才 `AddText`。缓存要线程安全。

### A-17 输入派发中的快照复制

- `src/Wheel.cpp:1228` 在每个按钮事件里都执行完整 `Snapshot()`（`:1546`，会复制 items、字形串、名称编辑器、配置等），包括轮盘关闭时；`:1437` 和 `PumpDetails`（`:685`）在每次派发都会再复制一次。`Cancel()` 不清理 `view.items`、`inventoryGlyphs`，所以关闭后依然在复制。`Config()`（`src/Settings.cpp:82`）每次调用都会复制 3 个字符串，其中 `font` 超出 SSO 长度，必然发生堆分配。一个按钮事件会触发约 10 次。
- 建议：关闭且没有排队或关闭中的状态时走快速路径，不做快照；打开时每批只做一次快照；关闭时清理大字段（渲染关闭动画使用 `Renderer.cpp:113` 的 `lastView`，不依赖 `view`）。**注意不能把任何 `IsOpen()`/`ValidPlayer()` 调用移进 `viewMutex` 内（HANDOFF §5.1）。**

### A-18 文本截断的二次复杂度

- `src/Draw.cpp:89-115`：每删除一个 UTF-8 字符就重新测量整串，并构造 `text+"..."`。异常长的模组物品名或效果说明在每帧绘制时会被反复截断。
- 建议：改为二分查找 UTF-8 边界，或累加字形宽度；抽成可注入测量函数的纯函数以便单测。

### A-19 自定义排序反复生成身份字符串

- `include/InventoryOrderModel.h:46` 的 `rank` 每次比较都调用 `Token()` 拼接字符串；`Observe`（`:34-38`）每次 `Apply` 都为全部已知键（最多 1 万个）生成 `Token`。实时模式下每 250 ms 可能刷新一次目录（`src/Wheel.cpp:389-400`）。
- 建议：为 `Key` 提供哈希，用 `unordered_map<Key,size_t,KeyHash>`；排序前一次性计算 rank。序列化格式与身份定义不变（HANDOFF §5.4/5.5）。

### A-20 每批输入都查询 Text Bridge 导出函数

- `TextBridge::Query()` 每个输入批次都执行 `GetModuleHandleW`+`GetProcAddress`（`include/TextBridgeClient.h:7-14`，调用点 `src/Wheel.cpp:1123`）。开销小，可以缓存函数指针（Text Bridge 是普通 SKSE DLL，加载后不会卸载）。

### A-21 测试基础设施

- 22 个测试目标（`xmake.lua:136-282`）需要逐个构建和运行，没有汇总脚本。
- `RuntimeLayoutTests` 会校验 `build/.../FavoriteWheel.dll` 的导出版本（`tests/RuntimeLayoutTests.cpp:121`），而构建该 DLL 会触发 `after_build` 部署（`xmake.lua:33-134`）。
- `SettingsVfsTests` 需要 MO2 usvfs，长期不能运行。
- `build/windows/x64/release/UIRenderTargetTests.exe` 是没有对应目标的残留产物。
- 建议：新增 `scripts/run-tests.ps1`，只构建运行纯逻辑目标并汇总退出码；RuntimeLayoutTests 改为通过参数接收 DLL 路径（可选）。

### A-22 超大文件拆分建议（独立任务，不进入本次计划）

- `src/Wheel.cpp` 建议按职责拆分：排序/拖拽（`:209-363`）、目录与详情任务（`:364-400`、`:682-726`）、动作调度（`:727-812`）、设置与对话框（`:540-562`、`:941-1114`）、输入路由（`:1115-1447`）、菜单与事件接收器（`:1449-1543`）、生命周期与安装（`:442-496`、`:1546-1627`）。
- `src/Draw.cpp` 建议拆成：基础图元（`:36-209`）、皮肤绘制（`:210-345`）、面板（`:519-714`）、详情卡（`:715-863`）、`DrawWheel`（`:869-1226`）。
- 前置条件：先完成 A-17 和测试 runner。拆分时必须保持锁的进出顺序（特别是 `EndSortDrag` 在取得 `viewMutex` 前计算 `ValidPlayer()`），每一步都要能单独回滚。

## 4. 已核实无问题（与 HANDOFF 第 5 节逐条对照）

| HANDOFF §5 | 当前代码 | 结论 |
|---|---|---|
| 1. `EndSortDrag` 不在 `viewMutex` 内调用 `ValidPlayer()` | `src/Wheel.cpp:334-363`：`allowed` 在 `:338` 计算，`viewMutex` 在 `:341` 才取得；短按后的 `MoveSortedItem` 在 `:362`，已在锁作用域外 | 保持 |
| 2. `DirectoryRequestNeeded` 重试 | `include/QueuedTask.h:9-11`，调用点 `src/Wheel.cpp:1437-1444` | 保持 |
| 3. 排序序列化固定 classic locale | `include/InventoryOrderModel.h:82,90` | 保持；A-03 把同样做法补到循环记录 |
| 4. 记录 `0x46575352`、版本 1、模式编号 | `include/InventoryOrder.h:6`、`include/InventoryOrderModel.h:14` | 保持；所有计划项都不改编号 |
| 5. 身份字段与 10,000 键 / 16 MB 上限 | `src/InventoryOrder.cpp:9-13`、`include/InventoryOrderModel.h:30,89` | 保持；A-14 只列为待决策 |

其他核实项：

- 锁顺序（详见 `docs/ARCHITECTURE.md` 的“线程模型与锁顺序”）：`lifecycleMutex`（递归锁）→ `openMutex`/`actionMutex`/`viewMutex`/`pauseMutex`；`actionMutex`→`viewMutex`（`TakeAction` 在 `src/Wheel.cpp:738-742` 调用 `ValidPlayer`/`IsOpen`）；`viewMutex`→`settingsMutex`/`Ordering::mutex`；`Outfits::mutex`→`Combat`/`Cycles`/`Ordering` 的锁（`src/Outfits.cpp:111-140`）。没有发现反向嵌套。`Combat::Remove` 先释放自己的锁再通知 Cycles（`src/CombatPresets.cpp:262-265`）。
- 没有发现第二处“持有 `viewMutex` 时调用 `IsOpen`/`Snapshot`/`Cancel`/`RequestDirectory(false)`”的路径。`ChangeSorting` 在调用 `RequestDirectory(true)` 前会先 `unlock`（`src/Wheel.cpp:286`）。
- `CaptureItemInfo` 临时创建的 `InventoryEntryData` 只删除链表节点，不删除游戏的 ExtraDataList（CommonLib `InventoryEntryData::~InventoryEntryData` 只 `delete extraLists`；`AddExtraList(nullptr)` 直接返回）。
- `Combat::Tick` 中对 `Form(...)->As<...>()` 的解引用，在同一 tick 内已经由 `Check()` 确认对象存在且类型正确（`src/CombatPresets.cpp:126-151` 先于 `:388-395` 执行）。
- `Renderer` 每帧取 RTV 后不跨帧持有引用，并恢复 OM 目标；ImGui 后端会自行保存和恢复其余管线状态（`src/Renderer.cpp:121-178`）。
- Papyrus VM 回调只设置原子量或 `AddTask`（`src/Favorites.cpp:148-162`、`src/CombatPresets.cpp:170-175`）；库存事件接收器只做原子递增（`src/Wheel.cpp:1517-1526`）。

## 5. 本次验证

- 构建并运行（xmake 增量构建，均为不依赖 `FavoriteWheel` 的独立目标，日志中没有部署记录）：WheelLogicTests、GameSessionTests、HandEquipTests、LayoutSkinTests、CombatCycleTests、CombatPresetTests、OutfitTests、NameEditorTests、FaceLightClientTests、ItemInfoTests、InventoryTests、TimeTests、InventoryOrderTests、SettingsTests、SettingsPersistenceTests、SectorMeshTests，**16 个全部退出码为 0**。日志在 `build/takeover-review/`（已被 `.gitignore` 忽略）。
- 未运行：RuntimeLayoutTests（导出版本检查需要构建会部署的 DLL）、ActorRuntimeTests（需要 CommonLib 依赖构建，本次没有执行）、SettingsVfsTests（需要 usvfs）、SkinTextureTests 与 WheelPreview（需要 D3D/离屏渲染）。没有进行游戏内验证。
- A-06 的缺字结论来自对语言文件的脚本分析（统计内置键译文未覆盖的字符），没有进行实际渲染截图。
- 性能类结论（A-16 到 A-20）来自调用路径和复杂度分析，没有测量游戏内帧时间。
