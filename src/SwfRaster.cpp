#include "SwfRaster.h"
#ifdef WHEEL_SWF_DIAGNOSTICS
#include "SwfRasterDiagnostics.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <miniz.h>
#include <plutovg.h>

// Adobe SWF File Format v19: shapes, display lists and bitmap storage.
// All coordinates remain in twips until the final viewport transform.
namespace Wheel::Swf {
namespace {
    constexpr std::size_t MaxFile = 32 * 1024 * 1024, MaxTags = 100000;
    constexpr std::size_t MaxEdges = 500000, MaxStyles = 16384, MaxPixels = 16 * 1024 * 1024;
    constexpr unsigned MaxDepth = 32, MaxDraws = 4096;
    // Per-Render work budgets. Measured on BOOBIES + Handy Icon Collection
    // (1165 nonempty icons): max 66 paths (66 canvas passes), 277 replayed ops,
    // 4 instances, 43810 device points; see build/i4-phase1/round4/REPORT.md.
    // Each budget keeps >=2x headroom over that maximum while bounding one
    // Render to tens of milliseconds and capping input-dependent allocations.
    // Exceeding one fails the icon so the caller uses the built-in icon.
    constexpr std::size_t MaxReplayOps = 4096;        // timeline ops visited by all Display() calls
    constexpr std::size_t MaxPathPoints = 131072;     // device points in one PlutoVG path
    constexpr std::size_t MaxRenderPoints = 262144;   // device points across one Render
    constexpr std::size_t MaxCanvasPasses = 160;      // full-canvas scan/clear passes (paths + gradient setup)
    // Paths reach PlutoVG in device space, through float and FreeType 26.6 fixed
    // point stored in a 32-bit long on Windows. Keep far inside that range.
    constexpr double MaxDevice = 16384;
    void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    struct Reader {
        std::span<const std::uint8_t> data;
        std::size_t bit = 0;
        std::size_t Left() const { return data.size() - (bit + 7) / 8; }
        void Align() { bit = (bit + 7) & ~std::size_t(7); }
        std::uint32_t U(unsigned n) {
            Require(n <= 32 && bit <= data.size()*8 && n <= data.size()*8-bit, "truncated SWF bit field");
            std::uint32_t value=0;
            for (unsigned i=0;i<n;++i,++bit) value=(value<<1)|((data[bit/8]>>(7-bit%8))&1);
            return value;
        }
        std::int32_t S(unsigned n) {
            auto v=U(n); if (!n) return 0;
            return static_cast<std::int32_t>(n==32 ? v : (v & (1u<<(n-1))) ? v | (~0u<<n) : v);
        }
        std::span<const std::uint8_t> Bytes(std::size_t n) {
            Align(); Require(n<=Left(), "truncated SWF tag"); auto v=data.subspan(bit/8,n); bit+=n*8; return v;
        }
        unsigned Byte() { return Bytes(1)[0]; }
        unsigned Word() { auto b=Bytes(2); return b[0]|(b[1]<<8); }
        std::uint32_t Dword() { auto b=Bytes(4); return b[0]|(b[1]<<8)|(b[2]<<16)|(std::uint32_t(b[3])<<24); }
        std::string String() {
            Align(); std::string s;
            while (true) { auto c=Byte(); if (!c) return s; Require(s.size()<4096,"SWF string limit"); s+=char(c); }
        }
        void Rect() { Align(); auto n=U(5); for(int i=0;i<4;++i) S(n); Align(); }
    };
    struct Point { double x=0,y=0; bool operator==(const Point&) const = default; };
    struct Matrix {
        double a=1,b=0,c=0,d=1,tx=0,ty=0;
        Point Map(Point p) const { return {a*p.x+c*p.y+tx,b*p.x+d*p.y+ty}; }
        Matrix operator*(const Matrix& m) const {
            return {a*m.a+c*m.b,b*m.a+d*m.b,a*m.c+c*m.d,b*m.c+d*m.d,a*m.tx+c*m.ty+tx,b*m.tx+d*m.ty+ty};
        }
        Matrix Inverse() const {
            double det=a*d-b*c; Require(std::isfinite(det)&&std::abs(det)>1e-18,"singular fill matrix");
            return {d/det,-b/det,-c/det,a/det,(c*ty-d*tx)/det,(b*tx-a*ty)/det};
        }
        bool Finite() const {
            return std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c)&&std::isfinite(d)&&std::isfinite(tx)&&std::isfinite(ty);
        }
    };
    Matrix ReadMatrix(Reader& r) {
        r.Align(); Matrix m;
        if(r.U(1)) { auto n=r.U(5); m.a=r.S(n)/65536.;m.d=r.S(n)/65536.; }
        if(r.U(1)) { auto n=r.U(5); m.b=r.S(n)/65536.;m.c=r.S(n)/65536.; }
        auto n=r.U(5);m.tx=r.S(n);m.ty=r.S(n);r.Align();return m;
    }
    using Color=std::array<double,4>;
    struct CX {
        Color mul{1,1,1,1},add{0,0,0,0};
        CX operator*(const CX& inner) const {
            CX v; for(int i=0;i<4;++i) { v.mul[i]=mul[i]*inner.mul[i];v.add[i]=mul[i]*inner.add[i]+add[i]; } return v;
        }
        Color Apply(Color c,Variant variant) const {
            for(int i=0;i<4;++i) c[i]=std::clamp(c[i]*mul[i]+(variant==Variant::Mask?0:add[i]),0.,1.);
            if(variant==Variant::Mask) c[0]=c[1]=c[2]=1;
            return c;
        }
    };
    CX ReadCX(Reader& r,bool alpha) {
        r.Align();auto hasAdd=r.U(1),hasMul=r.U(1),n=r.U(4);CX v;int count=alpha?4:3;
        if(hasMul) for(int i=0;i<count;++i) v.mul[i]=r.S(n)/256.;
        if(hasAdd) for(int i=0;i<count;++i) v.add[i]=r.S(n)/255.;
        r.Align();return v;
    }
    Color ReadColor(Reader& r,bool alpha) { Color c{r.Byte()/255.,r.Byte()/255.,r.Byte()/255.,1};if(alpha)c[3]=r.Byte()/255.;return c; }
    struct Stop { unsigned ratio; Color color; };
    struct Fill {
        unsigned type=0,id=0,spread=0,interpolation=0;
        Color color{0,0,0,1};Matrix matrix;std::vector<Stop> stops;double focal=0;
    };
    struct Edge { Point from,to,control;bool curve=false,breakBefore=false; };
    struct Path {
        Fill fill;std::vector<Edge> edges;std::vector<std::vector<Edge>> contours;
        double width=0,miter=3;unsigned cap=0,join=0;bool stroke=false,noClose=false;
        std::string unsupported;
    };
    struct Shape { std::vector<Path> paths;bool winding=false;std::string unsupported; };
    struct Op {
        unsigned frame=0,depth=0,id=0;bool remove=false,move=false,hasID=false,hasMatrix=false,hasCX=false;
        Matrix matrix;CX cx;std::string unsupported;
    };
    struct Timeline { std::vector<Op> ops;std::map<std::string,unsigned,std::less<>> labels;unsigned frames=0; };
    struct Character { std::optional<Shape> shape;std::optional<Timeline> sprite;std::optional<Bitmap> bitmap;std::string unsupported; };
    std::vector<std::uint8_t> Inflate(std::span<const std::uint8_t> input,std::size_t size) {
        Require(size<=MaxFile*2,"inflated data limit");std::vector<std::uint8_t> out(size);
        mz_stream stream{};stream.next_in=input.data();stream.avail_in=static_cast<unsigned>(input.size());
        stream.next_out=out.data();stream.avail_out=static_cast<unsigned>(size);
        Require(mz_inflateInit(&stream)==MZ_OK,"inflate init failed");
        auto status=mz_inflate(&stream,MZ_FINISH);bool ok=status==MZ_STREAM_END&&stream.total_out==size&&stream.avail_in==0;
        mz_inflateEnd(&stream);Require(ok,"invalid compressed SWF data");return out;
    }
    struct ComScope { HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~ComScope(){if(SUCCEEDED(hr))CoUninitialize();} };
    Bitmap DecodeImage(std::span<const std::uint8_t> input) {
        ComScope com;using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
        Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"WIC factory unavailable");
        Require(SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(input.data()),static_cast<DWORD>(input.size())))&&
            SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame)),"WIC image decode failed");
        Bitmap b;Require(SUCCEEDED(frame->GetSize(&b.width,&b.height))&&b.width&&b.height&&std::uint64_t(b.width)*b.height<=MaxPixels,"bitmap dimensions limit");
        b.rgba.resize(std::size_t(b.width)*b.height*4);
        Require(SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))&&
            SUCCEEDED(converter->CopyPixels(nullptr,b.width*4,static_cast<UINT>(b.rgba.size()),b.rgba.data())),"WIC RGBA conversion failed");return b;
    }
    void Unpremultiply(std::uint8_t* p) {
        for(unsigned k=0;k<3;++k) p[k]=p[3]?static_cast<std::uint8_t>(std::min(255u,(unsigned(p[k])*255+p[3]/2)/p[3])):0;
    }
}
struct Movie { Timeline main;std::unordered_map<unsigned,Character> characters; };
namespace {
    struct Parser {
        Movie movie;std::map<unsigned,std::size_t> unknown;std::vector<std::string>& warnings;
        std::size_t tags=0,edges=0,styles=0,pixels=0,implicitSpriteEnds=0,trailingSpriteBytes=0;std::vector<std::uint8_t> jpegTables;
        Fill ReadFill(Reader& r,unsigned version) {
            Fill f;f.type=r.Byte();
            if(f.type==0) f.color=ReadColor(r,version>=3);
            else if(f.type==0x10||f.type==0x12||f.type==0x13) {
                f.matrix=ReadMatrix(r);f.spread=r.U(2);f.interpolation=r.U(2);auto count=r.U(4);
                Require(count>0,"empty gradient");
                for(unsigned i=0;i<count;++i) {auto ratio=r.Byte();auto c=ReadColor(r,version>=3);Require(f.stops.empty()||ratio>=f.stops.back().ratio,"unordered gradient stops");f.stops.push_back({ratio,c});}
                if(f.type==0x13)f.focal=static_cast<std::int16_t>(r.Word())/256.;
                if(f.spread==3||f.interpolation>1) {warnings.push_back("reserved gradient mode: using pad/sRGB");if(f.spread==3)f.spread=0;if(f.interpolation>1)f.interpolation=0;}
            } else if(f.type>=0x40&&f.type<=0x43) {f.id=r.Word();f.matrix=ReadMatrix(r);}
            else throw std::runtime_error("unsupported fill type "+std::to_string(f.type));
            return f;
        }
        unsigned Count(Reader& r,unsigned version) {auto n=r.Byte();return n==255&&version>=2?r.Word():n;}
        void ReadStyles(Reader& r,unsigned version,Shape& s,std::vector<unsigned>& fills,std::vector<unsigned>& lines) {
            fills.clear();lines.clear();
            auto count=Count(r,version);Require((styles+=count)<=MaxStyles,"style limit");
            for(unsigned i=0;i<count;++i) {Path p;p.fill=ReadFill(r,version);fills.push_back(static_cast<unsigned>(s.paths.size()));s.paths.push_back(std::move(p));}
            count=Count(r,version);Require((styles+=count)<=MaxStyles,"style limit");
            for(unsigned i=0;i<count;++i) {
                Path p;p.stroke=true;p.width=r.Word();
                if(version==4) {
                    p.cap=r.U(2);p.join=r.U(2);bool fill=r.U(1),noH=r.U(1),noV=r.U(1),pixelHint=r.U(1);
                    r.U(5);p.noClose=r.U(1);auto endCap=r.U(2);r.Align();
                    if(p.join==2)p.miter=r.Word()/256.;
                    p.fill=fill?ReadFill(r,version):Fill{};if(!fill)p.fill.color=ReadColor(r,true);
                    if(noH||noV||pixelHint||endCap!=p.cap||p.cap==3||p.join==3)p.unsupported="unsupported LineStyle2 scale/hint/asymmetric cap";
                } else p.fill.color=ReadColor(r,version>=3);
                lines.push_back(static_cast<unsigned>(s.paths.size()));s.paths.push_back(std::move(p));
            }
        }
        void Stitch(Path& p) {
            // fill1 follows the edge; fill0 reverses it. Integer twip endpoints are exact keys.
            using Key=std::pair<std::int64_t,std::int64_t>;
            auto key=[](Point v)->Key{return {std::llround(v.x),std::llround(v.y)};};
            if(p.stroke) {
                for(const auto& e:p.edges) {
                    if(e.breakBefore||p.contours.empty()||key(p.contours.back().back().to)!=key(e.from))p.contours.emplace_back();
                    p.contours.back().push_back(e);
                }
            } else {
                std::map<Key,std::vector<std::size_t>> starts;
                for(std::size_t i=0;i<p.edges.size();++i)starts[key(p.edges[i].from)].push_back(i);
                std::vector<bool> used(p.edges.size());
                for(std::size_t i=0;i<p.edges.size();++i) {
                    if(used[i])continue;std::vector<Edge> chain;auto index=i;
                    while(true) {
                        used[index]=true;chain.push_back(p.edges[index]);auto end=key(chain.back().to);
                        if(end==key(chain.front().from))break;
                        auto it=starts.find(end);Require(it!=starts.end(),"open fill contour");
                        auto& candidates=it->second;while(!candidates.empty()&&used[candidates.back()])candidates.pop_back();
                        Require(!candidates.empty(),"unclosed fill contour");index=candidates.back();candidates.pop_back();
                    }
                    p.contours.push_back(std::move(chain));
                }
            }
            p.edges.clear();p.edges.shrink_to_fit();
        }
        Shape ReadShape(Reader& r,unsigned version) {
            Shape s;r.Rect();if(version==4){r.Rect();auto flags=r.Byte();s.winding=(flags&4)!=0;}
            std::vector<unsigned> fills,lines;ReadStyles(r,version,s,fills,lines);
            auto fillBits=r.U(4),lineBits=r.U(4);unsigned f0=0,f1=0,line=0;Point pos;bool strokeBreak=true;
            auto index=[&](unsigned i,const std::vector<unsigned>& list) {Require(i<=list.size(),"invalid shape style index");return i?list[i-1]+1:0;};
            auto put=[&](unsigned i,Edge e,bool reverse){
                if(!i)return;if(reverse)std::swap(e.from,e.to);s.paths[i-1].edges.push_back(e);
            };
            while(true) {
                auto edge=r.U(1);
                if(!edge) {
                    auto flags=r.U(5);if(!flags)break;if(flags&9)strokeBreak=true;
                    if(flags&1){auto n=r.U(5);pos={double(r.S(n)),double(r.S(n))};}
                    if(flags&2)f0=index(r.U(fillBits),fills);if(flags&4)f1=index(r.U(fillBits),fills);if(flags&8)line=index(r.U(lineBits),lines);
                    if(flags&16){r.Align();ReadStyles(r,version,s,fills,lines);fillBits=r.U(4);lineBits=r.U(4);}
                } else {
                    Require(++edges<=MaxEdges,"shape edge limit");bool straight=r.U(1)!=0;auto n=r.U(4)+2;Edge e;e.from=pos;
                    if(straight) {bool general=r.U(1)!=0;if(general){pos.x+=r.S(n);pos.y+=r.S(n);}else if(r.U(1))pos.y+=r.S(n);else pos.x+=r.S(n);}
                    else {e.curve=true;e.control={pos.x+r.S(n),pos.y+r.S(n)};pos={e.control.x+r.S(n),e.control.y+r.S(n)};}
                    Require(std::abs(pos.x)<1e8&&std::abs(pos.y)<1e8,"shape coordinate limit");e.to=pos;
                    if(f0!=f1){put(f0,e,true);put(f1,e,false);}e.breakBefore=strokeBreak;put(line,e,false);strokeBreak=false;
                }
            }
            r.Align();
            for(auto& p:s.paths) {
                Stitch(p);
                if(!p.stroke&&!p.contours.empty()) {
                    // Rotate every contour, and reverse all together to retain winding holes.
                    auto less=[](Point a,Point b){return std::pair(a.x,a.y)<std::pair(b.x,b.y);};
                    for(auto& chain:p.contours) {
                        auto first=std::min_element(chain.begin(),chain.end(),[&](const Edge& a,const Edge& b){return less(a.from,b.from);});
                        std::rotate(chain.begin(),first,chain.end());
                    }
                    const auto& first=p.contours.front();
                    if(less(first.back().from,first.front().to))for(auto& chain:p.contours) {
                        std::reverse(chain.begin(),chain.end());for(auto& e:chain)std::swap(e.from,e.to);
                    }
                }
            }
            return s;
        }
        Bitmap Lossless(Reader& r,bool alpha) {
            auto format=r.Byte();Bitmap b;b.width=r.Word();b.height=r.Word();
            Require(b.width&&b.height&&std::uint64_t(b.width)*b.height<=MaxPixels,"bitmap dimensions limit");
            auto count=format==3?r.Byte()+1:0;std::size_t stride=0,table=count*(alpha?4:3);
            if(format==3)stride=(b.width+3)&~3u;else if(format==4&&!alpha)stride=(b.width*2+3)&~3u;else if(format==5)stride=b.width*4;else throw std::runtime_error("unsupported lossless format");
            auto bytes=Inflate(r.Bytes(r.Left()),table+stride*b.height);b.rgba.resize(std::size_t(b.width)*b.height*4);
            for(unsigned y=0;y<b.height;++y)for(unsigned x=0;x<b.width;++x) {
                auto p=&b.rgba[(std::size_t(y)*b.width+x)*4];auto src=&bytes[table+y*stride];p[3]=255;
                if(format==3){auto index=src[x];Require(index<count,"invalid bitmap palette index");auto c=&bytes[index*(alpha?4:3)];std::copy_n(c,alpha?4:3,p);}
                else if(format==4){unsigned v=src[2*x]|(src[2*x+1]<<8);p[0]=((v>>10)&31)*255/31;p[1]=((v>>5)&31)*255/31;p[2]=(v&31)*255/31;}
                else {src+=x*4;p[0]=src[1];p[1]=src[2];p[2]=src[3];if(alpha)p[3]=src[0];}
                if(alpha)Unpremultiply(p);
            }return b;
        }
        void Add(unsigned id,Character c) {
            if(c.bitmap) {
                // Bitmaps may also be placed directly; their natural size is in pixels.
                Shape shape;Path path;path.fill.type=0x41;path.fill.id=id;path.fill.matrix={20,0,0,20,0,0};
                double w=c.bitmap->width*20.,h=c.bitmap->height*20.;
                path.contours={{{{0,0},{w,0}},{{w,0},{w,h}},{{w,h},{0,h}},{{0,h},{0,0}}}};
                shape.paths.push_back(std::move(path));c.shape=std::move(shape);
            }
            Require(id&&movie.characters.find(id)==movie.characters.end(),"duplicate/zero character ID");
            if(c.bitmap){pixels+=std::size_t(c.bitmap->width)*c.bitmap->height;Require(pixels<=MaxPixels,"movie bitmap budget");}
            movie.characters.emplace(id,std::move(c));
        }
        Timeline Tags(Reader& r,unsigned nesting,unsigned declared=0) {
            Require(nesting<=MaxDepth,"sprite nesting limit");Timeline timeline;bool ended=false;
            while(r.Left()) {
                Require(++tags<=MaxTags,"tag limit");auto header=r.Word(),type=header>>6;std::size_t length=header&63;if(length==63)length=r.Dword();Reader t{r.Bytes(length)};
                try {
                if(type==0){Require(length==0,"invalid End");ended=true;break;}
                // WitcherSigns ends a one-frame sprite with 00 05 (empty tag 20),
                // not an overlong child payload. FFDec retains an error stub and
                // Ruffle skips the failed definition; neither needs an outer read.
                // Tolerate only this terminal empty definition after a placement.
                if(nesting&&declared==1&&type==20&&length==0&&!r.Left()&&timeline.frames==0&&
                    std::any_of(timeline.ops.begin(),timeline.ops.end(),[](const Op& op){return op.hasID&&!op.remove;})) {
                    warnings.push_back("ignored terminal empty DefineBitsLossless in one-frame sprite");continue;
                }
                if(type==1){Require(length==0,"invalid ShowFrame");Require(++timeline.frames<=65535,"frame limit");}
                else if(type==43){auto label=t.String();timeline.labels.try_emplace(std::move(label),timeline.frames);if(t.Left())t.Byte();}
                else if(type==2||type==22||type==32||type==83) {
                    auto id=t.Word();Character c;
                    try {c.shape=ReadShape(t,type==2?1:type==22?2:type==32?3:4);}
                    catch(const std::runtime_error& e) {
                        if(!std::string_view(e.what()).starts_with("unsupported fill type"))throw;
                        c.unsupported=e.what();warnings.push_back("character "+std::to_string(id)+": "+c.unsupported);t.Bytes(t.Left());
                    }
                    Add(id,std::move(c));
                } else if(type==39) {
                    auto id=t.Word();auto declared=t.Word();Character c;c.sprite=Tags(t,nesting+1,declared);if(c.sprite->frames==0&&declared==1)c.sprite->frames=1;Add(id,std::move(c));
                } else if(type==20||type==36){auto id=t.Word();Character c;c.bitmap=Lossless(t,type==36);Add(id,std::move(c));}
                else if(type==8){auto bytes=t.Bytes(t.Left());jpegTables.assign(bytes.begin(),bytes.end());}
                else if(type==6||type==21||type==35) {
                    auto id=t.Word();auto size=type==35?t.Dword():t.Left();auto bytes=t.Bytes(size);std::vector<std::uint8_t> image(bytes.begin(),bytes.end());
                    if(type==6){Require(jpegTables.size()>=4,"missing JPEGTables");auto joined=jpegTables;joined.resize(joined.size()-2);if(image.size()>=2&&image[0]==255&&image[1]==216)image.erase(image.begin(),image.begin()+2);joined.insert(joined.end(),image.begin(),image.end());image=std::move(joined);}
                    // Older encoders insert EOI/SOI pairs between JPEG tables and data.
                    std::vector<std::uint8_t> clean;clean.reserve(image.size());
                    for(std::size_t i=0;i<image.size();) {
                        if(i+3<image.size()&&image[i]==255&&image[i+1]==217&&image[i+2]==255&&image[i+3]==216)i+=4;
                        else clean.push_back(image[i++]);
                    }
                    image=std::move(clean);
                    Character c;c.bitmap=DecodeImage(image);if(type==35){auto alpha=Inflate(t.Bytes(t.Left()),std::size_t(c.bitmap->width)*c.bitmap->height);for(std::size_t i=0;i<alpha.size();++i){auto pixel=&c.bitmap->rgba[i*4];pixel[3]=alpha[i];Unpremultiply(pixel);}}Add(id,std::move(c));
                } else if(type==4||type==26||type==70) {
                    Op op;op.frame=timeline.frames;
                    if(type==4){op.id=t.Word();op.hasID=true;op.depth=t.Word();op.matrix=ReadMatrix(t);op.hasMatrix=true;if(t.Left()){op.cx=ReadCX(t,false);op.hasCX=true;}}
                    else {
                        auto flags=t.Byte(),flags2=type==70?t.Byte():0;op.depth=t.Word();op.move=(flags&1)!=0;op.hasID=(flags&2)!=0;
                        if((flags2&8)||((flags2&16)&&op.hasID)){t.String();op.unsupported="unsupported class/image placement";}
                        if(op.hasID)op.id=t.Word();if(flags&4){op.hasMatrix=true;op.matrix=ReadMatrix(t);}if(flags&8){op.hasCX=true;op.cx=ReadCX(t,true);}
                        if(flags&16)t.Word();if(flags&32)t.String();if(flags&64){t.Word();op.unsupported="unsupported clip depth";}
                        if(flags2&0x67)op.unsupported="unsupported filter/blend/cache/visibility placement";
                        // ClipActions are deliberately ignored, just like DoAction/InitAction.
                        t.Bytes(t.Left());
                    }
                    Require(op.depth>0,"zero placement depth");timeline.ops.push_back(std::move(op));
                } else if(type==5||type==28) {
                    Op op;op.frame=timeline.frames;op.remove=true;if(type==5){op.id=t.Word();op.hasID=true;}op.depth=t.Word();timeline.ops.push_back(op);
                } else if(type==12||type==59) {t.Bytes(t.Left());}
                else {++unknown[type];t.Bytes(t.Left());}
                Require(t.Left()==0,"unexpected tag payload bytes");
                } catch(const std::exception& e) {throw std::runtime_error("tag "+std::to_string(type)+" at "+std::to_string(r.bit/8-length)+": "+e.what());}
            }
            if(!ended&&nesting) ++implicitSpriteEnds;
            Require(ended||nesting,"missing End tag");if(r.Left()&&nesting){trailingSpriteBytes+=r.Left();r.Bytes(r.Left());}
            Require(!r.Left(),"bytes after End tag");
            for(const auto& [label,frame]:timeline.labels)if(frame==timeline.frames){++timeline.frames;warnings.push_back("implicit final ShowFrame");break;}
            for(const auto& [label,frame]:timeline.labels)Require(frame<timeline.frames,"label without ShowFrame");return timeline;
        }
    };
    struct Instance { unsigned id=0;Matrix matrix;CX cx;std::string unsupported; };
    // Cumulative cost of one Render; every loop that scales with input draws from it.
    struct Work { std::size_t instances=0,ops=0,points=0,coverage=0; };
    std::map<unsigned,Instance> Display(const Timeline& timeline,unsigned frame,Work& work) {
        std::map<unsigned,Instance> list;
        for(const auto& op:timeline.ops) {
            if(op.frame>frame)break;Require(++work.ops<=MaxReplayOps,"timeline replay budget");auto it=list.find(op.depth);
            if(op.remove){if(it!=list.end()&&(!op.hasID||it->second.id==op.id))list.erase(it);continue;}
            if(op.move&&it==list.end())throw std::runtime_error("move at missing depth");
            Instance inst=op.move?it->second:Instance{};
            if(op.hasID)inst.id=op.id;if(op.hasMatrix)inst.matrix=op.matrix;if(op.hasCX)inst.cx=op.cx;
            if(!op.unsupported.empty())inst.unsupported=op.unsupported;
            Require(inst.id!=0,"placement without character");list[op.depth]=std::move(inst);
        }return list;
    }
    struct Draw { const Path* path;const Shape* shape;Matrix matrix;CX cx; };
    void Collect(const Movie& m,const Timeline& t,unsigned frame,Matrix matrix,CX cx,std::vector<Draw>& draws,unsigned depth,Work& work) {
        Require(depth<=MaxDepth,"render sprite depth/cycle limit");
        for(const auto& [slot,i]:Display(t,frame,work)) {
            Require(++work.instances<=MaxDraws,"display instance limit");Require(i.unsupported.empty(),i.unsupported.c_str());
            auto it=m.characters.find(i.id);Require(it!=m.characters.end(),"missing/unsupported character");auto& c=it->second;
            Require(c.unsupported.empty(),c.unsupported.c_str());auto mat=matrix*i.matrix;auto color=cx*i.cx;
            // Nested scales can overflow double well before reaching PlutoVG.
            Require(mat.Finite(),"non-finite instance transform");
            if(c.sprite)Collect(m,*c.sprite,0,mat,color,draws,depth+1,work);
            else if(c.shape)for(const auto& p:c.shape->paths) {
                if(p.contours.empty())continue;Require(p.unsupported.empty(),p.unsupported.c_str());Require(draws.size()<MaxDraws,"render path limit");draws.push_back({&p,&*c.shape,mat,color});
            } else throw std::runtime_error("unsupported direct bitmap placement");
        }
    }
    struct Bounds {
        double x0=std::numeric_limits<double>::infinity(),y0=x0,x1=-x0,y1=-x0;
        void Add(Point p) {Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1e12&&std::abs(p.y)<1e12,"geometry bounds limit");x0=std::min(x0,p.x);y0=std::min(y0,p.y);x1=std::max(x1,p.x);y1=std::max(y1,p.y);}
        void EdgeBounds(const Edge& e,Matrix m) {
            auto p=m.Map(e.from),q=m.Map(e.to),c=m.Map(e.control);Add(p);Add(q);if(!e.curve)return;
            auto extremum=[&](double a,double b,double v){auto den=a-2*v+b;if(std::abs(den)<1e-12)return;auto t=(a-v)/den;if(t>0&&t<1)Add({(1-t)*(1-t)*p.x+2*(1-t)*t*c.x+t*t*q.x,(1-t)*(1-t)*p.y+2*(1-t)*t*c.y+t*t*q.y});};
            extremum(p.x,q.x,c.x);extremum(p.y,q.y,c.y);
        }
    };
    template<class T,void (*Destroy)(T*)> using Handle=std::unique_ptr<T,decltype(Destroy)>;
    using VGPath=Handle<plutovg_path_t,plutovg_path_destroy>;
    using Surface=Handle<plutovg_surface_t,plutovg_surface_destroy>;
    using Canvas=Handle<plutovg_canvas_t,plutovg_canvas_destroy>;
    // Build the path directly in device pixels. Every coordinate is mapped and
    // range-checked in double before narrowing to float, so PlutoVG only ever
    // receives an identity matrix and finite, bounded points (no inf/NaN can
    // reach its float -> fixed-point conversion).
    VGPath MakePath(const Path& p,const Matrix& toDevice,Work& work) {
        std::size_t elements=0,count=0;
        for(const auto& chain:p.contours){elements+=4;count+=1;for(const auto& e:chain){elements+=e.curve?4:2;count+=e.curve?3:1;}}
        Require(count<=MaxPathPoints&&(work.points+=count)<=MaxRenderPoints,"render path point budget");
        VGPath path(plutovg_path_create(),plutovg_path_destroy);Require(bool(path),"path allocation failed");
        // Reserve once (move/line/close = 2 elements, quad->cubic = 4) so the path never regrows.
        plutovg_path_reserve(path.get(),static_cast<int>(elements));
        auto map=[&](Point v){
            auto d=toDevice.Map(v);
            Require(std::isfinite(d.x)&&std::isfinite(d.y)&&std::abs(d.x)<=MaxDevice&&std::abs(d.y)<=MaxDevice,"device coordinate out of range");
            return std::pair{float(d.x),float(d.y)};
        };
        for(const auto& chain:p.contours) {
            auto [fx,fy]=map(chain.front().from);plutovg_path_move_to(path.get(),fx,fy);
            for(const auto& e:chain) {
                if(e.curve){auto [cx,cy]=map(e.control);auto [x,y]=map(e.to);plutovg_path_quad_to(path.get(),cx,cy,x,y);}
                else {auto [x,y]=map(e.to);plutovg_path_line_to(path.get(),x,y);}
            }
            if(!p.stroke||(!p.noClose&&chain.back().to==chain.front().from))plutovg_path_close(path.get());
        }return path;
    }
    // Device-space stroke width; Flash strokes scale with the full instance
    // transform (RMS of the matrix columns, exactly PlutoVG's own estimate).
    double StrokeScale(const Matrix& m){return std::sqrt((m.a*m.a+m.b*m.b+m.c*m.c+m.d*m.d)*.5);}
    // Reach of a stroke beyond its centre line for a given width. Square caps
    // reach sqrt(2)*w/2 at a diagonal; miter joins up to miterLimit*w/2 (the
    // FreeType stroker bevels past the limit).
    double StrokeReach(const Path& p,double width){
        double reach=width*.5;
        if(p.cap==2)reach*=std::sqrt(2.);
        if(p.join==2)reach=std::max(reach,width*.5*std::max(1.,p.miter));
        return reach;
    }
    Color Mix(Color a,Color b,double t,bool linear) {
        Color c;for(int i=0;i<4;++i) {if(linear&&i<3)c[i]=std::pow(std::pow(a[i],2.2)*(1-t)+std::pow(b[i],2.2)*t,1/2.2);else c[i]=a[i]*(1-t)+b[i]*t;}return c;
    }
    struct Paint {
        const Fill& fill;Matrix inverse;std::array<Color,256> gradient{};const Bitmap* bitmap=nullptr;
        Paint(const Movie& movie,const Draw& draw,Matrix viewport):fill(draw.path->fill) {
            if(fill.type==0)return;inverse=(viewport*draw.matrix*fill.matrix).Inverse();
            if(fill.type>=0x40) {auto it=movie.characters.find(fill.id);Require(it!=movie.characters.end()&&it->second.bitmap.has_value(),"missing/unsupported bitmap fill");bitmap=&*it->second.bitmap;}
            else for(unsigned i=0;i<256;++i) {
                auto it=std::upper_bound(fill.stops.begin(),fill.stops.end(),i,[](unsigned ratio,const Stop& stop){return ratio<stop.ratio;});
                if(it==fill.stops.begin())gradient[i]=it->color;else if(it==fill.stops.end())gradient[i]=fill.stops.back().color;
                else {auto& a=*(it-1);gradient[i]=Mix(a.color,it->color,double(i-a.ratio)/(it->ratio-a.ratio),fill.interpolation==1);}
            }
        }
        Color Texel(int x,int y) const {
            int w=bitmap->width,h=bitmap->height;
            if(fill.type==0x40||fill.type==0x42){x=(x%w+w)%w;y=(y%h+h)%h;}
            else {x=std::clamp(x,0,w-1);y=std::clamp(y,0,h-1);}
            auto p=&bitmap->rgba[(std::size_t(y)*w+x)*4];return {p[0]/255.,p[1]/255.,p[2]/255.,p[3]/255.};
        }
        Color Sample(double x,double y) const {
            if(fill.type==0)return fill.color;auto p=inverse.Map({x,y});
            Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1e12&&std::abs(p.y)<1e12,"paint coordinate limit");
            if(bitmap) {
                if(fill.type==0x40||fill.type==0x42){p.x=std::fmod(p.x,bitmap->width);p.y=std::fmod(p.y,bitmap->height);}
                else if(p.x<0||p.y<0||p.x>=bitmap->width||p.y>=bitmap->height)return {0,0,0,0};
                if(fill.type>=0x42)return Texel(int(std::floor(p.x)),int(std::floor(p.y)));
                p.x-=.5;p.y-=.5;auto ix=int(std::floor(p.x)),iy=int(std::floor(p.y));double fx=p.x-ix,fy=p.y-iy;
                auto a=Texel(ix,iy),b=Texel(ix+1,iy),c=Texel(ix,iy+1),d=Texel(ix+1,iy+1);
                for(int k=0;k<3;++k){a[k]*=a[3];b[k]*=b[3];c[k]*=c[3];d[k]*=d[3];}
                auto v=Mix(Mix(a,b,fx,false),Mix(c,d,fx,false),fy,false);if(v[3]>0)for(int k=0;k<3;++k)v[k]/=v[3];return v;
            }
            double t=0;
            if(fill.type==0x10)t=(p.x+16384)/32768.;
            else {
                double x=p.x/16384.,y=p.y/16384.,f=std::clamp(fill.focal,-.99999,.99999),dx=x-f;
                double a=dx*dx+y*y,b=f*dx,root=std::sqrt(std::max(0.,b*b+a*(1-f*f)));
                t=a==0?0:a/(root-b);
            }
            if(fill.spread==1){t=std::fmod(std::abs(t),2.);if(t>1)t=2-t;}
            else if(fill.spread==2)t-=std::floor(t);else t=std::clamp(t,0.,1.);
            auto ratio=t*255;auto lo=unsigned(ratio);return Mix(gradient[lo],gradient[std::min(255u,lo+1)],ratio-lo,false);
        }
    };
}
std::shared_ptr<const Movie> Parse(std::span<const std::uint8_t> file,std::vector<std::string>& warnings) {
    try {
        Require(file.size()>=8&&file.size()<=MaxFile,"SWF file size/header limit");Reader header{file};auto signature=header.Bytes(3);header.Byte();auto size=header.Dword();
        Require(signature[1]=='W'&&signature[2]=='S',"invalid SWF signature");Require(signature[0]!='Z',"unsupported ZWS compression");Require(size>=12&&size<=MaxFile,"SWF declared size limit");
        std::vector<std::uint8_t> expanded;std::span<const std::uint8_t> body;
        if(signature[0]=='C'){expanded=Inflate(file.subspan(8),size-8);body=expanded;}
        else {Require(signature[0]=='F'&&size==file.size(),"invalid/truncated FWS length");body=file.subspan(8);}
        Reader r{body};r.Rect();r.Word();r.Word();Parser parser{{},{},warnings};parser.movie.main=parser.Tags(r,0);
        if(parser.trailingSpriteBytes)warnings.push_back("ignored bytes after sprite End: "+std::to_string(parser.trailingSpriteBytes));
        if(parser.implicitSpriteEnds)warnings.push_back("implicit bounded sprite End: "+std::to_string(parser.implicitSpriteEnds));
        for(auto [tag,count]:parser.unknown)warnings.push_back("unsupported tag "+std::to_string(tag)+": "+std::to_string(count));
        return std::make_shared<const Movie>(std::move(parser.movie));
    } catch(const std::exception& e){warnings.push_back(e.what());return {};}
}
static Result RenderImpl(const Movie& movie,std::string_view label,Variant variant,std::uint32_t edge,bool dropPaths=false
#ifdef WHEEL_SWF_DIAGNOSTICS
    ,const DiagnosticViewport* fixed=nullptr,DiagnosticFault fault=DiagnosticFault::None,DiagnosticWork* stats=nullptr
#endif
) {
    Result result;
    try {
        Require(edge>=1&&edge<=256,"render edge must be 1..256");
        unsigned width=edge,height=edge;
#ifdef WHEEL_SWF_DIAGNOSTICS
        if(fixed) {
            width=fixed->width;height=fixed->height;
            Require(width>0&&height>0&&width<=2048&&height<=2048,"diagnostic canvas limit");
            Require(std::isfinite(fixed->pixelsPerTwip)&&fixed->pixelsPerTwip>0&&std::isfinite(fixed->xMin)&&std::isfinite(fixed->yMin),"invalid diagnostic viewport");
        }
#endif
        auto labelIt=movie.main.labels.find(label);Require(labelIt!=movie.main.labels.end(),"frame label not found");
        std::vector<Draw> draws;Work work;
#ifdef WHEEL_SWF_DIAGNOSTICS
        struct Report{Work& w;DiagnosticWork* out;const std::vector<Draw>& d;~Report(){if(out)*out={w.instances,w.ops,w.points,w.coverage,d.size()};}} report{work,stats,draws};
#endif
        Collect(movie,movie.main,labelIt->second,{}, {},draws,0,work);
#ifdef WHEEL_SWF_DIAGNOSTICS
        if(draws.empty()&&fixed){result.bitmap=Bitmap{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};return result;}
#endif
        Require(!draws.empty(),"empty frame");
        // Geometry bounds in twips. Strokes add their exact device-independent
        // reach (cap/join aware); sub-pixel strokes are re-padded below once the
        // viewport scale, and therefore the 1 px floor, is known.
        Bounds bounds;std::size_t pointWork=0;
        for(const auto& draw:draws) {
            Bounds b;for(const auto& chain:draw.path->contours)for(const auto& e:chain){Require(++pointWork<=MaxEdges*2,"render edge budget");b.EdgeBounds(e,draw.matrix);}
            if(draw.path->stroke) {
                // Reach in the parent space: the largest singular value of the
                // instance matrix is the most any direction can be stretched.
                auto m=draw.matrix;double f=m.a*m.a+m.b*m.b+m.c*m.c+m.d*m.d,det=m.a*m.d-m.b*m.c;
                double stretch=std::sqrt((f+std::sqrt(std::max(0.,f*f-4*det*det)))*.5);
                double pad=StrokeReach(*draw.path,draw.path->width)*stretch;Require(std::isfinite(pad),"stroke bounds limit");
                b.x0-=pad;b.y0-=pad;b.x1+=pad;b.y1+=pad;
            }
            bounds.Add({b.x0,b.y0});bounds.Add({b.x1,b.y1});
        }
        double span=std::max(bounds.x1-bounds.x0,bounds.y1-bounds.y0);Require(span>0&&std::isfinite(span),"empty geometry");double scale=edge*.9/span;
        // Strokes thinner than one device pixel are drawn 1 px wide, so their
        // reach is fixed in pixels rather than twips. Widen once at this scale.
        double floorPad=0;
        for(const auto& draw:draws)if(draw.path->stroke&&draw.path->width*StrokeScale(draw.matrix)*scale<1.)floorPad=std::max(floorPad,StrokeReach(*draw.path,1.)/scale);
        if(floorPad>0){bounds.x0-=floorPad;bounds.y0-=floorPad;bounds.x1+=floorPad;bounds.y1+=floorPad;span+=2*floorPad;scale=edge*.9/span;}
        Matrix viewport{scale,0,0,scale,edge*.5-scale*(bounds.x0+bounds.x1)*.5,edge*.5-scale*(bounds.y0+bounds.y1)*.5};
        #ifdef WHEEL_SWF_DIAGNOSTICS
        if(fixed) {
            scale=fixed->pixelsPerTwip;
            viewport={scale,0,0,scale,-fixed->xMin*scale,-fixed->yMin*scale};
            const double cx=(bounds.x0+bounds.x1)*.5,cy=(bounds.y0+bounds.y1)*.5;
            if(fault==DiagnosticFault::Mirror)viewport=viewport*Matrix{-1,0,0,1,2*cx,0};
            if(fault==DiagnosticFault::Scale09)viewport=viewport*Matrix{.9,0,0,.9,.1*cx,.1*cy};
            if(fault==DiagnosticFault::Offset4px)viewport.tx+=4;
            dropPaths=fault==DiagnosticFault::DropPaths;
        }
#endif
        Require(viewport.Finite(),"non-finite viewport");
        Surface surface(plutovg_surface_create(width,height),plutovg_surface_destroy);Require(bool(surface),"surface allocation failed");Canvas canvas(plutovg_canvas_create(surface.get()),plutovg_canvas_destroy);Require(bool(canvas),"canvas allocation failed");
        // The canvas matrix stays identity: MakePath already emits checked device points.
        plutovg_canvas_set_rgba(canvas.get(),1,1,1,1);
        std::vector<Color> accum(std::size_t(width)*height,Color{0,0,0,0});
        const std::size_t area=std::size_t(width)*height,coverageBudget=area*MaxCanvasPasses;
        std::size_t drawIndex=0;
        for(const auto& draw:draws) {
            // Offline fault injection retains the original viewport; production
            // Render always uses dropPaths=false. Drop ceil(path count / 5).
            if(dropPaths&&(draws.size()-1-drawIndex++)%5==0)continue;
            const auto& p=*draw.path;const auto toDevice=viewport*draw.matrix;Require(toDevice.Finite(),"non-finite device transform");
            // Each path scans and clears the canvas once; a gradient also builds its table.
            work.coverage+=area+(p.fill.type>=0x10&&p.fill.type<0x40?area/4:0);
            Require(work.coverage<=coverageBudget,"render coverage budget");
            auto path=MakePath(p,toDevice,work);
            plutovg_color_t clear{0,0,0,0};plutovg_surface_clear(surface.get(),&clear);
            plutovg_canvas_set_fill_rule(canvas.get(),draw.shape->winding?PLUTOVG_FILL_RULE_NON_ZERO:PLUTOVG_FILL_RULE_EVEN_ODD);
            if(p.stroke) {
                // Flash hairlines and subpixel strokes have a one-device-pixel floor,
                // using the complete instance transform. Strokes are built in device
                // space, so the width is scaled here instead of by a canvas matrix.
                const double device=p.width*StrokeScale(toDevice);Require(std::isfinite(device),"stroke width limit");
                const double lineWidth=std::max(1.,device);Require(lineWidth<=MaxDevice,"stroke width limit");
                plutovg_canvas_set_line_width(canvas.get(),float(lineWidth));
                plutovg_canvas_set_line_cap(canvas.get(),p.cap==1?PLUTOVG_LINE_CAP_BUTT:p.cap==2?PLUTOVG_LINE_CAP_SQUARE:PLUTOVG_LINE_CAP_ROUND);
                plutovg_canvas_set_line_join(canvas.get(),p.join==1?PLUTOVG_LINE_JOIN_BEVEL:p.join==2?PLUTOVG_LINE_JOIN_MITER:PLUTOVG_LINE_JOIN_ROUND);
                plutovg_canvas_set_miter_limit(canvas.get(),float(p.miter));plutovg_canvas_stroke_path(canvas.get(),path.get());
            }else plutovg_canvas_fill_path(canvas.get(),path.get());
            Paint paint(movie,draw,viewport);auto data=plutovg_surface_get_data(surface.get());auto stride=plutovg_surface_get_stride(surface.get());
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                unsigned coverage=data[y*stride+x*4+3];if(!coverage)continue;
                // Evaluate style/CXFORM before coverage and source-over, including Mask.
                auto c=draw.cx.Apply(paint.Sample(x+.5,y+.5),variant);c[3]*=coverage/255.;auto& dst=accum[y*width+x];
                for(int k=0;k<3;++k)dst[k]=c[k]*c[3]+dst[k]*(1-c[3]);dst[3]=c[3]+dst[3]*(1-c[3]);
            }
        }
                Bitmap b{width,height,{}};b.rgba.resize(std::size_t(width)*height*4);bool visible=false;
        for(std::size_t i=0;i<accum.size();++i){auto c=accum[i];auto a=static_cast<std::uint8_t>(std::lround(c[3]*255));b.rgba[i*4+3]=a;visible|=a!=0;for(int k=0;k<3;++k)b.rgba[i*4+k]=a?static_cast<std::uint8_t>(std::clamp(std::lround(c[k]/c[3]*255),0l,255l)):0;}
        #ifdef WHEEL_SWF_DIAGNOSTICS
        if(!fixed)
#endif
        Require(visible,"empty transparent frame");result.bitmap=std::move(b);
    } catch(const std::exception& e){result.warnings.push_back(e.what());}
    return result;
}
Result Render(const Movie& movie,std::string_view label,Variant variant,std::uint32_t edge) {
    return RenderImpl(movie,label,variant,edge);
}
#ifdef WHEEL_SWF_DIAGNOSTICS
Result RenderViewport(const Movie& movie,std::string_view label,Variant variant,const DiagnosticViewport& viewport,DiagnosticFault fault) {
    return RenderImpl(movie,label,variant,256,false,&viewport,fault);
}
Result RenderMeasured(const Movie& movie,std::string_view label,Variant variant,std::uint32_t edge,DiagnosticWork& work) {
    return RenderImpl(movie,label,variant,edge,false,nullptr,DiagnosticFault::None,&work);
}
// Linked only by I4IconDump, absent from the plugin and public interface.
Result RenderWithDroppedPaths(const Movie& movie,std::string_view label,Variant variant,std::uint32_t edge) {
    return RenderImpl(movie,label,variant,edge,true);
}
#endif
}
