#include "InventoryPolicy.h"
#include "InventoryPages.h"
#include "Wheel.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
namespace {
    void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
}
int main() {
    using namespace Wheel;
    InventoryBudget plain{300};Check(plain.Spare()==300,"A stack with no extra lists remains visible");
    InventoryBudget mixed{12};Check(mixed.Take(2)==2 && mixed.Take(3)==3 && mixed.Spare()==7,"Special and plain counts partition the inventory without double counting");
    InventoryBudget oversubscribed{2};Check(oversubscribed.Take(5)==2 && oversubscribed.Take(1)==0 && oversubscribed.Spare()==0,"Tracked counts cannot exceed physical inventory");
    InventoryBudget damaged{3};Check(damaged.Take(0)==1 && damaged.Take(-2)==1 && damaged.Spare()==1,"Invalid per-instance counts remain bounded");
    Check(!InventoryVisible(false,false) && InventoryVisible(false,true) && InventoryVisible(true,false),"Item source preserves the original favorites gate by default");
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
    for(int i=0;i<10000;++i){int page=i%1001;auto slice=catalog.Page(Category::Armor,page);Check(slice.items.size()<=slots,"Snapshots remain bounded with 60018 catalog entries");checksum+=slice.items.front().key.form;}
    std::cout<<"60018-entry synthetic catalog: 10000 page snapshots in "
        <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms, checksum="<<checksum<<'\n';
    catalog.Set({});int page=9;auto empty=catalog.Page(Category::Food,page);
    Check(page==0 && empty.items.empty() && empty.total==0,"Empty category returns a valid empty page");
    View preview;preview.page=2;preview.items.resize(23);Check(ItemCount(preview)==23 && PageItemIndex(preview,2)==22,"Existing full-list preview snapshots retain their indexing");
    std::cout<<"Inventory count partitioning, source gates, large paging and exact selection tests passed\n";
}
