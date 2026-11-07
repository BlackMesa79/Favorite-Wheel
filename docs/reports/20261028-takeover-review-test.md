# 接手审查修复：测试报告（第 0 轮：测试编写）

- Task ID: 20261028-takeover-review
- 分支 / 工作区: `wf/20261028-takeover-review-T` / `D:\MyFiles\Documents\ChatGPT\fw-wt\T`
- 轮次: **第 0 轮（仅编写测试；实现由三个 Coder 在其他 worktree 并行进行，本工作区代码仍是修复前版本）**
- 提交: `012350f tests: acceptance tests for AC-1..5,8`（本地提交，未推送、未合并）
- 状态: **本轮不判定 PASS/FAIL**（修复尚未合入）。修复前预期结果见第 4 节。

## 0. 结论摘要

- 新增 6 个独立测试函数（AC-1、AC-2、AC-5、AC-8 在 `tests/SettingsTests.cpp`，AC-3 在 `tests/CombatCycleTests.cpp`，AC-4 在 `tests/InventoryOrderTests.cpp`），共 +209 行，全部为追加；已有测试一行未改、顺序未动，只在各 `main` 中加了一行调用。
- 修复前实测（本机，ACP=936）：
  - AC-1 **断言失败**：主题 ID 是 GBK 字节（`b2 e2 ca d4 d6 f7 cc e2`），不是 UTF-8。
  - AC-3 **断言失败**：全局 locale 带千位分组时 Encode 输出被改写成 `999,999`。
  - AC-4 **编译失败**：`Wheel::Ordering::RemoveDuplicateKeys` 不存在。
  - AC-2、AC-8 **修复前即通过**。AC-8 是 T-6 的回归护栏（现在没有缓存，所以天然一致）；AC-2 的“抛异常”路径在本机**无法复现**，原因见 §5.1，需要 Architect/Coder 知晓。
- 用“预期修复”的临时副本（只在 `build/manual/` 里打补丁，未改仓库）验证过：6 个测试函数**全部可以满足**，说明测试没有写错方向（§3.3）。
- 未修改 `tests/` 以外的任何文件；`xmake.lua` 无需改动（没有新增依赖）。

## 1. 验收标准覆盖表

| AC | 测试函数 | 位置 | 覆盖内容 | 修复前预期 |
|---|---|---|---|---|
| AC-1 | `Ac1_Utf8ThemeId` | `tests/SettingsTests.cpp:383` | UTF-8 主题文件名 → UTF-8 主题 ID → `EditSettings`+`SaveSettings()` → 重新 `LoadSettings` 后 `Config().theme`，并核对 `Style().id`、`Name`、三个颜色键 | **失败**（断言：`Themes() exposes the UTF-8 theme ID taken from the file name`） |
| AC-2 | `Ac2_UnreadableThemeEntry` | `tests/SettingsTests.cpp:409` | Themes 里同时放：被独占句柄占用的 `busy.ini`、目录 `folder.ini`、0 字节 `empty.ini`、非法 UTF-8 `broken.ini`，Languages 里放非法 UTF-8；再加“资源根目录是普通文件”。要求 `LoadResources` 不抛异常，内置英文与经典主题仍可用 | **通过**（本机无法复现抛异常，见 §5.1） |
| AC-3 | `Ac3_CycleRecordsIgnoreDigitGrouping` | `tests/CombatCycleTests.cpp:69` | 某组成员与 current 都是 999999（另加一组 1234/5678），在自定义千位分组 locale 下 Encode 与 classic 逐字节一致；同一 locale 下 Decode 往返；classic 文本在该 locale 下也能读回；结束时恢复全局 locale | **失败**（断言：`digit grouping in the global locale cannot change the cycle record bytes`） |
| AC-4 | `Ac4_RemoveDuplicateKeys` | `tests/InventoryOrderTests.cpp:93` | 重复 Token 折叠到首次出现的位置、相对顺序保留；全同键组折叠为 1；所有组 mode 不变；无重复的组完全不动；去重后 `Encode`→`Decode` 成功且 `Encode` 稳定；重复调用幂等；同时守住“`Decode` 仍然拒收写过重复键的记录” | **编译失败**（`C3861: RemoveDuplicateKeys: 找不到标识符`） |
| AC-5 | `Ac5_BuiltinEnglishCoverage` | `tests/SettingsTests.cpp:438` | 空语言根目录下，`assets/Languages/en.ini` 中除 `Name`/`Font` 外的每个键都必须 `Tr(config,key)!=key`；`UIGlyphs(config)` 必须包含 `swapMouseHands` 的英文译文 | **失败**（断言：`the built-in English table has no value for swapMouseHands`） |
| AC-8 | `Ac8_TranslationCacheConsistency` | `tests/SettingsTests.cpp:461` | 8 种语言 × en.ini 全部键：缓存版 `Tr` 与“重新 `LoadResources` 后首次调用”一致（对比时按键-优先、逐次切换语言，专门压缓存失效）；切换 `language` 后 `UIGlyphs` 变化、切回后逐字节还原；改 `switchKey` 后 `UIGlyphs` 刷新、改回后还原 | **通过**（现在没有缓存；作为 T-6 的回归护栏） |
| AC-12 / AC-13 | 不在本轮范围 | — | 脚本/构建检查，由 Coder 验证 | — |

