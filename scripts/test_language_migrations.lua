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
    print("Legacy hint migration: known defaults, custom preservation, CRLF and idempotence passed")
end
