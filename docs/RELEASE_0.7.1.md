# 0.7.1 发布记录

日期：2026-10-15。用户确认恢复原版渲染后，从标题画面执行 `coc qasmoke` 进入测试房间，轮盘已正常显示，并授权整理为 0.7.1。
同日用户确认第二版手位测试通过，并授权将武器换手与法术双持收回一并纳入正式 0.7.1，更新完整安装包、源码包和 changelog。

## 发布内容

- 补充无明确 SKSE 新游戏/读档通知时的初始场景检测，确认玩家模型、attached cell、加载标志和启动菜单状态后，在游戏任务线程完成初始化并安装玩家更新钩子。正常读档、失败读档和旧检测任务保持状态保护。
- 跟随游戏手柄收藏映射时，同时读取主键和原生修饰键，保留没有按修饰键时主键的正常用途。同一收藏/功能组合冲突时优先收藏；需要功能轮盘时可另外选择修饰键。
- 单把武器已在另一手时，一次点击完成卸下、重新解析物品实例及装备到目标手。纠正将原生卸装返回值误判为成功标志而提前退出的问题；卸装后不直接复用可能失效的附加数据指针。
- 同一普通法术已为 L+R 时，再点击指定手只保留该手；单手状态仍可追加到另一手。通过已有原生 `UnequipSpell` 入口清除相反手，不直接改法术槽、不主动解除轮盘暂停。双手法术和能力保持原有装备方式。
- 渲染器保持与正式 0.7.0 源码字节一致。两版未通过显示实测的菜单渲染实验已撤回，不纳入正式发布。没有将闭源帧生成器或 TRP 的兼容问题列为已修复。

## 验证范围

本地 COC 入口和显示已由用户游戏内确认。手柄修复已通过本地策略回归，但未收到对应 Kontrol/自定义映射反馈者的完整游戏验证。保留准确的实测范围，不将 COC 通过扩展为帧生成适配通过。
第二版武器移手及法术 L+R 收回已由用户实测确认通过；测试经过见 `docs/CROSS_HAND_EQUIP_0.7.1_TEST.md`。

Release 构建及 18 项当前测试套件：HandEquip、GameSession、Settings、SettingsPersistence、SettingsVfs、WheelLogic、Time、RuntimeLayout、Inventory、Outfit、NameEditor、ItemInfo、FaceLightClient、ActorRuntime、SkinTexture、LayoutSkin、SectorMesh、CombatPreset。HandEquip 覆盖原生卸装的两种返回值、双向移手、失效实例保护、法术收回方向及追加双持。RuntimeLayout 检查导出版本 0.7.1、七个支持运行时的数据布局与七份真实 Address Library 数据库。最终结果和包哈希见 `release-materials/0.7.1/verification.txt`。

## 交付与升级

- `dist/FavoriteWheel-0.7.1.zip`：完整安装包，包含现有语言、三套主题、材质与许可，不包含主 INI。
- `dist/FavoriteWheel-0.7.1-source.zip`：当前源码、依赖、发布材料和逐文件 SHA256 清单。
- `release-materials/0.7.1`：简短英文/中文 changelog、同步版本和控制说明的详情页及验证记录。

正常升级并停用之前的独立测试补丁，避免旧 DLL 覆盖正式版。保留主 INI、自定义资源及匹配的 `.skse` 角色存档；本次不改变预设存档格式或重分配玩家按键。

按以往流程制作本地正式发布材料并同步 MO2；未执行 Nexus 上传、Git 提交/推送或远程 Release 创建。
