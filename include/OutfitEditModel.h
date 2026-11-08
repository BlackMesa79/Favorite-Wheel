#pragma once
#include "OutfitModel.h"
namespace Wheel::Outfits::Edit {
    enum class Filter { All, Head, Body, Hands, Feet, Jewelry, Other };
    inline constexpr const char* filterKeys[]{"outfitEditAll","outfitEditHead","outfitEditBody","outfitEditHands","outfitEditFeet","outfitEditJewelry","outfitEditOther"};
    struct Row {
        Piece piece;
        std::uint32_t runtimeMask=0;
        bool worn=false;
        std::uint32_t Mask() const {return piece.mask?piece.mask:runtimeMask;}
    };
    inline Filter Category(std::uint32_t mask) {
        switch(PartOf(mask)) {
        case Part::Head:return Filter::Head;case Part::Body:return Filter::Body;
        case Part::Hands:return Filter::Hands;case Part::Feet:return Filter::Feet;
        case Part::Ring:case Part::Amulet:return Filter::Jewelry;default:return Filter::Other;
        }
    }
    inline bool Matches(const Row& row,Filter filter){return filter==Filter::All || Category(row.Mask())==filter;}
    inline bool Contains(const std::vector<Row>& rows,const Piece& piece) {
        return std::any_of(rows.begin(),rows.end(),[&](const Row& row){return Same(row.piece,piece);});
    }
    enum class Result { Added, Duplicate, Limit };
    inline Result Add(std::vector<Row>& draft,const Row& row,std::vector<std::string>& replaced) {
        replaced.clear();
        if(Contains(draft,row.piece))return Result::Duplicate;
        const auto conflicts=std::count_if(draft.begin(),draft.end(),[&](const Row& old){return (old.Mask()&row.Mask())!=0;});
        if(draft.size()-conflicts>=64)return Result::Limit;
        std::erase_if(draft,[&](const Row& old){if(!(old.Mask()&row.Mask()))return false;replaced.push_back(old.piece.label);return true;});
        draft.push_back(row);return Result::Added;
    }
    inline bool Erase(std::vector<Row>& draft,std::size_t index) {
        if(index>=draft.size())return false;draft.erase(draft.begin()+index);return true;
    }
    inline bool Save(const Preset& original,const std::vector<Row>& draft,Preset& result) {
        if(draft.empty() || draft.size()>64)return false;
        auto next=original;next.pieces.clear();
        for(const auto& row:draft)next.pieces.push_back(row.piece);
        std::vector<Preset> validated;
        if(!Decode(Encode({next}),validated))return false;
        result=std::move(validated.front());return true;
    }
    inline constexpr int visibleRows=5;
    struct State {
        Preset original;
        std::vector<Row> draft,inventory;
        std::vector<std::size_t> available;
        Filter filter=Filter::All;
        int column=0,selected[2]{0,0},scroll[2]{0,0};
        bool loading=false,saving=false;
        std::string messageKey,replaced;
        int clickedColumn=-1,clickedRow=-1;
        double clickedAt=0;
        int Count(int col) const {return static_cast<int>(col?available.size():draft.size());}
        void Clamp(bool reveal=false) {
            for(int col=0;col<2;++col) {
                selected[col]=std::clamp(selected[col],0,std::max(0,Count(col)-1));
                scroll[col]=std::clamp(scroll[col],0,std::max(0,Count(col)-visibleRows));
                if(reveal && col==column)scroll[col]=std::clamp(scroll[col],std::max(0,selected[col]-visibleRows+1),selected[col]);
            }
        }
        void Refresh() {
            available.clear();
            for(std::size_t i=0;i<inventory.size();++i)if(Matches(inventory[i],filter) && !Contains(draft,inventory[i].piece))available.push_back(i);
            Clamp();clickedColumn=clickedRow=-1;
        }
        void Execute() {
            if(loading || saving)return;
            replaced.clear();messageKey.clear();
            if(column==0)Erase(draft,selected[0]);
            else if(selected[1]<Count(1)) {
                std::vector<std::string> names;
                const auto result=Add(draft,inventory[available[selected[1]]],names);
                if(result==Result::Limit)messageKey="outfitEditCountError";
                else if(result==Result::Duplicate)messageKey="outfitEditDuplicate";
                else if(!names.empty()) {messageKey="outfitEditReplaced";for(const auto& name:names){if(!replaced.empty())replaced+=", ";replaced+=name;}}
            }
            Refresh();Clamp(true);
        }
    };
}
