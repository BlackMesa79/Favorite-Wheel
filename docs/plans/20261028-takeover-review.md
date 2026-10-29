# 接手审查修复计划

- Task ID: 20261028-takeover-review
- 状态: APPROVED（本批次：T-1、T-2、T-3、T-4、T-6）
- 基准分支: wf/20261028-takeover-review（基线 069d0c3，0.9.0）
- 审查依据: docs/reports/20261028-takeover-review-audit.md（下文用 A-xx 指代审查编号）

## 1. 背景与目标

项目已交给新团队。本次 review 没有发现 BLOCKER，但确认了 2 个 MAJOR，以及若干一致性问题和性能隐患。用户已看过审查报告，本批次只修其中 5 项：两个 MAJOR（A-01、A-02），以及序列化健壮性、缺字和每帧翻译开销这三项低风险修复。长期背景见 docs/ARCHITECTURE.md 末尾的“2026-10-28 接手补充”一节。

## 2. 范围

### In Scope
- T-1 主题 ID 改为 UTF-8，资源加载不再向外抛异常（A-01）
- T-2 打包前校验 DLL 版本与新鲜度；版本号只在一处定义（A-02、A-11）
- T-3 序列化健壮性：循环记录固定 classic locale；排序记录读档后去重（A-03、A-04）
- T-4 补齐内置英文翻译键，并加完整性测试（A-06）
- T-6 缓存语言解析结果和 UI 字形串（A-16）

### Out of Scope
- 后续批次（下次另立 task-id，编号沿用草案）：
  - T-5 战斗快捷槽角标同步（A-07）
  - T-7 文本截断改为非二次复杂度（A-18）
  - T-8 自定义排序哈希 rank（A-19）
  - T-9 Data 路径锚定 exe 目录（A-08）
  - T-10 统一测试 runner 脚本（A-21）
  - T-11【需决策：存档兼容】战斗预设接受 health=0（A-05）
  - T-12 输入快照与 Text Bridge 缓存（A-17、A-20）
  - T-13 死代码与重复辅助函数（A-12、A-13）
- A-09、A-10、A-14、A-15：等用户决定后另立任务；A-22 拆分大文件：另立 task-id。
- 不做新功能：功能轮盘排序、拼音排序、排序键重绑定、菜单内施放能力、额外功能小轮盘、能力音效实验（已撤回，禁止恢复）。
- 不处理 PureDark/TRP 可见性问题，不修改渲染目标策略或 Present hook 方式。
- 不改 HANDOFF_CURRENT.md 及历史 RELEASE/专题文档的正文；不打包、不发布、不上传。

## 3. 与本次相关的现状

- 长期背景：见 docs/ARCHITECTURE.md §“2026-10-28 接手补充”（模块索引、线程与锁顺序、序列化总表、INI 总表、构建与部署）。
- `src/UIResources.cpp:298,340,389-426`：资源加载、语言解析、`Tr`、`UIGlyphs`
- `src/WheelFonts.cpp:71-159`：`PrepareFonts`
- `include/CombatCycleModel.h:48-68`、`src/InventoryOrder.cpp:37-47`、`include/InventoryOrderModel.h:31-106`
- `scripts/package.ps1:5-15`、`xmake.lua:2,24-34,242-249`、`src/main.cpp:35,44`、`tests/RuntimeLayoutTests.cpp:121`
- 插件导出版本由 `commonlibsse-ng.plugin` 规则从 `set_version` 生成（`xmake.lua:2,26`）；`RuntimeLayoutTests` 会加载 `build/windows/x64/release/FavoriteWheel.dll` 来检查导出版本（`tests/RuntimeLayoutTests.cpp:116-121`），所以要先构建 `FavoriteWheel`。

## 4. 技术方案

原则是“最小改动加单元测试”：先把可测的逻辑抽成 inline 或纯函数，再接回原调用点。

