#include "OutfitModel.h"
#include "OutfitEditModel.h"
#include "OutfitStep.h"
#include "OutfitPlan.h"
#include <cstdlib>
#include <iostream>
#include <clocale>
using namespace Wheel::Outfits;
void Check(bool result,const char* label){if(!result){std::cerr<<label<<'\n';std::exit(1);}}
struct Candidate{Piece piece;bool worn=false;std::uint32_t mask=0;};
int main(){
    {
        using namespace Edit;
        auto row=[](unsigned id,std::uint32_t mask,std::uint32_t runtime=0) {
            Piece p;p.base={"Skyrim.esm",id};p.label="Piece "+std::to_string(id);p.mask=mask;
            return Row{p,runtime,false};
        };
        const auto head=row(1,1),body=row(2,4),oldBody=row(3,0,4),hands=row(4,8);
        Preset original{12,"Unchanged name",{head.piece,oldBody.piece}};
        original.loose=true;original.parts.fill(PartMode::Ignore);original.pieces[0].optional=true;
        const auto before=original;
        std::vector<Row> draft{{original.pieces[0],1,false},oldBody};std::vector<std::string> replaced;
        Check(Add(draft,hands,replaced)==Result::Added && draft.size()==3 && replaced.empty(),"Editor adds non-conflicting instance");
        Preset retained;
        Check(Save(original,draft,retained) && retained.pieces[0].optional && retained.pieces[1].mask==0 && retained.parts==original.parts && retained.loose,"Unchanged pieces preserve optional and zero mask without adding rule behavior");
        Check(Add(draft,hands,replaced)==Result::Duplicate && draft.size()==3,"Editor rejects Same identity");
        Check(Add(draft,body,replaced)==Result::Added && draft.size()==3 && replaced==std::vector<std::string>{oldBody.piece.label},"Legacy zero mask resolves conflict using runtime base armor mask");
        auto multi=row(5,1|4|8);
        Check(Add(draft,multi,replaced)==Result::Added && draft.size()==1 && replaced.size()==3,"All overlapping pieces are replaced");
        Check(!Erase(draft,4) && Erase(draft,0) && draft.empty(),"Erase validates index and allows empty draft");
        Preset result=original;
        Check(!Save(original,draft,result) && result==before,"Empty draft refuses save transactionally");
        Check(original==before,"Discarding a draft leaves original unchanged");
        for(unsigned i=1;i<=64;++i)Check(Add(draft,row(i,0),replaced)==Result::Added,"Editor accepts up to 64 pieces");
        const auto full=draft;
        Check(Add(draft,row(65,0),replaced)==Result::Limit && draft.size()==64 && draft.front().piece==full.front().piece,"65th piece rejected without mutation");
        draft[0].runtimeMask=4;
        Check(Add(draft,body,replaced)==Result::Duplicate,"Duplicate rejected before conflict replacement");
        Check(Add(draft,row(66,4),replaced)==Result::Added && draft.size()==64,"Replacement at 64-piece cap succeeds");
        Check(Save(original,draft,result) && result.id==original.id && result.name==original.name && result.parts==original.parts && result.loose==original.loose,"Save preserves identity and legacy rules");
        std::vector<Preset> decoded;
        Check(Decode(Encode({result}),decoded) && decoded[0]==result,"Edited preset roundtrips exactly");
        draft.push_back(row(67,0));Check(!Save(original,draft,result),"Oversized draft refuses save");
        for(const auto [mask,filter]:std::vector<std::pair<std::uint32_t,Filter>>{{1,Filter::Head},{4,Filter::Body},{8,Filter::Hands},{128,Filter::Feet},{64,Filter::Jewelry},{32,Filter::Jewelry},{1u<<20,Filter::Other},{0,Filter::Other},{5,Filter::Body}}) {
            const auto candidate=row(1,0,mask);
            for(int f=0;f<7;++f)Check(Matches(candidate,static_cast<Filter>(f))==(f==0 || f==static_cast<int>(filter)),"Filters follow PartOf including jewelry and runtime fallback");
        }
        State state;state.original=before;state.draft={{before.pieces[0],1,false}};
        state.inventory={head,body,hands};state.Refresh();
        Check(state.available==std::vector<std::size_t>{1,2},"Inventory excludes saved Same instances");
        state.column=1;state.Execute();Check(state.draft.size()==2 && state.available.size()==1,"Selected inventory action updates visible list");
        state.column=0;state.Execute();Check(state.draft.size()==1 && state.available.size()==2,"Removing returns instance to inventory list");
        Check(before==state.original,"Actions never mutate original preset");
        state.draft.clear();state.Refresh();state.Execute();Check(state.draft.empty(),"Empty list confirmation is safe");
        state.column=1;state.loading=true;state.Execute();Check(state.draft.empty(),"Loading blocks draft actions");
        state.loading=false;state.saving=true;state.Execute();Check(state.draft.empty(),"Saving blocks draft actions");
        state.saving=false;state.inventory.clear();
        for(unsigned i=0;i<12;++i)state.inventory.push_back(row(i+1,0));state.Refresh();
        state.selected[1]=11;state.Clamp(true);
        Check(state.scroll[1]==7,"Keyboard selection reveals the final row");
        state.filter=Filter::Head;state.Refresh();
        Check(state.available.empty() && state.selected[1]==0 && state.scroll[1]==0,"Empty filter clamps stale selection and scroll");
        std::cout<<"Outfit editor: add/remove, all conflicts, legacy masks, duplicates, 1-64 bounds, filters, draft isolation and roundtrip passed\n";
    }
    Check(!ValidName("   ") && !ValidName("a\nb") && ValidName("中文"),"Validate display names");
    using D=StepDecision;
    Check(DecideStep(true,false,false,0)==D::Issue,"Submit unequip once");
    Check(DecideStep(true,false,true,.016)==D::Wait && DecideStep(true,false,true,.5)==D::Wait,"Delayed unequip waits without repeat");
    Check(DecideStep(false,false,true,.1)==D::Advance,"Only advance after observed unequip");
    Check(DecideStep(false,true,false,0)==D::Issue,"Submit equip once");
    Check(DecideStep(false,true,true,.8)==D::Timeout,"Rejected equip times out without repeated calls");
    Check(DecideStep(true,true,true,.8)==D::Advance,"Observed completion wins at timeout boundary");
    Piece armor{{"Skyrim.esm",0x1234},{"",0xFF000ABC},{"Skyrim.esm",0x14},42,1.25f,"旅者 \"甲\"","Armor"};
    {
        Check(!PieceDataProblem(armor),"Ordinary armor has no capture-data error");
        auto piece=armor;piece.health=0;
        Check(!PieceDataProblem(piece),"Zero health is preserved, not rejected");
        piece.health=-.1f;
        Check(std::string(PieceDataProblem(piece))=="health-negative","Negative health remains invalid");
        for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            piece.health=bad;Check(std::string(PieceDataProblem(piece))=="health-not-finite","Non-finite health remains invalid");
        }
        Piece ring{{"Skyrim.esm",0x76F12},{},{},0,0.f,"","Madesi's Silver Ring"};
        Piece robes{{"DealingwithDaedra.esp",0x5117F0},{},{},80,0.f,"","Cultist Robes"};
        std::vector<Preset> roundtrip;
        const Preset reported{1,"Reported outfit",{ring,armor,robes}};
        Check(Decode(Encode({reported}),roundtrip) && roundtrip[0]==reported,"Reported zero-health apparel survives save/load without normalization");
        std::vector<Candidate> inventory{{robes,true},{ring,true},{armor,true}};std::vector<int> matched;
        Check(Resolve(roundtrip[0],inventory,matched) && Wearing(inventory,matched),"Reloaded zero-health pieces resolve to their exact equipped instances");
        inventory[1].piece.health=1.f;
        Check(!Resolve(roundtrip[0],inventory,matched),"Zero health is not silently substituted with a different instance signature");
        auto invalid=reported;invalid.pieces[0].health=-1.f;
        Check(!Decode(Encode({invalid}),roundtrip) && roundtrip[0]==reported,"Negative health rejects transactionally without replacing loaded outfits");
        piece=armor;piece.base.id=0;
        Check(std::string(PieceDataProblem(piece))=="base-id-zero","Missing base reference is diagnosed");
        piece=armor;piece.customName=std::string(513,'x');
        Check(std::string(PieceDataProblem(piece))=="custom-name-too-long","Overlong custom names have a distinct reason");
        piece=armor;piece.label=std::string(513,'x');
        Check(std::string(PieceDataProblem(piece))=="display-label-too-long","Overlong generated labels have a distinct reason");
    }
    {
        auto piece=[&](unsigned id){auto value=armor;value.base.id=id;value.unique=static_cast<std::uint16_t>(id);return value;};
        const auto oldBody=piece(1),oldRing=piece(2),commonBoots=piece(3),newRing=piece(4),newBody=piece(5);
        std::vector<Candidate> current{{oldBody,true,4},{oldRing,true,64},{commonBoots,true,128},
            {newRing,false,64},{newBody,false,4}};
        const std::vector<int> target{3,2,4}; // Preset save order need not put Body first.
        const auto plan=ReplacementPlan(current,target,false);
        Check(plan.size()==4 && plan[0].piece==newBody && plan[0].equip && plan[1].piece==newRing && plan[1].equip,
            "Replacement covers the body before accessories, then cleans old apparel");
        Check(!plan[2].equip && !plan[3].equip && plan[2].piece==oldBody && plan[3].piece==oldRing,
            "Only previously worn pieces outside the target are cleaned up after equipping");
        Check(std::none_of(plan.begin(),plan.end(),[&](const auto& op){return op.piece==commonBoots;}),
            "Common worn instances are never unequipped or reequipped");
        auto simulated=current;
        for(const auto& op:plan) {
            const int at=Match(op.piece,simulated);
            Check(at>=0,"Replacement simulation re-resolves each saved instance");
            if(op.equip)for(auto& item:simulated)if(item.mask&simulated[at].mask)item.worn=false;
            simulated[at].worn=op.equip;
            Check(simulated[2].worn && (simulated[0].worn || simulated[4].worn),
                "Native slot replacement never requires a plugin-driven bare-body stage or removal of shared boots");
        }
        Check(Wearing(simulated,target),"Replacement finishes with the exact target, without old accessories");
        current[0].worn=false;current[1].worn=false;current[3].worn=true;current[4].worn=true;
        Check(ReplacementPlan(current,target,false).empty(),"A completed target has no replacement operations");
        const auto off=ReplacementPlan(current,target,true);
        Check(off.size()==3 && std::none_of(off.begin(),off.end(),[](const auto& op){return op.equip;}),
            "Clicking an exact preset again removes all managed apparel only");
        current[1].worn=true;
        const auto extra=ReplacementPlan(current,target,false);
        Check(extra.size()==1 && !extra[0].equip && extra[0].piece==oldRing,
            "A fully worn target plus extra apparel only removes the extra item");
        Check(OutfitBudgetAvailable(0,0) && OutfitBudgetAvailable(1,2.9) &&
            !OutfitBudgetAvailable(2,0) && !OutfitBudgetAvailable(1,3.),
            "Per-tick budget bounds both engine submissions and elapsed work");
    }
    {
        auto accessory=armor;accessory.base.id=0x9876;accessory.mask=64;accessory.optional=true;
        auto extra=armor;extra.base.id=0x9877;
        Preset legacy{7,"Full outfit",{armor,accessory}};
        legacy.parts.fill(PartMode::Ignore);
        legacy.parts[static_cast<unsigned>(Part::Ring)]=PartMode::Unequip;
        std::vector<Candidate> current{{armor,true,4},{accessory,false,64},{extra,true,0}};
        std::vector<int> resolved;
        Check(Resolve(legacy,current,resolved)&&resolved==std::vector<int>{0,1},
            "Ignore/Unequip and optional metadata never skip present outfit pieces");
        const auto plan=ReplacementPlan(current,resolved,false,legacy);
        Check(plan.size()==2&&plan[0].equip&&plan[0].piece==accessory&&!plan[1].equip&&plan[1].piece==extra,
            "Full replacement equips optional pieces and removes unmatched zero-mask apparel");
        Check(!Wearing(current,resolved,legacy),"Preset overload requires every piece worn");
        current[1].worn=true;
        Check(!Wearing(current,resolved,legacy),"Ignored regions do not permit extra worn apparel");
        current[2].worn=false;
        Check(Wearing(current,resolved,legacy)==Wearing(current,resolved)&&Wearing(current,resolved),
            "Preset overload recognizes the same complete outfit as legacy");
        current.pop_back();current.pop_back();
        Check(!Resolve(legacy,current,resolved),"Missing optional pieces fail preflight instead of yielding negative indices");
        current.push_back({accessory,false,64});current[1].piece.health+=.5f;
        Check(!Resolve(legacy,current,resolved),"Strict matching still rejects changed instances");
        legacy.loose=true;
        Check(Resolve(legacy,current,resolved)&&resolved==std::vector<int>{0,1},"Saved loose matching remains supported");
        std::vector<Preset> roundtrip;
        Check(Decode(Encode({legacy}),roundtrip)&&roundtrip==std::vector<Preset>{legacy},
            "Dormant FWO 2 part, optional and loose metadata survives unchanged");
    }
    Preset outfit{1,"夜行套装",{armor}};
    std::vector<Preset> decoded;
    Check(Decode(Encode({outfit}),decoded) && decoded==std::vector<Preset>{outfit},"Unicode and instance fields survive serialization");
    {
        const auto oldCpp=std::locale();const std::string oldC=std::setlocale(LC_ALL,nullptr);
        unsigned checked=0;
        for(const auto name:{"nb-NO","de-DE","fr-FR","en-US"}) {
            try {
                std::locale::global(std::locale(name));
                if(!std::setlocale(LC_ALL,name))continue;
                std::vector<Preset> restored;
                Check(Decode(Encode({outfit}),restored) && restored==std::vector<Preset>{outfit},
                    "Outfit float/Unicode serialization is independent of Norwegian and other process locales");
                ++checked;
            }catch(const std::runtime_error&){}
        }
        std::locale::global(oldCpp);std::setlocale(LC_ALL,oldC.c_str());
        Check(checked>0,"Locale regression exercised an installed Windows locale");
        std::cout<<"Outfit process-locale round trips passed: "<<checked<<'\n';
    }
    const auto original=decoded;
    Check(!Decode("FWO 9 1",decoded) && decoded==original,"Bad schema is transactional");
    {
        const std::string v1="FWO 1 1\n1 \"Legacy\" 1\n\"Skyrim.esm\" 4660 \"\" 0 \"\" 0 42 1 \"\" \"Legacy armor\"\n";
        std::vector<Preset> migrated;
        Check(Decode(v1,migrated)&&migrated.size()==1&&!migrated[0].loose&&
            std::all_of(migrated[0].parts.begin(),migrated[0].parts.end(),[](PartMode m){return m==PartMode::Set;})&&
            !migrated[0].pieces[0].optional&&Encode(migrated).find("FWO 2 ")==0,
            "FWO 1 migrates to strict required-item replacement semantics and writes FWO 2");
    }
    Check(!Decode(Encode({outfit})+"trailing",decoded),"Reject trailing data");
    Check(!Decode("FWO 1 1000000",decoded),"Reject excessive count");
    Check(!Decode(Encode({outfit,outfit}),decoded),"Reject duplicate ids");
    {
        std::vector<Preset> atLimit;
        for(unsigned i=1;i<=100;++i){auto p=outfit;p.id=i;atLimit.push_back(p);}
        std::vector<Preset> restored;
        Check(Decode(Encode(atLimit),restored) && restored.size()==100,"The real limit still accepts exactly 100 presets");
        auto extra=outfit;extra.id=101;atLimit.push_back(extra);
        Check(!Decode(Encode(atLimit),restored) && restored.size()==100,"An actual 101st preset fails without replacing existing data");
        auto invalid=outfit;invalid.pieces[0].health=-1.f;
        Check(!Decode(Encode({invalid}),restored) && restored.size()==100,"One invalid item is rejected transactionally even when the count is below the limit");
        auto boundary=outfit;boundary.pieces[0].label=std::string(512,'x');
        Check(Decode(Encode({boundary}),restored),"Maximum supported generated-label length still round-trips");
        boundary.pieces[0].label+='x';
        Check(!Decode(Encode({boundary}),restored) && restored.size()==1,"Oversized labels remain rejected without deleting the previous preset");
    }
    auto bad=outfit;bad.pieces[0].health=std::numeric_limits<float>::infinity();
    Check(!Decode(Encode({bad}),decoded),"Reject invalid health");
    Check(!Portable(outfit),"Dynamic enchantment is character-local");
    auto portable=outfit;portable.pieces[0].enchantment={"Skyrim.esm",0xAB};
    Check(Portable(portable),"Static references are transferable");
    std::vector<Candidate> inventory{{armor,true}};
    std::vector<int> matches;
    Check(Resolve(outfit,inventory,matches) && Wearing(inventory,matches),"Exact equipped preset toggles off");
    auto alternate=armor;alternate.unique=99;alternate.health=2.f;
    inventory.push_back({alternate,false});
    Check(Match(armor,inventory)==0,"Do not equip another tempered instance");
    inventory[0].worn=false;
    Check(Resolve(outfit,inventory,matches)&&!Wearing(inventory,matches),"Present but unworn preset equips");
    inventory.erase(inventory.begin());
    Check(!Resolve(outfit,inventory,matches),"Missing target stops preflight");
    inventory={{armor,true},{armor,false}};
    Check(Match(armor,inventory)==-2,"Ambiguous unique instance rejected");
    Piece plain{{"Skyrim.esm",0x1234},{},{},0,1.f,"","Plain"};
    inventory={{plain,false},{plain,true}};
    Check(Match(plain,inventory)==1,"Equivalent plain copies prefer worn item");
    inventory={{armor,true},{alternate,true}};
    Check(Resolve(outfit,inventory,matches)&&!Wearing(inventory,matches),"Extra armor still triggers full replacement");
    Check(WearingPieces(inventory,matches),"An extra worn item must not hide the preset equipped badge");
    auto duplicate=outfit;duplicate.pieces.push_back(armor);
    Check(!Resolve(duplicate,inventory,matches),"One instance cannot satisfy two pieces");
    {
        Preset accessories{7,"Many accessories",{}};
        std::vector<Candidate> many;
        // A multipart set is limited by worn pieces, not the total forms shipped
        // by its mod. Exercise the full supported 64-piece save/load boundary.
        for(unsigned i=0;i<64;++i) {
            Piece piece{{"AccessoryPack.esp",0x800+i},{},{},0,1.f,"","Accessory"};
            accessories.pieces.push_back(piece);many.push_back({piece,true});
        }
        Check(Decode(Encode({accessories}),decoded) && Resolve(decoded[0],many,matches) && Wearing(many,matches),
            "A full 64-piece outfit saves, resolves and recognizes worn state");
        auto over=accessories;over.pieces.push_back(plain);
        Check(!Decode(Encode({over}),decoded),"65 pieces are rejected at save validation, not marked missing later");
        auto modified=accessories.pieces[17];modified.enchantment={"Skyrim.esm",0xA123};
        modified.health=1.25f;modified.customName="Named accessory";
        accessories.pieces[17]=modified;many[17].piece=modified;
        many.push_back({modified,false});
        Check(Resolve(accessories,many,matches) && matches[17]==17 && Wearing(many,matches),
            "An equivalent named/enchanted/tempered spare with no unique ID cannot make a newly saved multipart outfit missing");
        many[17].worn=false;
        Check(Resolve(accessories,many,matches),"Equivalent complete signatures remain usable after unequipping");
        ResolveProblem problem;
        many[17].piece.health=2.f;many.back().piece.customName="Different accessory";
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Changed && problem.piece==17,
            "Different temper or name never substitutes for the saved accessory and identifies its index");
        many.back().piece=modified;many.back().piece.unique=9;
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Changed,
            "A newly unique instance does not silently substitute for a no-UID recipe");
        many.erase(many.begin()+17);many.pop_back();
        Check(!Resolve(accessories,many,matches,&problem) && problem.reason==ResolveReason::Missing && problem.piece==17,
            "Absent base form is distinguished from a changed signature");
        Preset exact{8,"Exact",{armor}};
        std::vector<Candidate> collision{{armor,true},{armor,false}};
        Check(!Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::Ambiguous,
            "Duplicate unique IDs remain an error even when one instance is worn");
        collision.pop_back();exact.pieces.push_back(armor);
        Check(!Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::Duplicate && problem.piece==1,
            "Duplicate recipe entry is distinguished from physical absence");
        exact.pieces.pop_back();
        Check(Resolve(exact,collision,matches,&problem) && problem.reason==ResolveReason::None,
            "A successful recheck clears a previous diagnostic");
    }
    {
        auto boots=armor;boots.base.id=0x2345;boots.unique=43;boots.label="Boots";
        auto necklace=plain;necklace.base.id=0x3456;necklace.label="Necklace";
        Preset manual{9,"Manually equipped",{armor,boots}};
        // No applied-preset ID or history is needed. Resolve current worn flags
        // even when inventory order differs and spare copies are present.
        std::vector<Candidate> current{{boots,true},{alternate,false},{armor,true}};
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches)&&Wearing(current,matches),
            "Manually wearing all saved instances shows equipped and toggles off an exact set");
        current.push_back({necklace,true});
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches)&&!Wearing(current,matches),
            "An extra necklace or hidden armor accessory preserves the badge without changing full-set transaction checks");
        current[0].worn=false;
        Check(Resolve(manual,current,matches)&&!WearingPieces(current,matches),
            "A single unequipped preset piece removes the badge despite extra apparel");
        current[0].worn=true;current[2].worn=false;current[1].worn=true;
        Check(Resolve(manual,current,matches)&&!WearingPieces(current,matches),
            "A different worn same-base instance cannot falsely mark the saved instance equipped");
        current[2].worn=true;current[1].worn=false;
        Check(Resolve(manual,current,matches)&&WearingPieces(current,matches),
            "Re-equipping the saved item manually restores the badge on the next snapshot");
        Check(!WearingPieces(current,{})&&!Wearing(current,{}),"An empty match list is never an equipped preset");
        Check(!WearingPieces(current,{-1})&&!WearingPieces(current,{99}),"Invalid candidate indices never show equipped");
    }
    std::cout<<"Outfit serialization, malformed input, instance matching, manual wear badges, preflight and toggle tests passed\n";
}