## 2. 边界与风险覆盖（对照 plan §8）

- **T-3 数字格式**（A-03）：成员 ID 取 999999（跨越 6 位）与 1234；同时验证“写”和“读”两个方向，以及“classic 写、分组 locale 读”的真实跨语言场景。
- **T-3 去重**（A-04）：
  - 只有一组有重复（`state[0]`）不足以证明“其他组不变”，因此额外让 `state[7]` 出现三个完全相同键，并在断言里明确“第 0、7 组允许变化，其余组必须逐字节相同”。
  - Token 相等意味着字段全等，因此“保留首次出现”只能通过**位置**观测：`{100 Sword},{200 Axe},{100 Sword},{300 Shield}` → `{100 Sword},{200 Axe},{300 Shield}`。
  - 明确要求 `Decode` 仍然拒收重复记录（plan 写明“Decode 保持现有严格校验”）；如果 Coder 改用“放宽 Decode”的写法，这条会失败并提示偏离 plan。
- **T-4 缺字**（A-06）：以 `assets/Languages/en.ini` 为唯一权威键表（当前 326 个键），排除 `Name`/`Font`，包含 `compactUseSwapped`、`swapMouseHands`、`swapMouseHandsHelp` 三个已知缺失键；同时验证 `UIGlyphs` 真的把 `swapMouseHands` 串进了字形预热文本，而不只是 `Tr` 能查到。
- **T-6 缓存**（A-16）：
  - 语言维度：8 种语言 × 全部键，逐键轮换语言（这正好是最容易暴露“只按首次语言缓存”的调用顺序）。
  - 失效维度：`language` 切换、`switchKey` 变更（plan §4.1/§8 明确缓存键包含 hotkey/switchKey/actionHotkey；“热键变更必须让缓存失效”）。
  - 观察方式是纯函数等值比较，不依赖时间/随机数，也不碰锁顺序与 `viewMutex`（不新增 HANDOFF §5 的违规调用）。
- **T-1 编码**（A-01）：主题文件名、`Name=测`、颜色键同时存在，且要求保存后**重新 LoadSettings** 仍相等（覆盖 `SettingsIni::Wide` 的 UTF-8 转换路径）；`Style(Config()).id` 覆盖“选中该主题”这一用户可见结果。

## 3. 执行记录

### 3.1 构建方式（xmake 无法配置）

`xmake f -m release -y --deploy_dir=` 在本机失败：

```
checking for platform ... windows (x64)
checking for Microsoft C/C++ Compiler (x64) ... ok
error: curl: (28) Failed to connect to github.com port 443 after 21072 ms: Could not connect to server
```

原因是 `includes("extern/CommonLibVR")` 会在配置阶段解析 xmake-repo 包（directxmath / directxtk / spdlog 等），而本机没有 xmake 全局包缓存（`~/.xmake` 不存在）且无法访问网络。**没有构建 `FavoriteWheel` 目标，也没有发生任何部署。**

因此改用 `cl.exe` 直接编译三个测试目标（等价于 `xmake build <Target>` 的编译单元组合，不含 `commonlibsse-ng` 依赖）：

```
cl /nologo /std:c++latest /utf-8 /EHsc /O2 /D NOMINMAX /D WIN32_LEAN_AND_MEAN /I include \
   tests/SettingsTests.cpp src/Settings.cpp src/UIResources.cpp /link user32.lib
cl /nologo /std:c++latest /utf-8 /EHsc /O2 /D NOMINMAX /D WIN32_LEAN_AND_MEAN /I include tests/CombatCycleTests.cpp
cl /nologo /std:c++latest /utf-8 /EHsc /O2 /D NOMINMAX /D WIN32_LEAN_AND_MEAN /I include tests/InventoryOrderTests.cpp
```

日志：`build/takeover-review/round0-manual-cl.log`。三个测试目标只依赖 `user32`（`GetKeyNameTextW`），与 `xmake.lua` 现有 `add_syslinks("user32")` 一致，**不需要改 `xmake.lua`**。

### 3.2 修复前实测结果

| 目标 | 结果 |
|---|---|
| SettingsTests | 编译通过；运行 `exit=1`，`AC-1: Themes() exposes the UTF-8 theme ID taken from the file name` |
| CombatCycleTests | 编译通过；运行 `exit=1`，`AC-3: digit grouping in the global locale cannot change the cycle record bytes` |
| InventoryOrderTests | **编译失败** `tests/InventoryOrderTests.cpp(104,119): error C3861: “RemoveDuplicateKeys”: 找不到标识符` |

因为三个测试文件都是“遇错即 `std::exit(1)`”，AC-1 失败会遮住同文件里后面的 AC-2/5/8，所以另外用“只调用单个 AC”的临时副本逐个确认：

| 单独运行 | 结果 |
|---|---|
| 仅 AC-2 | `exit=0`（通过） |
| 仅 AC-5 | `exit=1`，`AC-5: the built-in English table has no value for swapMouseHands` |
| 仅 AC-8 | `exit=0`（通过） |

诊断细节：本机 `GetACP()=936`，`path::stem().string()` 对 `测试主题` 产出 GBK 字节 `b2 e2 ca d4 d6 f7 cc e2`（8 字节），而 UTF-8 应为 `e6 b5 8b e8 af 95 e4 b8 bb e9 a2 98`（12 字节），所以 AC-1 在修复前必然失败——这也解释了 A-01 的“中文系统下非 ASCII 主题无法保存”。

