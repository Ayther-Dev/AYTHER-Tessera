#include "clown_media.hpp"
#include "clown_state_codec.hpp"
#include "gate_build_id.hpp"
#include <clownmdemu.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Private feasibility harness. This executable does not export the product ABI.
nlohmann::json run_media_tests();
namespace {
using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
using tessera::phase0::CanonicalAudio;
constexpr std::uint64_t limit = 64 * 1024 * 1024;
constexpr std::array<const char*,8> names{"up","down","left","right","a","b","c","start"};
void require(bool b, const char* code) { if (!b) throw std::runtime_error(code); }
void number(Bytes& b, std::uint64_t v, unsigned n) {
    for (unsigned i=0; i<n; ++i) b.push_back(static_cast<std::uint8_t>(v>>(i*8)));
}
Bytes number(std::uint64_t v, unsigned n) { Bytes b; number(b,v,n); return b; }
std::uint64_t read(View b, std::size_t off, unsigned n) {
    require(off<=b.size() && n<=b.size()-off,"WIRE_TRUNCATED");
    std::uint64_t v{}; for(unsigned i=0;i<n;++i) v|=std::uint64_t(b[off+i])<<(i*8); return v;
}
Bytes bytes(std::string_view s) { return {s.begin(),s.end()}; }
std::string hash(View b) { return picosha2::hash256_hex_string(b.begin(),b.end()); }
Bytes unhex(std::string_view s) {
    require(s.size()%2==0,"HEX_INVALID"); Bytes b;
    auto digit=[](char c) -> unsigned { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; throw std::runtime_error("HEX_INVALID"); };
    for(std::size_t i=0;i<s.size();i+=2)b.push_back(static_cast<std::uint8_t>(digit(s[i])*16+digit(s[i+1])));return b;
}
void append(Bytes& b, View v) { b.insert(b.end(),v.begin(),v.end()); }
void field(Bytes& b,std::uint32_t tag,std::uint32_t type,View v) {
    number(b,tag,4);number(b,type,4);number(b,v.size(),8);append(b,v);
}
void integer(Bytes& b,std::uint32_t tag,std::uint32_t type,std::uint64_t v) {
    field(b,tag,type,number(v,type==1||type==2?4:8));
}
struct Item {std::uint32_t type; View value;};
std::map<std::uint32_t,Item> fields(View b) {
    std::map<std::uint32_t,Item> m;std::size_t p{};std::uint64_t previous{};
    while(p<b.size()) {
        const auto tag=read(b,p,4),type=read(b,p+4,4),n=read(b,p+8,8);p+=16;
        require(tag>previous&&tag<=UINT32_MAX&&type>=1&&type<=11,"TLV_ORDER_OR_TYPE_INVALID");
        require(n<=limit&&n<=b.size()-p,"TLV_LENGTH_INVALID");
        m.emplace(static_cast<std::uint32_t>(tag),Item{static_cast<std::uint32_t>(type),b.subspan(p,static_cast<std::size_t>(n))});
        p+=static_cast<std::size_t>(n);previous=tag;
    } return m;
}
View get(const std::map<std::uint32_t,Item>& m,std::uint32_t t,std::uint32_t type,std::size_t n=SIZE_MAX) {
    const auto it=m.find(t);require(it!=m.end()&&it->second.type==type,"TLV_REQUIRED_FIELD_INVALID");
    require(n==SIZE_MAX||it->second.value.size()==n,"TLV_FIELD_SIZE_INVALID");return it->second.value;
}
Bytes replace_field(View record,std::uint32_t tag,View value) {
    Bytes out;for(const auto& [t,item]:fields(record))field(out,t,item.type,t==tag?value:item.value);return out;
}
Bytes checkpoint_envelope(View payload) {
    Bytes b=bytes("TSCHK001");number(b,1,2);number(b,0,2);number(b,64,4);number(b,64+payload.size(),8);append(b,unhex(hash(payload)));number(b,5,4);number(b,0,4);append(b,payload);return b;
}
Bytes array(const std::vector<Bytes>& elements) {
    Bytes b;number(b,elements.size(),8);for(const auto& e:elements){number(b,e.size(),8);append(b,e);}return b;
}
Bytes load(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);require(size<=limit,"FILE_LIMIT_EXCEEDED");
    std::ifstream f(path,std::ios::binary);require(bool(f),"FILE_OPEN_FAILED");Bytes b(static_cast<std::size_t>(size));
    f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(size));require(bool(f),"FILE_READ_FAILED");return b;
}
void atomic_write(const std::filesystem::path& path,View b) {
    require(b.size()<=limit,"FILE_LIMIT_EXCEEDED");
    auto temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
    HANDLE h=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(h!=INVALID_HANDLE_VALUE,"CHECKPOINT_TEMP_CREATE_FAILED");
    DWORD done{};const bool written=WriteFile(h,b.data(),static_cast<DWORD>(b.size()),&done,nullptr)&&done==b.size()&&FlushFileBuffers(h);
    const auto writeError=GetLastError();CloseHandle(h);
    if(!written){DeleteFileW(temporary.c_str());throw std::runtime_error("CHECKPOINT_WRITE_FAILED:"+std::to_string(writeError));}
    const bool exists=std::filesystem::exists(path);
    const bool published=exists?bool(ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr)):
        bool(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH));
    const auto publishError=GetLastError();
    if(!published){DeleteFileW(temporary.c_str());throw std::runtime_error("CHECKPOINT_PUBLISH_FAILED:"+std::to_string(publishError));}
}
bool equal(View a,View b){return std::equal(a.begin(),a.end(),b.begin(),b.end());}
struct Assignment {std::uint32_t port,control;std::int32_t value;};
std::vector<Assignment> inputs(bool pressed=false) {
    std::vector<Assignment> a;for(std::uint32_t i=0;i<8;++i)a.push_back({0,i,i==5&&pressed?1:0});return a;
}
Bytes input_wire(const std::vector<Assignment>& a) {
    std::vector<Bytes> elements;for(const auto& v:a){Bytes e;integer(e,1,1,v.port);integer(e,2,1,v.control);integer(e,3,2,static_cast<std::uint32_t>(v.value));elements.push_back(std::move(e));}return array(elements);
}
void validate_inputs(const std::vector<Assignment>& a) {
    require(a.size()==8,"INPUT_INCOMPLETE");std::array<bool,8> seen{};
    for(const auto& x:a){require(x.port==0&&x.control<8&&(x.value==0||x.value==1),"INPUT_INVALID");require(!seen[x.control],"INPUT_DUPLICATE");seen[x.control]=true;}
    require(!(a[0].value&&a[1].value)&&!(a[2].value&&a[3].value),"INPUT_CONTRADICTORY");
    for(std::size_t i=0;i<a.size();++i)require(a[i].control==i,"INPUT_ORDER_INVALID");
}
Json parse_json(View b) {
    std::vector<std::set<std::string>> keys;
    return Json::parse(b.begin(),b.end(),[&](int,Json::parse_event_t event,Json& parsed){
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        if(event==Json::parse_event_t::key)require(keys.back().insert(parsed.get<std::string>()).second,"JSON_DUPLICATE_KEY");
        if(event==Json::parse_event_t::object_end)keys.pop_back();return true;
    });
}
void keys(const Json& j,std::initializer_list<const char*> expected) {
    require(j.is_object()&&j.size()==expected.size(),"TRACE_KEYS_INVALID");for(auto k:expected)require(j.contains(k),"TRACE_KEY_MISSING");
}
std::vector<std::vector<Assignment>> trace_parse(View raw) {
    const auto j=parse_json(raw);keys(j,{"trace_version","system","controller_profile","format_scope","frames"});
    require(j.at("trace_version")==1&&j.at("system")=="mega_drive"&&j.at("controller_profile")=="one_three_button_pad"&&j.at("format_scope")=="phase0_probe_private_not_native_api_abi","TRACE_PROFILE_INVALID");
    const auto& frames=j.at("frames");require(frames.is_array()&&!frames.empty()&&frames.size()<=65536,"TRACE_LENGTH_INVALID");
    std::vector<std::vector<Assignment>> result;
    for(const auto& f:frames){keys(f,{"frame","ports"});require(f.at("frame").is_number_unsigned()&&f.at("frame").get<std::uint64_t>()==result.size()+1,"TRACE_ORDER_INVALID");
        const auto& ports=f.at("ports");require(ports.is_array()&&ports.size()==1,"TRACE_PORTS_INVALID");const auto& p=ports.at(0);
        keys(p,{"port","controls"});require(p.at("port")==0,"TRACE_PORT_INVALID");const auto& controls=p.at("controls");
        keys(controls,{"up","down","left","right","a","b","c","start"});auto a=inputs();
        for(std::size_t i=0;i<8;++i){require(controls.at(names[i]).is_boolean(),"TRACE_VALUE_INVALID");a[i].value=controls.at(names[i]).get<bool>()?1:0;}
        validate_inputs(a);result.push_back(std::move(a));
    }return result;
}

