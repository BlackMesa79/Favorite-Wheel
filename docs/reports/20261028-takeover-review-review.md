# 20261028-takeover-review：代码审查报告（第 1 轮）

- 审查范围：`git diff a9dbd65..HEAD`（T-1、T-2、T-3、T-4、T-6 的实现，以及 Tester 第 0、1 轮的测试提交）
- 依据：`docs/plans/20261028-takeover-review.md`（APPROVED）、`-code-A/B/C.md`、`-test.md`
- 方式：只读审查，没有修改代码，也没有重新构建或运行测试。测试结果引用 Tester 第 1 轮报告（18 个目标退出码均为 0，视觉回归逐字节一致，部署 DLL 与构建产物 SHA256 一致，用户 INI 未变）。

## 结论摘要

没有 BLOCKER，也没有 MAJOR。五个子任务的实现与 plan §4、§4.1 的契约一致；序列化格式、记录 ID、INI 键都没变；没有触碰 `viewMutex` 相关路径，也没有新增锁嵌套。下面列出 5 条 MINOR 和 2 条设计偏离，都不阻塞合并。

## 逐项核对

### T-6 缓存（重点 1）
- **线程安全**：`src/UIResources.cpp:232-236` 用 `shared_ptr<const ...>` 不可变快照加 `cacheMutex`。持锁期间只做 shared_ptr 的复制和移动（`:430-433`、`:436-439`、`:462-465`、`:474-477`、`:336-339`）。`ResolveLanguage`、`Tr`、`KeyLabel`、字符串拼接都在锁外执行，所以这是叶子锁，符合 §4.3-2。持锁期间最多会释放一个旧快照，也就是只做内存释放，不会调用项目函数。
- **返回值生命周期**：`ActiveLanguage` 和 `UIGlyphs` 都按值返回 `std::string`，不会把缓存内部的引用或指针交给调用方，所以没有悬垂风险。
- **失效完整性**：
  - 语言缓存的键是 `config.language`。解析结果还依赖 `languages` 和 `systemLanguage`，但两者只在 `LoadResources` 里修改，而 `LoadResources` 一进来就清空缓存（`:334-339`），因此完整。
  - 字形缓存的键是“解析后语言、hotkey、switchKey、actionHotkey”。字形串的其余内容（译文、语言名、主题名）只在 `LoadResources` 时变化，也就跟着被清空。`KeyLabel` 只依赖 hotkey 和 `Tr(...,"follow")`，所以键是完整的。
  - 字体、缩放、titleScale、labelScale 变化时，`WheelFonts.cpp:78-83` 会同时清空 `glyphs` 和 `preparedUIGlyphs`。`glyphs` 是只增的位集，切回旧语言时不会缺字。
- **生产调用点**：`LoadResources` 只在 `main.cpp:29` 启动时调用，早于任何渲染和任务回调，所以“清空缓存”和“替换资源”之间的时间窗在生产环境中不会被并发读到（见建议 S-1）。

### T-1（重点 2）
- 主题 ID（`:361`）与既有的语言 ID（`:349`）、`Settings.cpp:20`、`UIResources.cpp:244` 用的是同一种 `u8string()`→`std::string` 逐字节转换。项目使用 `cxx23`，`u8string` 是 `std::u8string`，用迭代器赋值可以正确拷贝字节。全仓库没有残留 `stem().string()` 或其他 ACP 回退路径。
- `Files()`（`:305-313`）对每个目录项使用 `is_regular_file(error_code)`，出错就跳过该项；迭代出错时结束枚举。
- `LoadResources` 的 try/catch（`:333`、`:408-413`）：内置英文和 classic 主题在进入循环前就已写入 `languages`/`themes`，外部资源先写进暂存副本，全部成功后才 `swap`。因此任何异常都会留下“内置英文 + classic”的一致状态，缓存也已清空，不会沿用旧结果。

### T-3（重点 3）
- `CombatCycleModel.h:50` 的 `Encode` 和 `:60` 的 `Decode` 都对各自唯一的流执行了 `imbue(classic)`，其中 `std::quoted` 读写的是同一个流。整个文件只有这两个流，已全部覆盖。
- `RemoveDuplicateKeys`（`InventoryOrderModel.h:39-44`）按组用 `erase_if` 加 `seen` 集合去重：保留首个，保持原有顺序，不改 `mode`。唯一的生产调用点在 `InventoryOrder.cpp:46`，位于 FormID 重定位之后、发布状态之前。这个位置是对的：读档时两个不同的原始键被重定位成同一个 FormID，也会被去掉。`Decode` 的严格校验没有改，测试也守住了这一点。

