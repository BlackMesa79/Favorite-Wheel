# FavoriteWheel 当前项目交接

更新日期：2026-11-11（Asia/Hong_Kong）。第 1、2A 节为当前状态；第 3–7 节及旧 `HANDOFF.md` 是历史交接记录，`docs/RELEASE_0.9.0.md` 只描述 0.9.0 发布。不要把历史待测事项或旧方案当成当前实现。

## 1. 当前状态

- 项目路径：`D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘`，Windows / PowerShell / C++ / xmake / SKSE。
- 当前版本 **0.10.0**，插件元数据 **0.10.0.0**；0.10.0 已打包。
- 当前分支：`feat/outfit-parts-p1`，HEAD `2899d45`（I4 第三期 T-5 设置/面板/翻译/I4Debug，见 2A.3d）；`609f066` 为 SGT/MISC 乐器修复；`162c959` 为 I4 第三期 T-4 游戏端接入（见 2A.3c）；`a581cbe` 为 I4 第二期 I4Rules/I4Entry（见 2A.3b）；`ef2517b` 为 test_grid 临时目录修复； `f51bd04` 为 I4 第一期 SwfRaster（见 2A.3）。2026-11-11 在 `f51bd04` 的干净 worktree（不含乐器修复，关闭部署）中复验：全部 C++ 测试（含加固）、主 DLL、I4IconDump 构建通过；发现 test_grid 依赖 build/ 下既有目录，已由 Coder 修复并在干净环境复验通过。上一个提交 `035c453` 是 PNG 图标/皮肤材质 mip 采样修复。
- 正在开发：**I4 图标库接入**，第一期（`f51bd04`）、第二期（`a581cbe`）、第三期 T-4（`162c959`）、T-5（`2899d45`）已提交；下一步 T-4b（磁盘缓存）、T-6（预览场景与文档），然后用户做完整 AC-10 验收，详见第 2A 节。
- **SGT/MISC 乐器修复**：已提交 `609f066`（独立 `fix:` 提交，在 T-5 之前）。2026-11-11 用户游戏内验收通过。验收项（用户提供的原测试单）：
  - A 组（Bug 1）：武器收起时，轮盘“其他”中点鼓/笛/鲁特琴，有动画和音效；拔出武器后点同一乐器，先收刀再正常演奏（修复前只有音效）。日志 `SGT instrument native use requested: form=000DABA7/A9/AB...`。
  - B 组（Bug 2）：用 Favorite Things 收藏的非原版 5 个 FormID 的 SGT 乐器，在轮盘点击不再提示“此类物品暂需在背包中使用”。日志 `MISC fallback equip: form=... extra=...`；带 SGT 脚本的会触发演奏。
  - C 组（回归）：原版 5 个 SGT 乐器（0xDABA7/A9/AB/105177/105109）仍走 `SGT instrument native use requested`，不走 fallback；未收藏的普通杂物仍提示 unsupported。
  - 主会话核对 13:27–13:29 的游戏日志：有 `000DABA9`、`00105177` 的 `SGT instrument native use requested`，与 A/C 组一致；该份日志中没有 `MISC fallback equip`，B 组依据用户口头确认。
  - 代码：收藏的 MISC 物品即使 `usable=false`，也走原生 `EquipObject(player, obj, extra, 1, nullptr, false, false, true, false)`（`Item.miscFallback` + `UseFavorite` 回退块 + `Wheel.cpp` 选择时不拦截）；回退路径与原 `TalentInstrument` 路径在装备前若武器已拔出先 `DrawWeaponMagicHands(false)`。
- 工作区仅剩未提交：`tests/WheelPreview.cpp`（4 个 i4-* 预览场景，用户决定留到 T-6）与本文件。
- 已部署 DLL：`6196D593…8206CE`（工作区构建，内容等同 `2899d45`；部署目录与构建产物一致）。干净 worktree 构建产物哈希不同（路径等元数据差异），以部署目录为准。
- 0.10.0 发布包：`dist/FavoriteWheel-0.10.0.zip`，SHA256 `43fca6fb36bc14378b206f19c28a3e5c460cf2fadd865a4f9879f614012641f7`；发布 DLL SHA256 `A436626BF7DD5AAF08BAC7FD999F62EB01655F908159E06AF3A370E183CBDF99`。
- 乐器修复已验收；版本号与发布策略待 I4 完成后一并决定。