struct Core {
    std::unique_ptr<ClownMDEmu> machine=std::make_unique<ClownMDEmu>();
    ClownMDEmu_Callbacks callbacks{};
    std::vector<cc_u16l> rom;
    CanonicalAudio audio{tessera::phase0::AudioConfig{48000,32768,4096}};
    std::array<std::uint16_t,256> palette{};
    Bytes pixels=Bytes(1024*1024*4),pcm;
    std::vector<cc_s16l> scratch=std::vector<cc_s16l>(32768);
    std::vector<std::int16_t> staging;
    std::vector<Assignment> input=inputs();
    std::uint32_t width{},height{},scanlines{};
    std::uint64_t cd_zero_samples{},palette_changes{};
    static Core& self(void* data){return *static_cast<Core*>(data);}
    explicit Core(View content) {
        require(content.size()>=512&&content.size()%2==0,"ROM_SIZE_INVALID");
        for(std::size_t i=0;i<content.size();i+=2)rom.push_back(static_cast<cc_u16l>((content[i]<<8)|content[i+1]));
        callbacks.user_data=this;
        callbacks.colour_updated=[](void* d,cc_u16f i,cc_u16f c){auto& p=self(d);require(i<p.palette.size(),"PALETTE_INDEX_INVALID");p.palette[i]=static_cast<std::uint16_t>(c);++p.palette_changes;};
        callbacks.scanline_rendered=[](void* d,cc_u16f line,const cc_u8l* row,cc_u16f,cc_u16f,cc_u16f w,cc_u16f h){
            auto& p=self(d);require(w<=1024&&h<=1024&&line<h,"VIDEO_GEOMETRY_INVALID");p.width=w;p.height=h;++p.scanlines;
            for(std::size_t x=0;x<w;++x){const auto c=p.palette[row[x]];const auto off=(std::size_t(line)*w+x)*4;
                p.pixels[off]=static_cast<std::uint8_t>((c&15)*17);p.pixels[off+1]=static_cast<std::uint8_t>(((c>>4)&15)*17);
                p.pixels[off+2]=static_cast<std::uint8_t>(((c>>8)&15)*17);p.pixels[off+3]=255;}
        };
        callbacks.input_requested=[](void* d,cc_u8f player,ClownMDEmu_Button button)->cc_bool {
            const auto& p=self(d);if(player!=0)return cc_false;
            constexpr std::array<ClownMDEmu_Button,8> buttons{CLOWNMDEMU_BUTTON_UP,CLOWNMDEMU_BUTTON_DOWN,CLOWNMDEMU_BUTTON_LEFT,CLOWNMDEMU_BUTTON_RIGHT,CLOWNMDEMU_BUTTON_A,CLOWNMDEMU_BUTTON_B,CLOWNMDEMU_BUTTON_C,CLOWNMDEMU_BUTTON_START};
            for(std::size_t i=0;i<8;++i)if(button==buttons[i])return p.input[i].value!=0;return cc_false;
        };
        callbacks.fm_audio_to_be_generated=[](void* d,ClownMDEmu* m,std::size_t count,void(*generate)(ClownMDEmu*,cc_s16l*,std::size_t)){
            auto& p=self(d);require(count*2<=p.scratch.size(),"FM_LIMIT");std::fill_n(p.scratch.data(),count*2,0);generate(m,p.scratch.data(),count);
            p.staging.assign(p.scratch.begin(),p.scratch.begin()+static_cast<std::ptrdiff_t>(count*2));p.audio.push_fm(p.staging);
        };
        callbacks.psg_audio_to_be_generated=[](void* d,ClownMDEmu* m,std::size_t count,void(*generate)(ClownMDEmu*,cc_s16l*,std::size_t)){
            auto& p=self(d);require(count<=p.scratch.size(),"PSG_LIMIT");std::fill_n(p.scratch.data(),count,0);generate(m,p.scratch.data(),count);
            p.staging.assign(p.scratch.begin(),p.scratch.begin()+static_cast<std::ptrdiff_t>(count));p.audio.push_psg(p.staging);
        };
        const auto zero=[](void* d,ClownMDEmu* m,std::size_t count,void(*generate)(ClownMDEmu*,cc_s16l*,std::size_t)){
            auto& p=self(d);require(count*2<=p.scratch.size(),"CD_LIMIT");std::fill_n(p.scratch.data(),count*2,0);generate(m,p.scratch.data(),count);
            require(std::all_of(p.scratch.begin(),p.scratch.begin()+static_cast<std::ptrdiff_t>(count*2),[](auto s){return s==0;}),"CD_NONZERO_PROFILE_UNSUPPORTED");p.cd_zero_samples+=count*2;
        };
        callbacks.pcm_audio_to_be_generated=zero;callbacks.cdda_audio_to_be_generated=zero;
        callbacks.cd_audio_read=[](void*,cc_s16l* samples,std::size_t count)->std::size_t{std::fill_n(samples,count*2,0);return count;};
        ClownMDEmu_InitialConfiguration config{};config.general.region=CLOWNMDEMU_REGION_OVERSEAS;config.general.tv_standard=CLOWNMDEMU_TV_STANDARD_NTSC;
        config.general.cd_add_on_enabled=cc_false;config.controller_manager.protocol=CONTROLLER_MANAGER_PROTOCOL_STANDARD;config.fm.ladder_effect_disabled=cc_true;
        ClownMDEmu_Initialise(machine.get(),&config,&callbacks);ClownMDEmu_SetCartridge(machine.get(),rom.data(),static_cast<cc_u32f>(rom.size()));
        ClownMDEmu_HardReset(machine.get(),cc_true,cc_false);
    }
    void palette_rebuild(){
        for(std::size_t i=0;i<64;++i){const auto c=machine->vdp.state.cram[i];palette[i]=static_cast<std::uint16_t>(c|((c&0x888)>>3));palette[64+i]=static_cast<std::uint16_t>(c>>1);palette[128+i]=static_cast<std::uint16_t>(0x888+(c>>1));}
    }
    void step(const std::vector<Assignment>& a){input=a;scanlines=0;ClownMDEmu_Iterate(machine.get());pcm=audio.finish_frame();require(scanlines>0,"VIDEO_MISSING");}
    Bytes video()const {Bytes b=bytes("TSVIDEO1");number(b,width,4);number(b,height,4);number(b,1,4);append(b,View(pixels).first(std::size_t(width)*height*4));return b;}
    Bytes sound()const {Bytes b=bytes("TSAUDIO1");number(b,48000,4);number(b,2,4);number(b,pcm.size()/4,8);append(b,pcm);return b;}
    Bytes regions(std::uint64_t mask)const {
        Bytes b;
        if(mask&1){Bytes m;for(auto w:machine->state.m68k.ram)number(m,w,2);append(m,View(machine->vdp.state.vram));field(b,1,5,m);}
        if(mask&2){Bytes m;for(auto w:machine->vdp.state.cram)number(m,w,2);field(b,2,5,m);}
        // The full VRAM image includes SAT bytes. No inferred draw order is claimed.
        if(mask&4)field(b,3,5,View(machine->vdp.state.vram));
        if(mask&8){Bytes g;number(g,width,4);number(g,height,4);field(b,4,5,g);}
        return b;
    }
};
struct Control {std::uint32_t id,type;std::uint64_t value;};
struct Pending {std::uint64_t id,epoch,frame;std::vector<Control> items;};
struct Result {
    int status{};std::uint64_t transaction{},epoch{},frame{};std::string cause;
    Result(int s=0,std::uint64_t t=0,std::uint64_t e=0,std::uint64_t f=0,std::string c={}):status(s),transaction(t),epoch(e),frame(f),cause(std::move(c)){}
};
struct Snapshot {std::uint64_t session,epoch,generation,mask;Bytes regions,original;bool temporal_complete{false};};
struct Session {
    Bytes rom,config,trace_wire;std::string rom_hash,config_hash,trace_hash,initial_hash;
    std::vector<std::vector<Assignment>> trace;
    std::unique_ptr<Core> core;
    std::uint64_t session=1,epoch=1,frame{},generation{},region_mask{},event_mask{},losses{},event_total{};
    std::vector<std::uint64_t> events;
    std::vector<std::weak_ptr<const Snapshot>> snapshots;
    std::optional<Pending> pending;
    bool closed{};
    Session(Bytes content,std::vector<std::vector<Assignment>> t):rom(std::move(content)),trace(std::move(t)),core(std::make_unique<Core>(rom)) {
        rom_hash=hash(rom);integer(config,1,1,1);field(config,2,6,bytes("clownmdemu"));field(config,3,5,unhex(TESSERA_GATE_BUILD_ID));integer(config,4,1,1);integer(config,5,1,48000);
        field(config,6,8,input_wire(inputs()));field(config,7,5,unhex(hash(Bytes(65536,0))));
        // Explicit profile: all other upstream options are the pinned zero-initialized defaults.
        integer(config,8,1,CanonicalAudio::master_ticks_per_frame);integer(config,9,1,CanonicalAudio::master_clock_hz);
        integer(config,10,1,1);integer(config,11,2,32768);integer(config,12,2,4096);field(config,13,6,bytes("integer-causal-ZOH-v1;CD-disabled-zero-required;zero-initialized-upstream-options"));
        config_hash=hash(config);trace_wire=bytes("TSTRACE1");field(trace_wire,1,5,unhex(rom_hash));field(trace_wire,2,5,unhex(config_hash));integer(trace_wire,3,3,trace.size());
        std::vector<Bytes> records;for(std::size_t i=0;i<trace.size();++i){Bytes f;integer(f,1,3,i+1);field(f,2,8,input_wire(trace[i]));records.push_back(std::move(f));}field(trace_wire,4,8,array(records));trace_hash=hash(trace_wire);
        initial_hash=hash(state());
    }
    Bytes state()const {
        Bytes b=bytes("TSSTATE1");field(b,1,6,bytes("tessera.phase0.clown.explicit"));integer(b,2,1,1);integer(b,3,1,0);field(b,4,5,unhex(rom_hash));field(b,5,5,unhex(TESSERA_GATE_BUILD_ID));field(b,6,5,unhex(config_hash));
        integer(b,7,3,frame);integer(b,8,3,frame*CanonicalAudio::master_ticks_per_frame);field(b,9,8,input_wire(core->input));
        Bytes m;field(m,1,5,clown_state_codec::encode_machine(*core->machine));field(m,2,5,core->audio.encode_state());field(b,65536,11,m);return b;
    }
    void event(std::uint64_t kind){if(!(event_mask&kind))return;++event_total;if(events.size()<4096)events.push_back(event_total);else ++losses;}
    std::vector<std::uint64_t> read_events(std::uint64_t& cursor)const {
        std::vector<std::uint64_t> batch;for(auto sequence:events)if(sequence>cursor)batch.push_back(sequence);
        // The sequence interval plus the out-of-queue loss counter exposes gaps.
        cursor=event_total;return batch;
    }
    void acknowledge_events(std::uint64_t first,std::uint64_t second){
        const auto through=std::min(first,second);std::erase_if(events,[&](auto sequence){return sequence<=through;});
    }
    Result request(std::uint64_t sid,std::uint64_t e,std::uint64_t f,std::uint64_t id,std::vector<Control> items) {
        if(closed)return {-4};if(sid!=session)return {-4};if(e!=epoch)return {-11};if(f!=frame+1)return {-13};
        if(id==0||items.empty()||items.size()>64)return {-1};std::set<std::uint32_t> seen;
        for(auto c:items){if(c.id<1||c.id>2||c.type!=1||!seen.insert(c.id).second)return {-1};
            if(c.value&~std::uint64_t(c.id==1?15:31))return {-6};}
        if(pending)return {-19};pending=Pending{id,e,f,std::move(items)};return {};
    }
    Result step(std::uint64_t e,std::uint64_t f,const std::vector<Assignment>& a) {
        if(closed)return {-4};if(e!=epoch)return {-11};if(f<=frame)return {-12};if(f!=frame+1)return {-13};
        try{validate_inputs(a);}catch(...){return {-15};}
        Result r{0,0,epoch,f,{}};
        if(pending){for(auto c:pending->items)(c.id==1?region_mask:event_mask)=c.value;r.transaction=pending->id;pending.reset();event(2);}
        core->step(a);++frame;++generation;event(4);return r;
    }
    Result next(){if(frame>=trace.size())return {1};return step(epoch,frame+1,trace[static_cast<std::size_t>(frame)]);}
    std::shared_ptr<const Snapshot> capture(){
        require(!closed&&frame>0&&core->scanlines>0,"FRAME_NOT_AVAILABLE");
        std::erase_if(snapshots,[](const auto& p){return p.expired();});
        if(snapshots.size()>=8){++losses;return {};}
        auto s=std::make_shared<const Snapshot>(Snapshot{session,epoch,generation,region_mask,core->regions(region_mask),core->video(),false});
        require(s->regions.size()+s->original.size()<=8*1024*1024,"CAPTURE_BYTE_BUDGET");snapshots.emplace_back(s);event(8);return s;
    }
    Bytes present(const std::shared_ptr<const Snapshot>& s,View replacement,std::string& cause)const {
        (void)replacement;require(bool(s),"SNAPSHOT_REQUIRED");
        if(!s->temporal_complete){cause="TEMPORAL_COVERAGE_INCOMPLETE";return s->original;}
        throw std::runtime_error("UNIMPLEMENTED_TEMPORAL_PATH");
    }
    Result cancel(std::string cause){Result r{0,pending?pending->id:0,epoch,frame,std::move(cause)};pending.reset();return r;}
    Result reset(){require(!closed,"SESSION_CLOSED");auto candidate=std::make_unique<Core>(rom);auto r=cancel("RESET");core=std::move(candidate);frame=0;++epoch;++generation;region_mask=event_mask=0;r.epoch=epoch;return r;}
    Result close(){auto r=cancel("CLOSE");closed=true;return r;}
    Bytes checkpoint()const {
        Bytes payload;field(payload,1,5,state());field(payload,2,5,unhex(trace_hash));integer(payload,3,3,trace.size());integer(payload,4,3,frame+1);field(payload,5,5,unhex(initial_hash));
        return checkpoint_envelope(payload);
    }
    Result restore(View b){
        require(b.size()>=64&&b.size()<=limit&&equal(b.first(8),bytes("TSCHK001")),"CHECKPOINT_HEADER_INVALID");
        require(read(b,8,2)==1&&read(b,10,2)==0&&read(b,12,4)==64&&read(b,16,8)==b.size()&&read(b,56,4)==5&&read(b,60,4)==0,"CHECKPOINT_VERSION_SIZE_INVALID");
        require(equal(b.subspan(24,32),unhex(hash(b.subspan(64)))),"CHECKPOINT_INTEGRITY_INVALID");auto c=fields(b.subspan(64));require(c.size()==5,"CHECKPOINT_FIELDS_INVALID");
        require(equal(get(c,2,5,32),unhex(trace_hash))&&read(get(c,3,3,8),0,8)==trace.size()&&equal(get(c,5,5,32),unhex(initial_hash)),"CHECKPOINT_REPLAY_IDENTITY_INVALID");
        const auto stateBytes=get(c,1,5);require(stateBytes.size()>=8&&equal(stateBytes.first(8),bytes("TSSTATE1")),"STATE_MAGIC_INVALID");auto s=fields(stateBytes.subspan(8));require(s.size()==10,"STATE_FIELDS_INVALID");
        require(equal(get(s,1,6),bytes("tessera.phase0.clown.explicit"))&&read(get(s,2,1,4),0,4)==1&&read(get(s,3,1,4),0,4)==0,"STATE_CODEC_INVALID");
        require(equal(get(s,4,5,32),unhex(rom_hash))&&equal(get(s,5,5,32),unhex(TESSERA_GATE_BUILD_ID))&&equal(get(s,6,5,32),unhex(config_hash)),"STATE_IDENTITY_INVALID");
        const auto completed=read(get(s,7,3,8),0,8);require(completed<=trace.size()&&read(get(s,8,3,8),0,8)==completed*CanonicalAudio::master_ticks_per_frame&&read(get(c,4,3,8),0,8)==completed+1,"STATE_REPLAY_CURSOR_INVALID");
        const auto expectedInput=completed?trace[static_cast<std::size_t>(completed-1)]:inputs();require(equal(get(s,9,8),input_wire(expectedInput)),"STATE_INPUT_INVALID");
        auto m=fields(get(s,65536,11));require(m.size()==2,"STATE_MACHINE_BLOCKS_INVALID");auto candidate=std::make_unique<Core>(rom);
        clown_state_codec::decode_machine(*candidate->machine,get(m,1,5));candidate->audio.decode_state(get(m,2,5));require(candidate->audio.frames_completed()==completed,"STATE_MEDIA_FRAME_INVALID");
        require(!candidate->machine->state.mega_cd.cd_inserted&&!candidate->machine->mega_cd.pcm.state.sounding&&!candidate->machine->mega_cd.cdda.state.playing,"STATE_CD_PROFILE_UNSUPPORTED");
        candidate->input=expectedInput;candidate->palette_rebuild();
        // Commit only after every field, context and media block has been validated.
        auto result=cancel("RESTORE");core=std::move(candidate);frame=completed;++epoch;++generation;result.epoch=epoch;result.frame=frame;return result;
    }
};

