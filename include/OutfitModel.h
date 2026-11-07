#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
namespace Wheel::Outfits {
    enum class PartMode : std::uint8_t { Set, Ignore, Unequip };
    enum class Part : std::uint8_t { Head, Body, Hands, Feet, Ring, Amulet, Other };
    inline constexpr std::array<std::uint32_t,7> partBits{
        (1u<<0)|(1u<<1)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<14), 1u<<2,
        (1u<<3)|(1u<<4), (1u<<7)|(1u<<8), 1u<<6, 1u<<5,
        0xFFFFFFFFu & ~((1u<<0)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6)|(1u<<7)|(1u<<8)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<14))};
    inline constexpr Part PartOf(std::uint32_t mask) {
        if(mask&partBits[1])return Part::Body;if(mask&partBits[0])return Part::Head;
        if(mask&partBits[2])return Part::Hands;if(mask&partBits[3])return Part::Feet;
        if(mask&partBits[4])return Part::Ring;if(mask&partBits[5])return Part::Amulet;return Part::Other;
    }
    inline constexpr std::uint32_t PartBits(Part p){return partBits[static_cast<std::size_t>(p)];}
    struct FormRef {
        std::string plugin;
        std::uint32_t id=0;
        bool operator==(const FormRef&) const = default;
    };
    struct Piece {
        FormRef base, enchantment, owner;
        std::uint16_t unique=0;
        float health=1.f;
        std::string customName, label;
        std::uint32_t mask=0;
        bool optional=false;
        bool operator==(const Piece&) const = default;
    };
    struct Preset {
        std::uint32_t id=0;
        std::string name;
        std::vector<Piece> pieces;
        std::array<PartMode,7> parts{};
        bool loose=false;
        bool operator==(const Preset&) const = default;
    };
    inline bool Same(const Piece& a,const Piece& b) {
        return a.base==b.base && a.enchantment==b.enchantment && std::abs(a.health-b.health)<.0001f &&
            a.customName==b.customName && a.unique==b.unique && (!a.unique || a.owner==b.owner);
    }
    inline bool Portable(const Preset& p) {
        return std::all_of(p.pieces.begin(),p.pieces.end(),[](const Piece& x){
            return !x.base.plugin.empty() && (!x.enchantment.id || !x.enchantment.plugin.empty());
        });
    }
    inline bool ValidName(const std::string& name) {
        return !name.empty() && name.size()<=128 &&
            std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c==127;}) &&
            std::any_of(name.begin(),name.end(),[](unsigned char c){return c>32;});
    }
    // Shared by the reader and capture diagnostics; keep rejected fields intact
    // so a failing modded item can be identified rather than silently altered.
    inline const char* PieceDataProblem(const Piece& x) {
        if(!x.base.id)return "base-id-zero";
        if(x.base.plugin.size()>=260 || x.enchantment.plugin.size()>=260 || x.owner.plugin.size()>=260)return "plugin-name-too-long";
        if(!std::isfinite(x.health))return "health-not-finite";
        // Zero is observed on real apparel instances; preserve it for exact matching.
        if(x.health<0)return "health-negative";
        if(x.customName.size()>512)return "custom-name-too-long";
        if(x.label.size()>512)return "display-label-too-long";
        return nullptr;
    }
    inline std::string Encode(const std::vector<Preset>& presets) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<"FWO 2 "<<presets.size()<<'\n'<<std::setprecision(std::numeric_limits<float>::max_digits10);
        auto ref=[&](const FormRef& r){out<<std::quoted(r.plugin)<<' '<<r.id<<' ';};
        for(const auto& p:presets) {
            out<<p.id<<' '<<std::quoted(p.name)<<' '<<p.pieces.size()<<' '<<p.loose;
            for(auto mode:p.parts)out<<' '<<static_cast<unsigned>(mode);out<<'\n';
            for(const auto& x:p.pieces) {
                ref(x.base);ref(x.enchantment);ref(x.owner);
                out<<x.unique<<' '<<x.health<<' '<<std::quoted(x.customName)<<' '<<std::quoted(x.label)<<' '<<x.mask<<' '<<x.optional<<'\n';
            }
        }
        return out.str();
    }
    inline bool Decode(const std::string& text,std::vector<Preset>& result) {
        if(text.size()>1024*1024) return false;
        std::istringstream in(text);in.imbue(std::locale::classic());std::string magic;unsigned version=0,count=0;
        if(!(in>>magic>>version>>count) || magic!="FWO" || (version!=1&&version!=2) || count>100) return false;
        std::vector<Preset> next;
        auto ref=[&](FormRef& r){return bool(in>>std::quoted(r.plugin)>>r.id) && r.plugin.size()<260;};
        for(unsigned i=0;i<count;++i) {
            Preset p;unsigned n=0;
            if(!(in>>p.id>>std::quoted(p.name)>>n) || !p.id || p.id>1000000 || !ValidName(p.name) || n==0 || n>64 ||
                std::any_of(next.begin(),next.end(),[&](const auto& other){return other.id==p.id;})) return false;
            if(version==2){unsigned loose=0;if(!(in>>loose)||loose>1)return false;p.loose=loose!=0;for(auto& mode:p.parts){unsigned value=0;if(!(in>>value)||value>2)return false;mode=static_cast<PartMode>(value);}}
            else p.parts.fill(PartMode::Set);
            for(unsigned j=0;j<n;++j) {
                Piece x;unsigned uid=0;
                unsigned optional=0;
                if(!ref(x.base)||!ref(x.enchantment)||!ref(x.owner)||!(in>>uid>>x.health>>std::quoted(x.customName)>>std::quoted(x.label)) ||
                    uid>65535 || PieceDataProblem(x)) return false;
                if(version==2&&(!(in>>x.mask>>optional)||optional>1))return false;x.optional=optional!=0;
                x.unique=static_cast<std::uint16_t>(uid);p.pieces.push_back(std::move(x));
            }
            next.push_back(std::move(p));
        }
        in>>std::ws;if(!in.eof())return false;
        result=std::move(next);return true;
    }
    // A unique id requires an exact match. Without one, copies with the same
    // complete signature are interchangeable, including temper/name/enchantment.
    // If the preset piece has a uid but no exact match is found, and there is
    // exactly one candidate with the same base, accept it (uid may change on
    // reload for mod-generated items like TNG). Log as uid-mismatch-accepted.
    template<class Candidate> int Match(const Piece& p,const std::vector<Candidate>& candidates) {
        int found=-1;
        for(int i=0;i<static_cast<int>(candidates.size());++i) if(Same(p,candidates[i].piece)) {
            if(found>=0 && p.unique)return -2;
            if(found<0 || candidates[i].worn) found=i;
        }
        if(found<0 && p.unique) {
            // Exact match failed due to uid mismatch; try single-candidate fallback.
            int single=-1;bool ambiguous=false;
            for(int i=0;i<static_cast<int>(candidates.size());++i) {
                const auto& c=candidates[i].piece;
                if(c.base==p.base && c.enchantment==p.enchantment && std::abs(c.health-p.health)<.0001f && c.customName==p.customName) {
                    if(single<0 || candidates[i].worn) single=i;
                    else if(!candidates[i].worn) ambiguous=true;
                }
            }
            if(!ambiguous && single>=0)return single;
        }
        return found;
    }
    enum class ResolveReason { None, Missing, Changed, Ambiguous, Duplicate };
    struct ResolveProblem {
        ResolveReason reason=ResolveReason::None;
        std::size_t piece=0;
    };
    inline std::uint32_t ManagedMask(const Preset& p){std::uint32_t mask=0;for(std::size_t i=0;i<p.parts.size();++i)if(p.parts[i]!=PartMode::Ignore)mask|=partBits[i];return mask;}
    template<class Candidate> bool Resolve(const Preset& p,const std::vector<Candidate>& inventory,std::vector<int>& matches,
        ResolveProblem* problem=nullptr) {
            matches.clear();std::vector<int> used;
            if(problem)*problem={};
            for(std::size_t at=0;at<p.pieces.size();++at) {
                const auto& piece=p.pieces[at];
                if(p.parts[static_cast<std::size_t>(PartOf(piece.mask))]!=PartMode::Set){matches.push_back(-1);continue;}
                int i=Match(piece,inventory);
                if(i<0&&p.loose)for(int n=0;n<static_cast<int>(inventory.size());++n)
                    if(inventory[n].piece.base==piece.base&&inventory[n].piece.enchantment==piece.enchantment&&
                        std::find(used.begin(),used.end(),n)==used.end()&&(i<0||inventory[n].worn))i=n;
                if(i<0 || std::find(used.begin(),used.end(),i)!=used.end()) {
                    if(piece.optional){matches.push_back(-1);continue;}
                    if(problem) {
                        const bool basePresent=std::any_of(inventory.begin(),inventory.end(),[&](const auto& c){return c.piece.base==piece.base;});
                        *problem={i==-2?ResolveReason::Ambiguous:i<0?(basePresent?ResolveReason::Changed:ResolveReason::Missing):ResolveReason::Duplicate,at};
                    }
                    return false;
                }
                used.push_back(i);
                matches.push_back(i);
            }
            return true;
        }
    // The badge describes the preset's pieces, including items equipped manually.
    // Additional worn apparel does not make those pieces unequipped.
    template<class Candidate> bool WearingPieces(const std::vector<Candidate>& inventory,const std::vector<int>& matches) {
            return !matches.empty() && std::all_of(matches.begin(),matches.end(),[&](int i){
                return i>=0 && static_cast<std::size_t>(i)<inventory.size() && inventory[i].worn;
            });
        }
    // Outfit transactions still require the complete managed set to match.
    template<class Candidate> bool Wearing(const std::vector<Candidate>& inventory,const std::vector<int>& matches) {
            return WearingPieces(inventory,matches) && std::count_if(inventory.begin(),inventory.end(),[](const auto& c){return c.worn;})==matches.size();
        }
    template<class Candidate> bool Wearing(const std::vector<Candidate>& inventory,const std::vector<int>& matches,const Preset& preset) {
        bool found=false;
        for(std::size_t at=0;at<matches.size();++at){const auto i=matches[at];if(i<0){if(preset.parts[static_cast<std::size_t>(PartOf(preset.pieces[at].mask))]==PartMode::Set&&!preset.pieces[at].optional)return false;continue;}found=true;if(static_cast<std::size_t>(i)>=inventory.size()||!inventory[i].worn)return false;}
        if(!found)return false;const auto managed=ManagedMask(preset);
        for(std::size_t i=0;i<inventory.size();++i)if(inventory[i].worn&&(inventory[i].mask&managed)&&std::find(matches.begin(),matches.end(),static_cast<int>(i))==matches.end())return false;
        return true;
    }

}
