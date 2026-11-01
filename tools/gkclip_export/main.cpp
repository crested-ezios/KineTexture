#include "Clip.hpp"
#include "Profiles.hpp"
#include "Presets.hpp"
#include "VmdWriter.hpp"
#include "SmplWriter.hpp"
#include <chrono>
#include <iostream>
#include <optional>

using namespace KineTexture;
using namespace KineTexture::GkExport;

namespace {
std::filesystem::path Path(const std::string& value) {
    return std::filesystem::path(std::u8string(value.begin(),value.end()));
}
std::string Json(const std::string& text) {
    std::ostringstream out;out<<'"';
    for(unsigned char c:text) {
        if(c=='"' || c=='\\')out<<'\\'<<char(c);
        else if(c>=32)out<<char(c); // Input strings are UTF-8; preserve complete sequences.
        else out<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c)<<std::dec;
    }out<<'"';return out.str();
}
template<class T> std::string Array(const std::vector<T>& items) {
    std::ostringstream out;out<<'[';
    for(std::size_t i=0;i<items.size();++i) {if(i)out<<',';out<<+items[i];}out<<']';return out.str();
}
void WriteFile(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path,std::ios::binary);
    Require(bool(stream),"cannot create staging file");
    stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));stream.close();
    Require(bool(stream),"failed to write staging file");
}
std::vector<std::uint8_t> BytesOf(const std::string& s) {return {s.begin(),s.end()};}
// copy_file with copy_options::none refuses existing destinations, including a
// concurrent writer. Roll back only files successfully created by this call.
void WriteNew(const std::filesystem::path& output,const std::vector<std::uint8_t>& bytes,
              const std::optional<std::string>& report=std::nullopt) {
    auto sidecar=output;sidecar+=".report.json";
    Require(!std::filesystem::exists(output) && (!report || !std::filesystem::exists(sidecar)),"refusing to overwrite output or report");
    auto parent=output.parent_path();if(parent.empty())parent=".";
    Require(std::filesystem::is_directory(parent),"output parent directory does not exist");
    std::filesystem::path stage;
    for(int attempt=0;attempt<100;++attempt) {
        const auto token=std::chrono::steady_clock::now().time_since_epoch().count();
        auto candidate=parent/(".kinetexture-export-"+std::to_string(token)+"-"+std::to_string(attempt));
        if(std::filesystem::create_directory(candidate)) {stage=candidate;break;}
    }
    Require(!stage.empty(),"cannot reserve staging directory");bool wrote_output=false,wrote_report=false;
    auto cleanup=[&] {std::error_code ec;std::filesystem::remove(stage/"payload",ec);std::filesystem::remove(stage/"report",ec);std::filesystem::remove(stage,ec);};
    try {
        WriteFile(stage/"payload",bytes);
        if(report)WriteFile(stage/"report",BytesOf(*report));
        Require(std::filesystem::copy_file(stage/"payload",output,std::filesystem::copy_options::none),"cannot create output");wrote_output=true;
        if(report) {Require(std::filesystem::copy_file(stage/"report",sidecar,std::filesystem::copy_options::none),"cannot create report");wrote_report=true;}
        cleanup();
    }catch(...) {
        std::error_code ec;if(wrote_report)std::filesystem::remove(sidecar,ec);if(wrote_output)std::filesystem::remove(output,ec);cleanup();throw;
    }
}
double Number(const std::string& s) {
    std::size_t end{};const auto n=std::stod(s,&end);Require(end==s.size() && std::isfinite(n),"expected finite number: "+s);return n;
}
void Help() {
    std::cout<<"Native development export of legacy canonical-local gkclip.\n"
        <<"Usage: "<<GK_EXPORT_NAME<<" INPUT --output OUTPUT (--preset NAME | --profile FILE) [options]\n"
        <<"       "<<GK_EXPORT_NAME<<" --preset NAME --write-profile FILE\n"
        <<"Presets: "<<(GK_EXPORT_VMD ? "canonical-mmd":"canonical-smpl, canonical-smplx")<<" (approximate frame alignment; explicit opt-in)\n"
        <<"Options:\n  --body-only       Allow omitted channels; enumerate them in OUTPUT.report.json\n"
        <<"  --fps NUMBER      Output sample rate (VMD requires 30; SMPL defaults to source FPS)\n"
        <<"  --neutral-shape   SMPL only: explicitly include ten zero preview betas\n"
        <<"  --betas X,Y,...   SMPL only: include caller-supplied shape coefficients\n"
        <<"  --help            Show usage\n"
        <<"No existing output/profile/report is overwritten. No game or Python is required.\n";
}
}
int Run(int argc,char** argv) {
    try {
        std::string input,output,profile_path,preset,write_profile,betas;
        bool body_only=false,neutral_shape=false;std::optional<double> fps;
        std::set<std::string> seen;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help" || arg=="-h") {Help();return 0;}
            if(!arg.starts_with("--")) {Require(input.empty(),"only one input is supported");input=arg;continue;}
            Require(seen.insert(arg).second,"duplicate option: "+arg);
            if(arg=="--body-only")body_only=true;
            else if(arg=="--neutral-shape")neutral_shape=true;
            else {
                Require(i+1<argc,"option requires a value: "+arg);const std::string value=argv[++i];
                if(arg=="--output")output=value;else if(arg=="--profile")profile_path=value;
                else if(arg=="--preset")preset=value;else if(arg=="--write-profile")write_profile=value;
                else if(arg=="--fps")fps=Number(value);else if(arg=="--betas")betas=value;
                else throw std::runtime_error("unknown option: "+arg);
            }
        }
        Require(preset.empty()!=profile_path.empty(),"supply exactly one of --preset or --profile");
        const Profile profile=preset.empty() ? ReadProfile(Path(profile_path)):MakePreset(preset);
        Require(GK_EXPORT_VMD ? profile.target=="vmd":(profile.target=="smpl" || profile.target=="smplx"),"profile target differs from executable");
        Require(!GK_EXPORT_VMD || (!neutral_shape && betas.empty()),"shape options are SMPL-only");
        Require(!neutral_shape || betas.empty(),"--neutral-shape and --betas are mutually exclusive");
        if(!write_profile.empty()) {
            Require(input.empty() && output.empty(),"--write-profile is a separate command without input/output");
            WriteNew(Path(write_profile),BytesOf(SerializeProfile(profile)));std::cout<<"Created "<<write_profile<<'\n';return 0;
        }
        Require(!input.empty() && !output.empty(),"input and --output are required; see --help");
        Require(!std::filesystem::exists(Path(output)) && !std::filesystem::exists(Path(output+".report.json")),"refusing to overwrite output or report");
        const Clip clip=ReadClip(Path(input));
        const double output_fps=fps.value_or(GK_EXPORT_VMD ? 30.0:double(clip.fps));
        Require(!GK_EXPORT_VMD || output_fps==30,"VMD uses a 30 FPS timebase; --fps must be 30");
        const auto converted=Convert(clip,profile,output_fps,body_only);
        std::vector<std::uint8_t> bytes;
        if(GK_EXPORT_VMD) {
            VmdAnimation animation;animation.model_name_cp932="KineTexture";
            Require(!profile.root_track.empty(),"VMD profile requires a separate root translation track");
            VmdBoneTrack root_track;root_track.name_cp932=profile.root_track;
            for(std::size_t f=0;f<converted.frames;++f)root_track.keys.push_back({std::uint32_t(f),converted.roots[f],{}});
            animation.bones.push_back(std::move(root_track));
            for(std::size_t j=0;j<profile.joints.size();++j) {
                if(!converted.valid[j])continue;
                VmdBoneTrack track;track.name_cp932=profile.joints[j].name;
                for(std::size_t f=0;f<converted.frames;++f)track.keys.push_back({std::uint32_t(f),{},converted.rotations[f*converted.joints+j]});
                animation.bones.push_back(std::move(track));
            }
            // Standard leg/toe IK must not override the baked FK preview.
            animation.disabled_ik_cp932={Unhex("8db691ab8268826a"),Unhex("894591ab8268826a"),Unhex("8db682c282dc90e68268826a"),Unhex("894582c282dc90e68268826a")};
            bytes=SerializeVmd(animation);
        }else {
            SmplMotion motion;motion.fps=converted.fps;motion.frame_count=converted.frames;motion.joint_count=converted.joints;
            motion.local_rotations=converted.rotations;motion.translations=converted.roots;motion.joint_valid=converted.valid;
            if(neutral_shape)motion.betas.assign(10,0);
            else if(!betas.empty()) {
                Require(betas.back()!=',',"empty beta coefficient");std::istringstream list(betas);std::string item;
                while(std::getline(list,item,',')) {motion.betas.push_back(Number(item));Require(motion.betas.size()<=300,"too many beta coefficients");}
            }
            bytes=SerializeSmplNpz(motion);
        }
        std::ostringstream report;report<<std::setprecision(17);
        report<<"{\n  \"schema\": \"kinetexture.gk-export.report/1\",\n  \"input\": "<<Json(input)
            <<",\n  \"source_version\": "<<clip.version<<",\n  \"source_frames\": "<<clip.frame_count<<",\n  \"source_fps\": "<<clip.fps
            <<",\n  \"source_header_duration\": "<<clip.duration<<",\n  \"source_last_key_seconds\": "<<(clip.frame_count-1)/double(clip.fps)
            <<",\n  \"source_root_scale_provenance_only\": "<<clip.root_scale<<",\n  \"target\": "<<Json(profile.target)
            <<",\n  \"output\": "<<Json(output)<<",\n  \"output_bytes\": "<<bytes.size()<<",\n  \"output_frames\": "<<converted.frames
            <<",\n  \"output_fps\": "<<converted.fps<<",\n  \"output_last_key_seconds\": "<<(converted.frames-1)/converted.fps
            <<",\n  \"profile_id\": "<<Json(profile.id)<<",\n  \"approximate_profile\": "<<(profile.approximate ? "true":"false")
            <<",\n  \"body_only_acknowledged\": "<<(body_only ? "true":"false")<<",\n  \"joint_valid\": "<<Array(converted.valid)
            <<",\n  \"omitted_source_hids\": "<<Array(converted.omitted_hids)<<",\n  \"missing_target_source_hids\": "<<Array(converted.missing_hids)
            <<",\n  \"omitted_camera\": "<<(clip.has_camera ? "true":"false")<<",\n  \"omitted_morph_names\": [";
        for(std::size_t i=0;i<clip.morphs.size();++i) {if(i)report<<',';report<<Json(clip.morphs[i]);}
        report<<"],\n  \"shape\": "<<Json(neutral_shape ? "explicit_neutral_preview":betas.empty() ? "unknown_omitted":"caller_supplied")
            <<",\n  \"profile_configuration\": "<<Json(SerializeProfile(profile))
            <<",\n  \"limitations\": [\"orientation projection; target geometry/contact/mesh quality is not established\", "
            <<"\"source helper bones, IK controllers and absolute stage origin cannot be recovered\", "
            <<"\"root is converted clip-relative displacement plus explicit profile offset; SMPL pelvis pivot equivalence is unverified\", "
            <<"\"missing SMPL joints contain neutral placeholders and joint_valid=0\", "
            <<"\"resampling uses root lerp and local quaternion slerp; final output-grid sample holds the endpoint\", "
            <<"\"offline export is independent of game rendering and live visual acceptance\"]\n}\n";
        WriteNew(Path(output),bytes,report.str());
        std::cout<<"Created "<<output<<" ("<<converted.frames<<" frames at "<<converted.fps<<" FPS); report: "<<output<<".report.json\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<GK_EXPORT_NAME<<": "<<error.what()<<'\n';return 2;}
}