## 2. 重要工作约定

- 先阅读项目 `AGENTS.md` 及会话提供的全局代理约定。
- 后续构建继续使用 xmake `after_build` 自动部署到 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
- 保留用户主配置及自定义资源，不能以源码示例 `FavoriteWheel.ini` 覆盖游戏配置。临时隔离部署结束后恢复既定部署目录。
- 每次部署后核对构建 DLL 与目标 DLL 的 SHA256。部署成功不代表游戏验证通过。
- 不抢键鼠、不切游戏焦点。需要接管前说明目的、窗口及范围并获得明确同意；优先静态检查、后台测试和离屏渲染。
- 委派约定（2026-11-10 用户确认）：实现交给 codeg 助手 **Coder**（`delegate_to_agent`，`assistant_id=2`），测试交给 **Tester**（`assistant_id=3`，已换为 Codex），都不传 `agent_type`。不要自动上传、提交、推送或修改其他模组。
- **委派限制（重要）**：凡是涉及 SwfRaster “异常输入 / 越界 / 资源耗尽 / 加固回归”的任务，Coder 和 Tester 都会拒绝（child_rejected），拒绝后该会话被锁定。加固用例已拆到 `SwfRasterHardeningTests`；普通回归只跑 `SwfRasterTests`，可以委派（2026-11-10 已验证 Tester 接受）。不要换措辞绕过；这类工作由主会话完成，或交给用户决定。
- 控制日志输出；读图片前生成长边不超过 1280 像素、目标不超过 300 KB 的预览，按具体检查目标逐张查看。

## 2A. I4 图标库接入（进行中）

### 2A.1 文档

- 方案（APPROVED，必读）：`docs/plans/20261110-i4-icons.md`。第 9 节是用户已确认的决策，§8 是变更记录。
- 背景与验证结论：`docs/I4_ICONS.md`（含“2026-11-10 离线验证结论”一节）。
- 各轮报告（都在 `build/`，不入库）：
  - 离线 PNG 验证：`build/i4-preview/REPORT.md`
  - 锯齿与配色：`build/i4-preview/REPORT-round2.md`
  - 第一期三轮实现：`build/i4-phase1/REPORT.md`、`round2/REPORT.md`、`round3/REPORT.md`
  - Tester 审查（FAIL，R1–R4）：`build/i4-phase1/tester/REPORT.md`
  - **第四轮加固与留出集（最新）**：`build/i4-phase1/round4/REPORT.md`

### 2A.2 已确认的决策

- 路线：不调用 I4 DLL，也不加载 SWF 电影。在本项目中移植 I4 规则匹配（MIT），自研 SWF 解析与栅格化（miniz + PlutoVG）。
- 默认配色 `I4IconColor=Theme`（白色遮罩乘以主题色）；另有 `I4`（遮罩 × iconColor）和 `Original` 两种。
- `I4IconScope=All`：没有规则的物品也使用 SkyUI 默认图标。
- 优先级（方案 4.4.1）：主题图标包 `[Forms]` → 图标包新增的 `[I4]` 段（按 `source|label` 重绘）→ I4 规则图标 → 图标包分类 → SkyUI 默认图标 → 内置图标；主题可声明 `I4Icons=Off`。
- 设置项放在主 INI 和设置面板，不放进主题。
- **磁盘位图缓存**：放在游戏根目录 `Data/SKSE/Plugins/FavoriteWheel/IconCache/`，**不放** `My Games`（避免多个 MO2 实例共享）。缓存键为 SWF 内容 SHA-256 + 标签 + 变体 + 边长 + 转换器版本。设计见方案 4.4、AC-11、T-4b，属于第三期，尚未实现。
- 分三期实施：第一期 SWF 栅格化 → 第二期规则引擎与字段推导 → 第三期接入游戏、缓存和设置。