临时文件清理说明：每个 AC 函数在成功路径末尾都会 `remove_all` 自己的 `build/acceptance-<ac>-<pid>` 目录；失败时因为测试框架是“遇错即 `std::exit(1)`”，进程直接退出、来不及清理（与已有测试的 `build/settings-test-<pid>` 行为一致）。这些目录都在 `.gitignore` 的 `/build/` 下，本轮已手工清理干净。

### 3.3 测试可满足性验证（防止把方向写错）

为避免“写了永远失败的测试”或把测试写成迎合某一种实现，在 `build/manual/` 里做了三份**临时补丁副本**（不触碰仓库文件，验证后已删除）：

| 补丁 | 内容 | 结果 |
|---|---|---|
| `probe-ac3` | 头文件副本给 `Encode`/`Decode` 加 `imbue(std::locale::classic())` | `exit=0` |
| `probe-ac4` | 注入参考实现 `RemoveDuplicateKeys`（按 Token 保留首次出现） | `exit=0` |
| `probe-acfix` | `src/UIResources.cpp` 副本：`entry.id` 改用 `u8string()`、`Files()` 用 `is_regular_file(error_code)`、补 3 个英文键 | SettingsTests 全量 `exit=0` |

结论：6 个测试函数都能被 plan 4.1 描述的契约满足，断言强度没有变成“只有某一种实现才能过”（例如 AC-4 不要求 `RemoveDuplicateKeys` 使用特定的容器或返回值）。

## 4. 覆盖的边界情况清单

- AC-1：UTF-8 文件名 + UTF-8 `Name` + 3 个颜色键；保存后重新读档（不是复用内存）；`Style()` 命中。
- AC-2：不可读条目（独占句柄）、目录伪装成 `.ini`、0 字节文件、非法 UTF-8 主题与语言文件、资源根目录本身是文件；两种根目录调用都不抛异常。
- AC-3：6 位数与 4 位数成员/current；自定义分组 `"," + "\3"`；写、读、跨 locale 读三个方向；全局 locale 恢复（断言前已恢复，失败也不会污染后续测试）。
- AC-4：首次出现位置保留、相对顺序保留、全同键组、mode 不变、无重复组不变、Encode 稳定性、幂等、`Decode` 仍严格。
- AC-5：326 个键全量（排除 `Name`/`Font`）；三个已知缺失键；`UIGlyphs` 预热。
- AC-8：8 语言 × 326 键；按键轮换语言压缓存；语言双向切换与 `switchKey` 双向变更的字形串还原。

## 5. 需要注意的问题（给 Architect / Coder）

### 5.1 AC-2 的“抛异常”路径在本机无法复现（重要）

我尝试构造“无法访问的目录项”让 `directory_entry::is_regular_file()` 抛异常，实测结论（MSVC 14.44 STL，`include/filesystem:2403-2434`）：枚举得到的 `directory_entry` 调 `status()` 时一定会重新查询，但：

1. **属性查询不受 DACL 约束**。给文件加 `DENY FILE_READ_ATTRIBUTES`（甚至把 DACL 清空成无 ACE）后，`GetFileAttributesExW` 仍然成功；只有需要**打开文件**的查询（`_All_data`，例如 `directory_entry` 构造时的 `refresh()`）才返回 `ERROR_ACCESS_DENIED`。
2. **损坏的目录项只会得到“找不到”**。悬空 junction（`mklink /J` 指向不存在目标）和自环 junction 的 `GetFileAttributesExW` 都返回成功、`is_regular_file()` 返回 `false`；而 `status()` 抛异常的条件是错误码既不是 `not_found` 也不是 `unknown`（`filesystem:2120` 附近的 `_Not_good()`），`ERROR_FILE_NOT_FOUND`/`ERROR_PATH_NOT_FOUND`/`ERROR_BAD_NETPATH`/`ERROR_INVALID_NAME` 都被当作 `file_type::not_found`（`xfilesystem_abi.h:51-63`），**不会抛**。
3. 因此真正会抛的场景只剩“损坏 reparse point（`ERROR_REPARSE_TAG_INVALID`）”或底层 I/O 故障，普通测试无法稳定制造。

所以 AC-2 的测试目前是**契约/回归护栏**：它证明“一堆坏条目不会让 `LoadResources` 抛异常、内置英文和经典主题仍可用”，但修复前也通过。**这不代表 T-1 的 `is_regular_file(error_code)` 与 try/catch 无用**——审查报告 A-01 的场景 2 本来就标注“待验证”，本轮结论是：在 Windows 上该场景不通过 DACL/悬空链接触发，建议保留修复（防御性），但不要期待测试能在修复前变红。

### 5.2 AC-5 的键表来源

测试以 `assets/Languages/en.ini` 为键表权威（当前 326 键）。如果后续有人在 en.ini 加键而不补内置表，AC-5 会失败——这是 plan 想要的方向；但若某键**故意**不内置（例如纯占位），需要同步更新测试并说明。

### 5.3 AC-8 与 T-6 的实现自由度

