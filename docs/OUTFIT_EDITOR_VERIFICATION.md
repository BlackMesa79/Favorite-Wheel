# 编辑预设实现与验证记录

日期：2026-11-08。分支：`feat/outfit-parts-p1`。起点：`cef8cba`。直接修改当前分支，未提交、未推送、未启动游戏、未接管键鼠。

## 结果与边界

已实现双列表草稿编辑面板（`outfitDialog=7`）、库存分类、穿着标记、防重复、槽位冲突自动替换、取消和保存校验。管理页仅装备预设分支增加“编辑预设”和半宽“导出预设”；战斗预设与循环页面原有布局及行为保留。移除了废弃的部件子轮盘、部件管理与部位规则接口、分支、预览桩和语言键，没有删除整个源文件。

存档兼容验证：`include/OutfitModel.h`、`include/OutfitPlan.h`、`include/OutfitStep.h` 与 HEAD 无差异；`src/Outfits.cpp` 在废弃接口之前的全部既有代码与 HEAD 文本一致，包括序列化、Capture、Match/Resolve/Wearing 调用、Apply、Tick 和换装计划。没有新增部位规则或可选件语义。

## 文件清单

- `assets/Languages/de.ini`
- `assets/Languages/en.ini`
- `assets/Languages/fr.ini`
- `assets/Languages/ja.ini`
- `assets/Languages/ko.ini`
- `assets/Languages/pt_BR.ini`
- `assets/Languages/ru.ini`
- `assets/Languages/zh_CN.ini`
- `docs/OUTFITS.md`
- `include/Favorites.h`
- `include/FunctionNavigation.h`
- `include/Outfits.h`
- `include/UILayout.h`
- `include/Wheel.h`
- `src/Draw.cpp`
- `src/Outfits.cpp`
- `src/UIResources.cpp`
- `src/Wheel.cpp`
- `tests/LayoutSkinTests.cpp`
- `tests/OutfitTests.cpp`
- `tests/WheelPreview.cpp`
- `include/OutfitEditModel.h`
- `docs/OUTFIT_EDITOR_VERIFICATION.md`

## 语言键

8 个语言文件与 `src/UIResources.cpp` 默认表同步。新增中文与英文文本；其余 6 种语言按项目习惯提供英文回退。历史部署语言文件由原有 after_build 保留自定义值并补齐新键，不强行删除用户文件中的旧键。

新增 25 个键：

`outfitEditTitle`, `outfitEditDraft`, `outfitEditInventory`, `outfitEditAll`, `outfitEditHead`, `outfitEditBody`, `outfitEditHands`, `outfitEditFeet`, `outfitEditJewelry`, `outfitEditOther`, `outfitEditRemove`, `outfitEditAdd`, `outfitEditSave`, `outfitEditWorn`, `outfitEditReplaced`, `outfitEditCountError`, `outfitEditDuplicate`, `outfitEditLoadError`, `outfitEditSaveError`, `outfitEditLoading`, `outfitEditSaving`, `outfitEditEmpty`, `outfitEditKeyHelp`, `outfitEditPadHelp`, `outfitEditMouseHelp`

删除 23 个键：

`outfitModeWhole`, `outfitModeOverlay`, `outfitLoose`, `outfitHead`, `outfitBody`, `outfitHands`, `outfitFeet`, `outfitRing`, `outfitAmulet`, `outfitOther`, `outfitSet`, `outfitIgnore`, `outfitUnequip`, `outfitEditPieces`, `outfitAddWorn`, `outfitPieceOptional`, `outfitPieceRequired`, `outfitPieceMakeOptional`, `outfitPieceMakeRequired`, `outfitPieceRemove`, `outfitPieceManage`, `outfitPartialApplied`, `outfitOptionalSkipped`

在本报告写入前已全仓 `rg` 确认废弃符号、旧语言键、旧 6 号对话框零匹配；`scripts/language_migrations.lua` 无旧键引用，无需修改。报告中对已删除语言键的记录是说明文字，不是程序残留。按 include/src/tests/scripts/assets/Languages 再次检查同样为零。