### 2A.3 第一期（SwfRaster）现状：已提交 `f51bd04`

新增文件：
- `include/SwfRaster.h`：公共接口，签名不得改动。
- `include/SwfRasterDiagnostics.h`：只在 `WHEEL_SWF_DIAGNOSTICS` 宏下启用。
- `src/SwfRaster.cpp`
- `tests/SwfRasterTests.cpp`（功能回归）、`tests/SwfRasterHardeningTests.cpp`（InvalidInputs、StrokeBounds、ExtremeTransforms、WorkBudgets、DeepTruncation）、共用的 `tests/SwfRasterFixtures.h`
- `tools/I4IconDump/`：离线对照工具，含 main.cpp、compare.py、grid_compare.py、grid_reference.py、reference.py、test_*.py、README.md。
- `include/third_party/`：miniz 导出头，以及 `plutovg_checked_alloc.h`。
- `licenses/LICENSE-{miniz,plutovg,plutovg-components,FreeType}.txt`

修改文件：`xmake.lua`（wheel-miniz、wheel-plutovg、SwfRasterTests、I4IconDump target）、`scripts/bootstrap.ps1`（miniz 3.0.2 `293d4db`、PlutoVG v1.3.1 `57f82ac`）、`THIRD_PARTY_NOTICES.md`，以及 docs 中的方案、I4_ICONS、README 索引。

关键结论：
- **AC-4（与 FFDec 的视觉一致性）**：
  - 度量：同一 SWF 舞台与 zoom 的原生像素网格，零位移连续 alpha 软 IoU ≥ 0.98，库级通过率 ≥ 98%，可见负对照误收率 ≤ 1%。阈值常量在 `tools/I4IconDump/grid_compare.py` 顶部。
  - 结果：BOOBIES 189/189，Handy 976/976。
  - **留出集**（11 个未参与校准的 SWF，649 个标签）：637/643 = 99.07%，负对照校准通过。6 个失败都是 celtic 分类图标的 LineStyle2 像素提示特性，属于方案范围外，会回退到内置图标。
- **第四轮加固**（主会话实现，因为 Coder 拒绝）：
  - 路径以 double 精度变换到设备坐标，并检查有限性与 ±16384 px 范围。
  - 描边边界按 cap/join 计算，乘以实例矩阵的最大奇异值。
  - 单次 Render 的工作量上限：重放 4096 个 op、单路径 131072 点、全帧 262144 点、整画布扫描 160 次。
  - PlutoVG 分配失败时 `__fastfail`（通过强制包含头实现，不修改上游源码）。
  - 用 Tester 的原始复现输入验证：问题已全部消除。
- 测试：SwfRasterTests 172999 项 + SwfRasterHardeningTests 5028 项（合计 178027，与拆分前一致）通过。2026-11-10 新 Tester 会话独立复验普通回归 PASS（SwfRasterTests、ImageSampler、IconLibrary、SkinTexture、Settings、WheelLogic、Python test_compare/test_grid、FavoriteWheel 与 I4IconDump 构建，部署 DLL 哈希一致）；加固测试只由主会话运行，待用户审阅。
- 真实图标耗时：单图标 P99 ≤ 11 ms，最大 71 ms（Handy 的 hawk，共 66 条路径）。
- 已知限制：
  - LineStyle2 的像素提示、noHScale/noVScale、不对称端点不支持。
  - 内存分配失败时终止进程，而不是回退到内置图标。
  - 方端/miter 的边界估计偏保守。

### 2A.3b 第二期（I4Rules / I4Entry）：已提交 `a581cbe`

