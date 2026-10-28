# 代码审查（2026-10-24）

## 范围与结论

以当前工作区（包括未提交和未跟踪的源码）为准，不以旧发布说明作为实现依据。检查了初始化、输入、任务生命周期、库存与装备、预设与序列化、配置、字体、绘制、资源加载及构建发布路径。本次只增加审查报告，没有修改业务代码、编译插件 DLL、部署或操作游戏窗口。

整体结构可继续演进，暂不建议重写。已有的值快照、执行前实例重验、会话世代检查、独立暂停所有权、分步装备、序列化限额与纯策略测试值得保留。优先处理发布产物一致性和稳定帧重复工作，再拆分输入与 UI 控制器。

以下性能判断来自调用路径和复杂度检查，没有测量游戏内 CPU/GPU 帧时间，不表示已确认某个玩家掉帧的原因。

## 优先处理的发现

### 1. [P1] 打包可以把旧 DLL 标记为新版本

- 位置：`scripts/package.ps1:5–17`、`xmake.lua:2`、`tests/RuntimeLayoutTests.cpp:115–121`。
- 脚本从 xmake.lua 读取包版本，只检查固定路径的 DLL 是否存在，随后直接复制。后面的 ZIP 哈希校验只能证明复制一致，不能证明 DLL 来自当前源码。
- 本次实际证据：源码版本 0.7.3；`build/windows/x64/release/FavoriteWheel.dll` 的文件版本是 0.7.2.0；RuntimeLayoutTests 在 `exported plugin version` 校验失败。没有运行打包脚本，也没有据此断言已发布包存在问题。
- 影响：升级版本或修改源码后忘记构建，仍能生成名称为新版本、内容为旧实现的安装包；源码包和二进制包也可能不对应。
- 建议：打包前验证 DLL 导出版本和文件版本；构建时生成含源码摘要、依赖版本、配置及 DLL 哈希的清单，打包时重新验证。仅比较版本仍不能发现同版本的旧构建。将构建、验证、打包、部署拆成明确阶段，自动部署继续作为显式开发选项。

### 2. [P2] 轮盘关闭后输入循环仍复制完整快照

- 位置：`src/Wheel.cpp:522–529`、`1256–1261`、`1361`。
- InputHook 尾部无条件调用 Snapshot，然后调用 PumpDetails；PumpDetails 也先 Snapshot 才判断是否打开。Snapshot 复制 items、物品详情、名称编辑状态、inventoryGlyphs 和配置字符串，并取得互斥锁。
- Cancel 关闭界面时不清空 items 和 inventoryGlyphs。因此至少打开过一次后，关闭期间仍每次输入派发复制这些缓存。
- 建议：先读取 open/模式等轻量状态，确认需要后再复制；一次输入周期复用同一快照。进一步把不常变化的目录与字形内容作为不可变共享数据，指针/选择状态单独传递。
- 验证：分别统计未打开、打开后关闭、打开三种状态的快照次数、分配次数和耗时，确保关闭期间详情路径不复制数据。

### 3. [P2] 字体稳定帧重复遍历全部译文

- 位置：`src/WheelFonts.cpp:83–98`、`src/UIResources.cpp:382–390`。
- PrepareFonts 每个可见帧调用 UIGlyphs；后者重新遍历 english 全部键，通过 Tr 解析语言并拼接所有界面文本，同时构造语言/主题名称和按键标签，然后再次扫描 UTF-8 加入字形集合。
- 虽然 atlas 不会每帧重建，重建前的准备工作仍每帧执行。catalog 已有缓存，UI 文本没有同等缓存。
- 建议：按已解析语言、资源版本和相关键位缓存静态 UI 字形；只在配置变化、物品详情变化或名称编辑变化时处理新增字形。保留当前按实际像素字号烘焙和缺字回退机制。
- 验证：在稳定帧确认不再执行整份译文遍历；切语言、改快捷键和输入新字符仍能正确更新。

### 4. [P2] 资源及预设导入导出仍依赖进程工作目录

- 位置：`include/UIResources.h:27`、`src/UIResources.cpp:291`、`src/Outfits.cpp:19`、`232–253`。
- 主 INI 已锚定游戏可执行文件目录，但语言/主题默认根目录和 Outfits 根目录还是相对路径。若启动环境或其他组件改变 CWD，同一个插件可能从一个位置读取设置，却从另一位置寻找主题或写入导出文件。
- 这是可由路径语义确认的条件性缺陷；本次未认定用户当前游戏环境已经触发。
- 建议：统一提供以游戏可执行文件目录为基准的绝对 Data 路径；测试通过参数注入独立根目录。继续保留 VFS 下的写入策略，不能简单替换成可能破坏映射的 rename 流程。

### 5. [P2] 预设文件 I/O 在输入派发内同步执行，且缺少总读取预算

- 位置：`src/Wheel.cpp:439`、`907`、`src/Outfits.cpp:238–275`。
- 点击导入/导出直接进行目录遍历、文件读写和解析。导入有单文件 1 MiB、预设数量 100 的限制，但没有文件数量、总字节数或时间预算；例如大量合法的零预设文件不会触发 added 的数量限制。
- 影响：大量导入文件、慢磁盘或模组管理器文件系统可延长输入派发，引起卡顿。这里不是库存装备线程问题，主要是同步文件 I/O 占用了交互路径。
- 建议：设置总文件数/总字节数限额；文件读取及纯文本解析在后台完成，携带会话世代和请求编号，再在游戏执行路径验证库存并提交。导出先复制值模型，再后台写文件。取消/读档后不得提交旧结果。