### T-2（重点 4）
- `xmake.lua:18-27` 的 `favoritewheel.version` 规则从 `target:version()` 派生 4 个宏，挂在 `FavoriteWheel`（`:37`）和 `RuntimeLayoutTests`（`:256`）上。`main.cpp:35,44` 和 `RuntimeLayoutTests.cpp:121` 都已改用宏。Coder-C 的 9.9.9 往返验证与 Tester 的最终核对都确认了这一点。
- `package.ps1:11-16` 的版本校验放在 `if (!$AllowStaleDll)` 之外，因此 `-AllowStaleDll` 无法绕过它。Tester 用 `old-version-allow-stale` 用例验证过：退出码为 1。
- 新鲜度检查覆盖了 `xmake.lua`、`src/`、`include/`，误报见 M-2。

### 通用约束（重点 5）
- HANDOFF §5 列出的回归点、锁顺序、序列化格式都没变。代码风格有轻微偏差，见 M-3。

### 测试有效性（重点 6）
- AC-1、AC-3、AC-4、AC-5 在修复前为红、修复后为绿，Tester 第 0 轮有实测记录。
- AC-8 对 hotkey/actionHotkey 做了变异验证，证明不是空转。语言缓存的 `LoadResources` 失效，由既有用例 `SettingsTests.cpp:320-330` 覆盖（edited 的语言是 auto，先加载 missing 再加载 automaticRoot，如果不失效就会失败）。
- 字形缓存的 `LoadResources` 失效没有被覆盖，见 M-1。
- AC-2 修复前也能通过，属于已知并已接受的护栏用例（测试报告 §5.1），不算空转。

## 问题列表

### BLOCKER
无。

### MAJOR
无。

### MINOR

**M-1 [测试覆盖] 字形缓存在 `LoadResources` 后失效没有测试守护**
- 位置：`tests/SettingsTests.cpp:458-499`（AC-8），对应实现 `src/UIResources.cpp:334-339`
- 问题：AC-8 的所有 reference 都来自同一个 `assets` 根目录。在它之前，AC-5 只用空根目录、以 `en` 和默认键位填过一次字形缓存；AC-8 重新加载 `assets` 后，只比较了“en 与 zh 不同”“切回后相等”。
- 失败场景：如果以后有人把 `LoadResources` 入口的 `glyphCache` 清空删掉，`UIGlyphs(english)` 会一直返回 AC-5 留下的旧串（缺少 7 种语言名和各主题名），但 AC-8 的断言仍然全部通过。游戏里切换资源包或热重载后会缺字。当前实现没有这个问题，只是缺少守护。（置信度：高，依据是对断言的静态推演）
- 建议：在 AC-8 开头断言 `UIGlyphs(english)` 包含 `Languages()` 中某个非英文语言的 `name`；或者先加载空根目录、调用一次 `UIGlyphs`，再加载 `assets`，断言两次结果不同。

**M-2 [脚本] 只改动 `xmake.lua` 时，新鲜度检查可能持续误报，而提示中的“Rebuild”修不好**
- 位置：`scripts/package.ps1:17-26`
- 问题：如果只改了 `xmake.lua` 中与 `FavoriteWheel` 无关的部分（例如某个测试目标），`xmake build FavoriteWheel` 判定编译参数没变，不会重新链接，DLL 的时间戳保持不变。
- 失败场景：此后每次打包都会报 `DLL is stale: ...xmake.lua...`，按提示重新构建也无效，用户只能加 `-AllowStaleDll`，这个开关就失去了把关意义。（置信度：中，取决于 xmake 的增量判定；plan §8 只记录了切换分支导致的误报）
- 建议：失败提示改为建议 `xmake build -r FavoriteWheel`，也就是强制重建。