- T-1：`entry.id` 改用 `u8string()`；`Files()` 改用 `is_regular_file(error_code)`；`LoadResources` 内部用 try/catch 兜底，出错时保留内置英文和经典主题并写日志。
- T-2：xmake 为 `FavoriteWheel` 和 `RuntimeLayoutTests` 注入版本宏；`main.cpp` 的两处日志和测试期望值都改用宏。`package.ps1` 比较 DLL 的 `FileVersion` 前三段与 `set_version`；如果 `src/`、`include/`、`xmake.lua` 中有文件比 DLL 新，就失败退出；打印 DLL 的 SHA256。
- T-3：`CombatCycleModel` 的 `Encode`/`Decode` 都 `imbue(std::locale::classic())`；`Ordering::Load` 解析 FormID 后调用新函数 `RemoveDuplicateKeys`。`Decode` 保持现有的严格校验。
- T-4：把 `assets/Languages/en.ini` 中所有键（除 `Name`/`Font`）补进 `src/UIResources.cpp` 的内置英文表。
- T-6：语言解析结果按 `config.language` 做单项缓存，`LoadResources` 时清空；UI 字形串按（解析后的语言、hotkey、switchKey、actionHotkey）缓存；`PrepareFonts` 只在缓存键变化时才执行 `AddText(UIGlyphs)`。缓存用互斥锁保护（`Tr` 可能在任务回调中调用）。
- 被否决的方案：把 Ordering 记录升到 v2（不需要，去重即可）。

### 4.1 接口契约（新增或修改的公开签名；其余实现细节由 Coder 决定）

```cpp
// include/InventoryOrderModel.h（T-3）
namespace Wheel::Ordering {
    // 每组按 Token(key) 保留首次出现的键，保持原有相对顺序；不改动 mode。
    void RemoveDuplicateKeys(State& state);
}
// include/UIResources.h（T-1）：签名不变，语义改为内部捕获所有异常，不再向外抛出
void LoadResources(const std::string& root="Data/SKSE/Plugins/FavoriteWheel"); // include/UIResources.h:27
```

- 宏（T-2，由 xmake 注入）：`FAVORITEWHEEL_VERSION_STRING`（字符串字面量，如 `"0.9.0"`）、`FAVORITEWHEEL_VERSION_MAJOR`/`_MINOR`/`_PATCH`（整数）。
- 脚本（T-2）：`scripts/package.ps1` 新增 `[switch]$AllowStaleDll`，只用来跳过新鲜度检查；版本不一致时始终失败，不能绕过。

### 4.2 数据结构 / 存储变更

- 所有 SKSE 记录 ID、版本、文本头、模式编号、身份字段和上限都不变（HANDOFF §5.4/5.5）；INI 键不变。

### 4.3 通用约束

1. **禁止回归 HANDOFF §5**：不得在持有 `viewMutex` 时调用 `IsOpen()`、`ValidPlayer()`、`Snapshot()`、`Cancel()`、`RequestDirectory(false)`；排序序列化继续使用 classic locale。
2. 锁顺序只能是 `lifecycleMutex`→`open`/`action`/`view`/`pause`、`actionMutex`→`viewMutex`、`viewMutex`→`settings`/`Ordering`、`Outfits`→`Combat`/`Cycles`/`Ordering`，不得新增反向嵌套。T-6 新增的缓存锁必须是叶子锁（持有期间不调用任何其他函数）。
3. **构建与部署（AGENTS.md）**：构建 `FavoriteWheel` 会通过 `after_build` 自动部署到 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
   - 不得覆盖用户 INI，也不要用源码里的示例 INI 替换游戏配置；部署前后核对用户主 INI 的 SHA256，两者必须一致。
   - 临时改过 `deploy_dir` 时，完成后必须恢复。
   - 不得接管键鼠或启动游戏；需要游戏内验证时交给用户。
4. 沿用项目的紧凑代码风格和英文注释；不引入新的第三方依赖。
5. 只验证纯逻辑时，只构建对应的测试目标，不要为了跑测试去构建 `FavoriteWheel`。

## 5. 验收标准

AC 编号沿用草案，便于和审查报告交叉引用；未选子任务的 AC 已删除。

