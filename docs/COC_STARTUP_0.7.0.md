# 从标题画面使用 coc 进入游戏的初始化修复

日期：2026-10-15。测试版保持 0.7.0 元数据，另行打包。

当前交付已撤回两版未通过显示实测的菜单渲染实验，恢复正式版原始渲染器，并保留本页 COC 初始化修复。最新包与验证见 [RESTORE_STABLE_RENDERER_0.7.0.md](RESTORE_STABLE_RENDERER_0.7.0.md)。

用户后续实测确认：`coc qasmoke` 进入测试房间后，打开轮盘已能正常看到界面。本地 COC 初始化与显示问题已通过该路径验证。

用户后续确认 COC 后按 Q 会暂停但界面不显示；这是单独的绘制目标选择问题。v2 保留本页初始化兜底，显示修正记录见 [NATIVE_UI_TARGET_REGRESSION_0.7.0.md](NATIVE_UI_TARGET_REGRESSION_0.7.0.md)。

用户反馈从标题画面执行 `coc qasmoke` 后按收藏键，日志记录 `blocked='game not active'`。现有 `main.cpp` 只在 SKSE `kNewGame` 或成功的 `kPostLoadGame` 中调用 `SetGameActive(true)`；`kPreLoadGame` 和失败读档调用 false。当前设计遗漏了没有经过这些会话通知的控制台启动入口。

此外，玩家更新钩子也在 `SetGameActive(true)` 中安装。仅绕过“game not active”的入口检查会导致开盘任务和部分动作仍没有执行器，因此不能只把条件删掉或在 DataLoaded 时直接标为 active。

## 实现

- `GameSession` 初始为 AwaitingWorld。一次明确的 SKSE 会话通知使它进入 Active 或 Inactive，结束隐式启动检测。
- 输入分发的空闲帧也可请求一次去重后的 SKSE 游戏任务；标题、加载、控制台和捏脸菜单存在时不请求检测。实际玩家、模型、父 cell 和加载标志只在游戏任务中检查。
- 必须已经安装轮盘、UI 可用、启动菜单关闭、玩家模型存在、父 cell 已 attached，且玩家不处于加载状态，才执行完整的 `SetGameActive(true)`。这也补装玩家更新钩子，并保留在全部 DataLoaded 回调之后串联 Face Lighting 的顺序。
- 检测任务记录 epoch，执行前重查；PreLoadGame、正常开新游戏、读档成功/失败会阻止旧任务激活状态。明确失败的读档不会因为旧角色模型仍存在就被重新视为 active。
- 正常会话开始后不再检测，不会反复清理角色状态或重复安装钩子。没有增加持续线程、延时等待或按键映射变更。

新增日志：`Game session recovered: source=attached-player-world startup=implicit-v1 cell=... thread=...`，随后应有 `Game active=true` 和玩家更新钩子安装/回调日志。它表示按真实场景状态完成的启动兜底，不声称能识别用户具体执行了哪一条控制台命令。

## 验证

18 个本地测试套件通过。GameSessionTests 覆盖标题画面、缺失 UI/玩家/模型、未 attached、仍加载、无消息的有效世界、过期任务、PreLoadGame 旧世界、失败读档和后续成功会话。RuntimeLayoutTests 校验七个支持版本的玩家加载标志访问位置，并读取七份真实 Address Library 数据库。

尚未启动用户游戏执行控制台命令。游戏内需验证：直接启动到标题画面，执行 `coc qasmoke`，等待加载并关闭控制台，开启收藏/功能轮盘；另查正常 New Game、正常读档、读档期间输入和退出再加载。测试包合并前一轮 UI 绘制阶段和手柄组合映射修复，原版正式发布 ZIP 与旧测试包不覆盖。

## 闭源帧生成器补充

用户已确认原始不可见日志对应的环境是闭源帧生成器加 ENB；不能将它视作已确认的 TRP 故障。提供的爱发电地址 `https://afdian.com/p/d3513e3ac2e311f1b28252540025c377`，公开抓取仅获得站点通用壳页，无法据此确认产品名称、版本、DLL 或渲染接口。

前一轮修复按原生菜单 UI 阶段和当前实际绑定目标绘制，不依赖 TRP 的专用接口，因而仍是这个环境中值得验证的通用修复方向。闭源模组可能额外接管 UI 捕获与合成，尚不能承诺兼容；应请该用户使用本测试包复测，并保留 `route=menu-postdisplay-v1` 的新日志。COC 的 `game not active` 是另一个已定位的初始化问题。