**M-3 [风格] 新增代码缩进不规整**
- 位置：`src/UIResources.cpp:308-311`（`Files()` 循环体的 `{` 单独成行，缩进错位）、`:333-407`（`try` 块内没有重新缩进）
- 问题：不影响行为，但不符合 plan §4.3-4“沿用紧凑风格”的约定，后续修改容易误读块边界。
- 建议：`Files()` 改回单行 `if(...)` 写法，在调用前声明 `statusError`；把 try 体抽成匿名命名空间里的 `LoadResourcesUnchecked(root)`，`LoadResources` 只负责 try/catch。

**M-4 [性能，未达预期] 每帧仍会整串复制并比较 UI 字形串**
- 位置：`src/WheelFonts.cpp:90-95`、`src/UIResources.cpp:466`
- 问题：命中缓存时 `UIGlyphs` 仍按值返回整串（300 多条译文，约 10 KB 量级），`PrepareFonts` 每帧还要拷贝一次并逐字节比较。开销比修复前小很多，但不是 plan 写的“键未变化时不做额外工作”。
- 失败场景：不会出错，只是热路径上还有 O(n) 的拷贝和比较。
- 建议：后续可以在内部暴露一个 generation 计数，或者让 `UIGlyphs` 返回 `shared_ptr<const std::string>`，`PrepareFonts` 只比较指针或计数。本批次可以不改。

**M-5 [可观测性] 资源加载失败只写 `OutputDebugStringA`**
- 位置：`src/UIResources.cpp:408-413`
- 问题：plan §4 写的是“写日志”，但玩家日志 `FavoriteWheel.log` 里看不到这条记录。Tester 的 O-1 也提到了这一点。
- 建议：保持当前做法，因为该文件也会编进测试目标，不能依赖 `SKSE::log`。可以在 `main.cpp:29` 之后，用 `Languages().size()==1 && Themes().size()==1` 之类的条件补一条 `SKSE::log::warn`；或者在 ARCHITECTURE 中注明。

### 建议（没有具体失败场景，不计入问题）
- S-1：`LoadResources` 在入口清空缓存，在末尾才替换资源。如果将来允许运行时热重载，中间这段时间里的并发读会把只有内置资源时的结果写回缓存，而且末尾不会再清空。建议届时在替换之后再清空一次。目前只有启动时调用，所以不受影响；而且 `languages`/`themes` 本身也不是线程安全的。
- S-2：`KeyLabel` 依赖 `GetKeyNameTextW`，结果会随当前键盘布局变化。缓存之后，运行中切换键盘布局不会刷新键名字形。属于极端情况。
- S-3：`ActiveLanguage` 和 `UIGlyphs` 都只缓存一项。如果渲染线程和设置预览使用的 `config.language` 不同而交替调用，缓存会反复失效，开销退回到修复前的水平，但结果仍然正确（Tester O-3）。
- S-4：新鲜度检查没有覆盖 `extern/` 和 xmake 配置选项（例如 `--skse_patch_safety=n`）。如果在关闭补丁安全模块的配置下构建出 DLL，版本和时间都会通过校验。建议在 `package.ps1` 中打印或校验 `.xmake` 中的关键配置项。

## 设计偏离

| # | 偏离 | 处理建议 |
|---|---|---|
| D-1 | plan §4 T-6 要求“`PrepareFonts` 只在缓存键变化时才执行 `AddText(UIGlyphs)`”，实现改为在消费端按内容比较（`WheelFonts.cpp:90-95`），没有新增公开接口，而且能识别资源重载。行为上更稳，代价见 M-4。 | 不需要 Coder 修正，建议 Architect 在 plan 变更记录中确认这一做法。 |
| D-2 | plan §4 T-1 要求“写日志”，实现只写调试输出（M-5）。 | 由 Architect 决定：接受现状并写入文档，或者按 M-5 的建议在 `main.cpp` 补一条 warn。 |

另外有一项流程差异，不属于代码偏离：Coder-C 的 AC-13 步骤①②是在 `deploy_dir=""` 的 worktree 中完成的，最终部署由 Tester 在主仓库中构建完成，部署 DLL 与构建产物的哈希一致，用户 INI 的哈希也没变，满足 AGENTS.md 的要求。AC-12 没有在 PowerShell 7 下原生运行（本机没有 PS7），这是测试环境的限制。

## 待用户执行（不属于代码问题）
- 游戏内验证：选择中文主题后保存并重新加载；切换语言后文本和字形立即刷新；保存后读档，排序不丢失。

结论：APPROVE
