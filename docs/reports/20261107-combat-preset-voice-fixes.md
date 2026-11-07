# 战斗预设 Voice 修复与验证

日期：2026-11-07  
分支：`feat/outfit-presets-v2`  
未创建 git commit。

## 根因与证据

### A：使用能力后预设失败

**确定的故障链**：本机已有日志 `D:/MyFiles/Documents/My Games/Skyrim Special Edition/SKSE/FavoriteWheel.log`（本次读取时末次修改为 2026-11-07 15:10:25）显示，捕获夜视及战吼后，Combat 多次停在 `step 2 = ClearVoice`，3 秒超时后结束为 `combatPartial`。全文件共有 7 次该步骤超时，没有 `Outfit <id> started`，也没有 `Wheel ... task failed`。

代表性记录：

```text
[15:07:43.481] Combat preset captured: id=7 name='夜视能力' right=0 left=0 twoHanded=false
[15:08:14.597] Combat preset captured: id=8 name='战吼能力' right=0 left=0 twoHanded=false
[15:08:49.773] Combat preset 1 issued step 2
[15:08:52.773] Combat preset 1 step 2 was not observed within 3 seconds
[15:08:52.773] Combat preset 1 finished: combatPartial
[15:09:23.027] Combat preset 1 issued step 0
[15:09:23.126] Combat preset 1 issued step 2
[15:09:26.154] Combat preset 1 step 2 was not observed within 3 seconds
```

原策略先清理右手、左手、Voice，再装备目标。即使 Voice 要设置为同一个能力或替换为另一能力，也强制先清空；清理 Power 使用 `EquipSpell(player, nullptr, VoiceSlot())`。因此清理失败会挡住后续装备，并可能留下已被卸下的手持物品。日志中的先执行 `step 0`、再在 `step 2` 超时与这一链条一致。

**不能据此确定的引擎细节**：旧日志没有同时记录 `selectedPower` 和 `equippedForms`，无法严格区分空指针 EquipSpell 未执行卸下，还是原读取路径没有及时反映完成。此次修复同时消除不必要的清理、改用原生卸下能力入口，并以角色 `selectedPower` 观察 Voice；仍需游戏内确认。

**Busy 与调度**：检查 `Wheel.cpp` 的 OpenBlockReason、Activate、快捷槽、循环入口、开关轮盘和 PumpAction。Busy 阻止新操作，但 PumpAction 在 Busy 时仍持续安排 Tick。Combat 每步 3 秒、总计 15 秒超时均经 Finish 清空 job/Busy；日志也显示超时后可再次开始，不能将其解释为永久 Busy。此次保留调度语义，补充 Tick 中断条件日志。

**Outfit 路径**：`Outfits.cpp` 只读取非盾牌护甲，不读取手持或 Voice；Wearing、ReplacementPlan 也仅比较护甲。完整穿齐同一 Outfit 再次应用会进入 `removeOnly`，这是原有切换语义，未改成 Combat 语义。现有日志没有 Outfit 应用证据，不能声称已复现或修复独立的 Outfit 故障；增加 apply 请求/Busy 日志，保留既有校验、removeOnly、步骤、结束日志。

另确认 `Combat::Load()` 原来只重定位左右手引用，漏掉 Voice。现对三个槽的 base/enchantment/owner 统一重定位，包括 Ignore/Empty 保留的物品；未解析的动态引用标为 `<unresolved>`。普通插件名加本地 FormID 仍按现有插件查询方式解析。

### B：持有武器/法术时捕获 Voice

**确定**：Check 的 `Both(form) != (p.twoHanded && !left)` 原来无差别作用于 Voice。在右手为双手武器/双手法术时，普通能力/龙吼的 `Both` 为 false，与 true 比较后报 `combatChanged`，导致 Capture/CaptureForCycle 失败。

改用共同的 `HandednessMatches` 策略：Voice 不参与双手校验；左右手仍验证真实物品的单双手属性。Resolve 仅对 Object 手槽分配库存，不对 Voice 分配数量，因此正常单手装备没有同一个校验缺陷。ReadHand 仍要求真实可识别的武器、盾牌、火把或普通法术；绑定武器、无库存对应实例、其他模组造成的异常装备状态仍会安全拒绝，不能将其伪装为空手。失败新增原始 FormID 和具体预检原因日志。保存操作仍捕获所有槽；保存仅 Voice 的效果通过将左右手改为 Ignore 实现，无需先卸下正常装备。

应用预设时，双手都 Ignore 不再读取手槽；单个 Ignore 槽的读取失败也不阻止应用。狼人 Voice 预设不再在 Apply 的额外 ReadHand 预检处被手部状态阻挡。捕获的严格性保持不变。

### C：文案

