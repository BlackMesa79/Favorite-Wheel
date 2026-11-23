#include "OutfitEditModel.h"
#include "OutfitExecution.h"
#include "OutfitPlan.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace Wheel::Outfits;
struct Candidate {Piece piece;bool worn=false;std::uint32_t mask=0;};
unsigned checks=0;
void Check(bool ok,const char* message) {
    ++checks;
    if(!ok){std::cerr<<message<<'\n';std::exit(1);}
}
Piece Item(unsigned id,std::uint32_t mask) {
    Piece piece;piece.base={"Example.esp",id};piece.mask=mask;piece.label="Item "+std::to_string(id);return piece;
}
const Piece body=Item(1,1u<<2),cover=Item(2,1u<<22),lantern=Item(3,1u<<13);
Preset Compatible() {
    Preset preset{1,"Dynamic accessories",{cover,body,lantern}};
    preset.missingPolicy=MissingPolicy::Skip;return preset;
}
void Resolution() {
    auto preset=Compatible();const std::vector<Candidate> inventory{{body,false,body.mask}};
    std::vector<int> matches;ResolveProblem problem;
    Check(Resolve(preset,inventory,matches,&problem)&&matches==std::vector<int>{0},"Absent cover and lantern do not block body");
    auto strict=preset;strict.missingPolicy=MissingPolicy::Required;
    Check(!Resolve(strict,inventory,matches,&problem)&&problem.reason==ResolveReason::Missing,"Strict preset remains blocked");
    Check(!Resolve(preset,std::vector<Candidate>{},matches,&problem)&&matches.empty(),"All missing never enables stripping or an empty apply");
    auto changed=body;changed.health=2;
    Check(!Resolve(preset,std::vector<Candidate>{{changed}},matches,&problem)&&problem.reason==ResolveReason::Changed,"Present but changed instance is not skipped");
    auto unique=body;unique.unique=9;preset.pieces={unique};
    Check(!Resolve(preset,std::vector<Candidate>{{unique},{unique}},matches,&problem)&&problem.reason==ResolveReason::Ambiguous,"Ambiguous instances still fail");
    preset.pieces={body,body};
    Check(!Resolve(preset,inventory,matches,&problem)&&problem.reason==ResolveReason::Duplicate,"Duplicate targets still fail");
}
void DynamicLifecycle() {
    const auto saved=Compatible();std::vector<Candidate> inventory{{body,false,body.mask}};
    std::vector<int> matches;
    Check(Resolve(saved,inventory,matches),"Resolve available body before applying");
    const auto execution=ExecutionPreset(saved,inventory,matches);
    Check(execution.pieces==std::vector<Piece>{body}&&saved.pieces.size()==3,"Execution freezes available targets without changing saved preset");
    auto operations=ReplacementPlan(inventory,matches,false,execution);
    Check(operations.size()==1&&operations[0].equip&&operations[0].piece==body,"Only available armor is equipped");
    inventory[0].worn=true;inventory.push_back({cover,true,cover.mask});inventory.push_back({lantern,true,lantern.mask});
    Check(Resolve(execution,inventory,matches)&&Wearing(inventory,matches,execution),"Regenerated auxiliary equipment does not cause a false partial failure");
    operations=ReplacementPlan(inventory,matches,true,execution);
    Check(operations.size()==1&&!operations[0].equip&&operations[0].piece==body,"Toggle off removes matched targets, not unrelated regenerated accessories");
    inventory[0].worn=false;
    Check(RemovalComplete(execution,inventory),"Removal completion permits unrelated script equipment");
    auto strict=execution;strict.missingPolicy=MissingPolicy::Required;
    Check(!RemovalComplete(strict,inventory),"Strict removal retains full-outfit verification");
    Check(Resolve(saved,inventory,matches)&&matches.size()==3,"Next run resolves newly returned accessories again");
    const auto withAccessories=ExecutionPreset(saved,inventory,matches);
    inventory.pop_back();inventory.erase(inventory.begin()+1);inventory[0].worn=true;
    Check(Resolve(withAccessories,inventory,matches)&&WearingPieces(inventory,matches),"Mid-operation disappearance leaves remaining targets valid");
    Check(MissingFromInventory(cover,inventory),"Absent operation can advance without requesting a stale extra pointer");
    inventory.push_back({cover,false,cover.mask});inventory.back().piece.health=2;
    Check(!MissingFromInventory(cover,inventory),"Changed instance is never treated as absent");
    Check(!Resolve(withAccessories,inventory,matches),"Changed mid-operation target still interrupts");
    Check(RemovalComplete(execution,std::vector<Candidate>{}),"Script-deleted removed target counts as removed");
}
void GlobalPolicy() {
    auto strict=Compatible();strict.missingPolicy=MissingPolicy::Required;
    const auto before=Encode({strict});
    const std::vector<Candidate> inventory{{body,true,body.mask}};
    std::vector<int> matches;ResolveProblem problem;
    const auto policy=EffectiveMissingPolicy(strict.missingPolicy,true);
    Check(ResolveWithPolicy(strict,inventory,matches,policy,&problem) && Wearing(inventory,matches,policy),
        "Global skip enables the usable and worn badges for a strict preset with an absent item");
    Check(Encode({strict})==before && !ResolveWithPolicy(strict,inventory,matches,EffectiveMissingPolicy(strict.missingPolicy,false)),
        "Turning off the global setting restores strict matching without altering the stored preset");
    auto enabled=Compatible();
    Check(ResolveWithPolicy(enabled,inventory,matches,EffectiveMissingPolicy(enabled.missingPolicy,false)),
        "Existing per-preset skip remains active when the global setting is off");
    Check(!ResolveWithPolicy(strict,std::vector<Candidate>{},matches,policy,&problem) && matches.empty(),
        "Global skip cannot apply an entirely absent outfit");
    auto changed=body;changed.health=2;
    Check(!ResolveWithPolicy(strict,std::vector<Candidate>{{changed}},matches,policy,&problem) && problem.reason==ResolveReason::Changed,
        "Global skip also rejects a changed instance still present in inventory");
}
void Serialization() {
    auto configured=Compatible();std::vector<Preset> decoded;
    Check(Decode(Encode({configured}),decoded)&&decoded==std::vector<Preset>{configured},"FWO 3 persists per-preset policy");
    const std::string piece="\"Example.esp\" 1 \"\" 0 \"\" 0 0 1 \"\" \"Body\"";
    Check(Decode("FWO 1 1\n1 \"Legacy\" 1\n"+piece+"\n",decoded)&&decoded[0].missingPolicy==MissingPolicy::Required,"FWO 1 defaults to strict");
    Check(Decode("FWO 2 1\n1 \"Legacy\" 1 0 0 0 0 0 0 0 0\n"+piece+" 4 0\n",decoded)&&decoded[0].missingPolicy==MissingPolicy::Required,"FWO 2 defaults to strict");
    const auto before=decoded;
    Check(!Decode("FWO 3 1\n1 \"Invalid\" 1 0 0 0 0 0 0 0 0 2\n"+piece+" 4 0\n",decoded)&&decoded==before,"Unknown policy rejected transactionally");
    auto strict=configured;strict.id=2;strict.missingPolicy=MissingPolicy::Required;
    Check(Decode(Encode({configured,strict}),decoded)&&decoded[0].missingPolicy==MissingPolicy::Skip&&decoded[1].missingPolicy==MissingPolicy::Required,"Policies remain independent between presets");
}
void Editor() {
    Edit::State edit;edit.original=Compatible();edit.missingPolicy=edit.original.missingPolicy;
    for(const auto& piece:edit.original.pieces)edit.draft.push_back({piece,piece.mask,false});
    const auto original=edit.original;Preset result;
    edit.ToggleMissing();
    Check(edit.original==original&&edit.missingPolicy==MissingPolicy::Required,"Toggle changes draft without mutating stale-edit baseline");
    Check(edit.Save(result)&&result.missingPolicy==MissingPolicy::Required&&result.pieces==original.pieces,"Saving changes only policy and draft");
    edit.loading=true;edit.ToggleMissing();
    Check(edit.missingPolicy==MissingPolicy::Required,"Loading blocks policy changes");
    edit.loading=false;edit.saving=true;edit.ToggleMissing();
    Check(edit.missingPolicy==MissingPolicy::Required,"Saving blocks policy changes");
    edit.saving=false;edit.ToggleMissing();
    Check(edit.Save(result)&&result.missingPolicy==MissingPolicy::Skip,"Enabled setting survives editor save");
}
}
int main() {
    Resolution();DynamicLifecycle();GlobalPolicy();Serialization();Editor();
    std::cout<<"OutfitMissingTests passed: "<<checks<<" checks\n";
}
