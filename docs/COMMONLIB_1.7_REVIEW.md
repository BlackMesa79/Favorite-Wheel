# FavoriteWheel 0.3.12：CommonLib 1.7 适配与 Review

日期：2026-10-05（Asia/Hong_Kong）。本地适配、构建与 Review 完成；1.7.99 / 1.7.104 游戏内验证待反馈。
用户已确认旧依赖下0.3.11功能正常，本版仍需在1.6.1170进行依赖升级回归。

## 依赖和版本范围

采用与面部光照相同的上游 CommonLibSSE-NG v11.0.0，提交
`94faaed0c60eddd8347767f2d4d29a97c93bde8c`。已通过远端tag核对，直接从上游该提交ZIP取得源码。
2776个上游文件逐个核对SHA256一致；新增本地UPSTREAM_REVISION.txt记录来源，没有修改上游代码。
沿用历史目录extern/CommonLibVR，启用SE/AE，关闭VR，完整源码构建，不借用其他工程的预编译库。

运行时白名单由1.5.97.0 / 1.6.1170.0扩展至1.7.99.0 / 1.7.104.0。
不使用“所有1.7版本”范围匹配，未知版本仍拒绝。SKSE64与Address Library必须匹配游戏本体；
1.7版本使用包含该游戏版本的v5地址库。同一DLL用于四个版本。

上游许可为GPL-3.0-or-later及Modding / Linking Exceptions。安装包包含COPYING对应文本、
Exceptions及保留的MIT/HDE64声明，源码包包含完整依赖快照。
bootstrap支持校验固定上游ZIP下载；来源和ZIP SHA256见CommonLibSSE-NG-REVISION.txt。

## Review 结果

| 路径 | 检查结果 |
| --- | --- |
| 加载和元数据 | 精确四版本白名单；日志记录库版本和提交。实际DLL导出名称、0.3.12版本、SKSEPlugin_Load与Address Library v5/旧AE标志通过检查。 |
| 输入hook | 保留ID67315 / 68617 + 0x7B；新库将1.7分类为AE。四份本机地址库均解析出输入ID，安装前仍检查E8签名并保留前一个hook。地址解析不证明该偏移在新游戏二进制中的完整语义；需1.7实机输入测试。 |
| 玩家Update | 保留主虚表hook槽0xAD，匹配固定上游Actor.cpp。四版本原生形状虚表夹具对照上游Update派发通过；面光快照仍在原hook之后、暂停之前，执行仍在解除暂停之后。 |
| 玩家/角色布局 | 使用库版本访问器。1.7玩家数据起点变为0x3E8；ActorState仍为0xC0，Actor数据0xE8，ControlMap数据0xF0。四版本独立字节偏移夹具通过。 |
| 渲染 | 修复REX RTV类型与原生D3D11指针不兼容的编译问题；转换后赋给ComPtr，AddRef/Release语义保留，没有用Attach接管游戏所有权。继续使用engine UI framebuffer与原Present hook，不改变HUD层级或加入模糊。swapChain及framebuffer原生偏移夹具通过。 |
| 收藏/消耗品/物品详情 | 继续使用上游Inventory/ExtraData、EquipObject/UnequipObject/EquipSpell/EquipShout及属性计算接口。临时InventoryEntryData仅拥有列表节点，仍借用实例extra；未改用直接DrinkPotion或发送合成动画事件。关键引擎ID在四份地址库解析通过。 |
| 装备预设/存档 | 保留先卸后穿及再次卸装策略、分帧任务与实例重验，记录版本/FWO1格式未变，未改写用户存档。OutfitTests通过。 |
| 面光/Text Bridge | 公共接口表、线程/session/revision与输入桥接代码未变。FaceLightClientTests、NameEditorTests通过。联动插件自身也须能在目标游戏版本加载。 |
| UI/导航 | 保留0.3.11光标与分类记忆行为，WheelLogicTests、SettingsTests及实际绘制WARP回归通过。 |

Review修复了旧ActorRuntimeTests用多运行时sizeof(Actor)分配原生夹具的问题：它描述的是部分C++布局，
不能作为完整原生Actor大小。现预留0x400字节；这属于测试内存修复，没有新增游戏对象偏移补丁。
RuntimeSupport头补齐CommonLib公共聚合include，避免依赖PCH或测试include顺序。
旧测试IDDatabase适配为上游IDDB；新增v5格式自动识别与实际DLL元数据检查。
没有发现其他阻止本地适配的代码问题。

上游构建出现CombatBehaviorTreeConditionalNode宏重定义、HDE64可能未初始化变量警告。
未修改上游库来隐藏警告。本地测试说明与固定上游布局一致，不等于独立验证了新游戏所有字段/调用语义。

## 已完成验证

- 完整CommonLib源码及FavoriteWheel Release构建，自动部署。
- 8个逻辑/运行时测试：RuntimeLayoutTests、ActorRuntimeTests、WheelLogicTests、OutfitTests、
  SettingsTests、NameEditorTests、FaceLightClientTests、ItemInfoTests全部通过。
- RuntimeLayoutTests读取本机1.5.97、1.6.1170、1.7.99、1.7.104实际地址库，检查15个关键ID及玩家主虚表；
  同时验证旧格式1/2和合成v5文件自动识别、AE ID分流和实际DLL兼容标志。合成偏移不是游戏地址。
- WARP使用实际DrawWheel，2560×1440中文收藏检查字体原生字号、预热、遮罩和稳定扇区不透底通过。
- 上游2776文件逐一与固定下载ZIP一致；部署前后的INI、语言/主题和MO2 meta哈希一致。

## 游戏内测试

先在当前1.6.1170回归Q/Shift+Q/R、A/D分类记忆与光标、翻页、HUD遮罩、药水/食物动画、
装备预设保存/换装/co-save、Text Bridge中文命名和面光命令。
随后交给1.7.99 / 1.7.104用户，配套各自SKSE64和Address Library v5。
新版本联动测试需各自可加载的FaceLighting/Text Bridge，以及支持该游戏版本的动画/渲染模组。
出现问题收集FavoriteWheel.log、skse64.log、确切游戏/SKSE/地址库和联动版本与失败步骤。
本轮没有启动游戏或改动游戏版本，也没有收到1.7的游戏内反馈。

最终Release / 安装DLL SHA256：17E30171289D2E63B0C722552652093C50DC0362E9C91E891D3D29A47AC33177。旧0.3.11回退DLL为build/FavoriteWheel-before-0312.dll；旧依赖完整备份为build/CommonLibVR-before-0312。