Json self_tests(const Bytes& rom,const std::vector<std::vector<Assignment>>& trace) {
    std::size_t checks{};auto check=[&](bool b,const char* c){++checks;require(b,c);};
    auto rejects=[&](auto action,const char* code){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,code);};
    Session s(rom,trace);const auto initial=s.state();
    auto pending=[&](std::uint64_t id){return s.request(s.session,s.epoch,s.frame+1,id,{{1,1,15},{2,1,31}});};
    check(s.request(s.session,s.epoch,1,1,{{1,1,15},{1,1,0}}).status==-1,"DUPLICATE_CONTROL_ACCEPTED");
    check(s.request(s.session,s.epoch,1,1,{{1,1,32}}).status==-6,"UNKNOWN_MASK_ACCEPTED");
    check(s.request(s.session,s.epoch,1,1,{{1,1,16}}).status==-6,"UNSUPPORTED_TEMPORAL_ACCEPTED");
    check(s.request(s.session,s.epoch,1,1,{{1,2,1}}).status==-1,"UNKNOWN_TYPE_ACCEPTED");
    check(pending(100).status==0&&s.region_mask==0,"PENDING_APPLIED_DURING_PAUSE");
    check(pending(101).status==-19&&s.pending->id==100,"PENDING_REPLACED");
    check(s.step(s.epoch+1,1,inputs()).status==-11&&s.pending->id==100,"STALE_INPUT_APPLIED_PENDING");
    auto invalid=inputs();invalid.pop_back();check(s.step(s.epoch,1,invalid).status!=0&&s.state()==initial&&s.pending->id==100,"INVALID_INPUT_MUTATED");
    invalid=inputs();invalid[1]=invalid[0];check(s.step(s.epoch,1,invalid).status!=0&&s.pending->id==100,"DUPLICATE_INPUT_MUTATED");
    check(s.step(s.epoch,2,inputs()).status==-13&&s.pending->id==100,"FUTURE_INPUT_MUTATED");
    const auto before=s.checkpoint();check(s.pending->id==100&&s.state()==initial,"EXPORT_CHANGED_PENDING");
    auto r=s.next();check(r.transaction==100&&r.epoch==s.epoch&&r.frame==1&&s.region_mask==15,"CONFIRMATION_INVALID");
    const auto snap=s.capture();auto reader2=snap;const auto retained=hash(snap->regions),original=hash(snap->original);
    std::string cause;Bytes magenta(snap->original.size(),255);check(s.present(snap,magenta,cause)==snap->original&&cause=="TEMPORAL_COVERAGE_INCOMPLETE","FALLBACK_INVALID");
    std::vector<std::shared_ptr<const Snapshot>> holds{snap};for(unsigned i=0;i<7;++i)holds.push_back(s.capture());
    check(!s.capture()&&s.losses==1,"SNAPSHOT_BUDGET_NOT_ENFORCED");
    for(unsigned i=0;i<4100;++i)s.event(4);check(s.events.size()==4096&&s.losses>1,"EVENT_SATURATION_INVALID");
    std::uint64_t cursor1{},cursor2{};const auto batch=s.read_events(cursor1);check(batch==s.read_events(cursor2)&&cursor1==cursor2&&cursor1>batch.size(),"EVENT_READERS_OR_GAP_INVALID");
    check(s.events.size()==4096,"EVENT_READER_CONSUMED_OTHER_CURSOR");
    check(pending(200).status==0,"CONTROL_BEFORE_RESTORE_FAILED");const auto preserve=s.state();const auto epoch=s.epoch;
    auto damaged=before;damaged.back()^=1;rejects([&]{s.restore(damaged);},"CORRUPT_CHECKPOINT_ACCEPTED");
    check(s.state()==preserve&&s.epoch==epoch&&s.pending->id==200,"FAILED_RESTORE_MUTATED");
    const auto payload=View(before).subspan(64);const auto state=fields(payload).at(1).value;
    auto rejectCheckpoint=[&](const Bytes& invalid){rejects([&]{s.restore(invalid);},"SEMANTIC_CHECKPOINT_ACCEPTED");check(s.state()==preserve&&s.epoch==epoch&&s.pending->id==200,"SEMANTIC_RESTORE_MUTATED");};
    damaged=before;damaged[8]=2;rejectCheckpoint(damaged);
    rejectCheckpoint(checkpoint_envelope(replace_field(payload,2,Bytes(32,0))));
    auto replaceState=[&](View changed){return checkpoint_envelope(replace_field(payload,1,changed));};
    auto stateField=[&](std::uint32_t tag,View value){Bytes changed(state.begin(),state.begin()+8);append(changed,replace_field(state.subspan(8),tag,value));return changed;};
    rejectCheckpoint(replaceState(stateField(4,Bytes(32,0))));
    rejectCheckpoint(replaceState(stateField(5,Bytes(32,0))));
    rejectCheckpoint(replaceState(stateField(6,Bytes(32,0))));
    rejectCheckpoint(replaceState(stateField(7,number(1,8))));
    rejectCheckpoint(replaceState(stateField(9,input_wire(inputs(true)))));
    auto cdCandidate=std::make_unique<Core>(rom);const auto machineRecord=fields(state.subspan(8)).at(65536).value;
    for(unsigned mode=0;mode<3;++mode){
        cdCandidate->machine->state.mega_cd.cd_inserted=mode==0;
        cdCandidate->machine->mega_cd.pcm.state.sounding=mode==1;
        cdCandidate->machine->mega_cd.cdda.state.playing=mode==2;
        const auto changed=replace_field(machineRecord,1,clown_state_codec::encode_machine(*cdCandidate->machine));
        rejectCheckpoint(replaceState(stateField(65536,changed)));
    }
    r=s.restore(before);check(r.transaction==200&&r.cause=="RESTORE"&&s.epoch==epoch+1&&s.state()==initial&&s.region_mask==15,"RESTORE_COMMIT_INVALID");
    rejects([&]{s.capture();},"RESTORE_EXPOSED_EMPTY_ORIGINAL");
    check(hash(reader2->regions)==retained&&hash(reader2->original)==original,"RETAINED_RESTORE_MUTATED");
    check(pending(300).status==0,"CONTROL_BEFORE_RESET_FAILED");s.core->machine->state.external_ram.buffer[0]=0x77;
    r=s.reset();check(r.transaction==300&&r.cause=="RESET"&&s.frame==0&&s.region_mask==0&&s.event_mask==0&&s.state()==initial,"RESET_NOT_RECORDED_INITIAL");
    check(hash(reader2->regions)==retained,"RETAINED_RESET_MUTATED");
    check(pending(400).status==0,"CONTROL_BEFORE_CLOSE_FAILED");r=s.close();check(r.transaction==400&&r.cause=="CLOSE"&&hash(reader2->original)==original,"CLOSE_CANCELLATION_INVALID");
    // End of trace must not execute an extra frame or apply a pending transaction.
    Session eof(rom,{inputs()});eof.next();eof.request(eof.session,eof.epoch,2,500,{{1,1,15}});const auto eofState=eof.state();
    check(eof.next().status==1&&eof.pending->id==500&&eof.state()==eofState,"EOF_MUTATED");
    // Backup omissions are explicit fields in the audited codec.
    auto encoded=clown_state_codec::encode_machine(*eof.core->machine);auto& timer=eof.core->machine->controller_manager.state.ea_multitap.controllers[0].countdown;
    timer=9876;auto withTimer=clown_state_codec::encode_machine(*eof.core->machine);timer=1234;clown_state_codec::decode_machine(*eof.core->machine,withTimer);check(timer==9876,"CONTROLLER_CODEC_OMISSION");
    encoded.back()^=1;const auto intact=clown_state_codec::encode_machine(*eof.core->machine);
    rejects([&]{clown_state_codec::decode_machine(*eof.core->machine,encoded);},"MACHINE_CORRUPTION_ACCEPTED");check(clown_state_codec::encode_machine(*eof.core->machine)==intact,"MACHINE_REJECT_MUTATED");
    rejects([&]{clown_state_codec::decode_machine(*eof.core->machine,View(encoded).first(180));},"MACHINE_TRUNCATION_ACCEPTED");
    const std::array<std::pair<std::size_t,std::uint8_t>,10> invalidFields{{{65728,2},{663787,4},{926132,3},{926175,5},{992550,33},{1054478,255},{1054735,65},{1055968,1},{1056000,8},{1132101,8}}};
    for(const auto [offset,value]:invalidFields){auto corrupt=intact;corrupt.at(offset)=value;const auto sha=hash(View(corrupt).subspan(188));std::copy(sha.begin(),sha.end(),corrupt.begin()+124);
        rejects([&]{clown_state_codec::decode_machine(*eof.core->machine,corrupt);},"MACHINE_SEMANTIC_INVALID_ACCEPTED");check(clown_state_codec::encode_machine(*eof.core->machine)==intact,"MACHINE_SEMANTIC_REJECT_MUTATED");}
    // Whole-trace negatives are rejected before a session can use them.
    auto fixture=Json{{"trace_version",1},{"system","mega_drive"},{"controller_profile","one_three_button_pad"},{"format_scope","phase0_probe_private_not_native_api_abi"},{"frames",Json::array({{{"frame",1},{"ports",Json::array({{{"port",0},{"controls",{{"up",false},{"down",false},{"left",false},{"right",false},{"a",false},{"b",false},{"c",false},{"start",false}}}}})}}})}};
    auto good=bytes(fixture.dump());check(trace_parse(good).size()==1,"TRACE_VALID_REJECTED");
    auto bad=fixture;bad["frames"][0]["frame"]=2;rejects([&]{trace_parse(bytes(bad.dump()));},"TRACE_HOLE_ACCEPTED");
    bad=fixture;bad["frames"].push_back(bad["frames"][0]);rejects([&]{trace_parse(bytes(bad.dump()));},"TRACE_DUPLICATE_FRAME_ACCEPTED");
    bad=fixture;bad["frames"][0]["ports"][0]["controls"].erase("a");rejects([&]{trace_parse(bytes(bad.dump()));},"TRACE_INCOMPLETE_ACCEPTED");
    rejects([&]{parse_json(bytes("{\"x\":1,\"x\":1}"));},"TRACE_DUPLICATE_KEY_ACCEPTED");
    return {{"checks",checks},{"pass",true},{"media",run_media_tests()}};
}

