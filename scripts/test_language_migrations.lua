function main()
    local migrate=import("scripts.language_migrations",{rootdir=os.projectdir()})
    local source="sortKeys=T / Shift+T\nsortMove=New move\n"
    local custom="; keep me\r\nsortKeys=My F3 shortcut\r\nCustom=abc\r\n"
    local updated,n=migrate.main(custom,source)
    assert(updated==custom and n==0,"custom text must stay byte-identical")
    local legacy="; keep me\r\nsortKeys=F3: Sort mode | F4: Arrange | Save game to keep order\r\nsortMove=ARRANGE: LMB / Enter earlier | RMB later | F4 done\r\nCustom=abc\r\n"
    updated,n=migrate.main(legacy,source)
    assert(n==2 and updated=="; keep me\r\nsortKeys=T / Shift+T\r\nsortMove=New move\r\nCustom=abc\r\n","migrate known values and preserve line endings")
    local again,count=migrate.main(updated,source)
    assert(again==updated and count==0,"migration is idempotent")
    local defaults = {
        {"de.ini","combatSave","AKTUELLE HÄNDE SPEICHERN"},
        {"de.ini","cycleQuickHint","ZYKLUSTASTE: AKTUELLE HÄNDE / MARKIERTE VOREINSTELLUNG HINZUFÜGEN"},
        {"de.ini","combatOverwrite","DURCH AKTUELLE HÄNDE ERSETZEN"},
        {"de.ini","combatSlotUnavailable","Rüste zuerst etwas in diesem Platz aus."},
        {"en.ini","combatSave","SAVE HANDS"},
        {"en.ini","cycleQuickHint","CYCLE SHORTCUT: ADD CURRENT HANDS / HOVERED PRESET"},
        {"en.ini","combatOverwrite","REPLACE WITH CURRENT HANDS"},
        {"en.ini","combatSlotUnavailable","Equip an item in this slot before setting it."},
        {"fr.ini","combatSave","ENREGISTRER LES MAINS ACTUELLES"},
        {"fr.ini","cycleQuickHint","RACCOURCI DU CYCLE : AJOUTER LES MAINS / LE PRÉRÉGLAGE SURVOLÉ"},
        {"fr.ini","combatOverwrite","REMPLACER PAR LES MAINS ACTUELLES"},
        {"fr.ini","combatSlotUnavailable","Équipez un élément dans cet emplacement avant de le définir."},
        {"ja.ini","combatSave","現在の両手の組合せを保存"},
        {"ja.ini","cycleQuickHint","サイクルキー：現在の両手／選択中のプリセットを追加"},
        {"ja.ini","combatOverwrite","現在の両手で上書き"},
        {"ja.ini","combatSlotUnavailable","設定する前にこのスロットへ装備してください。"},
        {"ko.ini","combatSave","현재 양손 조합 저장"},
        {"ko.ini","cycleQuickHint","순환 키: 현재 양손 / 가리킨 프리셋 추가"},
        {"ko.ini","combatOverwrite","현재 양손 조합으로 덮어쓰기"},
        {"ko.ini","combatSlotUnavailable","이 슬롯에 먼저 장착한 뒤 설정하세요."},
        {"pt_BR.ini","combatSave","SALVAR MÃOS ATUAIS"},
        {"pt_BR.ini","cycleQuickHint","ATALHO DO CICLO: ADICIONAR MÃOS / CONJUNTO SOB O CURSOR"},
        {"pt_BR.ini","combatOverwrite","SUBSTITUIR PELAS MÃOS ATUAIS"},
        {"pt_BR.ini","combatSlotUnavailable","Equipe algo neste espaço antes de defini-lo."},
        {"ru.ini","combatSave","СОХРАНИТЬ СНАРЯЖЕНИЕ РУК"},
        {"ru.ini","cycleQuickHint","КЛАВИША ЦИКЛА: ДОБАВИТЬ РУКИ / НАБОР ПОД КУРСОРОМ"},
        {"ru.ini","combatOverwrite","ЗАМЕНИТЬ ТЕКУЩИМИ РУКАМИ"},
        {"ru.ini","combatSlotUnavailable","Сначала экипируйте предмет в этот слот."},
        {"zh_CN.ini","combatSave","保存双手组合"},
        {"zh_CN.ini","cycleQuickHint","循环组合键：加入当前双手／悬浮的预设"},
        {"zh_CN.ini","combatOverwrite","以当前双手组合覆盖"},
        {"zh_CN.ini","combatSlotUnavailable","请先在该槽位装备物品。"},
    }
    for _,entry in ipairs(defaults) do
        local source=io.readfile(path.join("assets", "Languages", entry[1]))
        local legacy=entry[2].."="..entry[3].."\r\nCustom=keep\r\n"
        local migrated,count=migrate.main(legacy,source)
        assert(count==1 and migrated:find("Custom=keep\r\n",1,true),"combat default must migrate and retain unrelated bytes")
        local repeated,n=migrate.main(migrated,source)
        assert(repeated==migrated and n==0,"combat migration is idempotent")
        local custom=entry[2].."=Custom: "..entry[3].."\r\n"
        local kept,n=migrate.main(custom,source)
        assert(kept==custom and n==0,"custom combat wording must stay byte-identical")
    end
    print("Legacy hint migration: known defaults, custom preservation, CRLF and idempotence passed")
end
