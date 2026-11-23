# FavoriteWheel 当前项目交接

更新日期：2026-11-24（Asia/Hong_Kong）。第 1 节记录 0.11.1 当前交接，其余章节是各阶段记录。当前反馈进度见 `docs/reports/20261122-feedback-0.11.0.md`。

## 1. 当前状态（新会话从这里开始）

- 项目路径：`D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘`，Windows / PowerShell / C++23 / xmake / SKSE。
- 当前版本 **0.11.1**，插件元数据 **0.11.1.0**。除装备全局设置、手柄组合键、数字快捷键和 MO2 空缓存目录修复外，本版纳入 `kiriko-xx` 的 PR #4 及窗口阻塞状态修正。用户确认框架阻塞时的导航、恢复游戏后的轮盘入口及关闭框架后的键鼠与手柄入口均正常。Nexus 英文更新日志见 `release-materials/0.11.1/Nexus-changelog-en.txt`。
- 发布分支为 `main`。下表记录 0.10.0 后 I4 上线阶段的主要提交。

| 提交 | 内容 |
|---|---|
| `035c453` | fix：PNG 图标与皮肤材质 mip 采样 |
| `f51bd04` | I4 第一期：离线 SWF 解析与栅格化（SwfRaster） |
| `ef2517b` | fix：test_grid 临时目录 |
| `a581cbe` | I4 第二期：I4Rules 规则匹配、I4Entry::Derive |
| `162c959` | I4 第三期 T-4：游戏端接入、后台栅格化、纹理缓存 |
| `609f066` | fix：收藏的 MISC/SGT 乐器走原生装备路径（用户已验收） |
| `2899d45` | I4 第三期 T-5：设置项、设置面板、翻译、I4Debug、加载日志汇总 |
| `ce5b4a5` | docs：HANDOFF |
| `58a0cd3` | I4 第三期 T-4b：磁盘位图缓存 |
| （本文件） | docs：HANDOFF 整理 |

