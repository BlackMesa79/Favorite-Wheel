# Outfit Wheeler 参考评估与装备预设扩展路线

日期：2026-11-06。本文只做评估，未修改源码、未构建、未部署。

## 分析对象

- 安装包：`G:/Downloads/IDM/Compressed/Outfit Wheeler Dynamic Equipment Manager 187557 4.2 2026-10-29T15-15Z 4t2yDc4UJ.zip`（同目录还有 1.3–3.1 旧版）。
- 解压到独立临时目录后只做静态阅读：PrismaUI 前端（`index.html`、`app.js`、`modals.js`、`wheel.js`）、`OutfitWheeler.ini`、DLL 字符串及 RTTI 符号。未运行其中任何内容。
- 组成：`OutfitWheeler-DynamicEquipmentManager.dll`、699 字节 ESP（大概率只放衣柜容器）、PrismaUI 网页界面（依赖 PrismaUI）。DLL 闭源，所以只参考功能设计，不复制代码、图标或界面资源。

## Outfit Wheeler 功能清单（静态核实）

| 功能 | 实现线索 | 说明 |
| --- | --- | --- |
| 多轮盘 × 多槽位 | AddSlot/MoveSlot/MoveWheel/RenameWheel、Sort A-Z | 支持按轮盘分组、重命名、调整顺序 |
| 槽位外观 | SetSlotColor/SetWheelColor | 可以给每个预设或轮盘设颜色 |
| 整套 / 叠加两种类型 | SetSlotTypeListener，界面 "Full Outfit / Addon / Overlay"，日志 "Equipped Addon" | 叠加型只穿上自己的部件，不脱其他衣物 |
| 预设内容编辑 | AddItemToSlot/RemoveItemFromSlot、"+ Add Item"、"Save Gear" | 可以直接从背包往预设里加减单件，不只是整套捕获 |
| 附魔件回退 | "Custom enchanted item … not found with extra data, equipping base instance" | 找不到精确的附魔实例时，退而装备基础物品 |
| Studio 编辑模式 | Orbit/Pan/ZoomCamera、SetStudioPose、poserdata 姿势包、原版 Idle | 用环绕相机看真实玩家，并可摆姿势。**它是真实穿上装备，不是只显示** |
| 物品 3D 缩略图 | ItemThumbnailService/ItemPreviewRenderer，自带 D3D11 着色器，PNG 缓存到 `PrismaUI/views/.../thumbnails` | 后台把装备模型离屏渲染成图标 |
| 条件自动换装 | TESCombatEvent、ActorLocationChange、ActorCell 事件；动作 5 类、地点 13 类（关键词匹配）、天气/时间 5 类；0–5 星评分、"Weighted Selection" | 环境变化时，在满足条件的预设中按星级加权随机挑选；0 星表示排除 |
| 衣柜箱 | "Wardrobe Chest"、Stash/Retrieve、bAutoStashInactive | 把不用的预设物品放进隐藏容器以减轻负重，换装时再取回 |
| 随从换装 | SelectTargetActor、followerInventory、"Follower Teammate Default Outfit Suppression" | 可以管理随从的预设，并压制随从的默认服装 |
| 模组物品目录 / 生成物品 | RequestModCatalogItems、SpawnModItem | 按插件浏览装备并直接生成物品（作弊性质） |
| 全局存储与备份 | `Data/SKSE/Plugins/OutfitWheelerData.json(.bak)`、`backups/`、Export/Import/Restore | 数据不随存档，存为全局 JSON，并提供快照恢复 |
| 主题 | 取色器、预设色、表面深度 | 我们已有主题系统 |

## 是否值得引入

### 建议引入（按优先级）

1. **预设部件自定义（对应“部件自定义配置”）**——价值最高，风险最低，基本是纯逻辑，便于测试。
   - 预设模式分三种：整套（现有行为）、叠加（只穿不脱，比 OW 的 Addon 更明确）、自定义槽位（只清理并接管勾选的 biped 槽位，其余槽位保持现状）。
   - 部件级标记：必需 / 可选（缺失时跳过，不再让整套失败）/ 本次忽略。
   - 预设编辑器：从背包加减单件、替换某个部件，不必每次重新捕获整套。
   - 宽松匹配选项：找不到精确实例时回退到同 base（以及同附魔）的物品，并在界面明确标注“已回退”。默认保持现在的严格匹配。
   - 每个预设可设图标或颜色，接入现有 IconLibrary。
   - 需要把序列化升级到 `FWO 2` / 新 cosave 版本，并且仍能读取 v1。
2. **跨存档预设库与备份（对应“云存储”）**
   - 现状：预设存在角色 cosave 里，`.fwo` 只能手动导入导出。OW 的做法是全局 JSON 加自动备份。
   - 建议：增加一个全局预设库，各角色共享，可以一键“导入到当前存档”或“从当前存档发布到库”。写入时保留 `.bak` 和带时间戳的快照。
   - 存放位置建议用 `Documents/My Games/Skyrim Special Edition/SKSE/FavoriteWheel/`，不放进 MO2 的 Data/overwrite。用户把这个目录交给 OneDrive 等同步盘，就能跨电脑同步。**不建议自建在线云服务**（涉及账号、隐私、网络和 Nexus 规范）。Steam 云也不会同步这个目录。
   - 动态 FormID 和动态附魔的物品不能移植，沿用现有 `Portable()` 规则并提示用户。
