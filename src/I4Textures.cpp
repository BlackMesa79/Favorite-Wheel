#include "I4Textures.h"
#include "IconCache.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace Wheel::I4Textures {
namespace {
    using Microsoft::WRL::ComPtr;
    struct Source {
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        std::shared_ptr<const Swf::Movie> movie;
        std::string hash;
        bool hashed=false;
        bool queued=false, failed=false;
    };
    struct Work { I4::TextureKey key; std::uint64_t generation, cacheEpoch; };
    struct Ready { Work work; Swf::Bitmap bitmap; };
    struct State {
        std::mutex mutex;
        std::condition_variable changed;
        Reader reader;
        Scheduler schedule;
        Logger logger;
        std::map<std::string,Source> sources;
        std::vector<I4::Icon> snapshot;
        I4::RuntimeConfig config;
        std::set<I4::IconKey> pins, failed, uploadWarned;
        std::set<I4::TextureKey> uploadFailed;
        std::set<I4::TextureKey> requested;
        std::deque<std::string> reads, warnings;
        std::deque<Work> work;
        std::deque<Ready> ready;
        std::uint64_t generation=0;
        std::size_t parses=0, renders=0, cacheHits=0;
        std::filesystem::path cacheDirectory;
        std::uint64_t cacheEpoch=0, session=0;
        bool cacheMaintenance=true;
        ClearStatus clearStatus=ClearStatus::Idle;
        std::uint32_t edge=128;
        bool scheduled=false, stop=false;
        std::thread worker;
    };
    // Intentionally process-lifetime: static destruction must not join under the
    // Windows loader lock. Queued/in-flight SKSE reads retain valid shared state.
    // Offline tests stop/join explicitly via Shutdown(), outside DLL teardown.
    // 待落盘条目在进程退出时允许丢失；静态析构/DLL 卸载既不刷新也不 join。
    static State& state = *new State;
    // All COM objects and policy below belong exclusively to the render thread.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ImageSampler sampler;
    std::map<I4::TextureKey,ComPtr<ID3D11ShaderResourceView>> textures;
    I4::TextureBudget budget;
    unsigned uploaded=0;
    std::set<I4::IconKey> failedForFrame;