Json execute(const std::filesystem::path& directory,const std::string& traceName,const std::filesystem::path& checkpointDir,const std::string& restoreName,std::uint64_t count,const std::string& observer,bool original) {
    require(observer=="off"||observer=="full"||observer=="saturated","OBSERVER_MODE_INVALID");
    auto trace=trace_parse(load(directory/traceName));Bytes rom=load(original?std::filesystem::path(TESSERA_ORIGINAL_ROM):directory/"program.md");
    Session s(std::move(rom),std::move(trace));const auto tests=self_tests(s.rom,s.trace);
    if(!restoreName.empty())s.restore(load(restoreName));
    if(observer!="off")require(s.request(s.session,s.epoch,s.frame+1,900,{{1,1,15},{2,1,31}}).status==0,"OBSERVATION_SETUP_FAILED");
    auto records=Json::array();std::vector<double> emulation_ms,hash_ms;std::string stateHashes,videoHashes,audioHashes;
    std::shared_ptr<const Snapshot> retained;std::string retainedHash;std::uint64_t captured{},fallbacks{},oracle_checks{},audio_checks{},cursor1{},cursor2{};bool oracle=true;
    const auto begin=s.frame;require(count<=s.trace.size()-begin,"REQUESTED_FRAMES_EXCEED_TRACE");
    const auto end=count?begin+count:s.trace.size();
    for(std::uint64_t f=begin+1;f<=end;++f){
        const auto start=std::chrono::steady_clock::now();require(s.next().status==0,"STEP_FAILED");const auto after=std::chrono::steady_clock::now();
        const auto stateHash=hash(s.state()),videoHash=hash(s.core->video()),audioHash=hash(s.core->sound());const auto hashed=std::chrono::steady_clock::now();
        if(observer!="off"){
            auto snap=s.capture();require(bool(snap),"UNEXPECTED_CAPTURE_LOSS");++captured;auto reader=snap;
            if(!retained){retained=snap;retainedHash=hash(snap->regions);}
            require(hash(reader->regions)==hash(snap->regions)&&hash(retained->regions)==retainedHash,"READERS_INTERFERED");
            std::string cause;auto presented=s.present(snap,Bytes(snap->original.size(),255),cause);require(hash(presented)==videoHash&&!cause.empty(),"FALLBACK_CHANGED_ORIGINAL");++fallbacks;
            if(observer=="saturated")for(unsigned n=0;n<80;++n)s.event(4);
            else {const auto batch=s.read_events(cursor1);require(batch==s.read_events(cursor2)&&cursor1==cursor2,"EVENT_READERS_INTERFERED");s.acknowledge_events(cursor1,cursor2);}
        }
        const auto& m=*s.core->machine;const bool pressed=s.core->input[5].value!=0;
        const bool central=(s.trace.size()==60)?((f>=5&&f<=15)||(f>=25&&f<=35)||(f>=45&&f<=55)):
            ((f>=100&&f<=200)||(f>=350&&f<=550)||(f>=650&&f<=850));
        if(!original&&central){
            const auto center=(std::size_t(s.core->height/2)*s.core->width+s.core->width/2)*4;
            const auto marker=(m.vdp.state.vram[0xFFFC]<<8)|m.vdp.state.vram[0xFFFD];
            oracle=oracle&&m.state.m68k.ram[0]>>8==(pressed?0:16)&&m.state.m68k.ram[1]==0&&marker==(pressed?0x2468:0x1357)&&m.vdp.state.cram[0]==(pressed?0x000E:0x0E00)&&
                s.core->width==320&&s.core->height==224&&s.core->pixels[center]==(pressed?255:0)&&s.core->pixels[center+2]==(pressed?0:255)&&
                m.psg.state.tones[0].countdown_master==256&&m.psg.state.tones[0].attenuation==(pressed?0:15);++oracle_checks;
            std::int32_t minimum=32767,maximum=-32768;for(std::size_t i=0;i<s.core->pcm.size();i+=2){const auto raw=read(s.core->pcm,i,2);const auto sample=raw>=32768?static_cast<std::int32_t>(raw)-65536:static_cast<std::int32_t>(raw);minimum=std::min(minimum,sample);maximum=std::max(maximum,sample);}
            require(!s.core->pcm.empty()&&(pressed?maximum-minimum>16:maximum<=2&&minimum>=-2),"CANONICAL_AUDIO_ORACLE_FAILED");++audio_checks;
        }
        require(oracle,"INDEPENDENT_ROM_ORACLE_FAILED");
        if(!checkpointDir.empty()&&(f==500||f==1500||f==2500))atomic_write(checkpointDir/(std::to_string(f)+".tsc"),s.checkpoint());
        records.push_back({{"frame",f},{"state",stateHash},{"video",videoHash},{"pcm",audioHash},{"pcm_frames",s.core->pcm.size()/4}});
        stateHashes+=stateHash;videoHashes+=videoHash;audioHashes+=audioHash;
        emulation_ms.push_back(std::chrono::duration<double,std::milli>(after-start).count());hash_ms.push_back(std::chrono::duration<double,std::milli>(hashed-after).count());
    }
    auto summary=[](std::vector<double> v){std::sort(v.begin(),v.end());auto p=[&](double x){return v.empty()?0.0:v[std::min(v.size()-1,static_cast<std::size_t>(x*(v.size()-1)))];};return Json{{"p50",p(.50)},{"p95",p(.95)},{"p99",p(.99)}};};
    return {{"schema_version",2},{"backend","ClownMDEmu"},{"build_id",TESSERA_GATE_BUILD_ID},{"codec_schema",clown_state_codec::schema_identity()},
        {"trace",traceName},{"trace_sha256",s.trace_hash},{"rom_sha256",s.rom_hash},{"config_sha256",s.config_hash},{"process_id",GetCurrentProcessId()},
        {"begin_frame",begin},{"completed_frame",s.frame},{"frames",end-begin},{"observer",observer},{"records",std::move(records)},
        {"aggregate",{{"state",hash(bytes(stateHashes))},{"video",hash(bytes(videoHashes))},{"pcm",hash(bytes(audioHashes))}}},
        {"independent_oracle_checks",oracle_checks},{"audio_oracle_checks",audio_checks},{"oracles_pass",oracle},{"self_tests",tests},
        {"captured",captured},{"fallbacks",fallbacks},{"event_losses",s.losses},{"cd_zero_scalars_generated",s.core->cd_zero_samples},
        {"temporal_complete",false},{"capabilities",0x17FF},{"profile","MD NTSC-U, 3-button, no CD; FM ladder disabled; all generators active; ZOH S16LE48k stereo FM1 PSG1/8"},
        {"timing_ms",{{"emulation_and_pcm",summary(emulation_ms)},{"canonical_encoding_and_hashes",summary(hash_ms)}}},
        {"scope","private Gate 0 feasibility; no product ABI, Engine/Runtime integration or FX-03 acceptance"},{"pass",true}};
}
}
int main(int argc,char** argv){
    try {
        require(argc>=5,"USAGE: fixtures trace report observer [checkpoint-directory|-] [restore-file|-] [count] [original]");
        const std::filesystem::path checkpoints=argc>5&&std::string(argv[5])!="-"?argv[5]:"";
        const std::string restore=argc>6&&std::string(argv[6])!="-"?argv[6]:"";
        const auto count=argc>7?std::stoull(argv[7]):0;
        auto result=execute(argv[1],argv[2],checkpoints,restore,count,argv[4],argc>8&&std::string(argv[8])=="original");
        std::ofstream out(argv[3],std::ios::binary);out<<result.dump(2)<<'\n';require(bool(out),"REPORT_WRITE_FAILED");
        std::cout<<"CLOWN_QUALIFICATION_PASS frames="<<result.at("frames")<<" pid="<<GetCurrentProcessId()<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<"Error de validación local: "<<error.what()<<'\n';return 1;}
}
