# 名称输入兼容修复（0.2.7）

问题：旧预设名称框只是 DrawList 按钮，靠 NameKey 的 ToUnicodeEx 追加文字，没有光标；输入 hook 把 CharEvent 清零。Text Bridge 经游戏字符事件注入的中文被丢弃。独立 ImGui context 不会自动共享文本焦点或输入。

修复：纯值 NameEditor 持有 UTF-8 内容、字节边界光标和选区；Draw 展示闪烁光标、选区与横向滚动，并发布名称点击位置。游戏事件接收器接受 hook 链最终 CharEvent，再清零避免传给其他菜单。活动输入法下停止按键转文本，候选/待提交文字期间停止轮盘保存/退出/编辑命令。输入法快捷键与修饰键值保留给桥接 hook；退出名称页清理输入法。

Skyrim Text Bridge 0.3.8 的可选原生接口：

```cpp
uint32_t SkyrimTextBridge_QueryInputV1(uint32_t version, uint32_t* hotkey);
void SkyrimTextBridge_ResetInputV1();
```

版本参数必须为 1，标志位 1=可用、2=启用、4=直接输入捕获中、8=组合或提交队列待排出；hotkey 为实际 scan code。跨边界不传 STL、ImGui context 或游戏对象。无桥接 DLL 时保留普通键盘和剪贴板，无硬依赖。

自动检查：Unicode 插入/移动/选区/删除/128 字节边界，Text Bridge API 版本/状态/待提交保护及原有 IMM/Prisma/Meridian 检查；名称页 720p/1440p WARP 离屏检查。真实中文候选、Enter/Esc、编辑快捷键与 hook 顺序仍需实机确认。
