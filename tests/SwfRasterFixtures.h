// Shared fixture builders for SwfRasterTests and SwfRasterHardeningTests.
// Each test executable is a single translation unit including this header once.
#pragma once
#include "SwfRaster.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <random>
#include <stdexcept>
#include <miniz.h>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

using namespace Wheel::Swf;
namespace {
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    struct Bytes {
        std::vector<std::uint8_t> v;unsigned bit=0;
        void U(std::uint32_t x,unsigned n){for(unsigned i=n;i;--i){if(!bit)v.push_back(0);v.back()|=((x>>(i-1))&1)<<(7-bit);bit=(bit+1)%8;}}
        void Align(){bit=0;}
        void B(unsigned x){Align();v.push_back(static_cast<std::uint8_t>(x));}
        void W(unsigned x){B(x);B(x>>8);}
        void D(unsigned x){W(x);W(x>>16);}
        void Add(const Bytes& b){Align();v.insert(v.end(),b.v.begin(),b.v.end());}
        void Text(std::string_view s){for(unsigned char c:s)B(c);B(0);}
        void Rect(){U(13,5);U(0,13);U(2000,13);U(0,13);U(2000,13);Align();}
        void Matrix(int a=65536,int d=65536,int tx=0,int ty=0,int b=0,int c=0){
            U(1,1);U(31,5);U(a,31);U(d,31);U(1,1);U(31,5);U(b,31);U(c,31);U(20,5);U(tx,20);U(ty,20);Align();
        }
        void CX(int alpha=256,int red=256,int addR=0,int addA=0,bool hasAlpha=true){
            U(1,1);U(1,1);U(11,4);U(red,11);U(256,11);U(256,11);if(hasAlpha)U(alpha,11);
            U(addR,11);U(0,11);U(0,11);if(hasAlpha)U(addA,11);Align();
        }
        void Tag(unsigned kind,const Bytes& b={}){W((kind<<6)|63);D(static_cast<unsigned>(b.v.size()));Add(b);}
        void Label(std::string_view text){Bytes b;b.Text(text);Tag(43,b);}
    };
    Bytes Compressed(const Bytes& b){Bytes out;auto n=mz_compressBound(static_cast<mz_ulong>(b.v.size()));out.v.resize(n);Check(mz_compress(out.v.data(),&n,b.v.data(),static_cast<mz_ulong>(b.v.size()))==MZ_OK,"fixture compression");out.v.resize(n);return out;}
    struct Style {
        unsigned type=0,version=3,spread=0,interpolation=0;bool curve=false,stroke=false,fill0=false;
        std::array<unsigned,4> color{240,40,20,255};int focal=0;
    };
    void Color(Bytes& b,const std::array<unsigned,4>& c,bool alpha){for(int i=0;i<(alpha?4:3);++i)b.B(c[i]);}
    void Edge(Bytes& b,int dx,int dy){b.U(1,1);b.U(1,1);b.U(10,4);b.U(1,1);b.U(dx,12);b.U(dy,12);}
    void Curve(Bytes& b,int cx,int cy,int dx,int dy){b.U(1,1);b.U(0,1);b.U(10,4);b.U(cx,12);b.U(cy,12);b.U(dx,12);b.U(dy,12);}
    Bytes Shape(unsigned id,Style s={}) {
        Bytes b;b.W(id);b.Rect();if(s.version==4){b.Rect();b.B(4);}
        b.B(s.stroke?0:1);
        if(!s.stroke){
            b.B(s.type);
            if(s.type==0)Color(b,s.color,s.version>=3);
            else if(s.type>=0x40){b.W(10);b.Matrix(500*65536,500*65536);}
            else {b.Matrix(4000,4000,1000,500);b.U(s.spread,2);b.U(s.interpolation,2);b.U(2,4);b.B(0);Color(b,{255,0,0,255},s.version>=3);b.B(255);Color(b,{0,0,255,80},s.version>=3);if(s.type==0x13)b.W(s.focal);}
        }
        b.B(s.stroke?1:0);
        if(s.stroke){b.W(100);if(s.version==4){b.U(0,2);b.U(0,2);b.U(0,12);}Color(b,s.color,s.version>=3);}
        b.U(s.stroke?0:1,4);b.U(s.stroke?1:0,4);
        b.U(0,1);b.U(s.stroke?9:s.fill0?3:5,5);b.U(1,5);b.U(0,1);b.U(0,1);b.U(1,1);
        if(s.curve){Curve(b,1000,-1000,1000,1000);Edge(b,-2000,0);}
        else {Edge(b,2000,0);Edge(b,0,1000);Edge(b,-2000,0);Edge(b,0,-1000);}
        b.U(0,6);b.Align();return b;
    }
    void Define(Bytes& tags,unsigned id,Style s={}){tags.Tag(s.version==1?2:s.version==2?22:s.version==3?32:83,Shape(id,s));}
    void Place(Bytes& tags,unsigned id,unsigned depth=1,bool move=false,int alpha=256,int sx=65536,int sy=65536,int red=256,int addR=0,int addA=0,unsigned tag=26){
        Bytes p;if(tag==4){p.W(id);p.W(depth);p.Matrix(sx,sy,700,-300);p.CX(alpha,red,addR,addA,false);}
        else {p.B(14|(move?1:0));if(tag==70)p.B(0);p.W(depth);p.W(id);p.Matrix(sx,sy,700,-300);p.CX(alpha,red,addR,addA);}
        tags.Tag(tag,p);
    }
    Bytes File(Bytes tags,unsigned frames=1,bool cws=false){Bytes body;body.Rect();body.W(24*256);body.W(frames);body.Add(tags);body.Tag(0);Bytes file;file.B(cws?'C':'F');file.B('W');file.B('S');file.B(10);file.D(static_cast<unsigned>(body.v.size()+8));file.Add(cws?Compressed(body):body);return file;}
    Bytes Simple(Style s={},bool cws=false){Bytes tags;Define(tags,1,s);Place(tags,1);tags.Label("icon");tags.Tag(1);return File(tags,1,cws);}
    std::shared_ptr<const Movie> ParseOK(const Bytes& bytes){std::vector<std::string>w;auto m=Parse(bytes.v,w);if(!m)for(auto& s:w)std::cerr<<s<<'\n';Check(bool(m),"Parse fixture");return m;}
    Bitmap Draw(const Movie& m,std::string_view label="icon",Variant v=Variant::Original){auto r=Render(m,label,v,128);if(!r.bitmap)for(auto& s:r.warnings)std::cerr<<s<<'\n';Check(r.bitmap.has_value(),"Render fixture");return *r.bitmap;}
    unsigned Pixel(const Bitmap& b,unsigned x,unsigned y,unsigned c){return b.rgba[(y*b.width+x)*4+c];}
    std::array<unsigned,4> Bounds(const Bitmap& b){std::array<unsigned,4> r{b.width,b.height,0,0};for(unsigned y=0;y<b.height;++y)for(unsigned x=0;x<b.width;++x)if(Pixel(b,x,y,3)){r[0]=std::min(r[0],x);r[1]=std::min(r[1],y);r[2]=std::max(r[2],x);r[3]=std::max(r[3],y);}return r;}
    void White(const Bitmap& b){bool found=false;for(std::size_t i=0;i<b.rgba.size();i+=4)if(b.rgba[i+3]){found=true;Check(b.rgba[i]==255&&b.rgba[i+1]==255&&b.rgba[i+2]==255,"Mask RGB is white before compositing");}Check(found,"visible mask");}
}
