# FavoriteWheel 0.3.13：1.5.97 测试说明

本包为0.3.13版本，升级CommonLibSSE-NG v11.0.0并开放1.7.99/104测试目标。保留0.3.11的切分类光标和功能分类记忆。沿用0.3.10的紧凑顶部分类、角括号、菱形分页与220ms折扇动画。使用与1.6.1170相同的四版本DLL；1.5.97用户已反馈0.3.7没有明显问题，本版依赖升级需旧版本短回归。

## 安装

1. 游戏版本为Steam Skyrim SE 1.5.97.0，使用对应的SKSE64 2.0.20与Address Library的SE版。
2. 将FavoriteWheel-0.3.13-test.zip交给MO2安装并启用，通过SKSE启动；安装包已经包含SKSE/Plugins结构，不需再套Data文件夹。已有FavoriteWheel时更新原模组，保留自己的INI及语言/主题定制。
3. 测试时关闭其他Q键轮盘/收藏菜单替换功能。无需ESP或SKSE Menu Framework。中文默认读取本机C:/Windows/Fonts/msyh.ttc，没有该字体可在INI指定支持中文的字体。
4. 面部光照和Skyrim Text Bridge均为可选联动。面光自身须支持1.5.97并提供FaceLighting_GetAPI V1；未检测到兼容接口时隐藏面光分类，不影响收藏与装备预设。

## 优先检查

1. 能正常加载存档，FavoriteWheel.log包含FavoriteWheel 0.3.13和runtime/family=SE、输入/渲染hook成功信息。Q打开收藏轮盘、关闭后恢复游戏，HUD层级、遮罩和文字清晰度正常。
2. A/D切收藏或功能分类时光标停在原位置，W/S或滚轮翻页正常；下次Q打开保留收藏分类。Shift+Q或R切入功能轮盘恢复上次装备预设/面光类型；从随从子列表关闭后恢复面光主页，重启游戏才重置分类。
3. 收藏武器装备/再次点击卸下，护甲装备/卸下，药水和食物数量与效果正确；如有UAPNG/EAS，再测对应动画。悬浮详情的重量、价值、伤害/护甲和基础效果无异常。
4. 保存两套装备预设，切换先卸后穿，再点已穿整套可卸下；武器和盾牌保持。保存/读取存档及重启后预设仍在，名称输入正常；有Text Bridge时额外测试中文输入。
5. F2设置应用/取消、位置/大小和动画开关正常。连续开关或快速选物能立即响应；记录逐片展开/收拢是否太快，以及同一场景打开前后的帧率变化。
6. 有兼容面光时再测玩家、准星NPC、随从控制；没有面光可跳过。失焦/读档等打断后没有轮盘残留或粘键。

## 反馈

请说明游戏/SKSE版本、测试通过的项目和异常的重现步骤；若加载失败、崩溃或按Q仍打开原收藏菜单，请附FavoriteWheel.log、skse64.log，有崩溃日志时也一并提供。图像/动画问题说明分辨率、是否使用Community Shaders/ENB/帧生成及相关截图或短录像。

日志通常位于Documents/My Games/Skyrim Special Edition/SKSE/。完整检查清单见TESTING.md，兼容范围见RUNTIME_COMPATIBILITY.md。
