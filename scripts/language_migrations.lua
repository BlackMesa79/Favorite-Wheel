-- Replace only exact previously shipped hints. Unknown/custom text stays intact.
local old = {
    sortKeys = {
        ["F3: Sort mode | F4: Arrange | Save game to keep order"] = true,
        ["F3 切换排序 · F4 手动排序 · 保存游戏以保留顺序"] = true,
        ["Click the buttons below to open settings or arrange items."] = true,
        ["点击下方按钮打开设置、切换排序或调整顺序。"] = true,
        ["F2: Tools wheel | Settings / Sort / Arrange"] = true,
        ["F2 工具轮盘：设置／排序／手动排序"] = true,
    },
    sortMove = {
        ["LMB: Move earlier | RMB: Move later | Shift+T: Done"] = true,
        ["指向条目：左键前移／右键后移；Shift+T 完成。"] = true,
        ["ARRANGE: LMB / Enter earlier | RMB later | F4 done"] = true,
        ["排序中：左键／回车前移 · 右键后移 · F4 完成"] = true,
        ["Click an item to select it, then click Move earlier / Move later."] = true,
        ["点击条目选中，再点击前移／后移；点击完成结束。"] = true,
        ["LMB: Move earlier | RMB: Move later | F2: Tools / Done"] = true,
        ["指向条目：左键前移／右键后移；F2 工具轮盘中完成。"] = true,
    },
    sortMovePad = {
        ["ARRANGE: A earlier | X later | RS click done"] = true,
        ["排序中：A 前移 · X 后移 · 按右摇杆完成"] = true,
    },
    dualWheelHint = {["SWITCH WHEEL · ESC CLOSE · F2 TOOLS"]=true,["切换轮盘 · Esc 关闭 · F2 工具"]=true},
    lightNavigation = {["SWITCH WHEEL · ESC BACK · F2 TOOLS"]=true,["切换轮盘 · Esc 返回 · F2 工具"]=true},
    compactClose = {["ESC CLOSE  ·  F2 TOOLS"]=true,["Esc 关闭  ·  F2 工具"]=true},
    useHint = {["LMB USE / RIGHT HAND    RMB LEFT HAND    ESC CLOSE    F2 TOOLS"]=true,
        ["左键 使用 / 右手    右键 左手    Esc 关闭    F2 工具"]=true},
}
function main(current, source)
    local desired = {}
    for line in source:gmatch("[^\r\n]+") do
        local key,value=line:match("^([%w_]+)=(.*)$")
        if key then desired[key]=value end
    end
    local changed=0
    local updated=current:gsub("[^\r\n]+",function(line)
        local prefix,key,value=line:match("^(%s*)([%w_]+)=(.*)$")
        if key and old[key] and old[key][value] and desired[key] and desired[key]~=value then
            changed=changed+1
            return prefix..key.."="..desired[key]
        end
        return line
    end)
    return updated,changed
end
