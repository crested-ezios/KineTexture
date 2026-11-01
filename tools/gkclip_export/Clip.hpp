#pragma once

// Legacy Gakumas compatibility belongs here, never in KineTexture::Core.
#include <KineTexture/Retarget.hpp>
#include <KineTexture/Sampling.hpp>
#include <bit>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace KineTexture::GkExport {
inline void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
inline int ParentHid(int id) {
    switch(id) {
    case 0:return -1; case 1:case 2:case 7:return 0;
    case 3:return 1;case 4:return 2;case 5:return 3;case 6:return 4;
    case 8:return 7;case 54:return 8;case 9:case 11:case 12:return 54;
    case 10:return 9;case 13:return 11;case 14:return 12;
    case 15:return 13;case 16:return 14;case 17:return 15;case 18:return 16;
    case 19:return 5;case 20:return 6;case 21:case 22:return 10;
    default:
        if(id>=24 && id<=38) return (id-24)%3 ? id-1 : 17;
        if(id>=39 && id<=53) return (id-39)%3 ? id-1 : 18;
        return -2;
    }
}
struct Bone { int hid{}; std::string name; bool valid{}, head_space_eye{}; int parent{-1}; };
struct Clip {
    unsigned version{}, frame_count{}; float fps{}, duration{}, root_scale{};
    std::vector<Bone> bones;
    std::vector<Vector3> roots;
    std::vector<Quaternion> rotations;
    bool has_camera{};
    std::vector<std::string> morphs;
};
class ClipBytes {
    std::vector<std::uint8_t> bytes_; std::size_t pos_{};
public:
    explicit ClipBytes(const std::filesystem::path& path) {
        const auto size=std::filesystem::file_size(path);
        Require(size<=512ull*1024*1024,"input byte budget exceeded (512 MiB)");
        std::ifstream stream(path,std::ios::binary);
        Require(bool(stream),"cannot open input");
        bytes_.resize(static_cast<std::size_t>(size));
        Require(bool(stream.read(reinterpret_cast<char*>(bytes_.data()),static_cast<std::streamsize>(size))),"cannot read input");
    }
    std::size_t Remaining() const { return bytes_.size()-pos_; }
    void Need(std::size_t n) const { Require(n<=Remaining(),"truncated gkclip"); }
    std::uint32_t UInt(unsigned width) {
        Need(width); std::uint32_t result=0;
        for(unsigned i=0;i<width;++i) result|=std::uint32_t(bytes_[pos_++])<<(8*i);
        return result;
    }
    float Float() { float value=std::bit_cast<float>(UInt(4)); Require(std::isfinite(value),"nonfinite gkclip value"); return value; }
    std::string String(std::size_t n,bool allow_nul=false) {
        Need(n); std::string result(reinterpret_cast<const char*>(bytes_.data()+pos_),n); pos_+=n;
        Require(allow_nul || result.find('\0')==std::string::npos,"embedded NUL in name"); return result;
    }
};
inline Clip ReadClip(const std::filesystem::path& path) {
    ClipBytes b(path);const auto magic=b.String(8,true);
    Require(magic=="GKCLIPCL" || magic==std::string("GKCLIP\0\0",8),"expected canonical-local gkclip magic");
    Clip c; c.version=b.UInt(4); c.fps=b.Float(); c.frame_count=b.UInt(4);
    c.duration=b.Float(); const auto count=b.UInt(4); c.root_scale=b.Float();
    Require(c.version>=1 && c.version<=5,"unsupported gkclip version");
    Require(c.fps>=0.001f && c.fps<=1000 && c.frame_count>0 && c.frame_count<=1000000 &&
        count>0 && count<=256 && c.duration>=0 && c.root_scale>0,"invalid gkclip header");
    std::set<int> ids; std::set<std::string> names;
    for(unsigned i=0;i<count;++i) {
        Bone bone; bone.hid=int(b.UInt(2)); auto n=b.UInt(1),flags=b.UInt(1);
        Require(n>0 && flags<=(c.version>=5 ? 3u:1u),"invalid bone table flags/name");
        bone.name=b.String(n); bone.valid=(flags&1)!=0; bone.head_space_eye=(flags&2)!=0;
        Require(!bone.head_space_eye || ((bone.hid==21 || bone.hid==22) && bone.valid),"invalid head-space eye flag");
        Require(ids.insert(bone.hid).second && names.insert(bone.name).second,"duplicate bone ID/name");
        c.bones.push_back(bone);
    }
    for(std::size_t i=0;i<c.bones.size();++i) {
        const auto hid=ParentHid(c.bones[i].hid);
        for(std::size_t j=0;j<c.bones.size();++j) if(c.bones[j].hid==hid) c.bones[i].parent=int(j);
        Require(c.bones[i].parent<int(i),"gkclip bones must be in parent-first order");
        Require(!c.bones[i].head_space_eye || c.bones[i].parent>=0,"head-space eye requires Head");
    }
    const auto samples=std::size_t(count)*c.frame_count;
    Require(samples<=16000000,"decoded joint sample budget exceeded");
    b.Need(std::size_t(c.frame_count)*(12+16*count));
    c.roots.resize(c.frame_count); c.rotations.resize(samples);
    for(unsigned f=0;f<c.frame_count;++f) {
        c.roots[f]={b.Float(),b.Float(),b.Float()};
        for(unsigned j=0;j<count;++j) {
            auto& q=c.rotations[std::size_t(f)*count+j]; q={b.Float(),b.Float(),b.Float(),b.Float()};
            const double norm=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
            Require(std::abs(norm-1)<=0.001,"nonunit gkclip rotation");
        }
    }
    if(c.version>=4 || b.Remaining()) {
        const auto flag=b.UInt(1); Require(flag<=1,"invalid camera flag"); c.has_camera=flag!=0;
        if(c.has_camera) {
            b.Need(std::size_t(c.frame_count)*28);
            for(unsigned f=0;f<c.frame_count;++f) {
                for(int j=0;j<6;++j) b.Float();
                const auto fov=b.Float();Require(fov>=1 && fov<=179,"invalid camera FOV");
            }
        }
    }
    if(c.version>=4) {
        const auto n=b.UInt(4); Require(n<=1024,"morph count exceeds budget");
        std::set<std::string> unique;
        for(unsigned i=0;i<n;++i) {
            const auto length=b.UInt(2);Require(length>0,"empty morph name"); auto name=b.String(length);
            Require(unique.insert(name).second,"duplicate morph name");c.morphs.push_back(name);
        }
        b.Need(std::size_t(n)*c.frame_count*4);
        for(std::size_t i=0;i<std::size_t(n)*c.frame_count;++i) { const auto w=b.Float();Require(w>=0 && w<=1,"invalid morph weight"); }
    }
    Require(b.Remaining()==0,"unexpected trailing gkclip bytes");return c;
}
struct Joint {
    int source_hid{-1},parent{-1};std::string name;
    Quaterniond alignment{},rest{};
};
struct Profile {
    std::string id,target;
    unsigned min_version{1},max_version{5};
    Matrix3d basis{{{1,0,0},{0,1,0},{0,0,1}}};
    double meters_per_unit{1};Vector3d root_offset{};
    std::string root_track;
    std::vector<Joint> joints;
    bool approximate{};
};
struct Converted {
    double fps{};std::size_t frames{},joints{};
    std::vector<Quaterniond> rotations;
    std::vector<Vector3d> roots;
    std::vector<std::uint8_t> valid;
    std::vector<int> omitted_hids,missing_hids;
};
inline Quaterniond Product(Quaterniond a,Quaterniond b) { Quaterniond result;Require(NativeDetail::Normalize(NativeDetail::Multiply(a,b),result),"rotation product invalid");return result; }
inline Quaterniond Unit(Quaterniond q) { Quaterniond result;Require(NativeDetail::Normalize(q,result),"invalid rotation");return result; }
inline Converted Convert(const Clip& clip,const Profile& profile,double fps,bool body_only) {
    Require(profile.min_version>=1 && profile.max_version<=5 && profile.min_version<=profile.max_version &&
        clip.version>=profile.min_version && clip.version<=profile.max_version,"profile does not support this gkclip version/frame convention");
    Require(clip.frame_count>0 && !clip.bones.empty() && std::isfinite(clip.fps) && clip.fps>0 &&
        clip.roots.size()==clip.frame_count && clip.rotations.size()==std::size_t(clip.frame_count)*clip.bones.size(),"invalid in-memory clip shape");
    for(std::size_t b=0;b<clip.bones.size();++b)Require(clip.bones[b].parent>=-1 && clip.bones[b].parent<int(b),"invalid in-memory clip parent");
    Require(std::isfinite(fps) && fps>=0.001 && fps<=1000,"invalid output FPS");
    Require(!profile.joints.empty() && profile.joints.size()<=256,"profile needs 1..256 joints");
    Require(profile.target=="vmd" || ((profile.target=="smpl" || profile.target=="smplx") &&
        profile.joints.size()==(profile.target=="smpl" ? 24u:22u)),"invalid profile target/joint count");
    Require(profile.target=="vmd" || profile.meters_per_unit==1,"SMPL translation units must be meters");
    if(profile.target!="vmd") {
        constexpr int parents[24]={-1,0,0,0,1,2,3,4,5,6,7,8,9,9,9,12,13,14,16,17,18,19,20,21};
        for(std::size_t j=0;j<profile.joints.size();++j)
            Require(profile.joints[j].parent==parents[j],"SMPL profile parents must match standard model joint order");
    }
    CoordinateTransform basis;Require(bool(basis.Prepare(profile.basis,1/profile.meters_per_unit)),"invalid profile basis/units");
    Require(IsFinite(profile.root_offset),"invalid root offset");
    Converted out;out.fps=fps;out.joints=profile.joints.size();
    const double last=(clip.frame_count-1)/double(clip.fps);
    const double ticks=last*fps;
    Require(ticks<=1000000,"output frame budget exceeded");
    // Last grid sample holds the endpoint when the two time grids differ.
    out.frames=std::size_t(std::ceil(ticks-1e-7))+1;
    Require(out.frames*out.joints<=16000000,"output joint sample budget exceeded");
    std::vector<int> mapped(out.joints,-1);std::set<int> used;std::set<std::string> names;
    for(std::size_t j=0;j<out.joints;++j) {
        const auto& joint=profile.joints[j];
        Require(joint.parent>=-1 && joint.parent<int(j),"profile must be parent-first");
        Require(!joint.name.empty() && names.insert(joint.name).second,"duplicate/empty profile joint name");
        Require(IsFinite(joint.alignment) && IsFinite(joint.rest),"nonfinite profile rotation");
        const auto check=[](Quaterniond q) { return std::abs(std::hypot(std::hypot(q.x,q.y),std::hypot(q.z,q.w))-1)<=1e-6; };
        Require(check(joint.alignment) && check(joint.rest),"profile rotations must be unit xyzw");
        Require(joint.source_hid>=-1 && joint.source_hid<=65535,"invalid profile source HID");
        if(joint.source_hid>=0) {
            Require(used.insert(joint.source_hid).second,"duplicate profile source HID");
            for(std::size_t b=0;b<clip.bones.size();++b) if(clip.bones[b].hid==joint.source_hid) {
                Require(!clip.bones[b].head_space_eye,"head-space eye requires a measured target eye adapter; omit this HID explicitly");
                Require(ParentHid(joint.source_hid)!=-2,"unknown HID topology cannot be inferred");
                mapped[j]=int(b);
            }
        }
    }
    std::vector<std::uint8_t> source_valid(clip.bones.size());
    for(std::size_t b=0;b<clip.bones.size();++b) {
        const auto& bone=clip.bones[b];
        source_valid[b]=std::uint8_t(bone.valid && !bone.head_space_eye && ParentHid(bone.hid)!=-2 &&
            (bone.parent<0 || source_valid[bone.parent]));
        if(bone.valid && !used.contains(bone.hid)) out.omitted_hids.push_back(bone.hid);
    }
    out.valid.resize(out.joints);
    for(std::size_t j=0;j<out.joints;++j) {
        const auto& joint=profile.joints[j];
        out.valid[j]=std::uint8_t(mapped[j]>=0 && source_valid[mapped[j]] && (joint.parent<0 || out.valid[joint.parent]));
        if(!out.valid[j] && joint.source_hid>=0) out.missing_hids.push_back(joint.source_hid);
    }
    Require(!out.valid.empty() && out.valid[0],"target root rotation is missing");
    Require(body_only || (out.omitted_hids.empty() && !clip.has_camera && clip.morphs.empty()),
        "unmapped bone/camera/morph channels: use --body-only to explicitly allow omissions (listed in report)");
    Require(profile.target!="vmd" || body_only || out.missing_hids.empty(),"VMD target tracks missing; --body-only permits sparse output");
    out.roots.resize(out.frames);out.rotations.resize(out.frames*out.joints);
    std::vector<Quaternion> sample(clip.bones.size());
    std::vector<Quaterniond> source_world(clip.bones.size()),target_world(out.joints);
    const DenseClipView view{clip.fps,clip.frame_count,clip.bones.size(),clip.roots,clip.rotations};
    Require(bool(ValidateDenseClip(view)),"invalid in-memory clip values");
    for(std::size_t f=0;f<out.frames;++f) {
        // Keep the time-to-index calculation in double. Passing seconds through
        // the float playback API would perturb even source-rate authored keys
        // late in long clips. Interpolation still uses the shared core math.
        const double source_frame=std::min(double(clip.frame_count-1),fps==double(clip.fps) ? double(f):double(f)*double(clip.fps)/fps);
        const auto first=std::size_t(std::floor(source_frame));
        const auto next=std::min(first+1,std::size_t(clip.frame_count-1));
        const auto alpha=float(source_frame-double(first));
        const auto root=alpha==0 ? clip.roots[first]:Vector3::Lerp(clip.roots[first],clip.roots[next],alpha);
        for(std::size_t b=0;b<clip.bones.size();++b) {
            const auto a=clip.rotations[first*clip.bones.size()+b];
            sample[b]=alpha==0 ? a:Quaternion::Slerp(a,clip.rotations[next*clip.bones.size()+b],alpha);
        }
        Vector3d position;Require(bool(basis.Vector({root.x,root.y,root.z},position)),"root conversion failed");
        out.roots[f]=NativeDetail::Add(position,profile.root_offset);
        for(std::size_t b=0;b<clip.bones.size();++b) {
            const auto& q=sample[b];const Quaterniond local=Unit({q.x,q.y,q.z,q.w});
            source_world[b]=clip.bones[b].parent<0 ? local:Product(source_world[clip.bones[b].parent],local);
        }
        for(std::size_t j=0;j<out.joints;++j) {
            if(!out.valid[j]) continue;
            const auto& joint=profile.joints[j];Quaterniond world;
            Require(bool(basis.Rotation(source_world[mapped[j]],world)),"rotation basis conversion failed");
            target_world[j]=Product(world,joint.alignment);
            const auto local=joint.parent<0 ? target_world[j]:Product(NativeDetail::Conjugate(target_world[joint.parent]),target_world[j]);
            out.rotations[f*out.joints+j]=Product(NativeDetail::Conjugate(joint.rest),local);
        }
    }
    return out;
}
} // namespace KineTexture::GkExport