## 线程与锁

- 打开编辑器先发布 loading 状态，再通过 SKSE AddTask 在游戏线程复制原预设并执行一次 `Inventory(0,true)`。新部件来自 `Describe(armor,extra,true)` 并带实际 mask；旧部件的零 mask 按 base FormRef 查运行时 armor 得到冲突检测用 mask。
- 跨帧快照只含 Piece、运行时 mask、穿着标记；显示名在 Piece.label 中，分类由 PartOf 计算。没有跨帧 ExtraDataList 指针。绘制不扫描库存，不访问 Outfits 存储锁。
- 草稿增删、分类、选择、滚动在 viewMutex 内调用纯模型函数。打开、保存、取消使用 lifecycleMutex；取得 Outfits 锁或调用 Snapshot、EndOutfitDialog、RefreshFunctions 时不持有 viewMutex。
- 异步任务校验 epoch、openSerial、编辑请求号和对话框状态，避免取消、关盘或读档后的过期任务生效。保存期间阻止继续增删；取消先发生时，排队保存不会提交。
- 保存先验证 1–64 件及 Encode/Decode，再在原有 Commit 中提交；对原预设做一致性检查，拒绝覆盖外部变化。成功后退出并调用 RefreshFunctions 更新徽章。保存不执行 Apply 或换装。

## 操作

鼠标单击行选择，再点移除／添加；双击直接执行；滚轮只作用于光标所在列表。上下键或 W/S 选择，左右键或 A/D 切列，Enter 执行，Q/E 分类，F5 保存，Esc/Tab 取消。手柄方向键选择和切列，A 执行，LB/RB 分类，Y 保存，B 取消。保存使用 F5/Y，和行操作 Enter/A 分开，帮助文字已显示。

## 测试实际输出

以下 8 个指定目标全部构建成功，运行退出码均为 0；SettingsTests 在语言表最终调整后又运行通过，OutfitTests 在补充加载／保存期间禁止编辑、滚动与空筛选测试后又运行通过（输出同下）。

```text
OutfitTests (exit=0)
Outfit editor: add/remove, all conflicts, legacy masks, duplicates, 1-64 bounds, filters, draft isolation and roundtrip passed
Outfit process-locale round trips passed: 4
Outfit serialization, malformed input, instance matching, manual wear badges, preflight and toggle tests passed
LayoutSkinTests (exit=0)
Outfit management/editor controls, lists and 720p-4K panel bounds passed
Layout skins: 1350 sector/pointer cases and 13122 card/viewport cases passed
6480 hand marker layout cases passed
WheelLogicTests (exit=0)
Wheel logic tests passed
Deferred action executor and function category tests passed
Weapon toggle policy passed
Hand equipment and shared combat shortcut input policy passed
SettingsTests (exit=0)
Settings persistence, cancellation, failure, localization, themes and hit regions passed
SettingsPersistenceTests (exit=0)
missing: result=1 path='D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘\build\settings-persistence-13376\missing.ini' stage=saved-verified win32=0 writer=file-bytes-v1 bytes=1444
zero: result=1 path='D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘\build\settings-persistence-13376\zero.ini' stage=saved-verified win32=0 writer=file-bytes-v1 bytes=1444
bom: result=1 path='D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘\build\settings-persistence-13376\bom.ini' stage=saved-verified win32=0 writer=file-bytes-v1 bytes=1444
INI profile-cache, independent-process and deployment hardlink regressions passed
CombatPresetTests (exit=0)
Combat preset identity, capacity, serialization, slot binding and step policy passed
CombatCycleTests (exit=0)
Combat cycle model, serialization, conflict and input lifecycle tests passed
InventoryOrderTests (exit=0)
Inventory ordering modes, retained identities, scopes, cross-page moves and serialization passed

Legacy hint migration: known defaults, custom preservation, CRLF and idempotence passed
```