- 0.11.1 已部署 DLL 位于 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel/SKSE/Plugins/FavoriteWheel.dll`，SHA256 `F50E873BD1959B278E020CAF9BCFCAA4950C25E8F6791ED83A128B0B29B36E73`。最终构建仅部署 DLL，162 份其余文件已核对。
- 本版提供 `dist/FavoriteWheel-0.11.1.zip` 供 Nexus 上传。待跟进事项包括原反馈者环境的复测、左手戒指模组信息及 TNG 52 槽兼容。

### 1.1a I4 颜色一致性验收修复（2026-11-15，实机已验收，已提交 `9d0f26c`）

- 用户反馈部分书籍、法术在扇区偏白，详情为主题金色。部署配置为 classic / I4；原因是没有 iconColor 的图标直接继承绘制位置 ink，扇区文字色与详情强调色不同。
- src/IconLibrary.cpp：Theme 使用主题 accent；I4 有 iconColor 时保留指定色，缺失时使用 accent；Original 原色不变，保留调用位置 alpha。图标包覆盖和内置图标策略不变，无需清缓存。
- tests/I4IntegrationTests.cpp 补充缺色 Default/Rule、主题切换、位置及透明度回归，随修复提交；tests/WheelPreview.cpp 的缺色样本依赖原有 T-6 改造，继续保留为未提交改动。
- 主工作区与干净 worktree 集成测试均通过 844 项，三种配色预览逐字节一致；图标库测试通过，规则 1178 项、字段推导 1029 项通过。schools 截图与原基线一致。报告：build/i4-phase3/color-consistency/REPORT.md。
- 2026-11-15 用户重启游戏后确认扇区与详情图标颜色一致，本次问题实机验收通过；依据为用户反馈。已同步 docs/I4_GAME_ACCEPTANCE.md。此次确认不代表完整 AC-10 或全部配色切换场景通过。本次仅提交颜色修复、集成回归及验收记录；此前 T-6 改动保留在工作区，不推送。

### 1.1b TNG／动态辅助装备缺失兼容（2026-11-15，未提交、待实机验收）

- 用户授权 I4 颜色修复提交且不推送，提交为 `9d0f26c`。T-6 预览及文档改动保留未提交；本次装备兼容代码也暂未提交。
- 新反馈日志 G:/Downloads/Browser/Favorite Wheeler Log.txt：Plain 预设 3 件，TNG GenitalCover（TheNewGentleman.esp:AFF）完全不在背包，same_base_candidates=0、uid=0；不是此前 UID 改变问题。灯笼缺失为用户描述，日志没有对应条目证据。
- 用户选择每套预设独立设置。编辑页新增“跳过缺失物品”（默认关），鼠标／M／手柄 X 切换，F5／Y 保存；还需保存游戏持久化。按基础物品不存在判断缺失，不绕过歧义、改造属性、重复或装备保护。
- FWO 3／co-save 3 保存策略，读取旧 1、2 默认严格；覆盖、导入导出保留。固定本次可用目标，允许动态消失／重建；全部缺失不执行。说明与验收清单：docs/OUTFIT_MISSING_ITEMS.md。
- 主工作区与独立副本：OutfitMissingTests 29 项、OutfitTests、LayoutSkinTests、SettingsTests 通过；中英文 WARP 离屏预览逐字节一致且视觉检查通过。旧 LayoutSkinTests 固定 11 项断言在未改 HEAD 也失败，已修正为检查目标设置存在。
- 报告 build/outfit-missing/REPORT.md。没有接管键鼠，未验证 TNG／灯笼实际脚本交互。

### 1.1c 0.11.0 界面整理与发布准备（2026-11-15，未提交）

- 用户指出跳过缺失物品按钮越过内容区分隔线，设置外观页拥挤。此前只检查控件互不重叠，漏掉内容区边界；已补充内容区／底部操作区约束。
- 编辑页五行仍保留，单行 40 单位、列表高 200；移除／添加按钮、缺失开关全部位于分隔线以上，保存／取消回到底部操作区。绘制与鼠标行命中共用 rowHeight。
- 设置改为外观、图标、键盘、手柄、游戏行为五页。外观保留 11 项，I4 三项移至图标页；行距、按钮高度、鼠标页签命中、手柄循环顺序与八种语言同步。
- 版本号调整至 0.11.0；docs/RELEASE_0.11.0.md 为本地发布准备记录。英文回复草稿 release-materials/0.11.0/tng-reply-en.txt，未发送。
- 用户当前无法做本地实机测试，TNG／灯笼交由反馈者单独测试，不能将离屏 UI 验证或本次开发标记成兼容性实机通过。
- 2026-11-15 用户预览反馈后续调整：图标页签紧邻外观，手柄顺序同步；编辑页说明 y=218、分隔线 y=236、保存／取消 y=254..290、底部帮助 y=308。最终预览 build/ui-011/spacing/edit.jpg 与 settings.jpg。
- 验证记录：build/ui-011/REPORT.md。本轮不自动提交、推送、上传或正式发布。

### 1.1 SGT/MISC 乐器修复（`609f066`，已验收）

- 代码：收藏的 MISC 物品即使 `usable=false`，也走原生 `EquipObject(player, obj, extra, 1, nullptr, false, false, true, false)`（`Item.miscFallback` + `UseFavorite` 回退块 + `Wheel.cpp` 选择时不拦截）；回退路径与原 `TalentInstrument` 路径在装备前，若武器已拔出则先 `DrawWeaponMagicHands(false)`。
- 2026-11-11 用户游戏内验收通过（用户提供的原测试单）：
  - A 组（Bug 1）：武器收起时，轮盘“其他”中点鼓/笛/鲁特琴，有动画和音效；拔出武器后点同一乐器，先收刀再正常演奏（修复前只有音效）。日志 `SGT instrument native use requested: form=000DABA7/A9/AB...`。
  - B 组（Bug 2）：用 Favorite Things 收藏的非原版 5 个 FormID 的 SGT 乐器，在轮盘点击不再提示“此类物品暂需在背包中使用”。日志 `MISC fallback equip: form=... extra=...`；带 SGT 脚本的会触发演奏。
  - C 组（回归）：原版 5 个 SGT 乐器（0xDABA7/A9/AB/105177/105109）仍走 `SGT instrument native use requested`，不走 fallback；未收藏的普通杂物仍提示 unsupported。
  - 主会话核对 13:27–13:29 的游戏日志：有 `000DABA9`、`00105177` 的 `SGT instrument native use requested`，与 A/C 组一致；该份日志没有 `MISC fallback equip`，B 组依据用户口头确认。

## 2. 重要工作约定

- 先阅读项目 `AGENTS.md` 及会话提供的全局代理约定。
- 后续构建继续使用 xmake `after_build` 自动部署到 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
- 保留用户主配置及自定义资源，不能以源码示例 `FavoriteWheel.ini` 覆盖游戏配置。临时隔离部署结束后恢复既定部署目录。
- 每次部署后核对构建 DLL 与目标 DLL 的 SHA256。部署成功不代表游戏验证通过。
- 不抢键鼠、不切游戏焦点。需要接管前说明目的、窗口及范围并获得明确同意；优先静态检查、后台测试和离屏渲染。
- 分工约定（2026-11-13 用户更新）：开发、审阅、修复和测试全部由主会话负责，不再委派其他智能体。不要自动上传、提交、推送或修改其他模组。
- **历史委派限制（2026-11-13 起不再委派）**：凡是涉及 SwfRaster “异常输入 / 越界 / 资源耗尽 / 加固回归”的任务，Coder 和 Tester 都会拒绝（child_rejected），拒绝后该会话被锁定。加固用例已拆到 `SwfRasterHardeningTests`；普通回归只跑 `SwfRasterTests`，可以委派（2026-11-10 已验证 Tester 接受）。不要换措辞绕过；这类工作由主会话完成，或交给用户决定。
- 控制日志输出；读图片前生成长边不超过 1280 像素、目标不超过 300 KB 的预览，按具体检查目标逐张查看。
- **工作流程**（按 2026-11-13 分工更新）：主会话实现与自审 → 主会话回归与审阅 → 主会话修复 → **干净 worktree 复验** → 获授权后单独提交 → 更新本文件。各轮报告放 `build/i4-phase3/<任务>/REPORT.md`，不入库。
- **干净 worktree 复验做法**：
  - `git worktree add --detach ../fw-verify-x HEAD`，复制要提交的文件进去。
  - 用 PowerShell `New-Item -ItemType Junction` 把 `extern` 联接到主仓库；联接要用 `cmd //c rmdir` 删除，**不要用 rm -rf**。
  - 从主仓库复制 `build/.gens/commonlibsse-ng/windows/x64/release/minhook-v1.3.4`，避免 CommonLib 联网下载 minhook。GitHub 连接曾被重置；该目录自带 sha256 校验标记。
  - 配置：`xmake f -y -p windows -a x64 -m release --deploy_dir=`，关闭部署；不加 `-p windows` 会误选 mingw。
  - 首次构建会完整编译 CommonLib，约 6 分钟，建议放后台运行。
  - 提交后在主仓库执行 `git update-ref refs/heads/feat/outfit-parts-p1 <新> <旧>`，再执行 `git reset`（mixed，不改工作区文件）同步索引，然后 `git worktree remove --force`。
