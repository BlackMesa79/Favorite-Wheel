# 吟游诗人模组兼容调查

用户提供三个候选模组，尚未确认原反馈者的实际安装组合。本次不修改功能代码、不构建部署。

## 证据

- The Dragon Bard Ultimate：https://www.nexusmods.com/skyrimspecialedition/mods/151231 。作者说明中，新乐器配合 Play Instrument 能力使用；不能仅凭页面认定它通过点击原版鼓、笛、鲁特琴触发。未取得其脚本，具体调用机制仍待核实。
- Become a Bard：https://www.nexusmods.com/skyrimspecialedition/mods/2303 。作者说明为选择魔法菜单中的音乐能力、按龙吼键演奏。作者公开源码 https://github.com/pevey/BecomeABard 已只读获取至 build/feedback/BecomeABard：_lp_bardreadsongbookscript.psc 的 OnRead 学习/移除曲集能力；_lp_bardplaysongbookscript.psc 的 OnEffectStart 打开曲目选择并调用演奏。应将书籍阅读与能力使用分别处理，不能统一按杂项装备处理。
- BA Bard Songs：https://www.nexusmods.com/skyrimspecialedition/mods/47202 。作者描述主要为鼓、笛、鲁特琴曲库扩充，不能单凭安装该模组推导出玩家点击乐器的触发链。
- 新候选 Skyrim's Got Talent：https://www.nexusmods.com/skyrimspecialedition/mods/50357 。作者将 BA Bard Songs 列为要求，官方模组页面帖子描述通过点击库存中的乐器演奏。与用户原始反馈更贴近，但尚未确认实际安装，也未验证事件实现。

## 项目影响

当前 Supported 不接纳杂项；即便可显示并收藏，也会在使用前被拒绝。确认这一拦截点不等于确认应调用 EquipObject、OnEquipped 或其他脚本事件。避免对所有杂项伪造装备事件，也不直接调用作者内部任务以绕过其状态检查。

现有名称匹配的本地 mods 目录扫描未找到这三个模组；这不排除翻译目录名或其他位置的安装。乐器多格问题仍与使用触发分开处理，当前没有真实 ExtraDataList 数据证明可安全放宽合并条件。

下一步优先核实 Skyrim's Got Talent 的使用入口，并以 Become a Bard 的 OnRead 作为书籍支持的具体验收场景。三个候选模组不能统一认定为同一种兼容故障。