纯函数测试覆盖添加、移除、多重槽位替换、旧零 mask 的运行时回退、重复拒绝、64 件边界及满容量替换、空与超限保存拒绝、PartOf 分类、原预设隔离、历史字段保留、Encode/Decode 往返，以及选择／滚动收敛。布局测试分别检查管理页和编辑页实际同时显示的控件：管理页含名称、覆盖、删除、新入口、导出、apply/cancel；编辑页含名称区域、分类、双列表、增删、apply/cancel。两个面板互斥，编辑页不绘制管理页的覆盖／删除按钮。

开发阶段修正过测试缺少 `<vector>` 的编译错误、分类标签与列表标题的间距、预览缩放脚本的整数重载，以及文件换行处理问题。最终 `git diff --check` 返回 0；这些问题均已解决。

日志：`build/editor-test-results.txt`、`build/editor-tests-build.log`、`build/editor-model-final-build.log`、`build/editor-final-build.log`、`build/editor-preview-final-build.log`。

## 离屏视觉验证

使用 WheelPreview 的真实 Draw.cpp、ImGui 与 WARP D3D11 路径。管理面板使用 manage 模式；新增 outfit-edit 模式，含长名称、两列不同滚动位置、冲突替换提示和穿着标记。均已真实渲染并逐张检查，没有将单纯加载当作视觉通过。

| 图片 | 原始渲染 | 检查结论 |
| --- | --- | --- |
| `build/outfit-manage-zh.jpg` | 1280×720 | 编辑／导出两个按钮正常，与其他管理控件不重叠 |
| `build/outfit-edit-zh.jpg` | 1280×720 | 中文长名称省略、分类与标题间距、穿着标记及滚动范围正常 |
| `build/outfit-edit-en.jpg` | 1920×1080 | 英文省略号、分类、保存键与冲突提示正常 |
| `build/outfit-edit-4k.jpg` | 3840×2160 | 面板不越界，布局和信息完整 |
| `build/outfit-edit-message.jpg` | 最终英文预览局部 | 复查 Replaced: 后的空格及提示与分隔线间距正常 |

原始 BMP 和各自 `.log` 保存在同名 build 路径。送入视觉检查的完整预览为 1280×720 JPEG，全部低于 300 KB；局部提示图为 600×45。没有使用 Pillow。布局测试另覆盖 2560×1440 和多档缩放。

各预览均输出 `Rendered actual wheel UI at ...`，并通过字体图集复用与背景验证。预览仅使用合成实例，不代表真实库存或游戏输入时序验证。

## 部署与哈希

最终命令：`xmake build -j 2 FavoriteWheel`，实际输出：

```text
FavoriteWheel deployed to H:/Games/Dev Skyrim/mods/36 - FavoriteWheel (existing INI preserved)
build ok, spent 3.547s
```

构建产物：`build/windows/x64/release/FavoriteWheel.dll`。
部署目标：`H:/Games/Dev Skyrim/mods/36 - FavoriteWheel/SKSE/Plugins/FavoriteWheel.dll`。
两者 SHA256 一致：

```text
D0397DFF31687AF73EF07EBC2B8D49D36400AE6BCA3F139D6CE9E51A4BDFEDFD
```

没有修改源码 FavoriteWheel.ini、用户游戏配置或其他模组。自动部署沿用现有脚本，保留用户主 INI 和自定义资源。

## 未验证与偏离

没有功能范围偏离，没有已知未解决的编译或测试问题。新增 7 号对话框，6 号旧分支已删除。布局断言按互斥面板的实际显示控件分组，隐藏的旧管理按钮不会与编辑列表同时存在。

未经游戏验证：真实库存实例显示、游戏内键鼠／手柄交互、SKSE 任务时序及第三方模组组合仍需用户后续游戏实测。无法解析 base FormRef 的旧部件不能恢复运行时 mask，只能手动移除；符合“按运行时 armor 槽位回退”的能力边界。库存采用打开时快照，游戏时钟继续运行时的后续背包变化不会实时反映，取消并重开可刷新。
