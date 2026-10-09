# 设置持久化检查（0.5.4测试版）

2026-10-09：用户要求KeepOpen默认开启，并转述“游戏内编辑后Apply似乎无法在退出游戏并重新加载存档后保持设置”。反馈版本、管理器、日志与具体字段均未知，用户授权先修复发现的隐患，不能将未知环境的根因写成已确认。

## 检查结论

设置只由插件加载时LoadSettings读INI，SaveSettings由Apply调用；SetGameActive、存档序列化只重置会话/装备预设，不覆盖已应用的全局设置。Apply成功后saved快照更新、editing=false，之后Cancel/RevertSettings不撤销已应用值。因此不需要保存游戏来持久化设置；装备预设和原生快捷槽仍按各自存档规则。

发现两个真实代码隐患：路径基于进程当前目录，若启动器工作目录变化就可能读取另一处Data；保存使用Data下新建.tmp与MoveFileExW覆盖原INI，在虚拟文件系统中临时文件与已映射INI可能路由到不同物理目录。旧测试只覆盖普通文件系统与同进程重载，不足以证明实际MO2/其他管理器环境的持久化。未获取原用户环境，本机旧日志没有Apply记录，不能据此复现或证实其具体故障。

## 修改

- SettingsPath首次从SkyrimSE.exe目录解析，固定Data/SKSE/Plugins/FavoriteWheel.ini，而不是DLL物理mod目录，也不随cwd变化。测试自定义路径在设置时固定为绝对路径。
- 在系统临时目录准备完整INI，保留原未知键/注释，写入所有可编辑字段并按既有百分比格式归一化；先重读字段检查，再提交。
- 目标使用CreateFileW OPEN_ALWAYS读写句柄，保留原字节作回滚备份；定位写入、截尾、FlushFileBuffers，再经同句柄验证完整字节。失败尝试还原原字节；只读/占用/权限错误明确失败，不假装应用成功。
- 关闭句柄后刷新Win32 profile缓存，重开路径验证字节和字段。仅成功后更新saved/结束编辑；失败保持设置页错误与可撤销预览。临时文件自动清理，不留下Data下新.tmp文件。
- main日志记录Settings load路径、状态与Win32错误；Apply成功记录saved-verified及关键字段，失败记录具体阶段与错误码。仍有问题时对照重启前后load/save路径和有效INI来源。
- 保留轮盘默认改为true、缺键回退1、发行INI为KeepOpen=1，DefaultSettings同样开启。显式KeepOpen=0尊重用户选择，不自动覆写已有INI。

这一实现避免跨虚拟文件映射rename，并检查实际写入与重新读取，但原位写入不是断电原子事务，系统强制中断时无法保证回滚。正常写失败提供尽力回滚；外部程序同时改配置、管理器切换profile/重装覆盖INI也不属于插件可保证范围。系统临时目录和目标INI需可写，失败时错误保留在日志。未引入新依赖/地址/钩子/存档格式，不修改其他项目。

## 验证与复测

SettingsTests包含一个独立子进程（CREATE_NO_WINDOW）读取中文路径INI，验证保存后的语言、主题、位置、尺寸、时间模式、库存范围、手柄方案与明确KeepOpen=0；另验证新默认1、缺键1、恢复默认、取消/应用后关闭、未知键保留、工作目录变化、只读失败原字节完全不变。测试不会启动Skyrim，不把独立进程测试当作MO2真实虚拟化验收。

游戏内复测：改轮盘X位置、大小与手柄方案，Apply后关盘重开检查；退出游戏，再从同管理器/同profile启动并读任意存档检查。语言/时间/KeepOpen再分别抽查。日志应出现Settings saved stage=saved-verified，下一次启动load路径一致；如果仍恢复默认，提供前后两份日志、管理器/版本/有效INI来源（含Overwrite）与具体字段。缺INI时load-defaults会明确记录。

0.5.3新连续装备与手柄交互仍按CONTINUOUS_USE_0.5.3.md复测；本次只默认/持久化/诊断改变，没有UI绘制修改，不重复离屏视觉测试。

2026-10-09本地交付：Release编译117.359s成功，SettingsTests（含独立进程）、WheelLogicTests、TimeTests、RuntimeLayoutTests全部通过；六模拟运行时布局、地址库格式1/2/5及0.5.4实际DLL导出元数据通过。构建/自动部署DLL SHA256一致：0D2A4F8B36E5C0067D1B412A74025EA6FDE0251F24575B4D4351CFD4E4C9E8FC。部署目录仍H:/Games/Dev Skyrim/mods/36 - FavoriteWheel，8份已有INI/语言/主题/meta.ini哈希精确保持，旧DLL备份build/FavoriteWheel-before-054.dll。没有实机启动或原反馈者环境复现；测试日志仅留本机build/settings054-tests.txt。