- **部署约定补充**：只改代码时可设 `FAVORITEWHEEL_DEPLOY_DLL_ONLY=1`，只部署 DLL（默认关闭，用户确认保留）。新增翻译键时必须完整部署，并在部署前后对比部署目录全部 21 个 INI 的 SHA256：只允许语言文件追加键，用户主配置不得变化。
- WheelPreview 截图回归：`WheelPreview.exe <out.bmp> 1280 800 <mode> en classic`。
  - `schools` 稳定：`0e0d15982e18eff904983e25a855fae56a3bdc430ef0bce40386f18f29d27753`，从 I4 之前一直不变。
  - `normal` **不能当作严格的字节基线**：WheelPreview 会预热全部语言，字体图集的排布随语言文件中的中文字形集合变化。实测：I4 之前和 T-4b（`58a0cd3`）都是 `db6a9fa6…1dd60c`，T-5 的语言文件下是 `92983a4a…6e66e`（差 11 像素，在底部文字）；在 T-4b 树上把语言文件换回 T-5 版本，哈希又回到 `92983a4a…`。只要新增翻译含新汉字，就可能变。验证“I4 关闭时渲染不变”请用 `schools`，或在语言文件相同的前提下对比 `normal`。

## 2A. I4 图标库接入（进行中：第一、二期与第三期 T-4/T-5/T-4b 已提交，T-6 已实现未提交，剩完整实机验收）

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
- **磁盘位图缓存**：放在游戏根目录 `Data/SKSE/Plugins/FavoriteWheel/IconCache/`，**不放** `My Games`（避免多个 MO2 实例共享）。缓存键为 SWF 内容 SHA-256 + 标签 + 变体 + 边长 + 转换器版本。已在 T-4b 实现（见 2A.3e）。
- 分三期实施：第一期 SWF 栅格化 → 第二期规则引擎与字段推导 → 第三期接入游戏（T-4）、设置（T-5）、磁盘缓存（T-4b）、预览与文档（T-6）。
- 2026-11-11：`tests/WheelPreview.cpp` 的 i4-* 场景留到 T-6；乐器修复与 T-5 同轮、独立提交；示例 `FavoriteWheel.ini` 补齐全部 7 个 I4 键；`FAVORITEWHEEL_DEPLOY_DLL_ONLY` 保留。

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
- AC-8：`schools` 截图字节一致；`normal` 截图 11 像素差异（(903,645)–(964,651) 底部文字），原因是新增中文“图”字改变 WheelPreview 预热全部语言后的字体图集；隔离副本恢复旧中文语言文件后哈希回到基线。主会话决定接受，不改 WheelFonts。T-4b 新增的语言键使 `normal` 恢复为 `db6a9fa6…`，说明这一差异只来自语言字形集合（见第 2 节“WheelPreview 截图回归”）。
- 复验：Coder 报告 `build/i4-phase3/t5/REPORT.md`、`t5-logfix/REPORT.md`；Tester 回归全部通过（其 REPORT.md 未写出，结论由主会话从 `build/i4-phase3/tester-t5/` 下日志与 JSON 核对）；干净 worktree（基于 `609f066`）全部测试通过后提交。
- 2026-11-11 用户实机（T-4 DLL，T-5 部署前）：法术、药水显示 I4 图标，Theme 配色正常。`I4 load: files=31 rules=671 rejected=0 elapsed_ms=98.218`；12 次快照 42–44 项，`collect_derive_resolve_ms` 0.256–0.626（满足 AC-7 的 100 项 < 1 ms；尚无 500 项全背包实测）。SWF 诊断 3 条（i4iconaddon、magus/gourmet、skyui psychosteve：sprite End 容错、不支持 tag 9/69），对应图标正常。
- 2026-11-12 用户确认 T-5 设置面板及各开关操作正常；日志聚合也已核验，详见 2A.4。