| ID | 描述（可测试） | 验证方式 |
|---|---|---|
| AC-1 | 临时根目录放入 `测试主题.ini`（UTF-8 文件名，`Name=测` 加若干颜色键）：`Themes()` 包含 id 为 UTF-8 `测试主题` 的主题；`EditSettings` 选中它后 `SaveSettings()==true`；重新 `LoadSettings` 后 `Config().theme` 与之相等 | SettingsTests |
| AC-2 | `LoadResources` 的 Themes 目录中有无法访问的目录项时不抛异常，内置英文与经典主题仍然可用 | SettingsTests |
| AC-3 | 某组成员和 current 都设为 999999（会被千位分组的数字），在自定义千位分组的全局 locale 下执行循环记录 Encode，输出与 classic 下逐字节一致；在同一 locale 下 Decode 往返相等；测试结束后恢复全局 locale | CombatCycleTests |
| AC-4 | 对某组人为构造两个 Token 相同的键：`RemoveDuplicateKeys` 后只剩首个，其他组和 mode 不变；随后 `Encode`→`Decode` 成功 | InventoryOrderTests |
| AC-5 | `assets/Languages/en.ini` 中除 `Name`/`Font` 外的每个键，在空语言根目录下 `Tr(config,key)!=key`；`UIGlyphs(config)` 包含 `swapMouseHands` 的英文译文 | SettingsTests |
| AC-8 | 对全部 8 种语言 × 内置全部键，缓存版 `Tr` 的结果与重新 `LoadResources` 后首次调用一致；切换 `language` 后 `UIGlyphs` 内容随之变化，再切回后与原值相等 | SettingsTests |
| AC-12 | `package.ps1`：把 DLL 的版本或时间伪造成旧值时，脚本以非零退出码失败并提示原因；版本一致且 DLL 最新时正常进入后续步骤（在临时副本上验证，不覆盖 dist，不实际打包） | 手工脚本步骤 |
| AC-13 | 日志中的版本字符串、插件导出版本、RuntimeLayoutTests 的期望版本都来自 `set_version`。步骤：①把 `set_version` 临时改成 9.9.9，构建 `FavoriteWheel` 和 `RuntimeLayoutTests` 并运行，测试通过，同时用 `strings`/grep 确认 DLL 中含 `FavoriteWheel 9.9.9`；②**恢复 `set_version("0.9.0")`**，重新构建 `FavoriteWheel`（重新部署）和 `RuntimeLayoutTests`，运行通过；③最终部署目录中 `FavoriteWheel.dll` 的 SHA256 与 `build/windows/x64/release/FavoriteWheel.dll` 一致，用户 INI 的 SHA256 前后不变；④`git diff xmake.lua` 中没有残留 9.9.9 | 构建检查（报告中贴出两次 SHA256） |

## 6. 子任务拆分

| ID | 内容 | 审查编号 | 涉及文件 | 验收 | 依赖 | 可并行 | 风险 / 标注 |
|---|---|---|---|---|---|---|---|
| T-1 | 主题 ID 改 UTF-8，资源加载不抛异常 | A-01 | `src/UIResources.cpp`、`tests/SettingsTests.cpp` | AC-1、AC-2 | - | 与 T-2、T-3 并行 | 低；非 ASCII 主题从不可用变为可用 |
| T-4 | 补齐内置翻译键并加测试 | A-06 | `src/UIResources.cpp`、`tests/SettingsTests.cpp` | AC-5 | T-1（同文件串行） | 否 | 低；修复缺字 |
| T-6 | 翻译与字形缓存 | A-16 | `src/UIResources.cpp`、`src/WheelFonts.cpp`、`tests/SettingsTests.cpp` | AC-8 | T-4（同文件串行；缓存测试要覆盖补齐后的键） | 否 | 中：缓存失效遗漏会导致语言切换后显示旧文本 |
| T-3 | 循环记录 locale，排序记录去重 | A-03、A-04 | `include/CombatCycleModel.h`、`include/InventoryOrderModel.h`、`src/InventoryOrder.cpp`、`tests/CombatCycleTests.cpp`、`tests/InventoryOrderTests.cpp` | AC-3、AC-4 | - | 是 | 低；格式不变 |
| T-2 | 打包校验和统一版本号 | A-02、A-11 | `scripts/package.ps1`、`xmake.lua`、`src/main.cpp`、`tests/RuntimeLayoutTests.cpp` | AC-12、AC-13 | 代码改动无依赖；AC-13 的最终构建部署要等 A、B 组完成后再做 | 代码可并行 | 低；验证 AC-13 需要构建（会部署） |

**Coder 分组建议**