- 文件：`include/I4Rules.h`、`src/I4Rules.cpp`、`src/I4RuleTables.inc`（由 `build/i4-phase2/generate-tables.py` 从上游枚举生成，生成脚本未入库）、`include/I4Entry.h`、`src/I4Entry.cpp`、`tests/I4RulesTests.cpp`、`tests/I4EntryTests.cpp`、`tools/I4RuleAudit/`，nlohmann/json v3.11.3（`extern/json`）。
- 契约增补：`Load(span, const LoadOptions&)`（`pluginPrefix`、`sourceExists` 回调）、`Value::Array`。`Collect`、`LoadFromGame`、MergeMapper 重映射留到第三期。
- 结果：AC-1 1161 项、AC-2 1029 项；AC-5 本机 31 文件 / 671 规则与 `InventoryInjector.log` 一致（需传 `Skyrim.ccc` 补隐式 CC 插件）；AC-7 一般 100 条 ≈0.1–0.3 ms、500 条 ≈0.7–1.2 ms，偶见单次尖峰（一次 100 条 1.33 ms），计时方法未抗噪。
- 干净 worktree 复验通过后提交。报告：`build/i4-phase2/REPORT.md`。
- 上游源码（只读，不入库）：`build/research/InventoryInjector`（52cc4cb）、`build/research/skyui`（8354287）。
- `FAVORITEWHEEL_DEPLOY_DLL_ONLY=1`：只部署 DLL，用户确认保留。
- 本机另装 I4 Improved（Nexus 192976）：只有 DLL，不带规则；hook SkyUI `processList` 与 `FavoritesMenu::ProcessMessage`。AC-10 需确认与轮盘共存。见 `docs/I4_ICONS.md` 末节。

### 2A.3c 第三期 T-4（游戏端接入）：已提交 `162c959`

- 新文件：`include/I4Game.h`、`src/I4Game.cpp`（LoadFromGame、Collect）、`include/I4Runtime.h`（RuntimeConfig 默认值、TextureKey、预算/LRU）、`include/I4Textures.h`、`src/I4Textures.cpp`（SwfStore、单工作线程、上传与缓存）、`tests/I4IntegrationTests.cpp`。修改 Favorites（Item.i4 与快照）、IconLibrary（六级 Find 与 `[I4]` 段）、WheelIcons（三种配色）、UIResources（主题 `I4Icons=Off`）、Renderer、main、I4Rules（缺失 source 汇总告警）。
- 运行时配置固定默认值（开、Theme、All、32 MB），尚未接 INI/设置面板，属于 T-5。
- 复验：Tester 独立回归与审阅（`build/i4-phase3/tester/REPORT.md`）→ Coder 修复 C1/C2/Q1/Q2/Q3（`build/i4-phase3/fix1/REPORT.md`）→ 干净 worktree 全部测试通过，AC-8 截图哈希与改动前一致。
- 提交方式：在 worktree 中用 `git apply -R build/i4-phase3/instrument-fix.patch`（只作用于 Favorites.h/.cpp）剥离乐器修复后提交，再 `git update-ref` + `git reset`（mixed）同步主工作区；乐器修复当时仍在工作区未提交（2026-11-11 用户验收通过后改为与 T-5 一起提交，见第 1 节）。
- 已部署 DLL（含乐器修复）SHA256 `6C43D3DD…EF104`。
- 已知遗留：C3 渲染器异常后永久 failed，无设备恢复入口（T-4 前已存在）；MergeMapper 重映射未实现（本机未装）；AC-7 真实快照耗时、AC-10 游戏内验收未做。验收清单草稿见 `build/i4-phase3/tester/REPORT.md` 第 6 节。

### 2A.3d 第三期 T-5（设置、面板、翻译、I4Debug）：已提交 `2899d45`