### 2A.3e 第三期 T-4b（磁盘位图缓存）：已提交 `58a0cd3`

- 新文件：`include/IconCache.h`、`src/IconCache.cpp`（不依赖 CommonLib，可离线测试）、`tests/IconCacheTests.cpp`。修改 I4Textures（工作线程先查缓存，未命中再 Render 并写入）、I4Game（缓存目录）、I4Runtime、Settings、FrameworkSettings、8 种语言、示例 INI、docs。
- 位置：`<SkyrimSE.exe 目录>/Data/SKSE/Plugins/FavoriteWheel/IconCache/<sha256>.fwic`（用 `GetModuleFileNameW` 定位）。2026-11-12 用户实机确认本机 MO2 写进 `H:/Games/Dev Skyrim/overwrite/SKSE/Plugins/FavoriteWheel/IconCache/`，主会话已只读核实文件存在。
- 格式：魔数 `FWIC`，含格式版本、转换器版本（`IconCache.h` 中常量为 1，**修改 SwfRaster 栅格化逻辑时必须递增**），条目表包括标签、变体、边长、偏移、长度、CRC32；像素用 miniz 压缩。读取时把文件当作不可信输入完整校验，任何一项不合格就丢弃整个文件、只告警一次。SHA-256 用 BCrypt 计算。
- 写入：同目录写 `.tmp`，执行 `FlushFileBuffers` 后用 `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` 替换。**批量落盘**：每 32 条或 4 MiB 写一次，切换 SWF 或队列空闲时也会写出。退出游戏时尚未写出的条目丢弃，析构和卸载时不做 IO。50 标签 × 2 变体的合成 SWF 实测：首次写入 100 → 4 次，耗时 220 → 53 ms；第二次全部命中。
- 容量：按 mtime 做 LRU，在开启缓存或修改容量时 Trim；之后每成功写入 8 次，或累计写入达到容量的 1/4（最多 16 MiB）时再 Trim。两次 Trim 之间允许短暂超出预算。**mtime 更新失败、Trim 失败只告警一次，不停用缓存**（兼顾 MO2）。内容读写失败或目录不可写时，退化为只在内存中缓存。
- 设置：主 INI `[Display]` 下 `I4IconCache=1`、`I4IconCacheMB=64`（8–512）；为 0 时完全不读写磁盘。“清空图标缓存”按钮、开关、容量只放在 SKSE Menu Framework 外观页；轮盘内外观页已有 14 行，不再加。8 种语言各新增 6 键。
- 线程：哈希计算、缓存读写、Render 都在唯一的工作线程上执行；不持有 `state.mutex` 时才做 IO；游戏线程和渲染线程不会等待缓存。
- 复验：Coder 报告 `build/i4-phase3/t4b/`、`t4b-fix1/`（批量落盘、示例 INI）、`t4b-fix2/`（mtime/Trim 降级、文档）；Tester `build/i4-phase3/tester-t4b/REPORT.md`（PASS，确认问题 C1 与疑点 Q1 已由 fix2 修复）；干净 worktree 全部测试通过后提交。
- 构建和测试从未在游戏目录创建 `IconCache/`。实机首次运行时才会生成。