- **Coder-A（串行）T-1 → T-4 → T-6**：三项都改 `src/UIResources.cpp` 和 `tests/SettingsTests.cpp`，由同一个 Coder 按顺序完成。每完成一项就构建并运行 SettingsTests，通过后再开始下一项。涉及文件：`src/UIResources.cpp`、`src/WheelFonts.cpp`、`tests/SettingsTests.cpp`。
- **Coder-B T-3**：涉及文件为 `include/CombatCycleModel.h`、`include/InventoryOrderModel.h`、`src/InventoryOrder.cpp`、`tests/CombatCycleTests.cpp`、`tests/InventoryOrderTests.cpp`。与 A 组没有文件交集。
- **Coder-C T-2**：涉及文件为 `scripts/package.ps1`、`xmake.lua`、`src/main.cpp`、`tests/RuntimeLayoutTests.cpp`。可以和 A、B 组同时改代码，但**会触发部署的构建（AC-13）必须等 A、B 组提交后再做**，否则部署出去的 DLL 会混入半成品代码；另外多个 Coder 不要在同一个工作区同时运行 xmake（构建目录会冲突）。并行工作时请各自使用 worktree，或者把构建串行化。
- 合入顺序：B、A 先合入，C 最后合入，并以 C 的 AC-13 步骤②③的产物作为最终部署。

## 7. 测试策略

- 本批次没有 runner（T-10 不做）。统一做法：逐个执行 `xmake build <Target>`，然后运行 `build/windows/x64/release/<Target>.exe`，检查退出码是否为 0，并把日志保存到 `build/takeover-review/`。
- 本批次的主测试目标：
  - SettingsTests（AC-1、2、5、8）
  - CombatCycleTests（AC-3）
  - InventoryOrderTests（AC-4）
  - RuntimeLayoutTests（AC-13；需要先构建 `FavoriteWheel`，只由 Coder-C 或最终验收时运行）
- 回归（合入前全部运行，审查报告 §5 的 16 个目标）：WheelLogicTests、GameSessionTests、HandEquipTests、LayoutSkinTests、CombatCycleTests、CombatPresetTests、OutfitTests、NameEditorTests、FaceLightClientTests、ItemInfoTests、InventoryTests、TimeTests、InventoryOrderTests、SettingsTests、SettingsPersistenceTests、SectorMeshTests。最终验收时再加跑 RuntimeLayoutTests。
- 视觉：T-6 改动前后各用 `WheelPreview.exe` 输出 `gameplay`、`sorting`、`numeric-sort` 模式的图片，按像素比较，预期完全一致。图片先预处理成长边不超过 1280 像素再查看（HANDOFF §2）。
- 构建与部署：只有 T-2（AC-13）和最终合入需要构建 `FavoriteWheel`。要求见 §4.3-3 和 AC-13。
- 游戏内：T-1 的中文主题选择和 T-6 的语言切换后文本刷新，交给用户执行；不得把“部署成功”表述为“游戏验证通过”。
- 回归重点（给 Tester）：在中文 locale 下保存设置；切换语言后文本和字形立即更新；保存后读档排序不丢（HANDOFF §5.3–5.5）。

## 8. 风险点

- T-6 改动的是每帧都会走的热路径：缓存失效一旦遗漏，就会出现“显示旧文本”或“缺字”。`LoadResources`、语言切换、热键变更都必须让缓存失效；缓存锁必须是叶子锁。
- AC-13 会临时把 9.9.9 部署到用户的测试 mod 目录：如果中途失败，没有恢复并重新部署，游戏里就会留下一个错误版本号的 DLL。Coder-C 必须完成步骤②③后才能报告完成。
- 并行构建：A、B、C 同时在一个工作区运行 xmake，或者 C 过早部署，都会导致最终部署的 DLL 和合入后的源码不一致；以最终 SHA256 核对结果为准。
- `package.ps1` 的新鲜度检查依赖文件修改时间，切换 git 分支后可能误判为过期。按设计这种情况会失败退出，失败提示中要建议重新构建。

## 9. 变更记录

| 时间 | 变更内容 | 原因 |
|---|---|---|
| 2026-10-28 | 初版 | 接手审查后的修复计划草案 |
| 2026-10-29 | 定稿为 APPROVED：本批次只做 T-1、T-2、T-3、T-4、T-6，其余移入后续批次；删除未选子任务的方案和 AC（保留原 AC 编号）；测试改为逐个构建目标；补充 Coder 分组和 AC-13 恢复部署要求 | 按用户选择定稿 |
