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