    void RetryUploadsLocked() {
        // One attempt per texture key per snapshot/size/budget epoch, independent of warnings.
        for(const auto& key:state.uploadFailed)state.requested.erase(key);
        state.uploadFailed.clear();
    }
    void UpdatePinsLocked(const I4::RuntimeConfig& config) {
        std::set<I4::IconKey> pins;
        for(const auto& icon:state.snapshot)if(I4::Eligible(icon,false,config))pins.insert(I4::Identity(icon));
        if(state.pins!=pins){state.pins=std::move(pins);RetryUploadsLocked();}
    }
    void QueueLocked() {
        if(state.stop)return;
        for(const auto& id:state.pins) {
            if(state.failed.contains(id))continue;
            auto& source=state.sources[id.first];
            if(source.failed)continue;
            if(!source.bytes) {
                if(!source.queued){source.queued=true;state.reads.push_back(id.first);}
                continue;
            }
            for(auto variant:{Swf::Variant::Mask,Swf::Variant::Original}) {
                I4::TextureKey key{id.first,id.second,variant,state.edge};
                if(state.uploadFailed.contains(key))continue;
                if(state.requested.insert(key).second)state.work.push_back({std::move(key),state.generation,state.cacheEpoch});
            }
        }
        state.changed.notify_all();
    }
    void FailLocked(const I4::IconKey& id,const std::string& reason) {
        if(state.failed.insert(id).second)state.warnings.push_back("I4 icon failed: "+id.first+" | "+id.second+": "+reason);
    }
    void Worker() {
        std::filesystem::path directory;
        {std::lock_guard lock(state.mutex);directory=state.cacheDirectory;}
        IconCache::Store cache(directory,[](const std::string& message){
            std::lock_guard lock(state.mutex);state.warnings.push_back(message);
        });
        auto configure=[&] {
            const auto config=I4::Runtime();cache.Configure(config.iconCache && !directory.empty(),config.iconCacheMB);
        };
        auto flushIdle=[&] {
            bool idle;
            {std::lock_guard lock(state.mutex);idle=state.work.empty() && !state.stop && state.clearStatus!=ClearStatus::Queued;}
            // 与原子写入一样，检查后新到的 clear 会在这次 IO 结束后执行；Clear 丢弃未刷新的旧批次。
            if(idle){configure();cache.Flush();}
        };
        for(;;) {
            flushIdle();
            Work work;
            std::shared_ptr<const std::vector<std::uint8_t>> bytes;
            std::shared_ptr<const Swf::Movie> movie;
            std::string hash;bool hashed=false,maintenance=false,clear=false;
            std::uint64_t clearEpoch=0;
            {
                std::unique_lock lock(state.mutex);
                state.changed.wait(lock,[]{return state.stop || state.cacheMaintenance || state.clearStatus==ClearStatus::Queued ||
                    (!state.work.empty() && state.ready.size()<8);});
                if(state.stop)return; // 停止时直接丢弃工作线程局部 Store 的待写批次，析构没有 IO。
                maintenance=std::exchange(state.cacheMaintenance,false);
                clear=state.clearStatus==ClearStatus::Queued;clearEpoch=state.cacheEpoch;
                if(!maintenance && !clear) {
                    work=std::move(state.work.front());state.work.pop_front();
                    const I4::IconKey id{work.key.source,work.key.label};
                    if(work.generation!=state.generation || work.key.edge!=state.edge || !state.pins.contains(id)) {state.requested.erase(work.key);continue;}
                    auto& source=state.sources[work.key.source];
                    if(source.failed || state.failed.contains(id))continue;
                    bytes=source.bytes;movie=source.movie;hash=source.hash;hashed=source.hashed;
                }
            }
            // 缓存方法绝不持有 state.mutex，IO 不阻塞游戏/渲染线程取锁。
            configure();
            if(clear) {
                const bool ok=cache.Clear();
                std::lock_guard lock(state.mutex);
                if(clearEpoch==state.cacheEpoch)state.clearStatus=ok?ClearStatus::Done:ClearStatus::Unavailable;
            }
            if(maintenance || clear)continue;
            try {
                if(!hashed) {
                    hash=IconCache::Hash(*bytes);
                    std::lock_guard lock(state.mutex);
                    auto& source=state.sources[work.key.source];source.hash=hash;source.hashed=true;
                    if(hash.empty())state.warnings.push_back("I4 icon cache SHA-256 unavailable: "+work.key.source);
                }
                const IconCache::Key key{work.key.label,work.key.variant,work.key.edge};
                Swf::Result rendered;rendered.bitmap=cache.Get(hash,key);
                if(rendered.bitmap) {std::lock_guard lock(state.mutex);++state.cacheHits;}
                else {
                    if(!movie) {
                        std::vector<std::string> warnings;
                        movie=Swf::Parse(*bytes,warnings);
                        std::lock_guard lock(state.mutex);++state.parses;
                        auto& source=state.sources[work.key.source];source.movie=movie;
                        if(!warnings.empty()) {
                            std::string message="I4 SWF diagnostics: "+work.key.source;
                            for(const auto& warning:warnings)message+="; "+warning;
                            state.warnings.push_back(std::move(message));
                        }
                        if(!movie) {
                            source.failed=true;
                            if(warnings.empty())state.warnings.push_back("I4 SWF parse failed: "+work.key.source);
                            continue;
                        }
                    }
                    {std::lock_guard lock(state.mutex);++state.renders;}
                    rendered=Swf::Render(*movie,work.key.label,work.key.variant,work.key.edge);
                    bool persist;
                    {std::lock_guard lock(state.mutex);persist=!state.stop && work.cacheEpoch==state.cacheEpoch;}
                    // 清理前排队的工作可以继续上传，但不能在清空后重新填回磁盘。
                    if(persist && rendered.bitmap){configure();cache.Put(hash,key,*rendered.bitmap);}
                }
                flushIdle();
                std::lock_guard lock(state.mutex);
                if(work.generation!=state.generation)continue;
                if(work.key.edge!=state.edge){state.requested.erase(work.key);continue;}
                if(!rendered.bitmap || rendered.bitmap->width!=work.key.edge || rendered.bitmap->height!=work.key.edge ||
                    rendered.bitmap->rgba.size()!=std::size_t(work.key.edge)*work.key.edge*4) {
                    FailLocked({work.key.source,work.key.label},rendered.warnings.empty()?"empty/invalid bitmap":rendered.warnings.front());continue;
                }
                if(!state.pins.contains({work.key.source,work.key.label})) {state.requested.erase(work.key);continue;}
                state.ready.push_back({std::move(work),std::move(*rendered.bitmap)});
            } catch(const std::exception& error) {
                std::lock_guard lock(state.mutex);FailLocked({work.key.source,work.key.label},error.what());
            } catch(...) {
                std::lock_guard lock(state.mutex);FailLocked({work.key.source,work.key.label},"unknown raster exception");
            }
        }
    }
    void ScheduleReads() {
        Scheduler scheduler;std::uint64_t session;
        {
            std::lock_guard lock(state.mutex);
            if(state.stop || state.scheduled || state.reads.empty() || !state.schedule)return;
            state.scheduled=true;scheduler=state.schedule;session=state.session;
        }
        try { scheduler([session] {
            // SKSE game-thread task: at most two sources per dispatch. The next
            // render frame schedules the next batch, rather than recursively draining.
            for(unsigned n=0;n<2;++n) {
                std::string name;Reader reader;
                {
                    std::lock_guard lock(state.mutex);
                    if(state.stop || session!=state.session || state.reads.empty())break;
                    name=std::move(state.reads.front());state.reads.pop_front();reader=state.reader;
                }
                std::optional<std::vector<std::uint8_t>> bytes;
                try { if(reader)bytes=reader(name); } catch(...) {}
                std::lock_guard lock(state.mutex);
                // Shutdown may have completed while the resource reader ran unlocked.
                if(state.stop || session!=state.session)break;
                auto& source=state.sources[name];
                if(bytes && !bytes->empty())source.bytes=std::make_shared<const std::vector<std::uint8_t>>(std::move(*bytes));
                else if(!source.failed) {source.failed=true;state.warnings.push_back("I4 SWF read failed: "+name);}
                QueueLocked();
            }
            std::lock_guard lock(state.mutex);if(session==state.session)state.scheduled=false;
        }); } catch(...) {
            std::lock_guard lock(state.mutex);if(session==state.session)state.scheduled=false;
        }
    }
}
void Configure(Reader reader,Scheduler schedule,Logger logger,std::filesystem::path cacheDirectory) {
    std::lock_guard lock(state.mutex);
    state.reader=std::move(reader);state.schedule=std::move(schedule);state.logger=std::move(logger);
    if(!state.worker.joinable()) {
        ++state.session;state.sources.clear();state.snapshot.clear();state.pins.clear();state.failed.clear();
        state.uploadWarned.clear();state.reads.clear();state.warnings.clear();state.scheduled=false;
        state.parses=state.renders=state.cacheHits=0;state.cacheDirectory=std::move(cacheDirectory);
        state.clearStatus=ClearStatus::Idle;state.cacheMaintenance=true;state.config=I4::Runtime();
        state.stop=false;state.worker=std::thread(Worker);
    }
}
void ClearCache() {
    std::lock_guard lock(state.mutex);
    if(state.stop || !state.worker.joinable()) {state.clearStatus=ClearStatus::Unavailable;return;}
    ++state.cacheEpoch;state.clearStatus=ClearStatus::Queued;state.changed.notify_one();
}
ClearStatus CacheClearStatus() {std::lock_guard lock(state.mutex);return state.clearStatus;}
void RequestSnapshot(std::span<const I4::Icon> icons) {
    {
        std::lock_guard lock(state.mutex);
        state.snapshot.assign(icons.begin(),icons.end());
        UpdatePinsLocked(I4::Runtime());
        QueueLocked();
    }
    ScheduleReads();
}
void Reset() {
    textures.clear();budget.Clear();failedForFrame.clear();sampler.Reset();context.Reset();device.Reset();uploaded=0;
    std::lock_guard lock(state.mutex);
    ++state.generation;state.work.clear();state.ready.clear();state.requested.clear();state.uploadFailed.clear();
    state.changed.notify_all();
}
void Initialize(ID3D11Device* newDevice,ID3D11DeviceContext* newContext) {
    Reset();device=newDevice;context=newContext;
    if(device && context && !sampler.Initialize(device.Get())) {device.Reset();context.Reset();}
    std::lock_guard lock(state.mutex);QueueLocked();
}
void BeginFrame(float maximumIconPixels) {
    uploaded=0;
    const auto config=I4::Runtime();
    const auto limit=std::size_t(config.textureBudgetMB)*1024*1024;
    std::deque<std::string> warnings;Logger logger;
    {
        std::lock_guard lock(state.mutex);
        const auto edge=I4::TextureEdge(maximumIconPixels);
        const bool changed=state.edge!=edge || state.config.textureBudgetMB!=config.textureBudgetMB ||
            state.config.icons!=config.icons || state.config.scope!=config.scope;
        if(state.edge!=edge || config.textureBudgetMB>state.config.textureBudgetMB)RetryUploadsLocked();
        state.edge=edge;
        if(changed){UpdatePinsLocked(config);QueueLocked();}
        if(state.config.iconCache!=config.iconCache || state.config.iconCacheMB!=config.iconCacheMB) {
            state.cacheMaintenance=true;state.changed.notify_one();
        }
        state.config=config;
        budget.Pin(state.pins,state.edge);failedForFrame=state.failed;
        warnings.swap(state.warnings);logger=state.logger;
    }
    if(logger)for(const auto& warning:warnings)logger(warning);
    ScheduleReads();
    if(!device || !context)return;
    // Trim even when there are no pending uploads. Current snapshot textures stay pinned.
    std::vector<I4::TextureKey> trimmed;
    budget.Trim(limit,trimmed);
    for(const auto& old:trimmed)textures.erase(old);
    if(!trimmed.empty()) {
        std::lock_guard lock(state.mutex);for(const auto& old:trimmed)state.requested.erase(old);
    }
    unsigned attempts=0;
    while(attempts<I4::TextureBudget::uploadsPerFrame) {
        Ready ready;
        {
            std::lock_guard lock(state.mutex);
            if(state.ready.empty())break;
            ready=std::move(state.ready.front());state.ready.pop_front();state.changed.notify_one();
            const I4::IconKey id{ready.work.key.source,ready.work.key.label};
            if(ready.work.generation!=state.generation || ready.work.key.edge!=state.edge || !state.pins.contains(id) || state.failed.contains(id) || state.uploadFailed.contains(ready.work.key)) {
                state.requested.erase(ready.work.key);continue;
            }
        }
        ++attempts;
        const auto& key=ready.work.key;
        std::vector<I4::TextureKey> evicted;
        const bool admitted=budget.Admit(key,limit,evicted);
        for(const auto& old:evicted)textures.erase(old);
        {
            std::lock_guard lock(state.mutex);for(const auto& old:evicted)state.requested.erase(old);
        }
        ComPtr<ID3D11ShaderResourceView> view;
        if(admitted) {
            D3D11_TEXTURE2D_DESC desc{};desc.Width=key.edge;desc.Height=key.edge;desc.MipLevels=0;desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
            ComPtr<ID3D11Texture2D> texture;
            if(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)) && SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view))) {
                context->UpdateSubresource(texture.Get(),0,nullptr,ready.bitmap.rgba.data(),key.edge*4,0);
                context->GenerateMips(view.Get());textures.emplace(key,std::move(view));++uploaded;continue;
            }
            budget.Erase(key);
        }
        std::lock_guard lock(state.mutex);
        state.uploadFailed.insert(key);
        if(state.uploadWarned.insert({key.source,key.label}).second)
            state.warnings.push_back("I4 texture upload/budget failed: "+key.source+" | "+key.label);
    }
}
Image Find(const I4::Icon& icon) {
    if(!I4::Eligible(icon) || failedForFrame.contains(I4::Identity(icon)))return {};
    // edge is selected by BeginFrame on this same render thread.
    const I4::TextureKey key{I4::SourceKey(icon.source),icon.label,
        I4::Runtime().color==I4::ColorMode::Original?Swf::Variant::Original:Swf::Variant::Mask,state.edge};
    const auto found=textures.find(key);
    if(found==textures.end())return {};
    budget.Touch(key);return {found->second.Get(),&sampler};
}
Statistics Stats() {
    std::lock_guard lock(state.mutex);
    return {budget.Used(),textures.size(),state.work.size(),state.ready.size(),uploaded,state.parses,state.renders,state.cacheHits};
}
void Shutdown() {
    {std::lock_guard lock(state.mutex);state.stop=true;}
    state.changed.notify_all();if(state.worker.joinable())state.worker.join();Reset();
}
}
