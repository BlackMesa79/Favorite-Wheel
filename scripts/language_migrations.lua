-- Replace only exact previously shipped hints. Unknown/custom text stays intact.
local old = {
    kbMoveNavHint = {
        ["Scroll: page · Shift+Scroll / Middle: category · Z or C: bind + Apply in move mode"] = true,
        ["滚轮 翻页 · Shift+滚轮 / 中键 切类型 · Z或C：开启移动模式并配置、应用后生效"] = true,
        ["Scroll: page  ·  Shift+Scroll: category  ·  Middle: category"] = true,
        ["滚轮 翻页  ·  Shift+滚轮 切类型  ·  中键 切类型"] = true,
    },
    combatSave = {
        ["AKTUELLE HÄNDE SPEICHERN"]=true,
        ["ENREGISTRER LES MAINS ACTUELLES"]=true,
        ["SALVAR MÃOS ATUAIS"]=true,
        ["SAVE HANDS"]=true,
        ["СОХРАНИТЬ СНАРЯЖЕНИЕ РУК"]=true,
        ["保存双手组合"]=true,
        ["現在の両手の組合せを保存"]=true,
        ["현재 양손 조합 저장"]=true,
    },
    combatOverwrite = {
        ["DURCH AKTUELLE HÄNDE ERSETZEN"]=true,
        ["REMPLACER PAR LES MAINS ACTUELLES"]=true,
        ["REPLACE WITH CURRENT HANDS"]=true,
        ["SUBSTITUIR PELAS MÃOS ATUAIS"]=true,
        ["ЗАМЕНИТЬ ТЕКУЩИМИ РУКАМИ"]=true,
        ["以当前双手组合覆盖"]=true,
        ["現在の両手で上書き"]=true,
        ["현재 양손 조합으로 덮어쓰기"]=true,
    },
    combatConfirmOverwrite = {
        ["Diese Voreinstellung durch die aktuelle Handausrüstung ersetzen?"]=true,
        ["Remplacer ce préréglage par l'équipement actuel des mains ?"]=true,
        ["Replace this preset with your current hand equipment?"]=true,
        ["Substituir esta predefinição pelo equipamento atual das mãos?"]=true,
        ["Заменить этот набор текущим снаряжением рук?"]=true,
        ["現在の両手の装備でこのプリセットを上書きしますか？"]=true,
        ["用当前左右手装备覆盖此预设？"]=true,
        ["현재 양손 장비로 이 프리셋을 덮어쓰시겠습니까?"]=true,
    },
    combatUnsupported = {
        ["Hand equipment cannot be captured/restored."]=true,
        ["Handausrüstung kann nicht gespeichert oder wiederhergestellt werden."]=true,
        ["Impossible d'enregistrer ou de restaurer cet équipement des mains."]=true,
        ["Não é possível salvar ou restaurar o equipamento das mãos."]=true,
        ["Снаряжение рук невозможно сохранить или восстановить."]=true,
        ["当前手持装备无法保存或恢复。"]=true,
        ["現在の手持ち装備は保存・復元できません。"]=true,
        ["현재 손 장비를 저장하거나 복원할 수 없습니다."]=true,
    },
    combatCaptureFailed = {
        ["Aktuelle Handkombination konnte nicht gespeichert werden."]=true,
        ["Impossible d'enregistrer la combinaison actuelle des mains."]=true,
        ["Não foi possível salvar a combinação atual das mãos."]=true,
        ["Unable to capture the current hand combination."]=true,
        ["Не удалось сохранить текущую комбинацию рук."]=true,
        ["无法保存当前左右手组合。"]=true,
        ["現在の両手の組合せを保存できませんでした。"]=true,
        ["현재 양손 조합을 저장하지 못했습니다."]=true,
    },
    combatWorking = {
        ["Changement de l'équipement des mains..."]=true,
        ["Changing hand equipment..."]=true,
        ["Handausrüstung wird gewechselt..."]=true,
        ["Trocando o equipamento das mãos..."]=true,
        ["Смена снаряжения рук..."]=true,
        ["両手の装備を切り替えています..."]=true,
        ["正在切换左右手装备……"]=true,
        ["양손 장비 변경 중..."]=true,
    },
    combatInterrupted = {
        ["Application interrompue. Vérifiez l'équipement des mains."]=true,
        ["Combat preset interrupted. Check your hand equipment."]=true,
        ["Kampfvoreinstellung unterbrochen. Handausrüstung prüfen."]=true,
        ["Predefinição interrompida. Verifique o equipamento das mãos."]=true,
        ["Применение набора прервано. Проверьте снаряжение рук."]=true,
        ["战斗预设应用已中断，请检查左右手装备。"]=true,
        ["適用が中断されました。両手の装備を確認してください。"]=true,
        ["프리셋 적용이 중단되었습니다. 양손 장비를 확인하세요."]=true,
    },
    combatPartial = {
        ["Application incomplète. Vérifiez l'équipement des mains et le journal."]=true,
        ["Combat preset could not finish. Check your hand equipment and the log."]=true,
        ["Kampfvoreinstellung nicht abgeschlossen. Handausrüstung und Protokoll prüfen."]=true,
        ["Predefinição incompleta. Verifique o equipamento das mãos e o registro."]=true,
        ["Набор применён не полностью. Проверьте снаряжение рук и журнал."]=true,
        ["战斗预设未能完整应用，请检查左右手装备与日志。"]=true,
        ["適用を完了できませんでした。両手の装備とログを確認してください。"]=true,
        ["프리셋을 완전히 적용하지 못했습니다. 양손 장비와 로그를 확인하세요."]=true,
    },
    combatSlotUnavailable = {
        ["Equip an item in this slot before setting it."]=true,
        ["Equipe algo neste espaço antes de defini-lo."]=true,
        ["Rüste zuerst etwas in diesem Platz aus."]=true,
        ["Équipez un élément dans cet emplacement avant de le définir."]=true,
        ["Сначала экипируйте предмет в этот слот."]=true,
        ["設定する前にこのスロットへ装備してください。"]=true,
        ["请先在该槽位装备物品。"]=true,
        ["이 슬롯에 먼저 장착한 뒤 설정하세요."]=true,
    },
    cycleCapture = {
        ["ADD CURRENT HANDS"]=true,
        ["ADICIONAR MÃOS ATUAIS"]=true,
        ["AJOUTER LES MAINS ACTUELLES"]=true,
        ["AKTUELLE HÄNDE HINZUFÜGEN"]=true,
        ["ДОБАВИТЬ ТЕКУЩИЕ РУКИ"]=true,
        ["加入当前双手"]=true,
        ["現在の両手を追加"]=true,
        ["현재 양손 추가"]=true,
    },
    cycleStorage = {
        ["Salve o jogo para manter grupos, teclas e posições. Cada item usa um conjunto completo das mãos."]=true,
        ["Sauvegardez la partie pour conserver groupes, touches et positions. Chaque entrée utilise les deux mains."]=true,
        ["Save your game to retain groups, bindings and cycle positions. Groups use complete hand presets."]=true,
        ["Spiel speichern, um Gruppen, Tasten und Positionen zu behalten. Einträge verwenden vollständige Hand-Voreinstellungen."]=true,
        ["Сохраните игру, чтобы сохранить группы, клавиши и позиции. Каждый элемент задаёт обе руки."]=true,
        ["グループ・キー・位置の保持にはゲームを保存してください。各項目は両手の完全なプリセットです。"]=true,
        ["保存游戏以保留循环组、按键和位置。每项为完整双手预设。"]=true,
        ["그룹, 키, 위치를 유지하려면 게임을 저장하세요. 각 항목은 양손 전체 프리셋입니다."]=true,
    },
    cycleQuickHint = {
        ["ATALHO DO CICLO: ADICIONAR MÃOS / CONJUNTO SOB O CURSOR"]=true,
        ["CYCLE SHORTCUT: ADD CURRENT HANDS / HOVERED PRESET"]=true,
        ["RACCOURCI DU CYCLE : AJOUTER LES MAINS / LE PRÉRÉGLAGE SURVOLÉ"]=true,
        ["ZYKLUSTASTE: AKTUELLE HÄNDE / MARKIERTE VOREINSTELLUNG HINZUFÜGEN"]=true,
        ["КЛАВИША ЦИКЛА: ДОБАВИТЬ РУКИ / НАБОР ПОД КУРСОРОМ"]=true,
        ["サイクルキー：現在の両手／選択中のプリセットを追加"]=true,
        ["循环组合键：加入当前双手／悬浮的预设"]=true,
        ["순환 키: 현재 양손 / 가리킨 프리셋 추가"]=true,
    },
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
    combatStorageHint = {
        ["Presets are saved with your game. Empty hand slots are cleared. Spells are equipped, not cast."]=true,
        ["Voreinstellungen werden mit dem Spiel gespeichert. Leere Handplätze werden geleert. Zauber werden ausgerüstet, nicht gewirkt."]=true,
        ["Les préréglages sont enregistrés avec la partie. Les emplacements vides sont libérés. Les sorts sont équipés, pas lancés."]=true,
        ["プリセットはゲームのセーブに保存されます。空の手スロットは空に戻します。魔法は装備のみで自動発動しません。"]=true,
        ["프리셋은 게임 저장과 함께 보관됩니다. 빈 손 슬롯은 비워집니다. 주문은 장착만 하며 자동으로 시전하지 않습니다."]=true,
        ["As predefinições são salvas com o jogo. Espaços vazios são desocupados. Feitiços são equipados, não lançados."]=true,
        ["Наборы сохраняются вместе с игрой. Пустые слоты освобождаются. Заклинания экипируются, но не применяются."]=true,
        ["预设随游戏存档保存。空手槽会清空；法术只装备，不自动施放。"]=true,
    },
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