### 2A.4 待办（按顺序）

1. **T-5 与 T-4b 三项实机检查已完成（2026-11-12）**：
   - 用户确认设置页 I4 三项、缓存开关及清空按钮显示和操作正常，所有开关均尝试无问题。
   - 用户确认 MO2 实际落盘在 `H:/Games/Dev Skyrim/overwrite/SKSE/Plugins/FavoriteWheel/IconCache/`；主会话只读检查有 3 个 `.fwic`，共 191722 字节（检查时快照，清空/使用后会变化）。
   - 日志：`D:/MyFiles/Documents/My Games/Skyrim Special Edition/SKSE/FavoriteWheel.log`，修改时间 2026-11-12 14:59:35，共 206 行。缺失插件告警 62 行，加 `1285 references to 62 missing plugins` 信息汇总 1 行，符合“约 63 行”预期；加载 31 个文件、671 条规则、拒绝 0 条，耗时 179.587 ms。
   - 全日志共 65 条 `[W]`：上述 62 条，加 3 个 SWF 各一条汇总诊断（`i4iconaddon/icons.swf`、`magus/gourmet/icons.swf`、`skyui/icons_item_psychosteve.swf`）。`[E]`/`[C]` 为 0。不要把“63 行”写成全日志有 63 条警告。
   - 构建与部署 DLL SHA256 均为第 1 节记录的 `28EF5462…4513A9F`，未重新构建或部署。
   - Tester（`assistant_id=3`）已独立只读核验日志统计、缓存目录、DLL 哈希及 git/交接一致性，四项通过；未运行测试程序或重做实机测试，结果由委派会话返回，未另生成报告文件。
   - 证据边界：本次三项通过不代表完整 AC-10 已通过；`I4Debug=1` 逐物品日志及第二次启动磁盘缓存精确命中/Render 次数没有此次专项证据；当前游戏日志未输出 Stats 计数，I4Debug 也不提供该计数，需专门诊断支持，不以图标正常或文件存在代替。
2. **T-6（2026-11-13 已实现，未提交）**：WheelPreview I4 场景改用合成 SWF 夹具，走真实的 I4 渲染路径，不依赖第三方美术；然后处理工作区里 `tests/WheelPreview.cpp` 的 i4-* 场景（改造或替换后提交）。同时更新文档：`I4_ICONS.md`、`ICON_LIBRARY.md`（`[I4]` 段、`I4Icons=Off`）、README、方案 §8。
3. **用户完整 AC-10 验收**：当前清单在 `docs/I4_GAME_ACCEPTANCE.md`；取代旧 Tester 报告中的草稿，跳过已验收项目。重点包括：
   - 与背包图标对比（BOOBIES 覆盖的物品和法术）；
   - 首次打开时从回退图标切换到 I4 图标；
   - 切换窗口或设备重建后的恢复；
   - 全背包模式 500 项的快照耗时（AC-7）；
   - 与 **I4 Improved**（hook 了 `FavoritesMenu::ProcessMessage` 和 SkyUI `processList`）的共存。
