#pragma once
#include <Windows.h>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Wheel::SettingsIni {
    inline std::wstring_view Trim(std::wstring_view value) {
        const auto first=value.find_first_not_of(L" \t");
        return first==value.npos?std::wstring_view{}:value.substr(first,value.find_last_not_of(L" \t")-first+1);
    }
    inline bool Equal(std::wstring_view a,std::wstring_view b) {
        return a.size()==b.size() && (a.empty() || CompareStringOrdinal(a.data(),static_cast<int>(a.size()),
            b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL);
    }
    inline bool Wide(std::string_view input,std::wstring& output,UINT page=CP_UTF8) {
        output.clear();if(input.empty())return true;
        const auto flags=page==CP_UTF8?MB_ERR_INVALID_CHARS:0;
        const auto size=MultiByteToWideChar(page,flags,input.data(),static_cast<int>(input.size()),nullptr,0);
        if(!size)return false;
        output.resize(size);
        return MultiByteToWideChar(page,flags,input.data(),static_cast<int>(input.size()),output.data(),size)==size;
    }
    inline bool Utf8(std::wstring_view input,std::string& output) {
        output.clear();if(input.empty())return true;
        const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr);
        if(!size)return false;
        output.resize(size);
        return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),output.data(),size,nullptr,nullptr)==size;
    }
    // Read and edit the document itself. Profile APIs can return cached/default
    // values even when the physical INI has no keys at all.
    class Document {
        enum class Encoding {Utf16LE,Utf16BE,Utf8,Utf8Bom,Ansi};
        struct Line {std::wstring text,ending;};
        Encoding encoding=Encoding::Utf16LE;
        std::vector<Line> lines;
        std::wstring newline=L"\r\n";
        static std::optional<std::wstring_view> Section(std::wstring_view text) {
            text=Trim(text);
            if(text.empty() || text.front()!=L'[')return {};
            const auto end=text.find(L']');
            if(end==text.npos)return {};
            return Trim(text.substr(1,end-1));
        }
        static std::optional<std::wstring_view> Key(std::wstring_view text) {
            text=Trim(text);
            if(text.empty() || text.front()==L';' || text.front()==L'#' || text.front()==L'[')return {};
            const auto equal=text.find(L'=');
            if(equal==text.npos)return {};
            return Trim(text.substr(0,equal));
        }
        void Insert(std::size_t at,Line line) {
            if(at && lines[at-1].ending.empty())lines[at-1].ending=newline;
            lines.insert(lines.begin()+at,std::move(line));
        }
    public:
        bool Parse(const std::vector<char>& bytes) {
            *this=Document{};
            std::wstring text;
            const auto byte=[&](std::size_t i){return static_cast<unsigned char>(bytes[i]);};
            if(bytes.size()>=2 && ((byte(0)==0xFF && byte(1)==0xFE) || (byte(0)==0xFE && byte(1)==0xFF))) {
                if(bytes.size()%2)return false;
                encoding=byte(0)==0xFF?Encoding::Utf16LE:Encoding::Utf16BE;
                for(std::size_t i=2;i<bytes.size();i+=2)text.push_back(static_cast<wchar_t>(
                    encoding==Encoding::Utf16LE?byte(i)|(byte(i+1)<<8):(byte(i)<<8)|byte(i+1)));
                std::string valid;if(!Utf8(text,valid))return false;
            }else if(!bytes.empty()) {
                const bool bom=bytes.size()>=3 && byte(0)==0xEF && byte(1)==0xBB && byte(2)==0xBF;
                const auto skip=bom?3u:0u;
                const std::string_view source(bytes.data()+skip,bytes.size()-skip);
                encoding=bom?Encoding::Utf8Bom:Encoding::Utf8;
                if(!Wide(source,text)) {
                    if(bom || !Wide(source,text,CP_ACP))return false;
                    encoding=Encoding::Ansi;
                }
            }
            if(text.find(L'\0')!=text.npos)return false;
            bool firstEnding=true;
            for(std::size_t start=0;start<text.size();) {
                auto end=text.find_first_of(L"\r\n",start);
                if(end==text.npos){lines.push_back({text.substr(start),L""});break;}
                auto next=end+1;
                if(text[end]==L'\r' && next<text.size() && text[next]==L'\n')++next;
                auto ending=text.substr(end,next-end);
                if(firstEnding){newline=ending;firstEnding=false;}
                lines.push_back({text.substr(start,end-start),std::move(ending)});start=next;
            }
            return true;
        }
        std::optional<std::wstring> Get(std::wstring_view section,std::wstring_view key) const {
            std::wstring current;
            for(const auto& line:lines) {
                if(const auto s=Section(line.text)){current=*s;continue;}
                if(!Equal(current,section))continue;
                const auto k=Key(line.text);
                if(!k || !Equal(*k,key))continue;
                auto value=Trim(std::wstring_view(line.text).substr(line.text.find(L'=')+1));
                if(value.size()>=2 && ((value.front()==L'"' && value.back()==L'"') ||
                    (value.front()==L'\'' && value.back()==L'\'')))value=value.substr(1,value.size()-2);
                return std::wstring(value);
            }
            return {};
        }
        bool Set(std::wstring_view section,std::wstring_view key,std::wstring_view value) {
            if(value.find_first_of(std::wstring_view(L"\r\n\0",3))!=value.npos)return false;
            std::wstring rendered(value);
            if(Trim(value)!=value || (!value.empty() && (value.front()==L'"' || value.front()==L'\'')))rendered=L"\""+rendered+L"\"";
            std::wstring current;bool found=false,hasSection=false;
            std::size_t insertion=lines.size();
            for(std::size_t i=0;i<lines.size();++i) {
                if(const auto s=Section(lines[i].text)) {
                    if(Equal(current,section) && insertion==lines.size())insertion=i;
                    current=*s;hasSection=hasSection || Equal(current,section);continue;
                }
                const auto k=Key(lines[i].text);
                if(Equal(current,section) && k && Equal(*k,key)) {
                    // Update duplicates too, so another INI reader cannot
                    // select an older value from a repeated section/key.
                    const auto equal=lines[i].text.find(L'=');
                    lines[i].text.resize(equal+1);lines[i].text+=rendered;found=true;
                }
            }
            if(found)return true;
            if(!hasSection) {
                insertion=lines.size();
                Insert(insertion++,{L"["+std::wstring(section)+L"]",newline});
            }
            Insert(insertion,{std::wstring(key)+L"="+rendered,newline});return true;
        }
        bool Encode(std::vector<char>& bytes) const {
            std::wstring text;for(const auto& line:lines)text+=line.text+line.ending;
            auto effective=encoding;
            bytes.clear();
            if(effective==Encoding::Ansi) {
                BOOL replaced=FALSE;
                const auto size=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,&replaced);
                if(size && !replaced) {
                    bytes.resize(size);
                    if(WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,text.data(),static_cast<int>(text.size()),bytes.data(),size,nullptr,&replaced) && !replaced)return true;
                }
                // Preserve decoded comments/unknown keys while upgrading a
                // legacy code page that cannot represent a new Unicode value.
                effective=Encoding::Utf16LE;bytes.clear();
            }
            if(effective==Encoding::Utf16LE || effective==Encoding::Utf16BE) {
                const bool little=effective==Encoding::Utf16LE;
                bytes.push_back(static_cast<char>(little?0xFF:0xFE));bytes.push_back(static_cast<char>(little?0xFE:0xFF));
                for(const auto c:text) {
                    bytes.push_back(static_cast<char>(little?c&255:c>>8));
                    bytes.push_back(static_cast<char>(little?c>>8:c&255));
                }
                return true;
            }
            std::string utf8;if(!Utf8(text,utf8))return false;
            if(effective==Encoding::Utf8Bom)bytes={'\xEF','\xBB','\xBF'};
            bytes.insert(bytes.end(),utf8.begin(),utf8.end());return true;
        }
    };
}
