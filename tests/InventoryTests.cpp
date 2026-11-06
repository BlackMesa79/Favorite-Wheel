#include "InventoryPolicy.h"
#include "InventoryPages.h"
#include "AlchemyCategories.h"
#include "Wheel.h"
#include "MiscDisplayStack.h"
#include "FormMode.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
namespace {
    void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
}
int main() {
    using namespace Wheel;
    {
        Check(ClassifyFormMode(false,false)==FormMode::Human,"Humanoid vampires retain normal behavior");
        Check(ClassifyFormMode(true,false,true)==FormMode::Werewolf,"Werewolves have a dedicated restricted mode");
        Check(ClassifyFormMode(false,false,true)==FormMode::Werewolf,"Werewolf race detection does not depend on delayed menu state");
        Check(ClassifyFormMode(true,false)==FormMode::UnsupportedBeast,"Other beast forms still yield to vanilla");
        Check(ClassifyFormMode(false,true)==FormMode::VampireLord && ClassifyFormMode(true,true)==FormMode::VampireLord,
            "Vampire race restricts actions even before the menu beast flag arrives");
        for(auto mode:{FormMode::Human,FormMode::VampireLord,FormMode::Werewolf,FormMode::UnsupportedBeast})
            for(bool favorite:{false,true})for(bool spell:{false,true})
                for(bool voice:{false,true})
                Check(FormActionAllowed(mode,favorite,spell,voice)==(mode==FormMode::Human || (mode==FormMode::VampireLord && favorite && spell) ||
                    (mode==FormMode::Werewolf && favorite && spell && voice)),"Beast-form actions stay within their explicit allowlists");
        std::vector<Item> rows;MiscDisplayStacks groups;
        for(unsigned form=1;form<=3;++form)for(unsigned copy=1;copy<=30;++copy) {
            Item item;item.key={form,copy,static_cast<std::uint16_t>(copy)};item.name="Instrument";item.count=1;item.usable=false;
            groups.Append(rows,item,true,255,0);
        }
        Check(rows.size()==3 && rows[0].count==30 && rows[1].count==30 && rows[2].count==30,
            "Ninety separately identified drums/flutes/lutes produce three rows without losing counts");
        Check(rows[0].miscDisplayStack && rows[0].key.extra==1,"Display aggregate retains an actual native anchor");
        auto special=rows[0];special.count=1;special.miscDisplayStack=false;
        groups.Append(rows,special,true,1,0);groups.Append(rows,special,true,255,42);
        special.name="Named instrument";groups.Append(rows,special,true,255,0);
        special.name="Instrument";groups.Append(rows,special,false,255,0);groups.Append(rows,special,false,255,0);
        special.favorited=false;groups.Append(rows,special,true,-1,0);
        Check(rows.size()==9 && rows[0].count==30,"Bindings, ownership, names, unknown extras and favorite state stay separate");
        special.key.form=4;special.count=250;special.favorited=true;
        groups.Append(rows,special,true,255,0);
        Check(rows.back().count==250 && !rows.back().miscDisplayStack,"An existing ore stack stays one row with its original quantity");
        special.count=std::numeric_limits<int>::max();groups.Append(rows,special,true,255,0);
        Check(rows.back().count==std::numeric_limits<int>::max(),"Aggregated counts saturate without signed overflow");
    }
    {
        // A single favorited copy must retain the physical total and the exact action anchor.
        for(bool favoriteFirst:{false,true}) {
            std::vector<Item> rows;MiscDisplayStacks groups;
            for(unsigned form:{0xDABA9u,0xDABA7u}) {
                const int copies=form==0xDABA9u?3:2;
                for(int copy=0;copy<copies;++copy) {
                    const bool favorite=copy==(favoriteFirst?0:copies-1);
                    Item item;item.key={form,static_cast<std::uintptr_t>(form+copy),static_cast<std::uint16_t>(copy)};
                    item.name=form==0xDABA9u?"Drum":"Flute";item.count=1;item.favorited=favorite;item.scriptedInstrument=true;
                    groups.Append(rows,item,true,favorite?255:-1,0,true);
                }
            }
            Check(rows.size()==2 && rows[0].count==3 && rows[1].count==2,
                "Three drums and two flutes retain their physical totals with only one favorite per form");
            Check(rows[0].favorited && rows[1].favorited && rows[0].miscDisplayStack && rows[1].miscDisplayStack,
                "An unfavorited copy encountered first cannot hide the aggregate in favorite-only scope");
            Check(rows[0].key.extra==0xDABA9u+(favoriteFirst?0:2) && rows[1].key.extra==0xDABA7u+(favoriteFirst?0:1),
                "The exact favorited native anchor survives either collection order");
        }
        std::vector<Item> rows;MiscDisplayStacks groups;
        Item item;item.key.form=0xDABA9;item.name="Drum";item.count=1;item.favorited=true;
        groups.Append(rows,item,true,255,0,true);
        auto ordinary=item;ordinary.favorited=false;ordinary.key.extra=17;ordinary.count=2;
        groups.Append(rows,ordinary,true,-1,0,true);
        Check(rows.size()==1 && rows[0].count==3,"Untracked ordinary copies join the favorite display pool");
        groups.Append(rows,item,true,1,0,true);groups.Append(rows,item,true,250,0,true);
        groups.Append(rows,ordinary,true,-1,42,true);
        ordinary.name="Named drum";groups.Append(rows,ordinary,true,-1,0,true);
        groups.Append(rows,item,false,255,0,true);
        Check(rows.size()==6 && rows[0].count==3,
            "Instrument grouping preserves independent bindings, Important markers, ownership, names and unknown extras");
    }
    InventoryBudget plain{300};Check(plain.Spare()==300,"A stack with no extra lists remains visible");
    InventoryBudget mixed{12};Check(mixed.Take(2)==2 && mixed.Take(3)==3 && mixed.Spare()==7,"Special and plain counts partition the inventory without double counting");
    InventoryBudget oversubscribed{2};Check(oversubscribed.Take(5)==2 && oversubscribed.Take(1)==0 && oversubscribed.Spare()==0,"Tracked counts cannot exceed physical inventory");
    InventoryBudget damaged{3};Check(damaged.Take(0)==1 && damaged.Take(-2)==1 && damaged.Spare()==1,"Invalid per-instance counts remain bounded");
    Check(!InventoryVisible(false,false) && InventoryVisible(false,true) && InventoryVisible(true,false),"Item source preserves the original favorites gate by default");
    for(int slot=0;slot<256;++slot) {
        Check(InventoryFavorite(true,slot,false),"Without the Important provider, extension marker values keep their original favorite semantics");
        Check(InventoryFavorite(true,slot,true)==(slot!=0xFA),"Only the provider's Important marker is excluded, preserving unbound and extension favorites");
    }
    Check(!InventoryFavorite(false,-1,true) && !InventoryFavorite(false,-1,false),"Unfavorited copies stay unfavorited with either integration state");
    const bool important=InventoryFavorite(true,0xFA,true);
    Check(!InventoryVisible(false,important) && InventoryVisible(true,important),"Important items are hidden from favorites but usable in all-inventory mode");
    Check(!important,"Important copies cannot pass the favorite-only quick-slot mutation guard");
    Item stack;stack.weaponStack=true;stack.weaponStackHotkey=255;stack.quickSlot=-1;
    SetQuickSlotSnapshot(stack,2);
    Check(stack.quickSlot==2 && stack.weaponStackHotkey==2,"Binding a plain weapon updates its logical stack marker in the open wheel");
    SetQuickSlotSnapshot(stack,-1);
    Check(stack.weaponStackHotkey==255,"Clearing an item binding or yielding it to combat preserves favorite stack re-resolution");
    stack.weaponStackHotkey=250;SetQuickSlotSnapshot(stack,-1);
    Check(stack.weaponStackHotkey==250,"An unchanged native slot never resets an extension marker");
    {
        Item before;before.key={0x13989,0x100};before.weaponStack=true;before.weaponStackHotkey=255;before.count=2;
        auto after=before;after.key.extra=0x200;after.handEquip=HandEquip::Right;after.equipped=true;
        Check(before.key!=after.key && SelectionKey(before)==SelectionKey(after),
            "First steel-sword equip can migrate the native anchor without resetting the selected sector");
        after.count=1;
        Check(SelectionKey(before)==SelectionKey(after),"A surviving plain stack retains aim across quantity changes too");
        after.weaponStackHotkey=2;
        Check(SelectionKey(before)!=SelectionKey(after),"A different independently bound stack at the same sector resets aim");
        after=before;after.key.form=0x12EB7;
        Check(SelectionKey(before)!=SelectionKey(after),"A different weapon replacing the sector still resets aim");
        after=before;after.weaponStack=false;
        Check(SelectionKey(before)!=SelectionKey(after),"A plain pool replaced by an exact modified instance resets aim even at the same address");
        before.weaponStack=false;after=before;after.key.extra=0x200;
        Check(SelectionKey(before)!=SelectionKey(after),"Distinct exact instances keep the existing pointer-and-variant guard");
        after=before;after.key.health=1.2f;
        Check(SelectionKey(before)!=SelectionKey(after),"A changed exact variant is not mistaken for the prior hovered item");
    }
    InventoryBudget protectedCopies{5};
    protectedCopies.Take(2);protectedCopies.Take(1);
    Check(protectedCopies.Spare()==2,"Hidden Important copies still consume the physical count budget before visibility filtering");
    InventoryPages catalog;std::vector<Item> items;
    for(int category=0;category<categoryCount;++category)for(int i=0;i<10003;++i) {
        Item item;item.key.form=static_cast<unsigned>(category*10003+i+1);item.category=static_cast<Category>(category);
        item.name="Synthetic long inventory item name " + std::to_string(i);items.push_back(std::move(item));
    }
    catalog.Set(std::move(items));
    for(int category=0;category<categoryCount;++category) {
        int page=10000;auto last=catalog.Page(static_cast<Category>(category),page);
        Check(page==1000 && last.total==10003 && last.offset==10000 && last.items.size()==3,"Last page is clamped and contains only its remaining entries");
        Check(last.items[0].key.form==category*10003+10001,"Page selection stays in the correct category");
        View view;view.page=page;view.itemOffset=last.offset;view.totalItems=last.total;view.items=last.items;
        Check(ItemCount(view)==10003 && PageItemIndex(view,2)==2,"Renderer/action indices address visible page items, not the whole category");
        Check(view.items[PageItemIndex(view,2)].key==catalog.items[catalog.Index(static_cast<Category>(category),page,2)].key,"Hover detail cache resolves the exact selected catalog instance");
    }
    const auto start=std::chrono::steady_clock::now();std::uint64_t checksum=0;
    for(int i=0;i<10000;++i){int page=i%1001;auto slice=catalog.Page(Category::Armor,page);Check(slice.items.size()<=slots,"Snapshots remain bounded with the expanded category catalog");checksum+=slice.items.front().key.form;}
    std::cout<<catalog.items.size()<<"-entry synthetic catalog: 10000 page snapshots in "
        <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms, checksum="<<checksum<<'\n';
    {
        std::vector<Item> alchemy;
        for(int i=0;i<23;++i) {
            Item item;item.key.form=i+1;
            item.category=AlchemyCategory(i>=2,i==1);alchemy.push_back(item);
        }
        std::stable_sort(alchemy.begin(),alchemy.end(),[](const Item& a,const Item& b){return a.category<b.category;});
        catalog.Set(alchemy);int potionPage=0,poisonPage=2,foodPage=0;
        const auto potions=catalog.Page(Category::Potions,potionPage),poisons=catalog.Page(Category::Poisons,poisonPage),food=catalog.Page(Category::Food,foodPage);
        Check(potions.total==1 && potions.items[0].key.form==1 && poisons.total==21 && poisons.items.size()==1 &&
            poisons.items[0].key.form==23 && food.total==1 && food.items[0].key.form==2,"Mixed alchemy catalog separates poison pages without leaking into potions or food");
        Check(catalog.visible.Move(Category::Potions,1)==Category::Poisons && catalog.visible.Move(Category::Poisons,1)==Category::Food,
            "Poison category navigates between potions and food");
        alchemy.erase(std::remove_if(alchemy.begin(),alchemy.end(),[](const Item& item){return item.category==Category::Poisons;}),alchemy.end());
        catalog.Set(alchemy);
        Check(!catalog.visible.Contains(Category::Poisons) && catalog.visible.Select(Category::Poisons)==Category::Food &&
            catalog.visible.Move(Category::Potions,1)==Category::Food,"Removing the final poison hides its category and selects the next populated type");
    }
    catalog.Set({});int page=9;auto empty=catalog.Page(Category::Food,page);
    Check(page==0 && empty.items.empty() && empty.total==0,"Empty category returns a valid empty page");
    Check(catalog.visible.Count()==0 && catalog.visible.Move(Category::Food,1)==Category::Food,
        "An entirely empty inventory has no categories and navigation is stable");
    {
        std::vector<Item> sparse;
        for(auto category:{Category::Armor,Category::Potions,Category::Powers}) {
            Item item;item.category=category;sparse.push_back(item);
        }
        catalog.Set(sparse);
        const auto showAll=CategoryVisibility(catalog.visible,false);
        Check(showAll.Count()==categoryCount && showAll.Move(Category::Armor,1)==Category::Potions &&
            showAll.Move(Category::Potions,1)==Category::Poisons && showAll.Move(Category::Poisons,1)==Category::Food && showAll.Select(Category::Spells)==Category::Spells,
            "Disabling empty hiding restores all original category navigation and empty selections");
        Check(CategoryVisibility(catalog.visible,true).mask==catalog.visible.mask,"Enabled hiding uses the populated directory");
        Check(catalog.visible.Count()==3 && catalog.visible.At(0)==Category::Armor && catalog.visible.At(2)==Category::Powers,
            "A sparse catalog publishes only populated types in original order");
        Check(catalog.visible.Move(Category::Armor,1)==Category::Potions && catalog.visible.Move(Category::Armor,-1)==Category::Powers,
            "Navigation skips every hidden category and wraps in both directions");
        Check(catalog.visible.Select(Category::Potions)==Category::Potions,
            "A still populated remembered category survives a refresh");
        sparse.erase(sparse.begin()+1);catalog.Set(sparse);
        Check(catalog.visible.Select(Category::Potions)==Category::Powers,
            "Consuming the final entry selects the next populated category");
        sparse.erase(sparse.begin());catalog.Set(sparse);
        Check(catalog.visible.Move(Category::Powers,1)==Category::Powers && catalog.visible.Move(Category::Powers,-1)==Category::Powers,
            "One populated category never navigates away");
        Item returned;returned.category=Category::Potions;sparse.insert(sparse.begin(),returned);catalog.Set(sparse);
        Check(catalog.visible.Contains(Category::Potions) && catalog.visible.Count()==2,
            "An added item restores its previously hidden category on the next directory refresh");
        for(unsigned mask=0;mask<(1u<<categoryCount);++mask) {
            VisibleCategories visible{mask};
            for(int type=0;type<categoryCount;++type) {
                const auto current=static_cast<Category>(type),selected=visible.Select(current);
                if(!mask){Check(selected==current,"Empty catalog preserves session type");continue;}
                Check(visible.Contains(selected),"Selection always recovers into a visible category");
                Check(visible.At(visible.Index(selected))==selected,"Ribbon index maps back to the visible enum");
                Check(visible.Move(visible.Move(selected,1),-1)==selected,"Filtered navigation is reversible for every subset");
            }
        }
    }
    View preview;preview.page=2;preview.items.resize(23);Check(ItemCount(preview)==23 && PageItemIndex(preview,2)==22,"Existing full-list preview snapshots retain their indexing");
    std::cout<<"Inventory count partitioning, source gates, large paging and exact selection tests passed\n";
}
