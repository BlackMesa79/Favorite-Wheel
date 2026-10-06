# 运行时与面光接口兼容性（0.3.12）

更新：2026-10-05（Asia/Hong_Kong）；0.3.12升级CommonLibSSE-NG v11.0.0，新增1.7.99 / 1.7.104测试目标。

## 面光模组版本与接口版本

FavoriteWheel 不读取 Face Lighting 的文件版本或发布版本，不使用发布版本白名单。PostPostLoad 查询已加载的 FaceLighting.dll 的 FaceLighting_GetAPI(1)，要求返回表 apiVersion=1、structSize 至少为本客户端 V1 表大小、所需函数指针齐全。能力位分别控制玩家、角色和名单查询。附加的未知能力位和增大的 V1 表可接受；破坏 V1 布局的更改不能标作兼容 V1。

面光更新但继续提供兼容 V1 时，通常无需重新编译轮盘。未来面光提供 V2 时可以同时保留 V1；若只提供 V2，则现客户端拒绝联动，需要更新客户端。发布版本号本身不决定结果。当前两个工程的 FaceLightingAPI.h SHA-256 一致：5D87A48BB50AEFCF3BBA86E4294982A136D038BFD66D5802984135D3C04C6060。

兼容条件也包括线程、生命周期和命令语义。轮盘按已验证的面光玩家 Update（0xAD）之后执行 API；若面光未来改变被认可的线程入口，需要同步更新接入方式，不能仅凭 apiVersion 字段断言全部兼容。暂停前快照、恢复后 Execute、session/revision/目标校验仍保留。0.3.1 的 WrongThread 修复与分类导航已由用户实测确认基本正常。

未安装 DLL、没有导出、不提供 V1、表无效各有日志。0.3.13未检测到兼容接口时隐藏面光分类；接口已发现但暂时未就绪时保留状态提示，收藏和装备预设不依赖面光。轮盘不能让仅支持某个游戏版本的面光 DLL 在另一个版本加载；1.5.97 联测仍需面光本身支持该运行时。


## 0.3.12 当前范围

同一DLL精确接受Steam 1.5.97.0、1.6.1170.0、1.7.99.0、1.7.104.0。后两者是待实机验证的测试目标，需匹配SKSE64和Address Library v5。保持SE/AE启用、VR禁用；不开放其他1.7版本。实际DLL已确认导出Address Library v5/旧AE标志。

1.7由新库正确分为AE，沿用AE引擎ID；玩家数据起点由0x3E0变为0x3E8，ActorState/Actor/ControlMap对应0xC0/0xE8/0xF0。所有四份本机地址库关键ID解析与布局夹具通过；新库REX RTV转换已适配，保留engine UI framebuffer。输入+0x7B签名与Update槽位实际运行仍需1.7用户测试，不能只凭地址库解析宣称游戏兼容。完整Review和验证边界见COMMONLIB_1.7_REVIEW.md。下面0.3.2部分为历史适配依据。
## 0.3.2 历史运行时范围

同一 x64 Release DLL 接受 Steam Skyrim SE 1.5.97.0 和 1.6.1170.0。构建已同时启用 CommonLib SE/AE 分支并使用 Address Library 模式；启动加载检查保持精确版本集合。其他 SE/AE 版本、GOG 和 VR 尚未纳入范围，不因使用双运行时库就自动声明支持。

| 路径 | 1.5.97 | 1.6.1170 | 本地依据 |
| --- | --- | --- | --- |
| 输入 dispatch | ID 67315 + 0x7B | ID 68617 + 0x7B | 现有输入法工程与库地址选择；安装时检查 E8 指令签名 |
| 玩家 Update | 虚表 0xAD | 虚表 0xAD | 平面版 Actor Update 槽和现有面光 hook；保留前一个调用 |
| Actor 死亡 / Potion 虚调用 | 0x99 / 0x10F | 0x99 / 0x10F | 双运行时模拟 Actor 回归；当前实际吃喝路径为 EquipObject |
| ActorState / 玩家数据 | 0xB8 / 0x3D8 | 0xC0 / 0x3E0 | CommonLib 运行时访问器与独立字节布局测试 |
| ControlMap 数据 | 0xE8 | 0xF0 | 文本输入计数与原生偏移回归 |
| Button 数值 / 持续时间 | 0x28 / 0x2C | 0x28 / 0x2C | 吞键 release 对原生字节读写回归 |
| Renderer / UI framebuffer | 库平面版访问器 | 库平面版访问器 | swapChain 与 framebuffer RTV 独立哨兵检查；沿用已验证的 UI 层绘制 |
| 装备调用 | 库 SE ID | 库 AE ID | EquipObject/UnequipObject 及相关引擎函数的 RelocationID 分流 |

RuntimeLayoutTests 可选读取本机两套地址库，已确认输入/renderer/EquipObject/UnequipObject/PlayerCharacter 虚表 ID 均能解析。它没有读取或反汇编 1.5.97 可执行文件，不能证明 +0x7B 处原生指令或第三方 hook 组合必然一致；实际安装仍做指令签名检查。

## 验证状态与下一步

- 1.6.1170：2026-10-04 用户确认 0.3.2 测试正常。
- 1.5.97：双运行时 mock、布局、角色派发和安装地址库解析通过。2026-10-05用户转述1.5.97玩家测试0.3.7没有明显问题，已收到游戏内反馈；未提供逐项联动/渲染组合清单，不据此断言所有组合均已验证。0.3.8新UI/动画待回归。
- 升级不改变 INI、语言/主题自定义或预设 co-save 格式。不要把不同游戏版本的 SKSE DLL 混用；Address Library 与 SKSE 需匹配当前 SkyrimSE.exe。不会修改或降级游戏文件。

1.5.97 联测依次检查：SKSE 日志正常加载 → FavoriteWheel runtime/family=SE → renderer/input hook 安装 → Q 分类和关闭吞键 → Shift+Q/R/A/D → 收藏装备、卸武器、吃喝动画 → 预设保存/换装/co-save → Text Bridge → 可选面光快照和命令 result=0/1 → HUD 遮罩与帧率。

2026-10-06：用户确认依赖升级后的0.3.12在1.6.1170测试正常，认可发布准备。0.3.13仅增加无兼容面光接口时隐藏分类；1.7实机验证仍待反馈。
