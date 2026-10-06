# 详情卡基础物品信息（0.3.4）

日期：2026-10-05（Asia/Hong_Kong）。用户授权先接入实用属性/效果，完整说明和特殊修正后续处理。本版不依赖 SkyUI、I4 或新 SWF，也不打开背包/隐藏电影来抽取卡片。

## 数据来源与边界

| 内容 | 来源与说明 |
| --- | --- |
| 重量 | InventoryEntryData::GetWeight，单件记录重量，不承诺包含 Perk 导致的实际负重变化 |
| 价值 | InventoryEntryData::GetValue，当前物品实例的价值；不是交易菜单最终买卖价格 |
| 武器伤害 | PlayerCharacter::GetDamage，提供仅含选中 ExtraDataList 的临时 entry；跳过法杖，不将附魔强度相加到伤害 |
| 护甲值 | PlayerCharacter::GetArmorValue，同样提供当前收藏实例；不作为最终减伤百分比 |
| 弹药 | TESAmmo runtime data 的 damage，标题“基础伤害”，不误作弓箭合计伤害 |
| 魔法消耗 | 普通 Spell 类型的 CalculateMagickaCost(player)，持续施法标记 /秒；不声称双手施法或所有脚本修正已覆盖 |
| 效果 | AlchemyItem/ScrollItem/SpellItem 的 MagicItem::effects；装备附魔来自当前实例 GetEnchantment（包括基础与玩家附魔） |
| 效果字段 | EffectSetting 名称与 Effect 的 EFIT magnitude/duration/area；是基础记录，不是当前目标身上的 ActiveEffect |

尊重 kHideInUI、kNoMagnitude、kNoDuration、kNoArea。名称空时本地化回退；非有限值不显示；即时效果不添加伪持续时间。持续时间单位秒，范围单位英尺，强度使用中性标签，暂不自行推断点数/百分比/每秒语义。名称来自游戏及其翻译，标题和单位来自轮盘语言文件。

不调用物品装备、效果应用或脚本来预测结果。条件效果不在这里评估（部分条件要施放目标）；可见记录不意味着条件一定成立。原版描述模板、DNAM 占位符、HTML 标签、龙吼已解锁阶段/冷却、未知脚本效果、动态 UI 注入与特殊模组修正未实现。不宣称与原版/SkyUI 卡片逐字一致。

## 生命周期、实例与显示

CaptureItemInfo 在 CollectFavorites(true) 遍历时读取，CaptureSpellInfo 处理收藏法术；Open 在暂停前收集，随后全部为值快照。临时 InventoryEntryData 只拥有借用实例的链表节点，不复制/删除游戏 ExtraDataList，也不跨任务保存该引用。不能传整个同基础 Form 的 inventory entry，否则其他同名实例可能串入强化/附魔。

UseFavorite 调用 CollectFavorites(false) 只重验身份，避免为实际使用重新扫描/计算展示信息。Present 不执行库存遍历或引擎查询；开轮盘全程超过 8ms 记录 Favorites snapshot（数量/details/total_ms），实机可据此检查额外开销。

ItemInfo / EffectInfo 在 Item 末尾，是可选数字和 UTF-8 名称，不保存引擎指针。最多六条可见效果，其他计数明确显示；模组超长效果名按 UTF-8 边界限制为约 256 字节。详情卡按内容高度扩展，自动右/左放置；两边不足时中心仅回退首个属性或效果。没有新滚动区域，以保留轮盘滚轮分页含义。

所有收藏效果名在打开时加入 inventoryGlyphs，字体按现有输出字号预热；换分类/翻页不因之前未见的效果名重新烘焙。语言/主题切换仍按现有规则更新字体。属性标签在绘制时翻译，不把中文固定在快照里。

## SE / AE 与验证

沿用 1.5.97 / 1.6.1170。新增计算接口使用 CommonLib 双 ID：价值 15757/15995，附魔 15788/16026，护甲 39175/40249，伤害 39179/40253，法术消耗 11213/11321；RuntimeLayoutTests 增加本机两版地址库解析。地址存在只证明能解析，不代替实际执行契约和游戏内数值对比。

本地测试覆盖：非有限/负属性、零重量/价值、隐藏及无数值效果、持续施法单位、负效果强度、超量效果、超长 UTF-8 名字、本地化与缺数据回退；WARP 预览检查装备/药水/法术/六效果溢出，中英文和不同分辨率、卡片边界、字号与不透明度。模拟库存图不属于实机数据。

实机重点：同名基础/强化/玩家附魔武器和护甲的数值不得串行；药水/食物与附魔按记录显示可见效果；单手/持续法术消耗与背包对照；无数据/龙吼保持原提示；关闭后消耗品动画和装备切换照常。特殊效果与原版说明差异收集后再决定第二阶段实现。