4. 用户审阅 SwfRaster 加固部分（`tests/SwfRasterHardeningTests.cpp`、`build/i4-phase1/round4/REPORT.md`）。**不要委派**给 Coder 或 Tester。
5. 决定版本号与发布策略（0.11.0？），更新 changelog 和发布材料。

已知遗留（不在当前计划内，需用户决定是否立项）：
- 渲染器一旦异常就永久 failed，没有设备恢复入口（T-4 Tester 报告 C3，T-4 之前就存在）。
- MergeMapper 重映射未实现（本机未安装 MergeMapper）。
- `src/I4RuleTables.inc` 的生成脚本只在 `build/i4-phase2/generate-tables.py`，未入库。
- SwfRaster 已知限制见 2A.3。
- 本机 I4 规则引用了 62 个未安装的插件，这些引用永远不会匹配，属正常现象。

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
- **I4 相关回归集**（离线目标都是 `set_default(false)`，需显式 `xmake build -j 2 <目标>`）：IconCacheTests、I4IntegrationTests、I4RulesTests、I4EntryTests、SwfRasterTests、IconLibraryTests、SettingsTests、SettingsPersistenceTests、WheelLogicTests、ImageSamplerTests、SkinTextureTests，加上 `xmake lua scripts/test_language_migrations.lua`。当前检查数：IconCache 292、I4Integration 820、I4Rules 1178、I4Entry 1029、SwfRaster 172999。I4RulesTests 含壁钟计时断言，负载高时可能偶发失败，应单独重跑，不改阈值。`SwfRasterHardeningTests`（5028）只由主会话运行。
- AC-5 规则对照：`build/windows/x64/release/I4RuleAudit.exe "H:/Games/Dev Skyrim/mods" "H:/Games/Dev Skyrim/profiles/Basic - OStim" "D:/MyFiles/Documents/My Games/Skyrim Special Edition/SKSE/InventoryInjector.log" <输出目录> "H:/SteamLibrary/steamapps/common/Skyrim Special Edition/Skyrim.ccc"`，预期 31 文件、671 规则、0 差异。
- 打包脚本为 `scripts/package.ps1`，从 xmake 获取版本，可能覆盖同版本归档；只有实际需要重新打包时运行，先明确版本与保留策略。
- 离屏预览程序 `WheelPreview.exe <out.bmp> <宽> <高> <模式> <语言> <主题>`，可用 `normal`、`schools`、`drag-sort`、`drag-sort-static`、`drag-drop`、`drag-settled`、`numeric-sort`、`sorting`、`gameplay` 等模式；图片预处理后再读。
- Python：`C:\Users\BlackMesa\AppData\Local\Programs\Python\Python313\python.exe`，读写显式 UTF-8；本机已有 Pillow 和 NumPy。Node：`C:\Program Files\nodejs\node.exe`。
- 游戏日志：`D:\MyFiles\Documents\My Games\Skyrim Special Edition\SKSE\FavoriteWheel.log`；I4 自身日志在同目录 `InventoryInjector.log`；MO2 实例 `H:/Games/Dev Skyrim`，配置 `profiles/Basic - OStim`，游戏本体 `H:/SteamLibrary/steamapps/common/Skyrim Special Edition`。
- 接手时先看 `git status --short`、本文件及相关专题文档，再围绕用户新请求做最小必要改动。不要重做已完成发布流程，也不要把历史未实现想法自动当成新授权。

2026-11-13 用户要求部署：xmake FavoriteWheel -j 2 构建成功，after_build DLL-only 自动部署。构建/部署 SHA256 均为 A1B441C36B02AE989C31A12262F1BEAF33361E0F91EF54530F7279813C419C01；21 个 INI 哈希未变。未接管键鼠，实机验收交由用户。
