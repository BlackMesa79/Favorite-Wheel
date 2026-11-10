#include "SwfRaster.h"
#include "SwfRasterDiagnostics.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <miniz.h>

namespace Wheel::Swf { Result RenderWithDroppedPaths(const Movie&,std::string_view,Variant,std::uint32_t); }
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
using namespace Wheel::Swf;
namespace {
    void Check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("WIC PNG write failed");}
    void PNG(const fs::path& file,const Bitmap& b) {
        using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
        Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        Check(factory->CreateStream(&stream));Check(stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE));
        Check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));Check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
        Check(encoder->CreateNewFrame(&frame,nullptr));Check(frame->Initialize(nullptr));Check(frame->SetSize(b.width,b.height));
        auto format=GUID_WICPixelFormat32bppBGRA;Check(frame->SetPixelFormat(&format));
        auto bgra=b.rgba;for(std::size_t i=0;i<bgra.size();i+=4)std::swap(bgra[i],bgra[i+2]);
        Check(frame->WritePixels(b.height,b.width*4,static_cast<UINT>(b.rgba.size()),bgra.data()));Check(frame->Commit());Check(encoder->Commit());
    }
    std::string UTF8(const fs::path& p) {auto s=p.generic_u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
    std::string CSV(std::string_view v){std::string out="\"";for(char c:v){out+=c;if(c=='"')out+='"';}return out+'"';}
    std::string Join(const std::vector<std::string>& a) {std::string s;for(const auto& v:a){if(!s.empty())s+="; ";s+=v;}return s;}
    double Ms(Clock::time_point begin){return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();}
    // Enumeration is tooling only; Parse remains the authority on validity.
    std::vector<std::string> Labels(const std::vector<std::uint8_t>& file) {
        auto size=unsigned(file.at(4))|(unsigned(file.at(5))<<8)|(unsigned(file.at(6))<<16)|(unsigned(file.at(7))<<24);
        if(size<12||size>32*1024*1024)throw std::runtime_error("invalid label enumeration length");
        std::vector<std::uint8_t> data(file.begin()+8,file.end());
        if(file[0]=='C'){data.resize(size-8);mz_ulong len=data.size();if(mz_uncompress(data.data(),&len,file.data()+8,static_cast<mz_ulong>(file.size()-8))!=MZ_OK||len!=data.size())throw std::runtime_error("invalid CWS");}
        std::size_t pos=(5+4*(data.at(0)>>3)+7)/8+4;std::vector<std::string> labels;
        auto word=[&](){unsigned n=data.at(pos)|(unsigned(data.at(pos+1))<<8);pos+=2;return n;};
        while(pos+2<=data.size()) {
            auto h=word();std::size_t len=h&63;if(len==63){auto lo=word(),hi=word();len=lo|(hi<<16);}
            if(len>data.size()-pos)throw std::runtime_error("truncated label tag");
            if((h>>6)==0)break;
            if((h>>6)==43){auto end=std::find(data.begin()+pos,data.begin()+pos+len,0);if(end==data.begin()+pos+len)throw std::runtime_error("unterminated label");labels.emplace_back(data.begin()+pos,end);}
            pos+=len;
        }return labels;
    }
    int GridBatch(const fs::path& manifest,const fs::path& out) {
        fs::create_directories(out);std::ifstream list(manifest);
        if(!list)throw std::runtime_error("cannot open grid manifest");
        std::ofstream log(out/"grid-renders.csv",std::ios::binary);
        log<<"index,control,status,warnings\n";
        std::string input,other;DiagnosticViewport vp;unsigned index=0;
        while(list>>std::quoted(input)>>vp.xMin>>vp.yMin>>vp.pixelsPerTwip>>vp.width>>vp.height>>std::quoted(other)) {
            auto load=[](const std::string& path,std::vector<std::string>& warnings){
                std::ifstream in(fs::u8path(path),std::ios::binary);
                std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});return Parse(bytes,warnings);
            };
            std::vector<std::string> warnings;auto movie=load(input,warnings);
            const char* names[]={"original","mirror","drop_paths","scale_09","offset_4px","random_other","mask"};
            DiagnosticFault faults[]={DiagnosticFault::None,DiagnosticFault::Mirror,DiagnosticFault::DropPaths,DiagnosticFault::Scale09,DiagnosticFault::Offset4px};
            for(unsigned i=0;i<7;++i) {
                auto active=movie;auto w=warnings;
                if(i==5)active=load(other,w);
                Result result;if(active)result=RenderViewport(*active,"grid",i==6?Variant::Mask:Variant::Original,vp,i<5?faults[i]:DiagnosticFault::None);
                w.insert(w.end(),result.warnings.begin(),result.warnings.end());
                auto dest=out/(std::to_string(index)+"-"+names[i]+".png");
                if(result.bitmap)PNG(dest,*result.bitmap);
                else {std::error_code ec;fs::remove(dest,ec);}
                log<<index<<','<<names[i]<<','<<(result.bitmap?"ok":"failed")<<','<<CSV(Join(w))<<'\n';
            }
            ++index;
        }
        if(!list.eof())throw std::runtime_error("invalid grid manifest row");
        std::cout<<"grid labels="<<index<<'\n';return 0;
    }

}
int wmain(int argc,wchar_t** argv) {
    if(argc==4&&std::wstring_view(argv[1])==L"--grid-batch") {
        HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);int rc=1;
        try{rc=GridBatch(argv[2],fs::absolute(argv[3]));}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
        if(SUCCEEDED(com))CoUninitialize();return rc;
    }
    // --work <SWF directory> <csv>: production Render(256) work counters and timing per label.
    if(argc==4&&std::wstring_view(argv[1])==L"--work") {
        HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);int rc=1;
        try {
            std::vector<fs::path> inputs;for(const auto& e:fs::recursive_directory_iterator(argv[2]))if(e.is_regular_file()&&e.path().extension()==L".swf")inputs.push_back(e.path());std::sort(inputs.begin(),inputs.end());
            std::ofstream csv(fs::absolute(argv[3]),std::ios::binary);csv<<"source,label,status,parse_ms,render_ms,instances,ops,points,coverage,paths,warnings\n";
            for(const auto& input:inputs) {
                std::ifstream in(input,std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),{});
                std::vector<std::string> warnings;auto start=Clock::now();auto movie=Parse(data,warnings);double parseMs=Ms(start);
                std::vector<std::string> labels;try{labels=Labels(data);}catch(const std::exception&){}
                for(const auto& label:labels) {
                    DiagnosticWork work;Result r;double renderMs=0;
                    if(movie){start=Clock::now();r=RenderMeasured(*movie,label,Variant::Original,256,work);renderMs=Ms(start);}
                    csv<<CSV(UTF8(input))<<','<<CSV(label)<<','<<(r.bitmap?"ok":"failed")<<','<<parseMs<<','<<renderMs<<','<<work.instances<<','<<work.ops<<','<<work.points<<','<<work.coverage<<','<<work.paths<<','<<CSV(Join(r.warnings))<<'\n';
                }
            }
            rc=0;
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
        if(SUCCEEDED(com))CoUninitialize();return rc;
    }
    if(argc!=3&&argc!=4){std::cerr<<"Usage: I4IconDump <SWF directory> <output directory> [--negative-controls]\nComparison: python tools/I4IconDump/compare.py --help\n";return 2;}
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try {
        bool negatives=argc==4&&std::wstring_view(argv[3])==L"--negative-controls";
        if(argc==4&&!negatives)throw std::runtime_error("unknown option");
        fs::path root=argv[1],out=fs::absolute(argv[2]);fs::create_directories(out);
        std::ofstream rows(out/"renders.csv",std::ios::binary),files(out/"parses.csv",std::ios::binary);
        rows<<"source,label,original,mask,status,warnings,parse_render_ms,render_ms,drop_paths,drop_paths_warnings\n";files<<"source,bytes,parse_ms,warnings\n";
        std::vector<fs::path> inputs;for(const auto& e:fs::recursive_directory_iterator(root))if(e.is_regular_file()&&e.path().extension()==L".swf")inputs.push_back(e.path());std::sort(inputs.begin(),inputs.end());
        std::size_t fileIndex=0,total=0,nonempty=0;
        for(const auto& input:inputs) {
            auto fileSize=fs::file_size(input);if(fileSize>32*1024*1024)throw std::runtime_error("input file limit");
            std::ifstream in(input,std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),{});
            std::vector<std::string> warnings;auto start=Clock::now();auto movie=Parse(data,warnings);double parseMs=Ms(start);
            files<<CSV(UTF8(input))<<','<<data.size()<<','<<parseMs<<','<<CSV(Join(warnings))<<'\n';
            std::vector<std::string> labels;
            try{labels=Labels(data);}catch(const std::exception& e){warnings.push_back(e.what());}
            if(labels.empty()&&!movie)rows<<CSV(UTF8(input))<<",,,,parse_failed,"<<CSV(Join(warnings))<<",0,0,,\n";
            unsigned index=0;
            for(const auto& label:labels) {
                ++total;auto stem=std::to_string(fileIndex)+"-"+std::to_string(index++);auto original=out/(stem+"-original.png"),mask=out/(stem+"-mask.png");
                Result a,b;double renderMs=0,combinedMs=0;
                if(movie){start=Clock::now();a=Render(*movie,label,Variant::Original,256);renderMs=Ms(start);b=Render(*movie,label,Variant::Mask,256);
                    // Independent parse + one render, excluding PNG encoding and file I/O.
                    std::vector<std::string> ignored;start=Clock::now();auto again=Parse(data,ignored);if(again)Render(*again,label,Variant::Original,256);combinedMs=Ms(start);}
                auto all=warnings;all.insert(all.end(),a.warnings.begin(),a.warnings.end());all.insert(all.end(),b.warnings.begin(),b.warnings.end());
                if(a.bitmap){PNG(original,*a.bitmap);++nonempty;}else {std::error_code ec;fs::remove(original,ec);}
                if(b.bitmap)PNG(mask,*b.bitmap);else {std::error_code ec;fs::remove(mask,ec);}
                std::string status=a.bitmap&&b.bitmap?"ok":movie?"empty_or_unsupported":"parse_failed";
                std::string droppedPath,droppedWarnings;
                if(negatives&&a.bitmap){auto dropped=RenderWithDroppedPaths(*movie,label,Variant::Original,256);auto path=out/(stem+"-drop-paths.png");
                    if(dropped.bitmap){PNG(path,*dropped.bitmap);droppedPath=UTF8(path);}else {Bitmap empty{256,256,std::vector<std::uint8_t>(256*256*4)};PNG(path,empty);droppedPath=UTF8(path);}
                    droppedWarnings=Join(dropped.warnings);}
                rows<<CSV(UTF8(input))<<','<<CSV(label)<<','<<CSV(a.bitmap?UTF8(original):"")<<','<<CSV(b.bitmap?UTF8(mask):"")<<','<<status<<','<<CSV(Join(all))<<','<<combinedMs<<','<<renderMs<<','<<CSV(droppedPath)<<','<<CSV(droppedWarnings)<<'\n';
            }
            ++fileIndex;
        }
        std::cout<<"files="<<inputs.size()<<" labels="<<total<<" nonempty="<<nonempty<<'\n';
        if(SUCCEEDED(com))CoUninitialize();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(SUCCEEDED(com))CoUninitialize();return 1;}
}