3. **装备预览（沿用 `docs/REALTIME_OUTFIT_PREVIEW.md` 的路线）**
   - OW 的 Studio 是真实换装加环绕相机，会触发附魔效果、脚本 OnEquip 和负重变化，不符合你之前定下的“只显示、不实际装备”目标，不建议照搬它的核心机制。
   - 值得借鉴的是交互：悬浮或进入预览后，用鼠标拖动旋转、滚轮缩放、右键平移，加上“重置视角”。
   - 姿势或 Idle 优先级低：本轮盘暂停时动画不更新，而且依赖 poserdata 生态，放到最后再评估。
   - 这是风险最高的一项：需要 biped hook、两个运行时分别核对地址、处理暂停时的模型刷新，以及与 ShowPlayerInInventory、MenuStudio、FittingRoom 的兼容。必须先过原文档的研究门槛，再进入正式开发，并且需要你在游戏里实测。
4. **条件自动换装（可选，放在后期）**
   - 价值中高，玩家呼声通常很大。首版建议默认关闭，只做少量明确的条件：战斗进入/退出、游泳、骑乘、室内/室外、城镇、自宅；天气和时间放到第二步。
   - 必须有防抖、冷却，菜单打开、对话、过场时不切换，玩家手动换装后在一段时间内不自动覆盖，并且只在非暂停的游戏线程里执行现有的分帧 Apply。
   - 星级加权随机可以做，但规则要能解释清楚（例如“同条件下优先级高者优先，同级随机”）。
   - 与 OW、Skyrim Outfit System 等自动换装模组同装时会互相抢着换装，需要在说明里写明。

### 暂不建议或不引入

- **物品 3D 缩略图**：需要自己写离屏 D3D 渲染器并处理 NIF 加载，工作量大。可以放到装备预览的渲染管线做成之后复用，现在不单独做。
- **衣柜箱**：要靠 ESP 或持久容器，物品离开背包后会影响我们按 UniqueID 的匹配，还有任务物品、偷来的物品、存档残留等边界问题。收益只是负重。可以作为远期的独立可选功能。
- **随从换装**：NPC 默认服装在读档或换 cell 后会被重置，需要压制机制，还要考虑随从框架兼容，范围大约相当于一个独立模组。远期再评估。
- **模组物品目录 / 生成物品**：属于作弊工具，偏离轮盘定位，不做。
- **主题取色器、Studio 网页界面**：我们已有主题和 ImGui 体系，不需要引入 PrismaUI。

## 建议分期

| 阶段 | 内容 | 主要风险 | 验证方式 |
| --- | --- | --- | --- |
| P1 | 部件自定义：三种模式、部件标记、预设编辑、宽松匹配、图标/颜色、FWO 2 | 序列化兼容、多槽位衣物的冲突判断 | 纯逻辑单测为主，再在游戏里换装验收 |
| P2 | 跨存档预设库与备份 | 文件 I/O 不能放在交互路径上（见 CODE_REVIEW_2026-10-24 第 4 条）、多存档并发写 | 单测加隔离目录测试 |
| P3 | 只显示的装备预览加环绕相机 | hook、两版运行时、暂停时刷新、外观模组兼容 | 研究原型 → 你在游戏里实测（两版） |
| P4 | 条件自动换装（可选） | 抢换装、频繁切换、与其他模组冲突 | 状态机单测加游戏实测 |

分工按惯例：Codex CLI 负责实现，DeepSeek Harness 负责测试（P1/P2 的单测和回归、P3/P4 的状态机测试）。游戏内验收只能由你完成。

## 开工前需要确认

1. “云存储”是否指上面说的跨存档、跨电脑同步（同步盘方案）？还是你确实需要在线服务？
2. 预览是否仍坚持“只显示、不实际装备”？如果可以接受“真实试穿，退出时恢复原装备”，P3 的难度会大幅降低，但会带来附魔、脚本、负重方面的副作用。
3. 当前工作区有大量未提交的改动（0.9.0 之后的图标库、SKSE Menu Framework 设置、SGT/乐器、SimpleIME 等），分支是 `wf/20261028-takeover-review`。建议开新功能前先做一次提交或检查点，并开新分支。另外，乐器多格、SGT 演奏、附魔弓等待测或待修事项是否先收尾？

## 2026-11-06 用户决定（覆盖上文对应建议）

- **取消条件自动换装**：P4 不做。
- **“Cloud Storage”的真实含义**：OW 前端源码里，`CLOUD` 就是 Wardrobe Chest 的内部来源 ID（`sources.push({ id: "CLOUD", type: "CLOUD", name: "Wardrobe Chest" })`，空箱提示为 "Wardrobe Chest is empty."）。实际做法是把物品移进 ESP 中的隐藏容器，界面上能看到、能取回，并且不计负重。容器属于游戏世界对象，内容保存在存档里，所以不跨存档，也和网络云无关。FavoriteWheel 不实现这个衣柜箱。
- **P2 定为跨存档预设库**：按上文方案实施（全局库文件、导入到当前存档、从当前存档发布到库、自动备份）。
- **P3 预览首版改为真实试穿**：悬浮或选中预设后，确认进入试穿：
  1. 先记录当前装备快照，再用现有分帧 Apply 真实装备预设；
  2. 提供环绕、缩放的查看方式（形式待方案阶段确定）；
  3. 取消或退出时，按快照恢复原装备；确认时保留试穿结果。
  - 需要处理的副作用：附魔效果、脚本 OnEquip/OnUnequip、负重和护甲值变化；试穿期间物品丢失、读档、战斗时如何中止；快照中的装备不在背包时如何报告。
  - 只显示、不实际装备的预览（REALTIME_OUTFIT_PREVIEW.md）留待以后扩展。
- **Git**：0.9.0 之后的累积改动已提交到 `wf/20261028-takeover-review`（`f68a8bc`、`a139a5f`）。装备预设扩展在 `feat/outfit-presets-v2` 上进行。`20261106-user-feedback-implementation-and-test-checklist.md` 中的游戏内测试项仍未完成，新功能开发不应改动那部分行为。
