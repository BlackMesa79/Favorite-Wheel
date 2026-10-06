#include "FaceLightModel.h"
#include <cstdlib>
#include <iostream>
namespace {
    namespace API=FaceLightingAPI;
    unsigned calls=0,executeCalls=0;
    bool stale=false,notReady=false,failPlayer=false,wrongThread=false;
    API::Request submitted;
    void Check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
    API::Result Context(API::Context* out) noexcept {
        if(out->structSize!=sizeof(*out))return API::Result::InvalidArgument;
        if(wrongThread)return API::Result::WrongThread;
        out->ready=!notReady;out->session=42;out->revision=123;out->playerEnabled=1;return API::Result::Ok;
    }
    API::Result Actor(API::ActorState* out) noexcept {
        if(out->structSize!=sizeof(*out))return API::Result::InvalidArgument;
        out->target={42,100,200};out->revision=456;out->flags=API::Loaded|API::GroupEnabled;return API::Result::Ok;
    }
    API::Result Player(API::ActorState* out) noexcept {Actor(out);return failPlayer?API::Result::WrongThread:API::Result::Ok;}
    API::Result Query(const API::Token*,API::ActorState* out) noexcept {return Actor(out);}
    API::Result Followers(API::FollowerPage* out) noexcept {
        ++calls;
        if(out->structSize!=sizeof(*out) || out->session!=42 || out->reserved)return API::Result::InvalidArgument;
        if(!out->capacity){out->total=130;out->revision=789;return API::Result::Ok;}
        if(out->revision!=789)return API::Result::StaleRevision;
        if(stale && out->offset>=64)return API::Result::StaleRevision;
        out->count=std::min(out->capacity,130-out->offset);out->total=130;
        for(unsigned i=0;i<out->count;++i){if(Actor(out->rows+i)!=API::Result::Ok)return API::Result::InvalidArgument;out->rows[i].target.formID+=out->offset+i;out->rows[i].flags|=API::Follower;}
        return API::Result::Ok;
    }
    API::Result Execute(const API::Request* request) noexcept {++executeCalls;submitted=*request;return API::Result::StaleRevision;}
}
int main() {
    using namespace Wheel::FaceLightModel;
    API::Interface api{sizeof(API::Interface),1,API::PlayerControl|API::ActorControl|API::FollowerList|API::StatusQuery,0,Context,Actor,Player,Query,Followers,Execute};
    Check(Compatible(&api) && !Compatible(nullptr),"ABI discovery");
    auto small=api;small.structSize--;Check(!Compatible(&small),"reject short table");small=api;small.apiVersion=2;Check(!Compatible(&small),"reject version");
    small=api;small.Execute=nullptr;Check(!Compatible(&small),"reject missing callback");
    small=api;small.structSize+=16;small.capabilities|=1u<<30;
    Check(Compatible(&small),"accept an extended V1 table and unknown capabilities without requiring a provider release number");
    auto s=Read(&api);Check(s.available && s.result==API::Result::Ok && s.followersResult==API::Result::Ok && s.followers.size()==130 && calls==4,"initialized paginated caller buffers");
    Check(s.followers[129].target.formID==329,"pagination offsets");
    stale=true;s=Read(&api);Check(s.followers.empty() && s.followersResult==API::Result::StaleRevision,"discard partial stale pages");stale=false;
    failPlayer=true;s=Read(&api);Check(s.player.target.session==0 && s.playerResult==API::Result::WrongThread,"discard failed output");failPlayer=false;
    notReady=true;calls=0;s=Read(&api);Check(s.result==API::Result::NotReady && s.context.session==0 && calls==0,"not-ready discards context");notReady=false;
    wrongThread=true;calls=0;s=Read(&api);
    Check(s.available && s.result==API::Result::WrongThread && s.playerResult==API::Result::WrongThread &&
        s.targetResult==API::Result::WrongThread && s.followersResult==API::Result::WrongThread &&
        s.context.session==0 && s.followers.empty() && calls==0,"thread rejection remains distinct from missing mod and stops API reads");wrongThread=false;
    small=api;small.capabilities=API::PlayerControl;calls=0;s=Read(&small);Check(calls==0 && s.targetResult!=API::Result::Ok,"capability gates");
    API::ActorState state;Actor(&state);
    auto command=ActorRequest(state);Check(command && command->enabled==1 && command->target.session==42 && command->revision==456,"explicit actor enable");
    state.flags=API::Loaded;Check(!ActorRequest(state),"do not silently enable disabled source");
    state.flags|=API::Enabled;command=ActorRequest(state);Check(command && command->enabled==0,"allow disable while source off");
    state.flags|=API::Unsafe;Check(!ActorRequest(state),"unsafe actor disabled");state.flags=API::GroupEnabled;Check(!ActorRequest(state),"unloaded actor disabled");
    state.flags=API::Loaded|API::GroupEnabled;state.target.handle=0;Check(!ActorRequest(state),"invalid token disabled");
    API::Context c;Context(&c);auto player=ContextRequest(c,API::Command::SetPlayer);
    Check(player.enabled==0 && player.target.session==42 && !player.target.formID && player.revision==123 && !player.reserved,"player uses context stamp");
    auto group=ContextRequest(c,API::Command::SetFollowerGroup);Check(group.enabled==1 && group.revision==123,"group request");
    Check(Submit(nullptr,player)==API::Result::NotReady && executeCalls==0,"absent API submits nothing");
    auto result=Submit(&api,player);Check(result==API::Result::StaleRevision && executeCalls==1 && submitted.enabled==0,"single explicit submission, stale is not replayed");
    std::cout<<"Face Lighting ABI, paging, failure output, explicit preferences and source gating checks passed\n";
}
