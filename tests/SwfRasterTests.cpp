// Functional regression: parsing, timeline, paints, bitmaps and styles.
// Robustness cases (malformed input, bounds, budgets) live in SwfRasterHardeningTests.cpp.
#include "SwfRasterFixtures.h"

namespace {
    void SolidAndCurves(){
        auto m=ParseOK(Simple());auto b=Draw(*m);auto box=Bounds(b);
        Check(Pixel(b,64,64,0)==240&&Pixel(b,64,64,3)==255,"solid center straight RGBA");
        Check(box[0]==6&&box[2]==121&&box[1]>=34&&box[1]<=35&&box[3]>=92&&box[3]<=93,"90 percent centered geometry bounds");
        Check(Pixel(b,0,0,3)==0,"transparent canvas");bool aa=false;for(std::size_t i=3;i<b.rgba.size();i+=4)aa|=b.rgba[i]>0&&b.rgba[i]<255;Check(aa,"antialias coverage exists");
        Style s;s.curve=true;auto c=Draw(*ParseOK(Simple(s)));Check(Pixel(c,64,64,3)>240&&Pixel(c,10,50,3)==0,"quadratic silhouette");White(Draw(*ParseOK(Simple(s)),"icon",Variant::Mask));
        for(unsigned version=1;version<=4;++version){s={};s.version=version;auto a=Draw(*ParseOK(Simple(s)));Check(a.rgba==b.rgba,"DefineShape1-4 solid equivalence");}
        s={};s.fill0=true;Check(Draw(*ParseOK(Simple(s))).rgba==b.rgba,"fill0 reverse edge contour");
        Check(Draw(*ParseOK(Simple({},true))).rgba==b.rgba,"CWS FWS equivalence");
        std::cout<<"solid, curves, shape versions, fill0, CWS: PASS\n";
    }
    void TimelineTests(){
        Bytes tags;Define(tags,1);Style blue;blue.color={20,40,240,255};Define(tags,2,blue);
        Bytes sprite;sprite.W(3);sprite.W(2);Place(sprite,1);sprite.Tag(1);Place(sprite,2,1,true);sprite.Tag(1);sprite.Tag(0);tags.Tag(39,sprite);
        Place(tags,3);tags.Label("first\n");tags.Tag(1);
        Bytes move;move.B(9);move.W(1);move.CX(128,128,20,32);tags.Tag(26,move);tags.Label("moved");tags.Tag(1);
        Place(tags,2,1,true,256,65536,131072,256,0,0,70);tags.Label("replaced");tags.Tag(1);
        Bytes remove;remove.W(1);tags.Tag(28,remove);tags.Label("empty");tags.Tag(1);
        Place(tags,1,2,false,256,65536,65536,128,20,0,4);tags.Label("oldplace");tags.Tag(1);
        remove={};remove.W(1);remove.W(2);tags.Tag(5,remove);tags.Label("removed");tags.Tag(1);
        auto m=ParseOK(File(tags,6));auto first=Draw(*m,"first\n");Check(Pixel(first,64,64,0)==240,"sprite always first frame");
        Check(!Render(*m,"first",Variant::Original,128).bitmap,"label is not trimmed");
        auto moved=Draw(*m,"moved");Check(Pixel(moved,64,64,0)==140&&Pixel(moved,64,64,3)==160,"move preserves character/matrix; CXFORM add/multiply");
        auto mask=Draw(*m,"moved",Variant::Mask);White(mask);Check(Pixel(mask,64,64,3)==128,"Mask ignores CXFORM alpha additive term");
        auto replaced=Draw(*m,"replaced");auto box=Bounds(replaced);Check(Pixel(replaced,64,64,2)==240&&box[3]-box[1]==box[2]-box[0],"PlaceObject3 replacement and nonuniform matrix");
        Check(!Render(*m,"empty",Variant::Original,128).bitmap&&!Render(*m,"removed",Variant::Original,128).bitmap,"RemoveObject and RemoveObject2");
        Check(Pixel(Draw(*m,"oldplace"),64,64,0)==140,"PlaceObject CXFORM RGB");
        Check(!Render(*m,"missing",Variant::Original,128).bitmap,"missing label");Check(!Render(*m,"first\n",Variant::Original,257).bitmap,"edge cap");
        std::vector<std::future<std::vector<std::uint8_t>>> jobs;for(int i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[m]{return Render(*m,"first\n",Variant::Original,128).bitmap->rgba;}));for(auto& j:jobs)Check(j.get()==first.rgba,"concurrent immutable render");
        std::cout<<"sprites, labels, moves, replacement, remove, matrices, CXFORM, threads: PASS\n";
    }
    void SpriteEndCompatibility(){
        auto fixture=[](unsigned suffix,unsigned declared=1,bool placement=true,bool trailing=false){
            Bytes tags;Define(tags,1);Bytes sprite;sprite.W(2);sprite.W(declared);
            if(placement)Place(sprite,1);
            if(suffix==20){sprite.W(20<<6);} // Exact terminal 00 05 from WitcherSigns.
            else if(suffix==21){sprite.W((20<<6)|7);} // Declared child bytes are absent.
            else if(suffix==1)sprite.Tag(1);
            if(trailing)sprite.Tag(1);
            tags.Tag(39,sprite);Place(tags,2);tags.Label("icon");tags.Tag(1);return File(tags);
        };
        auto expected=Draw(*ParseOK(fixture(1)));
        std::vector<std::string> warnings;auto m=Parse(fixture(20).v,warnings);
        Check(bool(m)&&Draw(*m).rgba==expected.rgba,"terminal empty bitmap tag preserves sprite placement");
        Check(std::any_of(warnings.begin(),warnings.end(),[](const std::string& w){return w.find("terminal empty DefineBitsLossless")!=std::string::npos;}),"compatibility tolerance is diagnosed");
        Check(Draw(*ParseOK(fixture(0))).rgba==expected.rgba,"one-frame sprite without ShowFrame retains display list");
        for(const auto& invalid:{fixture(21),fixture(20,2),fixture(20,1,false),fixture(20,1,true,true)}){
            warnings.clear();Check(!Parse(invalid.v,warnings)&&!warnings.empty(),"tolerance excludes overlong payload and unrelated invalid definitions");
        }
        auto truncated=fixture(20);truncated.v.pop_back();warnings.clear();Check(!Parse(truncated.v,warnings),"physical truncation remains rejected");
        std::cout<<"bounded sprite terminal tag and implicit first frame: PASS\n";
    }
    void PaintTests(){
        for(unsigned type:{0x10u,0x12u,0x13u})for(unsigned spread=0;spread<3;++spread)for(unsigned interp=0;interp<2;++interp){
            Style s;s.type=type;s.spread=spread;s.interpolation=interp;s.focal=128;auto m=ParseOK(Simple(s));auto b=Draw(*m);White(Draw(*m,"icon",Variant::Mask));
            if(type==0x10){Check(Pixel(b,20,64,0)>Pixel(b,105,64,0),"linear gradient red slope");Check(Pixel(b,20,64,3)>Pixel(b,105,64,3),"gradient alpha slope");}
            if(type==0x12)Check(Pixel(b,64,64,0)>Pixel(b,110,64,0),"radial gradient radius");
        }
        Style s;s.stroke=true;auto stroke=Draw(*ParseOK(Simple(s)));Check(Pixel(stroke,64,64,3)==0,"stroke leaves center empty");Check(Pixel(stroke,64,34,3)>200,"stroke perimeter");White(Draw(*ParseOK(Simple(s)),"icon",Variant::Mask));
        s.version=4;Check(Draw(*ParseOK(Simple(s))).rgba==stroke.rgba,"LineStyle2 basic stroke");
        for(unsigned type=0x40;type<=0x43;++type){
            Bytes tags,bitmap,pixels;bitmap.W(10);bitmap.B(5);bitmap.W(2);bitmap.W(2);
            for(int i=0;i<4;++i){pixels.B(i%2?255:128);pixels.B(i%2?0:128);pixels.B(0);pixels.B(i%2?255:0);}bitmap.Add(Compressed(pixels));tags.Tag(36,bitmap);
            s={};s.type=type;Define(tags,1,s);Place(tags,1);tags.Label("icon");tags.Tag(1);auto m=ParseOK(File(tags));auto b=Draw(*m);White(Draw(*m,"icon",Variant::Mask));
            Check(Pixel(b,20,64,0)>200&&Pixel(b,20,64,3)>=120&&Pixel(b,20,64,3)<=140,"lossless premultiplied alpha decoded to straight");
            if(type&1)Check(Pixel(b,110,64,3)==0,"clipped bitmap outside transparent");else Check(Pixel(b,110,64,3)>0,"repeated bitmap wraps");
        }
        std::cout<<"gradients/spread/interpolation/focal, stroke, lossless alpha, bitmap modes: PASS\n";
    }
    Bytes JPEG() {
        using Microsoft::WRL::ComPtr;
        ComPtr<IStream> stream;ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
        Check(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)),"JPEG fixture stream");
        Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"JPEG fixture factory");
        Check(SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatJpeg,nullptr,&encoder)),"JPEG fixture encoder");
        Check(SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)),"JPEG fixture init");
        Check(SUCCEEDED(encoder->CreateNewFrame(&frame,nullptr))&&SUCCEEDED(frame->Initialize(nullptr))&&SUCCEEDED(frame->SetSize(2,2)),"JPEG fixture frame");
        auto format=GUID_WICPixelFormat24bppBGR;Check(SUCCEEDED(frame->SetPixelFormat(&format))&&format==GUID_WICPixelFormat24bppBGR,"JPEG fixture BGR");
        std::array<BYTE,12> pixels{0,0,120,0,0,120,0,0,120,0,0,120};
        Check(SUCCEEDED(frame->WritePixels(2,6,12,pixels.data()))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit()),"JPEG fixture encode");
        STATSTG stat{};Check(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME)),"JPEG fixture size");HGLOBAL memory=nullptr;Check(SUCCEEDED(GetHGlobalFromStream(stream.Get(),&memory)),"JPEG fixture bytes");
        auto ptr=static_cast<std::uint8_t*>(GlobalLock(memory));Bytes b;b.v.assign(ptr,ptr+stat.cbSize.LowPart);GlobalUnlock(memory);return b;
    }
    void ImageAndStyleTests() {
        auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto jpeg=JPEG();
        for(unsigned tag:{6u,21u,35u}) {
            Bytes tags,b;b.W(10);
            if(tag==6){Bytes table;table.B(255);table.B(216);table.B(255);table.B(217);tags.Tag(8,table);}
            if(tag==35)b.D(static_cast<unsigned>(jpeg.v.size()));b.Add(jpeg);
            if(tag==35){Bytes alpha;for(int i=0;i<4;++i)alpha.B(128);b.Add(Compressed(alpha));}
            tags.Tag(tag,b);Style s;s.type=0x42;Define(tags,1,s);Place(tags,1);tags.Label("icon");tags.Tag(1);
            auto m=ParseOK(File(tags));auto image=Draw(*m);auto red=Pixel(image,64,64,0);
            Check(red>=(tag==35?230u:110u)&&red<=(tag==35?250u:130u),"JPEG/JPEGTables RGB and JPEG3 unpremultiply");
            Check(Pixel(image,64,64,3)==(tag==35?128u:255u),"JPEG3 alpha");White(Draw(*m,"icon",Variant::Mask));
        }
        if(SUCCEEDED(com))CoUninitialize();
        for(bool alpha:{false,true})for(unsigned format:{3u,4u,5u}) {
            if(alpha&&format==4)continue;Bytes tags,b,raw;b.W(10);b.B(format);b.W(1);b.W(1);
            if(format==3){b.B(0);raw.B(alpha?64:128);raw.B(0);raw.B(0);if(alpha)raw.B(128);raw.B(0);raw.B(0);raw.B(0);raw.B(0);}
            else if(format==4){raw.W(16<<10);raw.W(0);}
            else {raw.B(alpha?128:0);raw.B(alpha?64:128);raw.B(0);raw.B(0);}
            b.Add(Compressed(raw));tags.Tag(alpha?36:20,b);Place(tags,10);tags.Label("icon");tags.Tag(1);
            auto image=Draw(*ParseOK(File(tags)));Check(Pixel(image,64,64,0)>=125&&Pixel(image,64,64,0)<=135,"lossless RGB formats and palette padding");Check(Pixel(image,64,64,3)==(alpha?128u:255u),"lossless alpha formats");
        }
        // StateNewStyles retains existing active styles until explicitly changed.
        Bytes shape;shape.W(1);shape.Rect();shape.B(1);shape.B(0);Color(shape,{240,40,20,255},true);shape.B(0);shape.U(1,4);shape.U(0,4);
        shape.U(0,1);shape.U(5,5);shape.U(1,5);shape.U(0,1);shape.U(0,1);shape.U(1,1);
        Edge(shape,1000,0);Edge(shape,0,1000);Edge(shape,-1000,0);Edge(shape,0,-1000);
        shape.U(0,1);shape.U(16,5);shape.Align();shape.B(1);shape.B(0);Color(shape,{0,240,0,255},true);shape.B(0);shape.U(1,4);shape.U(0,4);
        shape.U(0,1);shape.U(5,5);shape.U(12,5);shape.U(1000,12);shape.U(0,12);shape.U(1,1);
        Edge(shape,1000,0);Edge(shape,0,1000);Edge(shape,-1000,0);Edge(shape,0,-1000);shape.U(0,6);shape.Align();
        Bytes tags;tags.Tag(32,shape);Place(tags,1);tags.Label("icon");tags.Tag(1);auto image=Draw(*ParseOK(File(tags)));
        Check(Pixel(image,32,64,0)==240&&Pixel(image,96,64,1)==240,"new style arrays and style switch");
        Bytes unsupported;unsupported.Tag(99);Define(unsupported,1);Place(unsupported,1);unsupported.Label("icon");unsupported.Tag(1);
        std::vector<std::string> warnings;auto m=Parse(File(unsupported).v,warnings);Check(bool(m)&&!warnings.empty()&&Render(*m,"icon",Variant::Original,128).bitmap.has_value(),"unused unsupported tag only warns");
        std::cout<<"JPEG1/2/3, JPEGTables, palettes, 15/32-bit bitmaps, direct bitmap, new styles: PASS\n";
    }
}
int main(){try{SolidAndCurves();TimelineTests();SpriteEndCompatibility();PaintTests();ImageAndStyleTests();std::cout<<"SwfRasterTests: "<<checks<<" checks PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