- 主 INI：`[Display]` 下 `I4Icons=1`、`I4IconColor=Theme`（Theme/I4/Original）、`I4IconScope=All`（All/Rule）、`I4TextureBudgetMB=32`（8–128）；`[General]` 下 `I4Debug=0`。`I4IconCache*` 属于 T-4b，未加。
- 轮盘设置“外观”页 11→14 行（仅此页行距压缩为 24），新增 I4 图标、配色、范围三项和 `i4ApplyHint`（应用后生效）；SKSE Menu Framework 外观页同步。预算与 I4Debug 仅在 INI。8 种语言新增 9 键。
- I4Debug：`Resolve(Entry&, std::vector<RuleMatch>*)` 重载，输出命中规则“文件#序号”；关闭时不构造字符串。Tester 探针 6204 组对照确认与原 Resolve 结果一致。
- 加载日志（`include/I4LoadLog.h`）：重复警告合并计数，缺失插件按插件汇总并附引用次数；本机 1347 行 → 63 行（62 个插件、1285 次引用），附一行无害说明。
- AC-8：`schools` 截图字节一致；`normal` 截图 11 像素差异（(903,645)–(964,651) 底部文字），原因是新增中文“图”字改变 WheelPreview 预热全部语言后的字体图集；隔离副本恢复旧中文语言文件后哈希回到基线。主会话决定接受，不改 WheelFonts。当前 normal 哈希 `92983a4a…6e66e`。
- 复验：Coder 报告 `build/i4-phase3/t5/REPORT.md`、`t5-logfix/REPORT.md`；Tester 回归全部通过（其 REPORT.md 未写出，结论由主会话从 `build/i4-phase3/tester-t5/` 下日志与 JSON 核对）；干净 worktree（基于 `609f066`）全部测试通过后提交。
- 2026-11-11 用户实机（T-4 DLL，T-5 部署前）：法术、药水显示 I4 图标，Theme 配色正常。`I4 load: files=31 rules=671 rejected=0 elapsed_ms=98.218`；12 次快照 42–44 项，`collect_derive_resolve_ms` 0.256–0.626（满足 AC-7 的 100 项 < 1 ms；尚无 500 项全背包实测）。SWF 诊断 3 条（i4iconaddon、magus/gourmet、skyui psychosteve：sprite End 容错、不支持 tag 9/69），对应图标正常。
- T-5 的设置面板与配色切换尚未实机验证。

### 2A.4 待办（按顺序）

1. 用户审阅加固部分（`tests/SwfRasterHardeningTests.cpp` 与 `build/i4-phase1/round4/REPORT.md`）；不要把运行它的任务委派给 Coder/Tester。
2. `tests/WheelPreview.cpp` 的 4 个 I4 预览场景：用户 2026-11-11 决定暂不提交，留到 T-6 改为走真实 I4 渲染路径后再提交。
3. 用户实机检查 T-5：设置面板新三项、三种配色切换、I4Debug 输出、加载日志汇总。
4. 第三期剩余：T-4b（磁盘缓存）→ T-6（预览场景与文档）；最后由用户做游戏内验收（AC-10）。

### 2A.5 离线环境与资源

- 便携工具（不入库）：`build/tools/ffdec/ffdec.jar`（FFDec 26.4.0）、`build/tools/jre/zulu17.70.113-ca-jre17.0.21.0.3-win_x64/bin/java.exe`。Java 的 APPDATA/HOME/TEMP 必须限定在 build 下，并发 ≤2。
- 参照素材（只读）：`H:/Games/Dev Skyrim/mods/04 - B.O.O.B.I.E.S (aka Immersive Icons)`、`04 - The Handy Icon Collection Collective`；留出集来源是 moreHUD 和 `32 - Vel'dun UI`。第三方素材及其衍生 PNG 只能放在 `build/`，不得入库或打包。
- 常用命令：`xmake build -j 2 SwfRasterTests && xmake run SwfRasterTests`；`build/windows/x64/release/I4IconDump.exe --work <SWF目录> <csv>`（统计每个图标的工作量与耗时）；同网格流程见 `tools/I4IconDump/README.md`。
- Python 环境：本机 Python 3.13 有 Pillow 和 NumPy（第 7 节“Pillow 不可用”的说法已过时）。
- shell 注意：用 bash heredoc 写入 C++ 源码时，`'\n'` 会被吃掉变成真换行。编辑源码请用 Edit 工具。

## 3. 发布文件和证据

路径均相对项目根目录：