AC-8 只约束可观测结果（与重新加载后首次调用一致、语言/热键变化后字形串随之变化），不约束缓存实现（单槽/多槽、是否用互斥锁）。注意：测试**不**检查锁，也不在持有 `viewMutex` 的路径上调用任何东西，因此不会与 HANDOFF §5 或锁顺序约束冲突。

## 6. 直接修改的业务代码

无。本轮只改动 `tests/` 下三个文件：

- `tests/SettingsTests.cpp`（共 490 行）：新增 4 个函数声明（12-15 行）、`main` 内 1 行调用（347 行）、文件末尾追加 142 行（349-490 行：匿名命名空间辅助函数 + AC-1/2/5/8）。未新增 `#include`。
- `tests/CombatCycleTests.cpp`（共 90 行）：include 区新增 `#include <locale>`，新增声明（8 行）、`main` 内 1 行调用（63 行）、末尾追加函数（66-90 行）。
- `tests/InventoryOrderTests.cpp`（共 121 行）：新增声明（9 行）、`main` 内 1 行调用（87 行）、末尾追加函数（90-121 行）。

`xmake.lua`、`scripts/`、`src/`、`include/`、`docs/plans/` 均未修改；`extern/` 未触碰；未构建 `FavoriteWheel`，未部署，未接管键鼠。

## 7. 下一步建议

1. Coder-A 合入 T-1 后先跑 SettingsTests，预期 AC-1 变绿；T-4 后 AC-5 变绿；T-6 后 AC-8 仍必须绿（这是重点回归）。
2. Coder-B 合入 T-3 后：InventoryOrderTests 先要能编译（`RemoveDuplicateKeys` 必须声明在 `include/InventoryOrderModel.h` 的 `Wheel::Ordering` 里，签名为 `void RemoveDuplicateKeys(State&)`），再跑 CombatCycleTests 的 AC-3。
3. 合入后再统一跑 §7 的 16 个回归目标（含本文件所在三个目标）；`RuntimeLayoutTests` 留到最终验收（需要构建 `FavoriteWheel`）。
4. 若 Architect 认为 AC-2 需要“修复前必须变红”，需要先定义可稳定复现的触发方式（例如引入 `ERROR_REPARSE_TAG_INVALID` 的伪造 reparse point），否则建议按 §5.1 接受现状。

## 8. 待确认问题

- AC-2 的“无法访问目录项”期望值：接受“契约/回归护栏”（当前做法），还是要我另做伪造 reparse point 的强触发测试？
- AC-8 是否需要额外覆盖 `actionHotkey`/`hotkey` 变更（plan 4.1 的缓存键包含这两个字段，当前只测了 `switchKey`）？如需，我可以在下一轮补。

---

# 第 1 轮（模式 B：运行与验收）

- 分支 / 工作区: `wf/20261028-takeover-review` / `D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘`（主仓库；已合入 Coder-A/B/C 实现）
- 轮次: **第 1 轮**
- 覆盖: plan §7 的 16 个回归目标 + `RuntimeLayoutTests` + `SettingsVfsTests`；AC-1/2/3/4/5/8 的测试，AC-12 脚本复测，AC-13 的最终状态部分
- 状态: **PASS**（无实现缺陷需要 Coder 修复；三处环境说明见 §10.2）

## 9. 结论摘要

1. **18 个测试目标全部退出码 0**（16 个回归目标 + `RuntimeLayoutTests` + `SettingsVfsTests`）。没有任何 FAIL，也没有“环境阻塞”（`SettingsVfsTests` 需要 `usvfs_x64.dll`，本机 `H:\Games\Dev Skyrim\usvfs_x64.dll` 可用，已带参数跑通）。
2. 按编排者答复补了 AC-8 的 `hotkey` / `actionHotkey` 断言（`tests/SettingsTests.cpp` +9 行，纯追加），并用**变异测试**证明新断言不是空转：把缓存键里的 `hotkey`（或 `actionHotkey`）去掉后，新断言立刻变红（§12.3）。
3. AC-12 用本次构建产物在临时副本上复测 8 个用例：4 个新鲜度失败 + 2 个版本失败（含 `-AllowStaleDll` 不能绕过版本检查）均非零退出并给出原因；正常用例通过前置检查。`dist/` 未被触碰（最后一次写入仍是 2026-10-26 00:35），也没有真正打包。
4. `FavoriteWheel` 构建一次（250.7 s）并自动部署；**用户 INI 的 SHA256 与修改时间前后完全一致**，部署 DLL 与 `build/windows/x64/release/FavoriteWheel.dll` 的 SHA256 一致，`FileVersion = 0.9.0.0`，DLL 内两条日志字符串为 `FavoriteWheel 0.9.0 …`（来自宏），DLL 内无 `9.9.9` 残留。
5. 视觉回归：以 `a9dbd65` 的源码树重建 `WheelPreview` 为基线，`gameplay` / `sorting` / `numeric-sort` 三种模式在 1280×720 下渲染结果与当前实现**逐字节相同**。

## 10. 执行方式与环境说明

### 10.1 构建/运行方式

- 未执行 `xmake f`（遵循编排者要求）。`.xmake/windows/x64/xmake.conf` 保持原样：`deploy_dir = "H:/Games/Dev Skyrim/mods/36 - FavoriteWheel"`、`mode = "release"`、`arch = "x64"`，未修改。
- 统一做法：`xmake build <Target>` → 运行 `build/windows/x64/release/<Target>.exe`，退出码即结果；日志集中在 `build/takeover-review/round1-*.log`。
- 未启动游戏、未接管键鼠、未推送、未合并；`tests/` 以外只写入了 `docs/reports/20261028-takeover-review-test.md`（本报告），构建产物与临时文件都在忽略目录 `build/` 下。

