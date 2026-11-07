#pragma once
#include "Favorites.h"
#include <map>
#include <tuple>
#include <limits>
namespace Wheel {
    // Display-only grouping: never merge or mutate native ExtraDataLists.
    class MiscDisplayStacks {
        using Key=std::tuple<std::uint32_t,std::string,bool,int,std::uint32_t>;
        std::map<Key,std::size_t> rows;
    public:
        void Append(std::vector<Item>& output,Item item,bool compatible,int marker,std::uint32_t owner,bool mergeUnboundFavorites=false) {
            if(compatible) {
                // Ordinary instruments share an unbound display pool; preserve separate bindings and extension markers.
                const bool neutral=mergeUnboundFavorites && (marker==-1 || marker==255);
                Key key{item.key.form,item.name,neutral?false:item.favorited,neutral?-1:marker,owner};
                auto [it,inserted]=rows.emplace(std::move(key),output.size());
                if(!inserted) {
                    auto& row=output[it->second];
                    const auto count=static_cast<int>(std::min<std::int64_t>(std::int64_t(row.count)+item.count,std::numeric_limits<int>::max()));
                    // Keep a real favorited action anchor regardless of collection order.
                    if(item.favorited && !row.favorited)row=std::move(item);
                    row.count=count;row.miscDisplayStack=true;
                    return;
                }
            }
            output.push_back(std::move(item));
        }
    };
}
