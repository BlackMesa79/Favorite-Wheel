// Robustness regression for SwfRaster: malformed/truncated input, stroke
// bounds, extreme transforms and per-render work budgets.
#include "SwfRasterFixtures.h"

namespace {
    void InvalidInputs(){
        auto bytes=Simple();std::vector<std::string>w;auto zws=bytes;zws.v[0]='Z';Check(!Parse(zws.v,w)&&!w.empty(),"ZWS refused with warning");
        for(std::size_t i=0;i<bytes.v.size();++i){w.clear();Check(!Parse(std::span(bytes.v).first(i),w)&&!w.empty(),"truncated file returns warning");}
        std::mt19937 rng(0x4924);for(unsigned i=0;i<5000;++i){
            auto data=bytes.v;if(i%2){data.resize(rng()%512);for(auto& c:data)c=static_cast<std::uint8_t>(rng());}
            else for(unsigned n=0;n<1+i%8;++n)data[rng()%data.size()]=static_cast<std::uint8_t>(rng());
            w.clear();auto m=Parse(data,w);if(m){auto r=Render(*m,"icon",Variant::Mask,16);if(r.bitmap)Check(r.bitmap->rgba.size()==1024,"malformed-input run output bounded");}else Check(!w.empty(),"invalid input warning");
        }
        std::cout<<"ZWS, every truncation, 5000 deterministic malformed inputs: PASS\n";
    }
    // A shape whose single stroke is a short diagonal: (0,0)->(dx,dy) twips.
    Bytes StrokeShape(unsigned id,unsigned width,unsigned cap,unsigned join,unsigned miter,int dx,int dy,int dx2=0,int dy2=0){
        Bytes b;b.W(id);b.Rect();b.Rect();b.B(0);b.B(0);b.B(1);b.W(width);
        b.U(cap,2);b.U(join,2);b.U(0,1);b.U(0,1);b.U(0,1);b.U(0,1);b.U(0,5);b.U(0,1);b.U(cap,2);b.Align();
        if(join==2)b.W(miter);Color(b,{255,255,255,255},true);
        b.U(0,4);b.U(1,4);b.U(0,1);b.U(9,5);b.U(1,5);b.U(0,1);b.U(0,1);b.U(1,1);
        Edge(b,dx,dy);if(dx2||dy2)Edge(b,dx2,dy2);b.U(0,6);b.Align();return b;
    }
    Bitmap DrawStroke(unsigned width,unsigned cap,unsigned join,unsigned miter,int dx,int dy,int dx2=0,int dy2=0,std::uint32_t edge=128){
        Bytes tags;tags.Tag(83,StrokeShape(1,width,cap,join,miter,dx,dy,dx2,dy2));Place(tags,1);tags.Label("icon");tags.Tag(1);
        auto r=Render(*ParseOK(File(tags)),"icon",Variant::Original,edge);if(!r.bitmap)for(auto& s:r.warnings)std::cerr<<s<<'\n';
        Check(r.bitmap.has_value(),"stroke fixture renders");return *r.bitmap;
    }
    // No visible pixel may touch the outer ring: production framing keeps 5%
    // margin, so any coverage there means the bounds underestimated the stroke.
    // The bound may over-estimate (butt caps, bevels), so only require that
    // the drawing is not microscopic; tight framing is checked separately.
    void Unclipped(const Bitmap& b,const char* message){
        unsigned edge=0;for(unsigned i=0;i<b.width;++i)edge+=Pixel(b,i,0,3)+Pixel(b,i,b.height-1,3)+Pixel(b,0,i,3)+Pixel(b,b.width-1,i,3);
        if(edge){auto clip=Bounds(b);std::cerr<<"edge alpha "<<edge<<" box "<<clip[0]<<','<<clip[1]<<','<<clip[2]<<','<<clip[3]<<'\n';}
        Check(edge==0,message);auto box=Bounds(b);
        // A miter beyond its limit is beveled, so the conservative miter bound can
        // leave the visible stroke at ~40% of the canvas; it must still be visible.
        Check(std::max(box[2]-box[0],box[3]-box[1])+1>=b.width/4,message);
    }
    void StrokeBounds(){
        // Thick square cap on a 45 degree segment: the corner reaches sqrt(2)*w/2.
        Unclipped(DrawStroke(1000,2,0,0,100,100),"square cap diagonal is not clipped");
        Unclipped(DrawStroke(1000,0,0,0,100,100),"round cap diagonal is not clipped");
        Unclipped(DrawStroke(1000,1,0,0,100,100),"butt cap diagonal is not clipped");
        // Sharp miter (tiny angle, limit 10): tip extends ~limit*w/2.
        Unclipped(DrawStroke(200,1,2,10*256,1000,40,-1000,40),"sharp miter join is not clipped");
        Unclipped(DrawStroke(200,1,1,0,1000,40,-1000,40),"bevel join is not clipped");
        // Square-cap reach must be measured, not just padded: the result fills ~90%.
        auto square=DrawStroke(1000,2,0,0,100,100);auto box=Bounds(square);
        Check(box[2]-box[0]>=110&&box[3]-box[1]>=110,"square cap keeps 90 percent framing");
        // Sub-pixel stroke on a large shape: the 1 px floor stays inside the canvas.
        Unclipped(DrawStroke(1,2,0,0,2000,2000,0,0,64),"hairline floor is not clipped");
        std::cout<<"stroke bounds: caps, miter/bevel joins, hairline floor: PASS\n";
    }
    // Wraps a placement of character `inner` inside `levels` sprites, each scaling by sx.
    Bytes Nested(Bytes tags,unsigned levels,int sx,int sy=65536){
        unsigned inner=1;
        for(unsigned i=0;i<levels;++i){Bytes s;s.W(100+i);s.W(1);Place(s,inner,1,false,256,sx,sy);s.Tag(1);s.Tag(0);tags.Tag(39,s);inner=100+i;}
        Place(tags,inner);tags.Label("icon");tags.Tag(1);return File(tags);
    }
    // Either a correct image or a clean failure; never a "successful" garbage frame.
    void FramedOrRejected(const Result& r,const char* message){
        if(!r.bitmap){Check(!r.warnings.empty(),message);return;}
        Unclipped(*r.bitmap,message);
    }
    void ExtremeTransforms(){
        // Nested 16383x scales on a zero-width vertical hairline. 10 levels used
        // to overflow float (inf/NaN inside PlutoVG) and returned a full-canvas
        // "success". Now: a thin centred line, or a rejection once double overflows.
        for(unsigned levels:{10u,12u,25u,31u}){
            Bytes tags;tags.Tag(83,StrokeShape(1,0,0,0,0,0,1000));auto m=ParseOK(Nested(tags,levels,16383<<16));
            auto r=Render(*m,"icon",Variant::Original,256);
            if(r.bitmap){auto box=Bounds(*r.bitmap);Check(box[2]-box[0]<=3&&box[3]-box[1]>=200,"nested hairline stays a thin line");Unclipped(*r.bitmap,"nested hairline framed");}
            else Check(!r.warnings.empty(),"overflowing nested scale fails with warning");
            std::cout<<"  "<<levels<<" levels x16383: "<<(r.bitmap?"thin line":"rejected: "+r.warnings.back())<<'\n';
        }
        // Uniform 16383^4 (~7e16): finite, reframed by the viewport.
        {Bytes tags;Define(tags,1);FramedOrRejected(Render(*ParseOK(Nested(tags,4,16383<<16,16383<<16)),"icon",Variant::Original,256),"huge uniform scale");}
        // Zero scale on one axis: a zero-area fill has nothing to draw.
        {Bytes tags;Define(tags,1);auto r=Render(*ParseOK(Nested(tags,1,65536,0)),"icon",Variant::Original,256);
         Check(!r.bitmap&&!r.warnings.empty(),"degenerate scale is rejected");}
        // 8 levels of 1/65536 (~3e-39 twips): tiny but nonzero, reframed or rejected.
        {Bytes tags;Define(tags,1);FramedOrRejected(Render(*ParseOK(Nested(tags,8,1,1)),"icon",Variant::Original,256),"underflowing scale");}
        // Anisotropic transform (2x by 1x, twice = 4:1) on a filled 2:1 shape -> 8:1 sliver, still framed.
        {Bytes tags;Define(tags,1);auto r=Render(*ParseOK(Nested(tags,2,2<<16,65536)),"icon",Variant::Original,128);
         Check(r.bitmap.has_value(),"anisotropic scale renders");Unclipped(*r.bitmap,"anisotropic scale is framed");
         auto box=Bounds(*r.bitmap);Check(box[2]-box[0]>=110&&box[3]-box[1]>=10&&box[3]-box[1]<=18,"anisotropic aspect preserved");}
        std::cout<<"extreme transforms: overflow, huge, degenerate, underflow, anisotropic: PASS\n";
    }
    double Ms(std::chrono::steady_clock::time_point t){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();}
    void WorkBudgets(){
        // One sprite re-placing the same depth many times, instanced many times:
        // small output but quadratic replay unless the replay budget stops it.
        {
            Bytes tags;Define(tags,1);Bytes s;s.W(2);s.W(1);for(int i=0;i<20000;++i)Place(s,1,1,i>0);s.Tag(1);s.Tag(0);tags.Tag(39,s);
            for(unsigned d=1;d<=2000;++d)Place(tags,2,d);tags.Label("icon");tags.Tag(1);
            std::vector<std::string> w;auto m=Parse(File(tags).v,w);Check(bool(m),"replay fixture parses");
            auto t=std::chrono::steady_clock::now();auto r=Render(*m,"icon",Variant::Original,256);auto ms=Ms(t);
            Check(!r.bitmap&&!r.warnings.empty(),"timeline replay budget rejects repeated placements");
            Check(ms<200,"replay budget bounds render time");std::cout<<"  replay fan-out rejected in "<<ms<<" ms\n";
        }
        // Wide nested fan-out: 32 placements per level, three levels.
        {
            Bytes tags;Define(tags,1);unsigned inner=1;
            for(unsigned level=0;level<3;++level){Bytes s;s.W(100+level);s.W(1);for(unsigned d=1;d<=32;++d)Place(s,inner,d);s.Tag(1);s.Tag(0);tags.Tag(39,s);inner=100+level;}
            Place(tags,inner);tags.Label("icon");tags.Tag(1);std::vector<std::string> w;auto m=Parse(File(tags).v,w);Check(bool(m),"fan-out fixture parses");
            auto t=std::chrono::steady_clock::now();auto r=Render(*m,"icon",Variant::Original,256);auto ms=Ms(t);
            Check(!r.bitmap&&!r.warnings.empty(),"instance fan-out is rejected");Check(ms<200,"fan-out budget bounds render time");
            std::cout<<"  nested fan-out rejected in "<<ms<<" ms\n";
        }
        // Many overlapping paths: each costs a full canvas pass.
        {
            Bytes tags;for(unsigned id=1;id<=400;++id)Define(tags,id);for(unsigned id=1;id<=400;++id)Place(tags,id,id);tags.Label("icon");tags.Tag(1);
            auto m=ParseOK(File(tags));auto t=std::chrono::steady_clock::now();auto r=Render(*m,"icon",Variant::Original,256);auto ms=Ms(t);
            Check(!r.bitmap&&!r.warnings.empty(),"canvas pass budget rejects excessive paths");Check(ms<200,"pass budget bounds render time");
            std::cout<<"  400-path frame rejected in "<<ms<<" ms\n";
        }
        // Within budget: 100 layered paths still render.
        {Bytes tags;for(unsigned id=1;id<=100;++id)Define(tags,id);for(unsigned id=1;id<=100;++id)Place(tags,id,id);tags.Label("icon");tags.Tag(1);
         Check(Render(*ParseOK(File(tags)),"icon",Variant::Original,256).bitmap.has_value(),"100 paths stay within budget");}
        std::cout<<"work budgets: replay, fan-out, canvas passes: PASS\n";
    }
    // Truncate the payload of each nested tag while keeping all outer length
    // fields consistent, so the failure happens inside that tag's parser.
    void DeepTruncation(){
        Bytes inner;Style s;s.curve=true;Define(inner,1,s);
        auto build=[&](std::size_t cut){
            auto shape=Shape(1,s);shape.v.resize(std::min(cut,shape.v.size()));
            Bytes tags;tags.Tag(32,shape);
            Bytes sprite;sprite.W(2);sprite.W(1);Place(sprite,1);sprite.Tag(1);sprite.Tag(0);tags.Tag(39,sprite);
            Place(tags,2);tags.Label("icon");tags.Tag(1);return File(tags);
        };
        auto full=Shape(1,s).v.size();std::size_t rejected=0;
        for(std::size_t cut=0;cut<full;++cut){std::vector<std::string> w;auto m=Parse(build(cut).v,w);if(!m){Check(!w.empty(),"truncated shape warns");++rejected;}else{auto r=Render(*m,"icon",Variant::Original,64);Check(r.bitmap.has_value()||!r.warnings.empty(),"truncated shape fails cleanly");}}
        Check(rejected>=full*9/10,"truncated shape payloads are rejected");
        // Truncate inside the sprite's own tag list.
        Bytes tags;Define(tags,1);Bytes sprite;sprite.W(2);sprite.W(1);Place(sprite,1);sprite.Tag(1);sprite.Tag(0);
        for(std::size_t cut=4;cut<sprite.v.size();++cut){
            Bytes t=tags;Bytes part;part.v.assign(sprite.v.begin(),sprite.v.begin()+cut);t.Tag(39,part);Place(t,2);t.Label("icon");t.Tag(1);
            std::vector<std::string> w;auto m=Parse(File(t).v,w);if(m){auto r=Render(*m,"icon",Variant::Original,64);Check(r.bitmap.has_value()||!r.warnings.empty(),"truncated sprite fails cleanly");}else Check(!w.empty(),"truncated sprite warns");
        }
        // Truncated CWS stream: header length is correct but the zlib data stops early.
        auto cws=Simple({},true);for(std::size_t cut=9;cut<cws.v.size();cut+=3){std::vector<std::string> w;auto v=cws.v;v.resize(cut);Check(!Parse(v,w)&&!w.empty(),"truncated CWS rejected");}
        std::cout<<"deep truncation: shape records, sprite tag list, CWS stream: PASS\n";
    }
}
int main(){try{InvalidInputs();StrokeBounds();ExtremeTransforms();WorkBudgets();DeepTruncation();std::cout<<"SwfRasterHardeningTests: "<<checks<<" checks PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
