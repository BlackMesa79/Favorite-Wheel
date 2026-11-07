# FavoriteWheel 当前项目交接

更新日期：2026-11-07（Asia/Hong_Kong）。当前版本状态见第 1 节；第 3–7 节及旧 `HANDOFF.md` 保留历史交接记录，`docs/RELEASE_0.9.0.md` 仅描述 0.9.0 发布。不要把历史待测事项或旧方案误认为当前实现。

## 1. 当前状态

- 项目路径：`D:\MyFiles\Documents\ChatGPT\SKSE模组 收藏轮盘`，Windows / PowerShell / C++ / xmake / SKSE。
- 当前版本 **0.10.0**，插件元数据 **0.10.0.0**；尚未打包发布，未生成本版本 changelog 或发布材料。
- 0.9.0 发布包已在本地完成；main 已合入（PR #2）该版本之后的新功能，本次仅升级版本号。0.9.0 的发布文件及历史记录保持原样。
- 版本号升级通过 `chore/version-0.10.0` 分支提交并以 PR 合并到 main；未打包、未生成发布材料。构建后按约定自动部署并校验 DLL 哈希。

## 2. 重要工作约定

- 先阅读项目 `AGENTS.md` 及会话提供的全局代理约定。
- 后续构建继续使用 xmake `after_build` 自动部署到 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
- 保留用户主配置及自定义资源，不能以源码示例 `FavoriteWheel.ini` 覆盖游戏配置。临时隔离部署结束后恢复既定部署目录。
- 每次部署后核对构建 DLL 与目标 DLL 的 SHA256。部署成功不代表游戏验证通过。
- 不抢键鼠、不切游戏焦点。需要接管前说明目的、窗口及范围并获得明确同意；优先静态检查、后台测试和离屏渲染。
- 未获用户或适用指令明确要求，不启动子代理。不要自动上传、提交、推送或修改其他模组。
- 控制日志输出；读图片前生成长边不超过 1280 像素、目标不超过 300 KB 的预览，按具体检查目标逐张查看。

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

## 4. 0.9.0 当前实现

### 排序与拖拽

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