八种语言 `de/en/fr/ja/ko/pt_BR/ru/zh_CN` 的保存、覆盖、失败/执行提示及循环加入/底部提示统一为战斗预设含义，明确包含 Voice；内置英文回退同时更新。`combatSlotUnavailable` 说明没有保存物品时跳过指定，需使用覆盖入口重新捕获。

`language_migrations.lua` 仅精确匹配之前发布的默认值再替换。保留用户自定义值、其他键及换行；继续沿用 after_build 的 `.before-controls-migration` 备份机制。没有修改部署机制，也没有复制源码示例主 INI。

### D：槽位模式与保存物品分离

**确定**：原 CycleSlot 在 Set→Ignore 时将 Target 整体清空，Empty→Set 又读取当前装备；原函数还先修改共享预设再 Valid，失败时未回滚。

Target 新增 `remembered` 类型，原 Piece 始终保存物品身份。模式循环为指定→忽略→卸下→原指定；从未保存物品的槽在忽略/卸下间循环，跳过指定。空槽提示受现有动作通知开关控制。

`twoHanded` 保留为已保存右手物品的属性，`TwoHanded(p)` 仅在右手处于指定时激活它。恢复双手右槽与当前指定左槽冲突时，验证副本并拒绝，原预设保持不变；用户可先把左槽改为忽略/卸下。显式覆盖/重新捕获仍以当前装备整体更新预设，并保留原 ID/快捷槽。

序列化写入 FWC 3（SKSE 记录版本同步为 3），每槽在 kind 后追加 remembered，其余物品字段继续复用。兼容读取 FWC 1/FWC 2；旧记录中的指定物品首次编辑时自动成为保留内容。旧版本已经清空的 Ignore/Empty 物品无法恢复，需要一次显式覆盖。旧 DLL 不支持新 FWC 3，因此降级时需使用升级前存档。

顺带修正 CaptureForCycle 的去重条件：Ignore 的运行时通配匹配不能当成“当前装备与保存内容完全相同”，现在先要求各槽模式一致，避免循环捕获误复用一个忽略手槽的预设。

## 修改文件

- `include/CombatPresetModel.h`：保留类型、有效双手策略、原子模式切换、FWC 1/2/3、Voice 步骤策略。
- `src/CombatPresets.cpp`：Voice 读取/替换/原生 UnequipSpell（Papyrus 槽号 2）、捕获校验、Ignore 应用、循环捕获、重定位及诊断。
- `src/Outfits.cpp`、`src/Wheel.cpp`：请求/Busy 与调度中断诊断。
- `src/UIResources.cpp`、`assets/Languages/*.ini`：内置英文及八种语言。
- `scripts/language_migrations.lua`、`scripts/test_language_migrations.lua`：精确旧文案迁移及回归测试。
- `tests/CombatPresetTests.cpp`、`tests/CombatCycleTests.cpp`：Voice 校验、三槽模式保留、双手标志、冲突原子性、旧格式夹具、坏记录及循环成员回归。
- `tests/RuntimeLayoutTests.cpp`：受支持运行时的 selectedPower 字段偏移检查。


关键定位（本次最终文件行号）：

| 文件 | 入口 | 行号 |
| --- | --- | --- |
| `include/CombatPresetModel.h` | `inline bool HandednessMatches` | 29 |
| `include/CombatPresetModel.h` | `inline bool CycleSlotMode` | 88 |
| `include/CombatPresetModel.h` | `inline bool Decode` | 112 |
| `src/CombatPresets.cpp` | `bool ReadVoice` | 111 |
| `src/CombatPresets.cpp` | `std::string Check` | 163 |
| `src/CombatPresets.cpp` | `bool ClearSpell` | 234 |
| `src/CombatPresets.cpp` | `void Load(` | 371 |
| `src/CombatPresets.cpp` | `void Tick(` | 423 |
| `src/Outfits.cpp` | `void Apply(` | 274 |
| `src/Wheel.cpp` | `void PumpAction(` | 778 |

## 自动验证与部署

实际执行：

```powershell
xmake build -j 2 FavoriteWheel
# 对 xmake.lua 中全部 21 个 *Tests 目标，逐个执行：
xmake build -j 2 <Target>
xmake run <Target>
# SettingsVfsTests 使用独立 VFS 实例与测试目录：
xmake run SettingsVfsTests "H:/Games/Dev Skyrim/usvfs_x64.dll"
xmake lua scripts/test_language_migrations.lua
git diff --check
```

全部 21 项构建与运行退出码均为 0：

