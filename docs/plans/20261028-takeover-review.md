# 接手审查修复计划（草案）

- Task ID: 20261028-takeover-review
- 状态: DRAFT（等待用户挑选子任务；只有被选中的子任务才进入实现阶段）
- 基准分支: wf/20261028-takeover-review（基线 069d0c3，0.9.0）
- 审查依据: docs/reports/20261028-takeover-review-audit.md（下文用 A-xx 指代审查编号）

## 1. 背景与目标

项目已交给新团队。本次 review 没有发现 BLOCKER，但确认了 2 个 MAJOR，以及若干一致性问题和性能隐患，其中 3 项是 2026-10-24 审查中已经提出、至今未修的。本计划只收录建议修复的条目，并拆成可以独立挑选的子任务。需要用户先做决定的条目单独标注【需决策】。长期背景见 docs/ARCHITECTURE.md 末尾的“2026-10-28 接手补充”一节。

## 2. 范围

### In Scope（按推荐优先级排列）
- T-1 主题 ID 改为 UTF-8，资源加载不再向外抛异常（A-01）
- T-2 打包前校验 DLL 版本与新鲜度；版本号只在一处定义（A-02、A-11）
- T-3 序列化健壮性：循环记录固定 classic locale；排序记录读档后去重（A-03、A-04）
- T-4 补齐内置英文翻译键，并加完整性测试（A-06）
- T-5 战斗快捷槽绑定后同步清理两份目录的角标（A-07）
- T-6 缓存语言解析结果和 UI 字形串（A-16）
- T-7 文本截断改为非二次复杂度（A-18）
- T-8 自定义排序改用哈希 rank（A-19）
- T-9 Data 路径锚定 exe 目录（A-08）
- T-10 统一测试 runner 脚本（A-21）
- T-11【需决策：存档兼容】战斗预设接受 health=0（A-05）
- T-12 输入派发减少快照复制，并缓存 Text Bridge 导出函数（A-17、A-20）
- T-13 清理死代码并合并重复辅助函数（A-12、A-13，低优先）

### Out of Scope
- A-09 日志降噪、A-10 设置重开校验、A-14 排序键上限、A-15 未知记录透传：等用户决定后另立任务。
- A-22 拆分 Wheel.cpp/Draw.cpp：另起 task-id，前置条件是 T-10、T-12。
- 不做新功能：功能轮盘排序、拼音排序、排序键重绑定、菜单内施放能力、额外功能小轮盘、能力音效实验（已撤回，禁止恢复）。
- 不处理 PureDark/TRP 可见性问题，不修改渲染目标策略或 Present hook 方式。
- 不改 HANDOFF_CURRENT.md 及历史 RELEASE/专题文档的正文；不打包、不发布、不上传。

## 3. 与本次相关的现状

- 长期背景：见 docs/ARCHITECTURE.md §“2026-10-28 接手补充”（模块索引、线程与锁顺序、序列化总表、INI 总表、构建与部署）。
- `src/UIResources.cpp:298,340,389-426`：资源加载、语言解析、`Tr`、`UIGlyphs`
- `src/WheelFonts.cpp:71-159`：`PrepareFonts`
- `src/Draw.cpp:89-115`：`Fit`/`Wrapped`
- `src/Wheel.cpp:637-655`：快捷槽绑定任务；`:1115-1447`：`InputHook`；`:1546`：`Snapshot`
- `include/CombatCycleModel.h:48-68`、`src/InventoryOrder.cpp:37-47`、`include/InventoryOrderModel.h:31-106`
- `include/CombatPresetModel.h:54-70`：`Valid`
- `scripts/package.ps1:5-15`、`xmake.lua:2,24-34,242-249`、`src/main.cpp:35,44`、`tests/RuntimeLayoutTests.cpp:121`

## 4. 技术方案

各子任务互不依赖（T-12 除外）。原则是“最小改动加单元测试”：先把可测的逻辑抽成 inline 或纯函数，再接回原调用点。

