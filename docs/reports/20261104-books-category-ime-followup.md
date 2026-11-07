# 书籍独立分类与 SimpleIME 实测问题跟进

## 实测反馈
用户确认书籍可加入轮盘；要求单独分类。新建预设能显示输入法但无法中文上屏，重命名既有预设无法唤起输入法。此前输入法兼容尚未通过游戏验收。

## 修改
- 追加 Books 分类，沿用旧分类编号；显示“书籍”，复用卷轴图标，补齐八语言标签。
- 排序组在原有 18 组后追加书籍收藏/全背包两组；文本格式 FWS 2，仍接受 FWS 1 的旧记录。旧分类排序位置保持不变。
- 实际 SimpleIME.log 显示 favoritewheel.dll 被识别为 GFx 接收方。原因是暂停保护菜单对未处理的字符探测也返回 handled；此前仅在引擎 InputEvent 观察器接收文字，与实际投递通道不符。
- 轮盘菜单和暂停保护菜单均接收 Scaleform 字符消息，仅命名框打开时认领；探测字符 0 不插入。暂停菜单不再认领未处理的 Scaleform 消息。
- SimpleIME 存在时，原始 CharEvent 留给原生 UI / SimpleIME 过滤，再从 Scaleform 消息接收字符，避免把拼音直接写入，同时保留关闭输入法时的英文输入路径。
- 日志另有延迟启用被丢弃记录。新建和重命名共用的会话入口改为等待鼠标按钮、Enter 松开后声明文本输入，避免触发 SimpleIME 对该次点击的焦点延迟路径。没有修改 SimpleIME 安装或配置。

## 验证
- FavoriteWheel 构建成功，after_build 自动部署。
- WheelLogicTests、NameEditorTests、InventoryOrderTests 通过；包括旧排序迁移、书籍两个范围独立保存回读。git diff --check 通过。
- DLL 构建与部署 SHA256：6e0460cd4371429a409ff0a6ecee35c77cb9560d1062f79b376f3499efaa4242
- 主 INI SHA256 仍为 1CBC56EE9FFB670FB1107823B42494BF45D63C412450E6081DD90C8A19F205F6。部署目录中文 books 标签已确认。
- 未接管键鼠，未做游戏内验证。仍需确认新建/重命名中文选词上屏、切英文、取消重开，以及书籍分类显示。后台验证不能替代这几项实测。