### 10.2 环境注意事项（非缺陷）

| 事项 | 说明 |
|---|---|
| `git` 命令报 `missing config key GIT_CONFIG_KEY_0` | 本机环境变量 `GIT_CONFIG_COUNT=1` 但没有对应的 `GIT_CONFIG_KEY_0`，会让**所有** git 命令失败（`fatal: unable to parse command-line config`）。本轮每条 git 命令前都先清除该变量（`Remove-Item env:GIT_CONFIG_COUNT`）。这是工具环境问题，与仓库无关；建议后续会话留意。 |
| 本机没有 PowerShell 7 | `scripts/package.ps1:1` 的 `#Requires -Version 7.0` 在 `powershell.exe` 5.1 下会让脚本直接退出。AC-12 在**临时副本**里只删掉这一行（其余逻辑原样）后复测，属环境限制，不等同于 PS7 下的完整运行。 |
| `SettingsVfsTests` 需要参数 | 不带参数时只打印 `Usage: SettingsVfsTests <MO2 usvfs_x64.dll>` 并返回 1。本机存在 `H:\Games\Dev Skyrim\usvfs_x64.dll`，带参数运行后退出码 0，因此**不算环境阻塞**。 |

## 11. 本轮测试改动（仅 `tests/`）

`tests/SettingsTests.cpp`（+9 行，全部追加在 `Ac8_TranslationCacheConsistency` 末尾，未改任何既有断言、未削弱强度、未跳过任何用例）：

- `favorites.hotkey=20` → `UIGlyphs` 必须与基线不同；且刷新后的字形串必须包含 `KeyLabel(favorites)`（证明换的是“收藏热键标签”这个槽位，而不只是整体变长）；改回 `-1` 后与基线**逐字节相同**。
- `actions.actionHotkey=21` → `UIGlyphs` 必须与基线不同，且与上一组 `favorites` 的结果不同（两个热键占独立槽位）；改回后逐字节还原。
- 这两个字段正是 plan §4.1/§8 缓存键里要求“热键变更必须让缓存失效”的部分，原来的 AC-8 只覆盖了 `switchKey`。

没有新增 `#include`（`KeyLabel` 已在 `include/UIResources.h:39` 声明）。

## 12. 结果

### 12.1 构建结果

| 目标 | 构建退出码 | 耗时 | 是否实际重新编译 |
|---|---|---|---|
| WheelLogicTests | 0 | 1 s | 否（输入未变，`build ok`） |
| GameSessionTests | 0 | 1 s | 否 |
| HandEquipTests | 0 | 1 s | 否 |
| LayoutSkinTests | 0 | 1 s | 否 |
| CombatCycleTests | 0 | 3 s | 是（`include/CombatCycleModel.h`、测试文件变更） |
| CombatPresetTests | 0 | 1 s | 否 |
| OutfitTests | 0 | 1 s | 否 |
| NameEditorTests | 0 | 1 s | 否 |
| FaceLightClientTests | 0 | 1 s | 否 |
| ItemInfoTests | 0 | 1 s | 否 |
| InventoryTests | 0 | 1 s | 否 |
| TimeTests | 0 | 1 s | 否 |
| InventoryOrderTests | 0 | 4 s | 是（`include/InventoryOrderModel.h`、测试文件变更） |
| SettingsTests | 0 | 6 s | 是（`src/UIResources.cpp`、`src/Settings.cpp` 单元、本轮新增 9 行测试） |
| SettingsPersistenceTests | 0 | 1 s | 否 |
| SectorMeshTests | 0 | 1 s | 否 |
| SettingsVfsTests | 0 | 5 s | 是（同样编译 `src/UIResources.cpp`） |
| RuntimeLayoutTests | 0 | 10 s | 是（`tests/RuntimeLayoutTests.cpp`、版本宏） |
| FavoriteWheel | 0 | 251 s | 是（`src/*.cpp` 变更；`after_build` 自动部署） |
| WheelPreview | 0 | 4 s | 是（`src/UIResources.cpp`、`src/WheelFonts.cpp`；视觉回归用） |

说明：“否”表示 xmake 判定输入未变、无需重编（这些目标的源文件与头文件都不在本批次改动列表里，`git diff a9dbd65 HEAD` 只涉及 `src/UIResources.cpp`、`src/WheelFonts.cpp`、`src/InventoryOrder.cpp`、`src/main.cpp`、`include/CombatCycleModel.h`、`include/InventoryOrderModel.h`、`tests/*`、`scripts/package.ps1`、`xmake.lua`），因此它们的产物本来就是当前源码的产物。只有构建退出码非 0 才算失败，本轮没有。

### 12.2 运行结果（退出码）