| 文件 | 内容 |
| --- | --- |
| `dist/FavoriteWheel-0.9.0.zip` | 安装包，1,190,191 字节 |
| `dist/FavoriteWheel-0.9.0-source.zip` | 源码包，11,594,074 字节 |
| `dist/FavoriteWheel-0.9.0-SHA256SUMS.txt` | 两个 ZIP 的校验值 |
| `release-materials/0.9.0/Nexus-description-en.bbcode.txt` | 可用于更新 Nexus 的英文详情页 |
| `release-materials/0.9.0/Nexus-preview.html` | 本地详情页预览 |
| `release-materials/0.9.0/Nexus-summary.txt` | 英文简介 |
| `release-materials/0.9.0/Nexus-changelog-en.txt` | 简易英文 changelog |
| `release-materials/0.9.0/changelog-zh_CN.txt` | 简易中文 changelog |
| `release-materials/0.9.0/verification.txt` | 构建与测试记录 |
| `build/release-0.9.0-archive-verification.txt` | 归档验证记录 |
| `docs/RELEASE_0.9.0.md` | 发布范围和限制 |

发布 DLL SHA256：`1FB3738D44C66BD94668044401BE35169E6F39EB47C689B29C0CE28935A46058`。

发布时用户主 INI SHA256：`0D91EB80E30B93526990DE9C39634423605E4A14A9722B776F296BFE608425E2`。这是当时保留配置的证据，不应据此覆盖用户后续配置。

安装包包含 14 个文件，不包含用户主 INI；包内 DLL 与构建一致。源码清单 3,481 项已逐项核验。旧 0.8.0 包和材料保留。源码包代表打包时快照，不包含本次新增交接文件。

## 4. 0.10.0 当前实现

### 装备预设编辑面板

- 新增"编辑预设"面板，双列表界面（预设部件 / 库存装备），支持添加/移除装备。
- 分类过滤支持：全部/头部/身体/手部/脚部/饰品/其他。
- 自动处理槽位冲突并替换旧部件，防止重复添加，1-64 件数量校验。
- 升级装备预设存档格式为 FWO 2（向下兼容 FWO 1）。

### 战斗预设 Voice 槽位支持

- 左手/右手/Voice 三槽独立状态控制（指定/忽略/卸下）。
- 支持保存和恢复龙吼及 Voice Power。
- 狼人形态下可使用战斗预设和循环，仅 Voice 槽可修改。
- 新增"装备与循环通知"开关控制 HUD 通知。
- 升级战斗预设存档格式为 FWC 3（兼容 FWC 1/2）。

### 自定义图标库

- 外部图标包支持，放置在 `SKSE/Plugins/FavoriteWheel/IconPacks/<包名>/`。
- 主题 INI 通过 `IconPack=名称` 引用图标包。
- 支持按学派、功能和具体物品 FormID 自定义图标。
- 仓库附带示例图标包（`IconPacks/example/`）。

### 法术学派图标

- 变化、召唤、毁灭、幻术、恢复系法术使用独立内置图标。
- 龙吼、力量、书籍具有独立图标。
- 按游戏原生 `GetAssociatedSkill()` 自动分类。

### 书籍与物品分类

- 书籍加入轮盘并独立分类，支持阅读。
- 药物/食物使用后轮盘保持打开（可在设置中关闭）。

### 键鼠移动模式

- WASD/方向键可在移动模式下透传给游戏引擎。
- 鼠标滚轮翻页，鼠标中键/Z/C 键切换分类。
- 设置页新增移动模式开关和分类切换键配置。

### 兼容性改进

- 修复 TNG slot 52 等每次读档 uid 变化的物品报 Ambiguous 的问题。
- 修复 QJO 杂项任务界面打开时 Q 键仍能触发轮盘的问题。
- 原生自由相机激活时阻止轮盘呼出。
- Skyrim's Got Talent 原版乐器（鼓/笛/鲁特琴）限定原生使用入口。
- SimpleIME 中文输入候选框层级修复。

### 排序与拖拽（继承自 0.9.0）