- T-1：`entry.id` 改用 `u8string()`；`Files()` 改用 `is_regular_file(error_code)`；`LoadResources` 内部用 try/catch 兜底，出错时保留内置英文和经典主题并写日志。
- T-2：xmake 为 `FavoriteWheel` 和 `RuntimeLayoutTests` 注入版本宏；`main.cpp` 的两处日志和测试期望值都改用宏。`package.ps1` 比较 DLL 的 `FileVersion` 前三段与 `set_version`；如果 `src/`、`include/`、`xmake.lua` 中有文件比 DLL 新，就失败退出；打印 DLL 的 SHA256。
- T-3：`CombatCycleModel` 的 `Encode`/`Decode` 都 `imbue(std::locale::classic())`；`Ordering::Load` 解析 FormID 后调用新函数 `RemoveDuplicateKeys`。`Decode` 保持现有的严格校验。
- T-6：语言解析结果按 `config.language` 做单项缓存，`LoadResources` 时清空；UI 字形串按（解析后的语言、hotkey、switchKey、actionHotkey）缓存；`PrepareFonts` 只在缓存键变化时才执行 `AddText(UIGlyphs)`。缓存用互斥锁保护（`Tr` 可能在任务回调中调用）。
- T-7：抽出纯函数，用二分查找 UTF-8 截断点（测量函数单调时，结果与旧算法逐字节一致）；`Draw.cpp` 改为调用它。
- T-8：给 `Key` 加哈希；`Sort` 先一次性建立 `unordered_map<Key,size_t,KeyHash>` 作为 rank；`Observe` 改用 `unordered_set<Key,KeyHash>`。比较语义不变。
- T-9：用 exe 目录推导 Data 根目录，复用 `Settings.cpp` 里 `SettingsFile()` 已验证的做法；资源和 Outfits 导入导出改用这个根目录。
- T-12：`InputHook` 在“未打开、未排队、未关闭中”时走轻量路径，不做完整快照；打开时每批只做一次快照；`Cancel` 清理 `view.items`/`inventoryGlyphs`（关闭动画使用渲染线程的 `lastView`，不受影响）；`TextBridge::Query/Reset` 缓存函数指针。
- 被否决的方案：用递归锁替换 `viewMutex` 来“省事”（会掩盖 HANDOFF §5.1 类问题）；把 Ordering 记录升到 v2（不需要，去重即可）。

### 4.1 接口契约（新增或修改的公开签名；其余实现细节由 Coder 决定）

```cpp
// include/InventoryOrderModel.h（T-3、T-8）
namespace Wheel::Ordering {
    // 每组按 Token(key) 保留首次出现的键，保持原有相对顺序；不改动 mode。
    void RemoveDuplicateKeys(State& state);
    struct KeyHash { std::size_t operator()(const Key& key) const noexcept; };
}
// include/Favorites.h（T-5）
namespace Wheel {
    // items 中 quickSlot==slot 的条目通过 SetQuickSlotSnapshot(item,-1) 清除；slot 非法时不做任何事。
    inline void ClearQuickSlotSnapshots(std::vector<Item>& items,int slot);
}
// include/TextFit.h（T-7，新文件，header-only）
namespace Wheel {
    using TextMeasure=std::function<float(std::string_view)>;
    // 整串宽度 ≤ width 时原样返回；否则返回最长的 UTF-8 完整前缀加 "..."，
    // 使 measure(前缀+"...") ≤ width；前缀可以为空（结果为 "..."）。
    std::string FitText(std::string_view source,float width,const TextMeasure& measure);
    // 等价于现有 Wrapped：先 FitText(source,lineWidth*lines)，再按 wrappedHeight ≤ lineHeight*lines 截断。
    std::string WrapText(std::string_view source,float lineWidth,float lineHeight,int lines,
        const TextMeasure& measure,const TextMeasure& wrappedHeight);
}
// include/Settings.h（T-9）
namespace Wheel {
    std::filesystem::path GameDirectory();       // SkyrimSE.exe 所在目录；获取失败返回空路径
    std::filesystem::path PluginDataDirectory(); // GameDirectory()/"Data/SKSE/Plugins/FavoriteWheel"；为空时返回同名相对路径
}
// include/UIResources.h（T-9）：默认参数改为空串，空串表示 PluginDataDirectory()
void LoadResources(const std::string& root={}); // T-1 起：内部捕获所有异常，不再向外抛出
```