| # | 目标 | 退出码 | 输出末尾 |
|---|---|---|---|
| 1 | WheelLogicTests | 0 | 通过 |
| 2 | GameSessionTests | 0 | 通过 |
| 3 | HandEquipTests | 0 | 通过 |
| 4 | LayoutSkinTests | 0 | 通过 |
| 5 | CombatCycleTests | 0 | `Combat cycle model, serialization, conflict and input lifecycle tests passed` |
| 6 | CombatPresetTests | 0 | 通过 |
| 7 | OutfitTests | 0 | 通过 |
| 8 | NameEditorTests | 0 | 通过 |
| 9 | FaceLightClientTests | 0 | 通过 |
| 10 | ItemInfoTests | 0 | 通过 |
| 11 | InventoryTests | 0 | 通过 |
| 12 | TimeTests | 0 | 通过 |
| 13 | InventoryOrderTests | 0 | `Inventory ordering modes, retained identities, scopes, cross-page moves and serialization passed` |
| 14 | SettingsTests | 0 | `Settings persistence, cancellation, failure, localization, themes and hit regions passed` |
| 15 | SettingsPersistenceTests | 0 | 通过 |
| 16 | SectorMeshTests | 0 | 通过 |
| 17 | SettingsVfsTests（带 `H:\Games\Dev Skyrim\usvfs_x64.dll`） | 0 | `Isolated usvfs settings persistence passed` |
| 18 | RuntimeLayoutTests | 0 | `Runtime layouts, address-library formats 1/2/5 and DLL metadata passed` |

### 12.3 验收标准逐条判定

| AC | 判定 | 证据（测试函数 / 命令 / 产物） |
|---|---|---|
| AC-1 | **PASS** | `Ac1_Utf8ThemeId`（`tests/SettingsTests.cpp:383`）。本轮 SettingsTests 退出码 0，全程无断言消息。第 0 轮修复前该断言必红（主题 ID 是 GBK 字节），现已变绿：`src/UIResources.cpp:361` 用 `path.stem().u8string()` 生成 ID。 |
| AC-2 | **PASS** | `Ac2_UnreadableThemeEntry`（`tests/SettingsTests.cpp:409`）。按编排者答复，保持“契约/回归护栏”：坏条目（独占句柄、目录、0 字节、非法 UTF-8、根目录是文件）不会让 `LoadResources` 抛异常，内置英文与 classic 主题仍可用。`src/UIResources.cpp:310` 已改用 `is_regular_file(error_code)`，`:408-413` 有 try/catch 兜底。**该用例在修复前也通过**，属预期（第 0 轮 §5.1 已说明本机无法稳定制造 `ERROR_REPARSE_TAG_INVALID`）。 |
| AC-3 | **PASS** | `Ac3_CycleRecordsIgnoreDigitGrouping`（`tests/CombatCycleTests.cpp:69`）。`include/CombatCycleModel.h:50`、`:60` 的 `Encode`/`Decode` 都 `imbue(std::locale::classic())`；CombatCycleTests 退出码 0，且测试结束时恢复了全局 locale。 |
| AC-4 | **PASS** | `Ac4_RemoveDuplicateKeys`（`tests/InventoryOrderTests.cpp:93`）。`include/InventoryOrderModel.h:39-44` 提供 `Wheel::Ordering::RemoveDuplicateKeys(State&)`（按 `Token` 保留首次出现、不动 `mode`），`src/InventoryOrder.cpp:46` 在解析 FormID 后调用；`Decode` 仍保持严格校验。InventoryOrderTests 退出码 0。 |
| AC-5 | **PASS** | `Ac5_BuiltinEnglishCoverage`（`tests/SettingsTests.cpp:438`）。`src/UIResources.cpp:16-18` 补了 `swapMouseHands`、`swapMouseHandsHelp`、`compactUseSwapped` 三个键；326 个键全量校验 + `UIGlyphs` 预热校验通过。 |
| AC-8 | **PASS** | `Ac8_TranslationCacheConsistency`（`tests/SettingsTests.cpp:461`；本轮在 490-498 行追加 6 个 `Check`，文件现共 499 行）。①8 语言 × 全键：缓存版 `Tr` 与重新 `LoadResources` 后首次调用一致（逐键轮换语言）；②`language` 切换后 `UIGlyphs` 变化、切回逐字节还原；③`switchKey` 、④本轮新增的 `hotkey`、⑤`actionHotkey` 变更后刷新、改回逐字节还原。实现见 `src/UIResources.cpp:428-441`（`ActiveLanguage` 缓存）、`:459-480`（`UIGlyphs` 缓存，键 = active + hotkey + switchKey + actionHotkey）、`src/WheelFonts.cpp:90-95`（按内容比较后才 `AddText`）。**断言有效性验证**：在 `build/takeover-review/mutants/` 里用当前 `src/UIResources.cpp` 的副本分别把 `cached->hotkey==config.hotkey` 和 `cached->actionHotkey==config.actionHotkey` 改成 `true`（模拟“缓存键漏字段”），重编 SettingsTests 并运行：两者都 **exit=1**，失败消息正好是本轮新加的 `AC-8: rebinding the favorites hotkey refreshes the UI glyph string` / `AC-8: rebinding the action hotkey refreshes the UI glyph string`。说明新断言能真正抓住 T-6 的缓存失效遗漏。 |
| AC-12 | **PASS** | 见 §13。8/8 用例符合预期，`dist/` 未生成。 |
| AC-13（最终状态部分） | **PASS** | 见 §14。`FileVersion=0.9.0.0`、日志字符串来自宏、部署/构建 DLL 哈希一致、用户 INI 前后一致、`xmake.lua` 无 `9.9.9` 残留。 |

