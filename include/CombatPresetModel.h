#pragma once
#include "OutfitModel.h"
#include "QuickSlots.h"
#include <array>

namespace Wheel::Combat {
    using Outfits::Piece;
    using Outfits::FormRef;
    enum class Kind {Empty, Object, Spell, Voice, Ignore};
    enum class Slot {Right, Left, Voice};
    struct Target {
        Kind kind=Kind::Empty;
        Piece piece;
        Kind remembered=Kind::Empty; // Retained item type while Ignore/Empty is active.
        bool operator==(const Target&) const=default;
    };
    struct Preset {
        std::uint32_t id=0;
        std::string name;
        Target right,left;
        bool twoHanded=false;
        int quickSlot=-1; // Shared native keyboard slots 1..8; stored per character.
        Target voice;
        bool operator==(const Preset&) const=default;
    };
    inline bool IsSet(Kind kind) {return kind==Kind::Object || kind==Kind::Spell || kind==Kind::Voice;}
    inline Kind SavedKind(const Target& target) {return IsSet(target.kind)?target.kind:target.remembered;}
    inline bool TwoHanded(const Preset& p) {return p.twoHanded && IsSet(p.right.kind);}
    inline bool HandednessMatches(const Preset& p,Slot slot,bool both) {
        return slot==Slot::Voice || both==(TwoHanded(p) && slot==Slot::Right);
    }
    enum class Problem {None, Missing, Changed, Ambiguous, NotEnough};
    struct Resolution {
        std::array<int,2> index{-1,-1}; // right, left; spells/empty have no inventory index.
        Problem problem=Problem::None;
        bool left=false;
        explicit operator bool() const {return problem==Problem::None;}
    };
    template<class Candidate> Resolution Resolve(const Preset& preset,const std::vector<Candidate>& inventory) {
        Resolution result;
        std::vector<int> used(inventory.size());
        const std::array<const Target*,2> targets{&preset.right,&preset.left};
        for(int hand=0;hand<2;++hand) {
            const auto& target=*targets[hand];if(target.kind!=Kind::Object)continue;
            int found=-1,matching=0;bool base=false,signature=false;
            for(int i=0;i<static_cast<int>(inventory.size());++i) {
                const auto& candidate=inventory[i];
                base=base || candidate.piece.base==target.piece.base;
                if(!Outfits::Same(target.piece,candidate.piece))continue;
                signature=true;++matching;
                if(candidate.count<=used[i])continue;
                const bool worn=hand?candidate.left:candidate.right;
                if(found<0 || (worn && !(hand?inventory[found].left:inventory[found].right)))found=i;
            }
            if(target.piece.unique && matching>1)result.problem=Problem::Ambiguous;
            else if(found<0)result.problem=signature?Problem::NotEnough:base?Problem::Changed:Problem::Missing;
            else if(target.piece.unique && used[found])result.problem=Problem::NotEnough;
            if(result.problem!=Problem::None){result.left=hand!=0;return result;}
            result.index[hand]=found;++used[found];
        }
        return result;
    }
    inline bool Valid(const Preset& p) {
        if(!p.id || p.id>1000000 || !Outfits::ValidName(p.name))return false;
        if(p.quickSlot!=-1 && !ValidQuickSlot(p.quickSlot))return false;
        if(p.twoHanded && (SavedKind(p.right)!=Kind::Object && SavedKind(p.right)!=Kind::Spell))return false;
        if(TwoHanded(p) && IsSet(p.left.kind))return false;
        for(const auto* target:{&p.right,&p.left,&p.voice}) {
            if(target->kind<Kind::Empty || target->kind>Kind::Ignore)return false;
            if(target->remembered!=Kind::Empty && (!IsSet(target->remembered) || IsSet(target->kind)))return false;
            const auto kind=SavedKind(*target);
            if(kind==Kind::Empty) {
                if(target->piece!=Piece{})return false;
                continue;
            }
            const auto& x=target->piece;
            if(!x.base.id || x.base.plugin.size()>=260 || x.enchantment.plugin.size()>=260 || x.owner.plugin.size()>=260 ||
                !std::isfinite(x.health) || x.health<=0 || x.customName.size()>512 || x.label.size()>512)return false;
            if((kind==Kind::Spell || kind==Kind::Voice) &&
                (x.unique || x.enchantment.id || x.owner.id || x.health!=1.f || !x.customName.empty()))return false;
            if(target==&p.voice && kind!=Kind::Voice && kind!=Kind::Empty)return false;
            if(target!=&p.voice && kind==Kind::Voice)return false;
        }
        return true;
    }
    // Edit a copy so an incompatible two-handed/left-hand combination cannot
    // partially mutate the stored preset. Never inspect current equipment.
    inline bool CycleSlotMode(Preset& p,Slot slot) {
        auto next=p;
        auto& target=slot==Slot::Right?next.right:slot==Slot::Left?next.left:next.voice;
        if(IsSet(target.kind)){target.remembered=target.kind;target.kind=Kind::Ignore;}
        else if(target.kind==Kind::Ignore)target.kind=Kind::Empty;
        else if(IsSet(target.remembered)){target.kind=target.remembered;target.remembered=Kind::Empty;}
        else target.kind=Kind::Ignore; // No saved item: skip Set until explicit recapture.
        if(!Valid(next))return false;
        p=std::move(next);return true;
    }
    inline std::string Encode(const std::vector<Preset>& presets) {
        std::ostringstream out;out.imbue(std::locale::classic());
        out<<"FWC 3 "<<presets.size()<<'\n'<<std::setprecision(std::numeric_limits<float>::max_digits10);
        auto ref=[&](const FormRef& r){out<<std::quoted(r.plugin)<<' '<<r.id<<' ';};
        for(const auto& p:presets) {
            out<<p.id<<' '<<std::quoted(p.name)<<' '<<p.twoHanded<<'\n';
            for(const auto* target:{&p.right,&p.left,&p.voice}) {
                const auto& x=target->piece;out<<static_cast<int>(target->kind)<<' '<<static_cast<int>(target->remembered)<<' ';
                ref(x.base);ref(x.enchantment);ref(x.owner);
                out<<x.unique<<' '<<x.health<<' '<<std::quoted(x.customName)<<' '<<std::quoted(x.label)<<'\n';
            }
        }
        return out.str();
    }
    inline bool Decode(const std::string& text,std::vector<Preset>& result,unsigned expectedVersion=0) {
        if(text.size()>1024*1024)return false;
        std::istringstream in(text);in.imbue(std::locale::classic());std::string magic;unsigned version=0,count=0;
        if(!(in>>magic>>version>>count) || magic!="FWC" || (version<1 || version>3) ||
            (expectedVersion && expectedVersion!=version) || count>100)return false;
        std::vector<Preset> next;
        auto ref=[&](FormRef& r){return bool(in>>std::quoted(r.plugin)>>r.id) && r.plugin.size()<260;};
        for(unsigned i=0;i<count;++i) {
            Preset p;int twoHanded=0;
            if(!(in>>p.id>>std::quoted(p.name)>>twoHanded) || twoHanded<0 || twoHanded>1)return false;
            p.twoHanded=twoHanded!=0;
            for(auto* target:{&p.right,&p.left,&p.voice}) {
                if(version==1 && target==&p.voice)continue;
                unsigned kind=0,remembered=0,unique=0;auto& x=target->piece;
                if(!(in>>kind) || kind>(version==1?2:4))return false;
                if(version>=3 && (!(in>>remembered) || remembered>3))return false;
                if(!ref(x.base) || !ref(x.enchantment) || !ref(x.owner) ||
                    !(in>>unique>>x.health>>std::quoted(x.customName)>>std::quoted(x.label)) || unique>65535)return false;
                target->kind=static_cast<Kind>(kind);target->remembered=static_cast<Kind>(remembered);
                x.unique=static_cast<std::uint16_t>(unique);
            }
            if(!Valid(p) || std::any_of(next.begin(),next.end(),[&](const auto& other){return other.id==p.id;}))return false;
            next.push_back(std::move(p));
        }
        in>>std::ws;if(!in.eof())return false;result=std::move(next);return true;
    }
    struct SlotBinding {
        std::uint32_t id=0;int slot=-1;
        bool operator==(const SlotBinding&) const=default;
    };
    inline std::string EncodeBindings(const std::vector<Preset>& presets) {
        const auto count=std::count_if(presets.begin(),presets.end(),[](const auto& p){return ValidQuickSlot(p.quickSlot);});
        std::ostringstream out;out.imbue(std::locale::classic());out<<"FWQ 1 "<<count<<'\n';
        for(const auto& p:presets)if(ValidQuickSlot(p.quickSlot))out<<p.id<<' '<<p.quickSlot<<'\n';
        return out.str();
    }
    inline bool DecodeBindings(const std::string& text,std::vector<SlotBinding>& result) {
        if(text.size()>1024)return false;
        std::istringstream in(text);in.imbue(std::locale::classic());std::string magic;unsigned version=0,count=0;
        if(!(in>>magic>>version>>count) || magic!="FWQ" || version!=1 || count>nativeQuickSlots)return false;
        std::vector<SlotBinding> next;
        for(unsigned i=0;i<count;++i) {
            SlotBinding binding;
            if(!(in>>binding.id>>binding.slot) || !binding.id || binding.id>1000000 || !ValidQuickSlot(binding.slot) ||
                std::any_of(next.begin(),next.end(),[&](const auto& other){return other.id==binding.id || other.slot==binding.slot;}))return false;
            next.push_back(binding);
        }
        in>>std::ws;if(!in.eof())return false;result=std::move(next);return true;
    }
    inline void ApplyBindings(std::vector<Preset>& presets,const std::vector<SlotBinding>& bindings) {
        for(auto& p:presets) {
            p.quickSlot=-1;
            for(const auto& binding:bindings)if(binding.id==p.id){p.quickSlot=binding.slot;break;}
        }
    }
    inline std::uint32_t PresetForQuickSlot(const std::vector<Preset>& presets,int slot) {
        if(!ValidQuickSlot(slot))return 0;
        for(const auto& p:presets)if(p.quickSlot==slot)return p.id;
        return 0;
    }
    inline bool AssignQuickSlot(std::vector<Preset>& presets,std::uint32_t id,int slot,int* assigned=nullptr) {
        if(!ValidQuickSlot(slot))return false;
        auto selected=std::find_if(presets.begin(),presets.end(),[&](const auto& p){return p.id==id;});
        if(selected==presets.end())return false;
        const int next=AssignedQuickSlot(selected->quickSlot,slot);
        for(auto& p:presets)p.quickSlot=ReassignedQuickSlot(p.quickSlot,p.id==id,slot,next);
        if(assigned)*assigned=next;
        return true;
    }
    inline bool CombatQuickSlotMayRun(bool open,bool closing,bool queued,bool blocked,int modifiers,bool wheelEntrance) {
        return !open && !closing && !queued && !blocked && modifiers==0 && !wheelEntrance;
    }
    // Explicit restore, never toggle. Engine calls are issued once and their
    // observable completion is checked before advancing to the next hand.
    enum class Step {ClearRight, ClearLeft, ClearVoice, EquipRight, EquipLeft, EquipVoice, Verify};
    enum class Decision {Issue, Wait, Advance, Timeout};
    inline bool SameTarget(const Target& a,const Target& b) {
        return a.kind==Kind::Ignore || (a.kind==b.kind && (a.kind==Kind::Empty ||
            (a.kind==Kind::Spell || a.kind==Kind::Voice?a.piece.base==b.piece.base:Outfits::Same(a.piece,b.piece))));
    }
    inline bool StepComplete(const Preset& p,Step step,const Target& actual,bool callbackPending=false) {
        if(callbackPending)return false;
        if(step==Step::Verify)return false;
        if(step==Step::ClearVoice)return p.voice.kind!=Kind::Empty || actual.kind==Kind::Empty;
        if(step==Step::EquipVoice)return p.voice.kind==Kind::Ignore || SameTarget(p.voice,actual);
        const bool left=step==Step::ClearLeft || step==Step::EquipLeft;
        const auto& desired=left?p.left:p.right;
        const bool clearing=step==Step::ClearRight || step==Step::ClearLeft;
        return desired.kind==Kind::Ignore || SameTarget(desired,actual) || (clearing && (actual.kind==Kind::Empty ||
            (actual.kind==Kind::Spell && (desired.kind!=Kind::Empty || TwoHanded(p))))) ||
            (!clearing && left && TwoHanded(p));
    }
    inline Decision Decide(bool complete,bool issued,double elapsed) {
        if(complete)return Decision::Advance;
        if(!issued)return Decision::Issue;
        return elapsed>=3.?Decision::Timeout:Decision::Wait;
    }
}