### 6. [P2] 长文本截断采用逐字符反复测量

- 位置：`src/Draw.cpp:86–110`。
- Fit 每移除一个 UTF-8 字符就重新测量整个剩余字符串，且构造 `text + "..."`；Wrapped 也重复测量。最坏情况是二次复杂度，并且由逐帧绘制重复触发。
- 影响集中在异常长物品名、效果说明和较窄布局；普通短文本不应预设为明显性能瓶颈。
- 建议：使用字体测量提供的截断位置或单次累加字形宽度；以文本、字体 atlas 版本、字号、可用宽度和行数缓存结果。确保 UTF-8 边界完整。

## 结构与维护建议

| 模块 | 当前职责 | 建议 |
| --- | --- | --- |
| main / RuntimeSupport | SKSE 初始化、版本准入、生命周期 | 保留明确版本白名单；版本信息从构建配置生成，减少多处手写 |
| Wheel.cpp（1444 行） | 输入、页面状态、设置、文本编辑、生命周期、调度、菜单 | 优先分离 InputRouter、ViewController、ActionScheduler、SettingsController；先搬运职责并明确锁顺序，再改行为 |
| Favorites / Outfits / CombatPresets | 实例识别、库存扫描、装备执行 | 抽取共享实例签名描述和库存预算逻辑；避免三个实现持续分叉，保留各自装备语义 |
| Renderer / Draw / WheelFonts | D3D11、值快照绘制、字体管理 | 优先削减重复 CPU 工作，再用游戏测量决定是否缓存几何；不要凭静态审查改渲染目标或强加限帧 |
| Settings / SettingsIni | 事务式预览、编码兼容、VFS 持久化 | 保存的 FlushFileBuffers 与读回验证持有 settingsMutex，慢存储可能阻塞渲染侧 Config；可用独立保存锁和修订号缩短状态锁范围 |
| tests / xmake | 独立可执行测试 | 增加统一测试入口和失败汇总，区分纯策略、产物元数据、VFS、离屏和游戏实测；产物测试显式接收 DLL 路径 |
| docs / HANDOFF | 大量版本历史和当前说明混合 | 单独维护短的“当前架构/当前状态”，历史记录移入版本日志；ARCHITECTURE 当前仍夹杂旧版本结论 |

## 需要进一步验证的边界（不列为已确认缺陷）

1. Favorites 的法术收手通过 VM DispatchMethodCall 异步执行，而 keepPaused 路径在提交后立即认为完成并刷新目录。需要在游戏内验证暂停时 VM 完成时机、回调后的装备标记刷新，以及连续点击/读档边界。当前 HandEquipTests 主要验证调用决策，并未模拟真实 VM 调度。战斗预设已有 callbackPending 等待，可作为统一动作完成模型的参考。
2. Renderer 初始化创建 ImGui context 后没有立即显式 SetCurrentContext(imgui)。本地 ImGui CreateContext 在已有 current context 时会恢复旧 context，初始化函数对此前提不够稳健。当前独立静态链接通常使首次上下文为空，因此不据此声称普通其他 ImGui 模组必然产生冲突；可用 RAII context guard 加固并测试已有上下文情形。
3. 配置保存采用原位写入和内存回滚来保留 MO2/Vortex 映射；可处理返回错误，不能保证写入中途进程终止后的恢复。若要提高恢复能力，增加持久备份/恢复协议，而不是未经 VFS 验证直接改为替换目标文件。
4. 本次未建立输入派发、SKSE task、PlayerUpdate、序列化回调在真实运行时的线程时序，因此不把单凭多个 mutex 和非原子状态推测出的竞争写成确定死锁或崩溃。

## 本次验证

使用现有 release 配置，顺序构建 17 个独立测试目标，每次编译并发限制为 2；未构建 FavoriteWheel 目标，未触发 after_build 部署。

通过 16 个：WheelLogicTests、GameSessionTests、HandEquipTests、LayoutSkinTests、CombatCycleTests、CombatPresetTests、OutfitTests、NameEditorTests、FaceLightClientTests、ItemInfoTests、InventoryTests、TimeTests、SettingsTests、SettingsPersistenceTests、ActorRuntimeTests、SectorMeshTests。

RuntimeLayoutTests：七个运行时的 mock 布局检查通过，最后现有 DLL 导出版本检查失败（源码期待 0.7.3，现有 DLL 为 0.7.2）。这是产物不一致，不能表述为运行时布局全部失败，也不能忽略成全部测试通过。未向该测试提供真实游戏 Address Library 路径。

本次未运行 SettingsVfsTests、SkinTextureTests、WheelPreview，未进行离屏视觉验收、GPU 性能测量或游戏内联测。已有策略测试不能代替 SKSE hook、异步装备或第三方模组兼容的游戏验证。

建议实施顺序：发布一致性门槛 → 关闭状态快照及 UI 字形缓存 → 路径统一与导入预算 → 长文本布局缓存 → 控制器拆分和异步完成状态统一。