## 13. 部署与哈希核对（AC-13 最终状态）

部署目录 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`（未改 `deploy_dir`，未启动游戏）。

| 项目 | 构建前 | 构建后 | 结论 |
|---|---|---|---|
| `SKSE/Plugins/FavoriteWheel.ini` SHA256 | `0D91EB80E30B93526990DE9C39634423605E4A14A9722B776F296BFE608425E2` | `0D91EB80E30B93526990DE9C39634423605E4A14A9722B776F296BFE608425E2` | **一致**（文件存在，未被覆盖） |
| 同上，最后写入时间 | `2026-10-25T22:24:55.6372199+08:00` | `2026-10-25T22:24:55.6372199+08:00` | **未变** |
| 部署 `SKSE/Plugins/FavoriteWheel.dll` SHA256 | `1FB3738D44C66BD94668044401BE35169E6F39EB47C689B29C0CE28935A46058`（= 旧构建产物） | `17FF5243D039ADC12C285D2E7A73AF28486F66D4E6E13F1D7057ABBC48E9C8DC` | 已更新 |
| `build/windows/x64/release/FavoriteWheel.dll` SHA256 | `1FB3738D…A46058` | `17FF5243D039ADC12C285D2E7A73AF28486F66D4E6E13F1D7057ABBC48E9C8DC` | **与部署 DLL 一致** |
| 部署 DLL `FileVersion` / `ProductVersion` | 0.9.0.0 / 0.9.0.0 | 0.9.0.0 / 0.9.0.0 | 符合 `set_version("0.9.0")`，前三段匹配 |
| DLL 内 ASCII 字符串检索 | — | `FavoriteWheel 0.9.0 loaded;` = true；`FavoriteWheel 0.9.0 unsupported runtime` = true；`9.9.9` = **false** | 日志版本来自宏 |

- 宏来源：`xmake.lua:18-27`（`favoritewheel.version` 规则，由 `set_version` 派生 4 个宏）、`xmake.lua:37`/`xmake.lua:256` 分别挂到 `FavoriteWheel` 与 `RuntimeLayoutTests`；`src/main.cpp:35`、`src/main.cpp:44` 使用 `FAVORITEWHEEL_VERSION_STRING`；`tests/RuntimeLayoutTests.cpp:121` 使用 `FAVORITEWHEEL_VERSION_MAJOR/_MINOR/_PATCH`，本轮 RuntimeLayoutTests 退出码 0。
- `git grep -n '9\.9\.9'` 命中项只在 `docs/plans/…` 与 Coder-C 报告（说明文字），`xmake.lua` 只有 `set_version("0.9.0")`；`git diff a9dbd65 HEAD -- xmake.lua` 里没有 `9.9.9`。
- 部署目共 121 个受检文件（`*.ini`/`*.md`/`*.txt`/`*.dll`），前后差异只有 3 项：`SKSE/Plugins/FavoriteWheel.dll` 被替换、`docs/ARCHITECTURE.md` 更新、`docs/README.md` 新增（`xmake.lua:137-144` 的文档复制步骤）。**没有任何 `FavoriteWheel.ini`、`Languages/*.ini`、`Themes/*.ini` 被改动**。

## 14. AC-12 复测（临时副本，未覆盖 `dist/`，未打包）

做法：把 `scripts/package.ps1`、`xmake.lua`、本次构建的 `FavoriteWheel.dll`、伪造的 `src/`、`include/` 复制到 `build/takeover-review/package-fixture/`，脚本副本只做两处非逻辑改动：删掉 `#Requires -Version 7.0`（本机无 PS7），并在 `$dist = Join-Path …` 之前插入 `PREFLIGHT_PASSED` 标记 + `exit 0`，保证后续打包步骤永不执行。运行器：`build/takeover-review/ac12-package-check.ps1`。

| 用例 | 构造 | 退出码 | 命中原因文本 |
|---|---|---|---|
| fresh | DLL 比输入新 1 小时 | **0** | `DLL SHA256: 17FF5243…E9C8DC` + `PREFLIGHT_PASSED` |
| stale-xmake | `xmake.lua` 设为当前时间 | **1** | `DLL is stale: '…/xmake.lua' is newer than …` |
| stale-src | `src/nested/Probe.cpp` 设为当前时间 | **1** | `DLL is stale: …` |
| stale-include | `include/nested/Probe.h` 设为当前时间 | **1** | `DLL is stale: …` |
| old-timestamp | 输入不变、DLL 设为 3 小时前 | **1** | `DLL is stale: …` |
| allow-stale | 陈旧 `xmake.lua` + `-AllowStaleDll` | **0** | `PREFLIGHT_PASSED`（只跳过新鲜度） |
| old-version | 用真实旧 DLL（`FileVersion=0.7.1.0`）替换 | **1** | `DLL FileVersion '0.7.1.0' does not match project version '0.9.0'. Rebuild FavoriteWheel in Release mode before packaging.` |
| old-version-allow-stale | 同上 + `-AllowStaleDll` | **1** | 同上（版本检查不可绕过） |

- 8/8 用例结果符合 plan（失败用例非零退出并给出原因；正常用例通过前置检查）。
- 每个用例后都确认 `package-fixture/dist` **不存在**；仓库 `dist/` 的最后写入时间仍是 `2026-10-26 00:35:59`，未被覆盖。
- 日志：`build/takeover-review/round1-ac12-*.log`、`round1-ac12-results.txt`。
- 与 Coder-C 的差别：Coder-C 通过改 DLL 二进制伪造 0.8.0.0；本轮直接用真实的 0.7.1.0 旧 DLL，做法更简单且不动构建产物。

## 15. 视觉回归（T-6）

方法（不触碰仓库文件）：用 `git archive a9dbd65` 导出基线源码树到 `build/visual-baseline/base`，补上未纳入 git 的 `extern/imgui`，再按 `xmake.lua:175-183` 的同一组源文件/宏用 `cl.exe`（`D:\Program Files\Microsoft Visual Studio\2022\Professional`，`/O2 /std:c++latest /utf-8`，链接 `d3d11 dxgi d3dcompiler windowscodecs ole32`）编出 `WheelPreview-baseline.exe`；当前实现用 `xmake build WheelPreview` 的产物。两者都以 1280×720、同一 `assets/` 分别渲染后比较。

| 模式 | 基线 SHA256（前 16 位） | 当前 SHA256（前 16 位） | 结论 |
|---|---|---|---|
| gameplay | `78D4765E0AF41CEE` | `78D4765E0AF41CEE` | 逐字节相同 |
| sorting | `2FBBB1B8E6C9B43B` | `2FBBB1B8E6C9B43B` | 逐字节相同 |
| numeric-sort | `45267A7DF9E194CE` | `45267A7DF9E194CE` | 逐字节相同 |

三种模式两两哈希不同（不是空白图），说明比较有意义。`assets/` 在两提交间无差异（`git diff a9dbd65 HEAD -- assets` 为空），因此差异只可能来自 `src/UIResources.cpp` / `src/WheelFonts.cpp`，结果证明 T-1/T-4/T-6 没有改变渲染输出。图片与日志：`build/takeover-review/round1-visual-{base,head}-<mode>.bmp|.log`。基线源码树与临时 exe 已在核对后删除（体积大），重建方法即本节命令。

## 16. 缺陷与观察

**本轮未发现需要 Coder 修复的实现缺陷。** 以下为不影响验收的观察，供后续批次参考：

| 编号 | 位置 | 内容 | 建议 |
|---|---|---|---|
| O-1 | `src/UIResources.cpp:408-413` | `LoadResources` 的异常兜底只写 `OutputDebugStringA`，不进 SKSE 日志，玩家侧看不到。plan §4 T-1 写的是“出错时…并写日志”。考虑到该文件同时被测试目标编译（不能用 `SKSE::log`），这属于合理取舍。 | 建议在 HANDOFF 记一句“资源加载失败只在调试器输出可见”，避免以后有人以为日志里会有记录。 |
| O-2 | `tests/SettingsTests.cpp:409` | AC-2 用例在修复前也通过（Windows 上普通 DACL/悬空 junction 不触发 `is_regular_file` 抛异常）。 | 按编排者答复保持现状；如将来要“必红”的触发方式，需要伪造 `ERROR_REPARSE_TAG_INVALID` 的 reparse point。 |
| O-3 | `src/UIResources.cpp:428-441` | `ActiveLanguage` 缓存是**单槽**：8 种语言逐键交替时几乎每次都要重算 `ResolveLanguage`（正确性已由 AC-8 覆盖，属性能层面）。 | 非本批目标，不要求改；如果以后 `Tr` 出现在更高频路径再考虑。 |
| O-4 | `scripts/package.ps1:17-26` | 新鲜度检查依赖文件修改时间，切换分支/`git checkout` 后会误判过期（plan §8 已记录）。本轮 `stale-xmake` 用例再次确认行为与设计一致（失败并提出重建建议）。 | 使用者遇到时可加 `-AllowStaleDll`（不跳过版本检查）。 |

## 17. 未覆盖 / 需人工确认

- 游戏内行为（中文主题选择、切语言后文本与字形刷新、保存后读档排序不丢）**未验证**：本轮未启动游戏，也未接管键鼠。部署成功不等于游戏内验证通过。
- AC-13 的步骤①（`set_version` 临时改 9.9.9 → 构建 → 运行）按编排者说明由 Coder-C 在其 worktree 中完成，本轮只复核最终状态（§13）。
- AC-12 在 PowerShell 7 下的原生运行未执行（本机无 PS7，见 §10.2）；如需要，可在有 PS7 的机器上直接跑 `scripts/package.ps1` 的同样用例。

## 18. 提交

- 本轮改动仅 `tests/SettingsTests.cpp`（+9 行）与本报告；提交信息 `tests(20261028-takeover-review): round 1`，本地提交，未推送。
- 临时产物（`build/takeover-review/mutants/*.cpp|*.exe`、`build/visual-baseline/`、测试遗留的 `build/settings-test-*`、`build/settings-persistence-*`、`build/FavoriteWheel-settings-test-*`、手工编译留下的根目录 `*.obj`）已清理；证据日志与图片保留在 `build/takeover-review/`。

统计：测试目标 18 个全部通过（0 失败）；验收标准 AC-1/2/3/4/5/8/12/13 全部 PASS；本轮新增断言 9 行（6 个 `Check`）。

总结论：**PASS**