- `T` 切换排序，`Shift+T` 切换手动排序，`F2` 直接进入设置。额外功能小轮盘和页脚按钮方案已撤回，不要重新恢复。
- 若用户入口或轮盘切换键占用 T，原绑定优先。尚未提供专用排序键重绑定。
- 手柄：左摇杆按下切换排序，右摇杆按下切换编辑；A 前移，X 后移。
- 只对人类状态的收藏／全部库存目录排序：9 类 × 2 个范围，共 18 组独立顺序。不影响功能轮盘、循环执行顺序或吸血鬼领主专用分支。
- 支持名称、已装备优先、自定义、类型／槽位，以及武器伤害、护甲值、法术魔法消耗；武器和护甲还支持重量、价值排序。
- 数值排序复用详情取值接口，仅在游戏线程收集所需类别；刷新目录或重开时更新，不持续轮询。无效值排后，零有效，同值按名称。弹药使用基础伤害，法杖没有物理伤害值；持续施法按每秒消耗，不与瞬发时长折算。
- 名称使用文本排序，未实现拼音排序。
- 手动模式下，左键短按前移，右键后移；左键移动达到阈值后拖拽，落在目标扇区时按位置插入，中间项目移位，不是两项交换。
- 按住拖动时滚轮可跨页；右键／Esc 取消拖拽并保留编辑模式。中心、空白或原位置松开不改变顺序；不支持跨类别拖动或向空扇区追加。
- 模式、类别、设置、关闭或目录刷新会取消当前拖动。动画包含源项淡化、跟随图标／名称、目标边缘提示及落点淡出；关闭动画仍有静态反馈，中间项目没有逐项飞行动画。
- 排序随角色 `.skse` 存档保存，需要保存游戏；不是全局 INI 顺序。新模式记录可能不被旧版接受。

主要代码：`include/InventoryOrderModel.h`、`include/InventoryOrder.h`、`src/InventoryOrder.cpp`、`include/SortDrag.h`、`src/Wheel.cpp`、`src/Draw.cpp`。测试：`tests/InventoryOrderTests.cpp`。

### 其他功能与修复

- F2 → 游戏行为 → 记住上次轮盘，默认关闭；INI 为 `[General] RememberLastWheel=0`。普通入口记住本会话最后的人类库存／功能轮盘；专用功能快捷键仍直接进入。受限形态不覆盖记忆，读档／新游戏清除会话记忆，设置本身保留。
- 首次开盘优化：Renderer 初始化时按帧缓冲尺寸预热本地化字体。用户确认首次 Q 打开更快；新字形、语言或尺寸变化仍可能重建。
- 换装保存允许有限的 `health=0`，精确保留并匹配，不归一化为 1，不修改游戏物品；负值、NaN、Inf 仍拒绝。实际反馈中的戒指和长袍均为零值导致整套捕获失败，未证实 ARMA 模型数量是原因。此前失败的预设需重新保存；旧版可能拒绝零值记录。
- 普通人类法术再次选择可卸下目标手；双手分别持同一单手法术时只清目标手，必需双手的法术一起清除匹配槽。能力、龙吼及吸血鬼领主专用逻辑保持原状。使用既有 Actor.UnequipSpell 路径，不直接写 selectedSpells，不临时解除暂停。
- 语言部署迁移只替换已知旧默认提示，保留用户自定义文本，一次性生成 `.before-controls-migration` 备份；当前提示为 T／Shift+T 和拖拽。中英已翻译，其他语言新增项使用英文回退。

## 5. 不要回归的修复与实现边界

1. 拖拽松手曾导致 `std::system_error: resource deadlock would occur`：`EndSortDrag` 持有 `viewMutex` 调用 `ValidPlayer()`，后者经 `IsOpen()` 再锁同一非递归锁。当前允许状态在取得 `viewMutex` 前计算，仍由递归 `lifecycleMutex` 保护。不得把该调用移回视图锁内；排序锁在 ResortInventory 前释放，短按 MoveSortedItem 在视图锁外执行。
2. 数值排序曾卡在加载中：旧队列任务被 revision 作废，新请求未入队，而 loaded 状态阻止重试。`DirectoryRequestNeeded` 使 `(!loaded || loading) && !queued` 时重试，已有目录走刷新请求。
3. 排序序列化显式使用 `std::locale::classic()`，避免地区数字分组污染格式。
4. 排序记录 `0x46575352`，版本 1，文本头 `FWS 1`。模式 ID 为 Default=0、NameAscending=1、NameDescending=2、EquippedFirst=3、Custom=4、Type=5、Slot=6、StatDescending=7、StatAscending=8、WeightAscending=9、ValueDescending=10；不能随意重编号。
5. 持久化身份包含 form、附魔、health 位值、unique ID 和显示名；武器／杂项聚合时 UID 为零，不保存指针或快捷键。相同身份共享排名，缺失项目保留，新项目追加；每组最多 10,000 个键，记录最多 16 MB。