| 测试目标 | 构建 / 运行 |
| --- | --- |
| WheelLogicTests | 0 / 0 |
| GameSessionTests | 0 / 0 |
| HandEquipTests | 0 / 0 |
| ActorRuntimeTests | 0 / 0 |
| SectorMeshTests | 0 / 0 |
| LayoutSkinTests | 0 / 0 |
| SettingsTests | 0 / 0 |
| SettingsVfsTests | 0 / 0 |
| SettingsPersistenceTests | 0 / 0 |
| CombatCycleTests | 0 / 0 |
| CombatPresetTests | 0 / 0 |
| OutfitTests | 0 / 0 |
| NameEditorTests | 0 / 0 |
| FaceLightClientTests | 0 / 0 |
| RuntimeLayoutTests | 0 / 0 |
| ItemInfoTests | 0 / 0 |
| InventoryTests | 0 / 0 |
| TimeTests | 0 / 0 |
| SkinTextureTests | 0 / 0 |
| InventoryOrderTests | 0 / 0 |
| IconLibraryTests | 0 / 0 |

语言迁移测试通过：覆盖八种语言的旧默认保存/覆盖/循环底部提示/空槽提示、未知自定义值保留、CRLF 和重复执行幂等性。`git diff --check` 通过。

详细测试记录：`build/voice-fixes-tests/results.json` 与同目录各目标 `.log`；最终构建记录：`build/voice-fixes-build.log`。SkinTextureTests/IconLibraryTests 使用 WARP 和独立夹具；SettingsVfsTests 使用隔离实例及 `CREATE_NO_WINDOW` 子进程。没有运行需要交互的 Preview 目标；21 个 Tests 目标没有遗漏。

首次构建发现 MakeFunctionArguments 不接受 `int&`，已改为显式 int 值参数，随后最终构建通过。依赖重编译出现已有 minhook `hde64.c` 的 C4701 警告，未修改第三方库。

最终通过既有 after_build 自动部署到：

`H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`

构建产物：`build/windows/x64/release/FavoriteWheel.dll`  
部署产物：`H:/Games/Dev Skyrim/mods/36 - FavoriteWheel/SKSE/Plugins/FavoriteWheel.dll`

两个文件 SHA-256 完全一致：

```text
527BD2D9DE0C1C7770A83276C9839A3DE2D893162C1444C37B90D8F750DF8A8E
```

部署前后核对主 `FavoriteWheel.ini` SHA-256 相同，字节未变；八种语言各只有 12 个旧默认值被迁移，其余已有值保持一致，迁移备份文件均存在。部署验证记录：`build/voice-fixes-deployment.json`。未修改 xmake 配置，`deploy_dir` 始终保持约定目录。


## 尚需游戏内验证

- [ ] 分别手持单手武器、双持武器、盾牌/火把、单手法术、双手武器/弓、双手法术，保存含夜视或龙吼的战斗预设；再将双手设为 Ignore，确认不必先空手。
- [ ] 依次切换夜视→战吼→龙吼→夜视；指定 Voice 直接替换，无 ClearVoice 超时；重复应用同一预设不卸下手持物品。
- [ ] Voice=卸下分别清除能力与龙吼；能力要求原生回调完成且 selectedPower 为空；同时确认其他手槽按预设处理。
- [ ] 保存装备 A，换成 B，然后三个槽各自完整循环指定/忽略/卸下/指定，确认恢复 A；在 Ignore/Empty 中存档读档后再恢复。
- [ ] 双手右槽忽略/卸下后恢复，双手标志正确；冲突左槽导致拒绝时数据不变；显式覆盖仍捕获 B。
- [ ] 旧 FWC 1/FWC 2 角色存档及动态 Voice 引用在重载后正确解析；快捷槽/循环绑定不变。
- [ ] 连续失败、中断、切菜单/失焦后，能重新打开轮盘并应用预设；确认日志有 Finish，不持续停留 Busy。
- [ ] 狼人形态保存与应用仅 Voice 预设、编辑 Voice、变回人形；此次未进行游戏内狼人验证。
- [ ] 在真正的“装备预设”（Outfit）页分别测试穿上、再次卸下、再穿上，并确认日志是 Outfit；其原有护甲切换语义保留。
- [ ] Dynamic Grip、Dual Wielding Unlocker 或其他改写装备状态的模组启用时复测；模型测试不能替代这些引擎交互。

若仍失败，请保留本轮 `FavoriteWheel.log`，记录实际页签、预设 ID、三个槽的模式/物品、当前形态、复现时间及画面提示。日志重点为 `Combat state`（right/left/selectedPower/voiceEquipped）、`apply started` 的目标模式与物品、`issued step`、超时/Finish、`callback_pending`、`Combat tick interrupted`。Outfit 重点为 `apply requested`、`validation`、`removeOnly` 和 `finished`。步骤号仍为 0 清右手、1 清左手、2 清 Voice、3 装右手、4 装左手、5 装 Voice、6 验证。

自动测试和文件部署不等于游戏内验证；本次未启动游戏、接管键鼠或覆盖用户未保存工作。