- 宏（T-2，由 xmake 注入）：`FAVORITEWHEEL_VERSION_STRING`（字符串字面量，如 `"0.9.0"`）、`FAVORITEWHEEL_VERSION_MAJOR`/`_MINOR`/`_PATCH`（整数）。
- 脚本（T-2）：`scripts/package.ps1` 新增 `[switch]$AllowStaleDll`；版本不一致时始终失败，不能绕过。
- 脚本（T-10）：`scripts/run-tests.ps1 [-Targets <string[]>] [-NoBuild]`；默认运行审查报告 §5 列出的 16 个目标；退出码等于失败目标数；遇到 `FavoriteWheel` 或 `RuntimeLayoutTests` 直接拒绝并退出码 64。

### 4.2 数据结构 / 存储变更

- T-1 到 T-10、T-12、T-13：所有 SKSE 记录 ID、版本、文本头、模式编号、身份字段和上限都不变（HANDOFF §5.4/5.5）；INI 键不变。
- T-11【需决策】：`FWC 1` 记录接受 `health==0`。格式文本不变，但 ≤0.9.0 读到这类记录会整条拒收（降级会丢失全部战斗预设）。必须在发布说明中写明。

### 4.3 通用约束

1. **禁止回归 HANDOFF §5**：不得在持有 `viewMutex` 时调用 `IsOpen()`、`ValidPlayer()`、`Snapshot()`、`Cancel()`、`RequestDirectory(false)`；`EndSortDrag` 保持“先算 allowed，再加视图锁”的顺序；`DirectoryRequestNeeded` 的语义不变；排序序列化继续使用 classic locale。
2. 锁顺序只能是 `lifecycleMutex`→`open`/`action`/`view`/`pause`、`actionMutex`→`viewMutex`、`viewMutex`→`settings`/`Ordering`、`Outfits`→`Combat`/`Cycles`/`Ordering`，不得新增反向嵌套。T-6 新增的缓存锁必须是叶子锁（持有期间不调用任何其他函数）。
3. 构建 `FavoriteWheel` 会自动部署到 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`（AGENTS.md）：不得覆盖用户 INI，部署后核对 DLL 的 SHA256；临时改了 `deploy_dir` 必须恢复。不得接管键鼠或启动游戏；需要游戏内验证时交给用户。
4. 沿用项目的紧凑代码风格和英文注释；不引入新的第三方依赖。
5. 只验证纯逻辑时，用测试目标或 T-10 的 runner，不要为了跑测试去构建 `FavoriteWheel`。

## 5. 验收标准

| ID | 描述（可测试） | 验证方式 |
|---|---|---|
| AC-1 | 临时根目录放入 `测试主题.ini`（UTF-8 文件名，`Name=测` 加若干颜色键）：`Themes()` 包含 id 为 UTF-8 `测试主题` 的主题；`EditSettings` 选中它后 `SaveSettings()==true`；重新 `LoadSettings` 后 `Config().theme` 与之相等 | SettingsTests |
| AC-2 | `LoadResources` 的 Themes 目录中有无法访问的目录项时不抛异常，内置英文与经典主题仍然可用 | SettingsTests |
| AC-3 | 某组成员和 current 都设为 999999（会被千位分组的数字），在自定义千位分组的全局 locale 下执行循环记录 Encode，输出与 classic 下逐字节一致；在同一 locale 下 Decode 往返相等；测试结束后恢复全局 locale | CombatCycleTests |
| AC-4 | 对某组人为构造两个 Token 相同的键：`RemoveDuplicateKeys` 后只剩首个，其他组和 mode 不变；随后 `Encode`→`Decode` 成功 | InventoryOrderTests |
| AC-5 | `assets/Languages/en.ini` 中除 `Name`/`Font` 外的每个键，在空语言根目录下 `Tr(config,key)!=key`；`UIGlyphs(config)` 包含 `swapMouseHands` 的英文译文 | SettingsTests |
| AC-6 | `ClearQuickSlotSnapshots`：slot=2 时，只把 quickSlot==2 的条目改为 -1；weaponStack 且已收藏的条目，其 `weaponStackHotkey` 按 `SetQuickSlotSnapshot` 规则更新；slot=-1 或 8 时不做任何改变 | InventoryTests |
| AC-7 | 手工：打开收藏 → R → 悬停战斗预设按 3 → R → 按 T，原 3 号物品不再显示“3” | 游戏内（用户执行） |
| AC-8 | 对全部 8 种语言 × 内置全部键，缓存版 `Tr` 的结果与重新 `LoadResources` 后首次调用一致；切换 `language` 后 `UIGlyphs` 内容随之变化，再切回后与原值相等 | SettingsTests |
| AC-9 | 用固定宽度测量函数和随机或边界 UTF-8 串（空串、恰好等宽、多字节字符、宽度 0）比较：`FitText`/`WrapText` 的输出与测试内保留的旧算法参考实现逐字节一致 | 新增或扩展测试（可放在 ItemInfoTests 或新测试目标 TextFitTests） |
| AC-10 | 5,000 个随机键（含重复名称、不同 UID/health）在 Custom、Name、Type、Stat 各模式下，新 `Sort` 的结果顺序与旧实现完全一致；现有 InventoryOrderTests 全部通过 | InventoryOrderTests |
| AC-11 | `PluginDataDirectory()` 等于测试 exe 所在目录加 `Data/SKSE/Plugins/FavoriteWheel`；`LoadResources("")` 与显式传入该路径的结果相同 | SettingsTests |
| AC-12 | `package.ps1`：把 DLL 的版本或时间伪造成旧值时，脚本以非零退出码失败并提示原因；版本一致且 DLL 最新时正常进入后续步骤（可在临时副本上验证，不覆盖 dist） | 手工脚本步骤 |
| AC-13 | 构建后日志中的版本字符串、RuntimeLayoutTests 的期望版本都来自同一个宏；把 `set_version` 临时改成 9.9.9 后，三处一起变化（验证后恢复） | 构建检查 |
| AC-14 | `run-tests.ps1` 默认运行 16 个目标并全部通过，退出码 0；人为让某个测试失败时，退出码为 1 且输出该目标名；传入 `FavoriteWheel` 时退出码 64 | 手工脚本步骤 |
| AC-15 | T-11：`health=0` 的战斗预设可以 Encode/Decode；`health<0`/NaN/Inf 仍然拒收；法术目标仍要求 `health==1` | CombatPresetTests |
| AC-16 | T-12：现有 16 个测试全部通过；游戏内回归：Q/Shift+Q 开关、关闭后不误攻击、拖拽松手（HANDOFF §5.1）、数值排序加载（§5.2）、手柄十字键翻页、Text Bridge 命名输入 | 测试 + 游戏内（用户执行） |
| AC-17 | T-13：删除的函数在 `src/`、`include/`、`tests/` 中没有残留引用；合并后的辅助函数被所有原调用点使用；全部测试通过 | grep + 测试 |

## 6. 子任务拆分

| ID | 内容 | 审查编号 | 涉及文件 | 验收 | 依赖 | 可并行 | 风险 / 标注 |
|---|---|---|---|---|---|---|---|
| T-1 | 主题 ID 改 UTF-8，资源加载不抛异常 | A-01 | `src/UIResources.cpp`、`tests/SettingsTests.cpp` | AC-1、AC-2 | - | 是 | 低；非 ASCII 主题从不可用变为可用 |
| T-2 | 打包校验和统一版本号 | A-02、A-11 | `scripts/package.ps1`、`xmake.lua`、`src/main.cpp`、`tests/RuntimeLayoutTests.cpp` | AC-12、AC-13 | - | 是 | 低；验证 AC-13 需要构建（会部署） |
| T-3 | 循环记录 locale，排序记录去重 | A-03、A-04 | `include/CombatCycleModel.h`、`include/InventoryOrderModel.h`、`src/InventoryOrder.cpp`、相关测试 | AC-3、AC-4 | - | 是 | 低；格式不变 |
| T-4 | 补齐内置翻译键并加测试 | A-06 | `src/UIResources.cpp`、`tests/SettingsTests.cpp` | AC-5 | - | 是 | 低；修复缺字 |
| T-5 | 快捷槽角标同步 | A-07 | `include/Favorites.h`、`src/Wheel.cpp`、`tests/InventoryTests.cpp` | AC-6、AC-7 | - | 是 | 低；修复显示 |
| T-6 | 翻译与字形缓存 | A-16 | `src/UIResources.cpp`、`src/WheelFonts.cpp`、`tests/SettingsTests.cpp` | AC-8 | T-4（建议） | 是 | 中：缓存失效遗漏会导致语言切换后显示旧文本 |
| T-7 | 文本截断改二分 | A-18 | `include/TextFit.h`（新）、`src/Draw.cpp`、测试 | AC-9 | - | 是 | 低；用 WheelPreview 对比图像 |
| T-8 | 排序哈希 rank | A-19 | `include/InventoryOrderModel.h`、`tests/InventoryOrderTests.cpp` | AC-10 | T-3（同文件，建议串行） | 否 | 中：比较语义必须完全不变 |
| T-9 | Data 路径锚定 | A-08 | `include/Settings.h`、`src/Settings.cpp`、`include/UIResources.h`、`src/UIResources.cpp`、`src/Outfits.cpp`、`src/main.cpp`、`tests/SettingsTests.cpp` | AC-11 | T-1（同文件，建议串行） | 否 | 低到中：MO2 下需要用户确认资源和导入导出正常 |
| T-10 | 测试 runner | A-21 | `scripts/run-tests.ps1`（新） | AC-14 | - | 是 | 低 |
| T-11 | 战斗预设零值 health | A-05 | `include/CombatPresetModel.h`、`tests/CombatPresetTests.cpp` | AC-15 | - | 是 | **【需决策】存档降级兼容；需写入发布说明** |
| T-12 | 输入快照与 Text Bridge 缓存 | A-17、A-20 | `src/Wheel.cpp`、`include/TextBridgeClient.h` | AC-16 | T-10 | 否 | 中到高：输入路径改动，必须游戏内回归 |
| T-13 | 死代码与重复函数 | A-12、A-13 | `src/Favorites.cpp`、`src/Outfits.cpp`、`src/CombatPresets.cpp`、`src/CombatCycles.cpp`、`include/*`、`tests/*` | AC-17 | T-12（若都改 Wheel.cpp） | 否 | 中：涉及装备语义，必须逐字保持原逻辑 |

推荐的最小组合：T-1、T-3、T-4、T-2、T-6（理由见审查报告）。第二批：T-5、T-8、T-7、T-9、T-10。T-11 等决策后再做；T-12、T-13 最后做。

## 7. 测试策略

- 纯逻辑：每个子任务把新增或修改的断言写进现有测试目标，用 T-10 的 runner（还没有时就逐个 `xmake build <Target>` 后运行 `build/windows/x64/release/<Target>.exe`）跑完 16 个目标。T-7 如果新建测试目标，要在 `xmake.lua` 中加 `set_default(false)` 的独立 binary，不能依赖 `FavoriteWheel`。
- 视觉：T-6、T-7 改动前后各用 `WheelPreview.exe` 输出 `gameplay`、`sorting`、`numeric-sort` 模式的图片，按像素比较（预期完全一致）；图片要先预处理成长边不超过 1280 像素再查看（HANDOFF §2）。
- 构建与部署：只有 T-2、T-12 以及最终合入需要构建 `FavoriteWheel`。构建后核对目标 DLL 的 SHA256，并确认用户主 INI 的哈希没有变化。
- 游戏内：AC-7、AC-16 以及 T-1、T-9 的主题和导入导出检查，交给用户执行；不得把“部署成功”表述为“游戏验证通过”。
- 回归重点（给 Tester）：拖拽松手不出现 `resource deadlock`（HANDOFF §5.1）；切换数值排序后条目能正常出现（§5.2）；保存后读档排序不丢（§5.3–5.5）；在中文 locale 下保存设置；切换语言后文本和字形立即更新。

## 8. 风险点

- T-6 和 T-12 都改动每帧或每次输入都会走的热路径：缓存失效或快路径判断一旦遗漏，就会出现“显示旧文本”或“关闭状态吞键/漏键”。T-12 必须在 T-10 落地后进行，并完成游戏内回归。
- T-8 的比较器必须与旧实现完全等价（包括 `stable_sort` 下对相等元素的处理），否则会改变用户已经保存的自定义顺序的显示结果。
- T-11 涉及存档兼容，必须由用户明确同意。

## 9. 变更记录

| 时间 | 变更内容 | 原因 |
|---|---|---|
| 2026-10-28 | 初版 | 接手审查后的修复计划草案 |