详见 `docs/CODE_REVIEW_2026-10-26.md`、`docs/DRAG_SORT_V1.md`、`docs/INVENTORY_SORTING_V1.md`、`docs/NUMERIC_SORT_AND_HINT_MIGRATION.md`、`docs/WHEEL_MEMORY_AND_ZERO_HEALTH_OUTFITS.md`、`docs/SPELL_RESELECT_TOGGLE.md`、`docs/FIRST_OPEN_FONT_WARMUP.md`。

## 6. 已做验证及未解决事项

- 0.9.0 发布记录包含 13 项通过：RuntimeLayoutTests、InventoryOrderTests、WheelLogicTests、HandEquipTests、OutfitTests、SettingsTests、SettingsPersistenceTests、CombatPresetTests、CombatCycleTests、InventoryTests、LayoutSkinTests、ItemInfoTests、GameSessionTests。
- 语言迁移回归通过；BBCode 配对验证通过、HTML 已生成。发布阶段没有重新做浏览器截图。
- RuntimeLayoutTests 校验 DLL 导出 0.9.0.0；运行时布局测试使用夹具，不等于各游戏运行时实测。
- SettingsVfsTests 已编译，但该轮没有运行（需 MO2 usvfs DLL），不计入通过项。
- 拖拽 WARP 离屏渲染曾检查 `build/drag-sort.jpg`、`build/drag-sort-static.jpg`、`build/drag-settled.jpg`。用户确认此前游戏测试正常，不能扩大为所有模组组合均通过。
- PureDark Upscaler／Theo's Render Pipeline 导致轮盘不可见的问题仍未证实修复，不要在 changelog 声称支持已解决。
- 用户自行解决吸血鬼领主 T-pose 后，还原形态恢复正常；用户明确取消能力音效修复并要求回滚，已执行。不要恢复音效实验或将外部动画问题归因于本模组。
- 尚未实现菜单内直接施放种族能力、功能轮盘排序。可选性能改进：自定义比较器避免重复生成身份字符串，减少数值模式扫描量；这些不是当前待交付任务。

## 7. 常用命令与接续方式

在项目根目录 PowerShell 中使用：

```powershell
xmake build -j 2 FavoriteWheel
& ./build/windows/x64/release/InventoryOrderTests.exe
xmake lua scripts/test_language_migrations.lua
node release-materials/0.9.0/render-description.cjs
```

- 其他测试按需要构建并执行同目录 `<Target>.exe`，不为交接文件重复完整构建。
- 打包脚本为 `scripts/package.ps1`，从 xmake 获取版本，可能覆盖同版本归档；只有实际需要重新打包时运行，先明确版本与保留策略。
- 离屏预览程序 `WheelPreview.exe` 可用 `drag-sort`、`drag-sort-static`、`drag-drop`、`drag-settled`、`numeric-sort`、`sorting`、`gameplay` 模式；图片预处理后再读。
- Python：`C:\Users\BlackMesa\AppData\Local\Programs\Python\Python313\python.exe`，读写显式 UTF-8；本机 Pillow 不可用，可用 System.Drawing 生成预览。Node：`C:\Program Files\nodejs\node.exe`。
- 游戏日志：`D:\MyFiles\Documents\My Games\Skyrim Special Edition\SKSE\FavoriteWheel.log`。
- 接手时先看 `git status --short`、本文件及相关专题文档，再围绕用户新请求做最小必要改动。不要重做已完成发布流程，也不要把历史未实现想法自动当成新授权。