#ifdef _WIN32
// Windows wide argv avoids the active ANSI codepage losing non-ASCII paths.
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> values;values.reserve(argc);
    for(int i=0;i<argc;++i) {
        std::string value;
        for(std::size_t j=0;argv[i][j];++j) {
            std::uint32_t c=argv[i][j];
            if(c>=0xd800 && c<=0xdbff) {
                const std::uint32_t low=argv[i][++j];
                if(low<0xdc00 || low>0xdfff)return 2;c=0x10000+((c-0xd800)<<10)+(low-0xdc00);
            }else if(c>=0xdc00 && c<=0xdfff)return 2;
            if(c<0x80)value+=char(c);
            else if(c<0x800) {value+=char(0xc0|(c>>6));value+=char(0x80|(c&63));}
            else if(c<0x10000) {value+=char(0xe0|(c>>12));value+=char(0x80|((c>>6)&63));value+=char(0x80|(c&63));}
            else {value+=char(0xf0|(c>>18));value+=char(0x80|((c>>12)&63));value+=char(0x80|((c>>6)&63));value+=char(0x80|(c&63));}
        }values.push_back(std::move(value));
    }
    std::vector<char*> pointers;for(auto& value:values)pointers.push_back(value.data());
    return Run(argc,pointers.data());
}
#else
int main(int argc,char** argv) {return Run(argc,argv);}
#endif
