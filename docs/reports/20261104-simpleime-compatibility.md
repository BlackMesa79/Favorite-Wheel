# SimpleIME 中文输入兼容修复

## 原因与依据

轮盘使用自绘 NameEditor，已有引擎 CharEvent 接收与 SkyrimTextBridge 专用查询，却没有调用 AllowTextInput 声明文本会话。SimpleIME 需要该声明，再识别无 Scaleform 字段的原生界面，将字符提交送到引擎输入事件队列。

核查作者源码 https://github.com/lin-414/SimpleIME-Unofficial-Update ，提交 125ab186819b22827418b34b211ecc4adb0b7fda，重点为 include/utils/Utils.cpp、src/hooks/ScaleformHook.cpp、src/hooks/InputDispatchHook.cpp。本地 DLL 字符串检查确认包含 SimpleIME.ImeAware、SimpleIME.Composing；配置 imgui_surface_input=true。此检查不等同于完整二进制版本一致性证明。

## 修改

- 仅检测到 SimpleIME.dll 时，命名/重命名会话成对调用 AllowTextInput(true/false)。关闭对话框、取消轮盘、强制隐藏均释放自身申请，不清空其他模组的计数。
- 会话开始在当前同进程窗口声明 SimpleIME.ImeAware；已有属性不覆盖，只移除自己拥有的属性。
- 查询 SimpleIME.Composing 新鲜时间戳，500ms 过期后恢复普通输入；组字时保留中文提交，忽略原始 ASCII 回声。
- 组字时不把确认、退格和光标操作当成轮盘命令；保留修饰键及 Ctrl+Space 的下游通路。
- SimpleIME 存在时关闭物理键到字符的备用转换，使用引擎字符事件避免重复英文；保留旧 TextBridge 路径作为兼容回退，不要求用户安装它。

## 验证与部署

NameEditorTests（新增时间戳和字符路由断言）、WheelLogicTests 通过，Release 构建及 diff 检查通过。已自动部署到约定目录。

构建/部署 DLL SHA256 一致：E63A3ECC3913BB9CDA0B312A3D5E235652D3680C6DC96B48034A81EDFE08D86D。
主 INI 前后均为 1CBC56EE9FFB670FB1107823B42494BF45D63C412450E6081DD90C8A19F205F6。
没有修改 SimpleIME 的 DLL、配置或安装状态，没有接管键鼠。未进行游戏内输入测试。

## 待实测

创建/重命名预设，输入中文并用空格/回车选词；确认拼音不混入、选词不会提前提交命名。切换英文、退格、取消命名再重新打开；关闭轮盘后检查游戏按键恢复正常。候选窗定位及各输入法差异仍以实测为准。
