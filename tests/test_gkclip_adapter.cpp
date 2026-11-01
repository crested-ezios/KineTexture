#include "../tools/gkclip_export/Clip.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <limits>

using namespace KineTexture;
using namespace KineTexture::GkExport;
namespace {
int checks=0;
void Check(bool value,const char* label) {
    ++checks;
    if(!value) throw std::runtime_error(std::string("FAILED: ")+label);
}
template<class F> void Reject(F&& action,const char* label) {
    bool rejected=false;
    try { action(); } catch(const std::runtime_error&) { rejected=true; }
    Check(rejected,label);
}
bool Near(double a,double b,double eps=2e-6) { return std::abs(a-b)<=eps; }
using M=std::array<std::array<double,3>,3>;
constexpr M I{{{1,0,0},{0,1,0},{0,0,1}}};
constexpr M X{{{1,0,0},{0,0,-1},{0,1,0}}};
constexpr M Y{{{0,0,1},{0,1,0},{-1,0,0}}};
constexpr M Z{{{0,-1,0},{1,0,0},{0,0,1}}};
constexpr M Reflection{{{-1,0,0},{0,1,0},{0,0,1}}};
M Mul(M a,M b) {
    M result{};
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k)
        result[i][j]+=a[i][k]*b[k][j];
    return result;
}
M Transpose(M a) {
    M result{};
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) result[i][j]=a[j][i];
    return result;
}
M Matrix(Quaterniond q) {
    const auto [x,y,z,w]=q;
    return {{{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
             {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
             {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}}};
}
bool Same(Quaterniond q,M expected) {
    const M actual=Matrix(q);
    for(int i=0;i<3;++i) for(int j=0;j<3;++j)
        if(!Near(actual[i][j],expected[i][j])) return false;
    return true;
}
Clip AnalyticClip() {
    const float h=std::sqrt(0.5f);
    Clip clip;clip.version=4;clip.fps=30;clip.frame_count=2;
    clip.duration=2.0f/30;clip.root_scale=0.01f;
    clip.bones={{0,"Hips",true,false,-1},{7,"Spine",true,false,0},{8,"Chest",true,false,1}};
    clip.roots={{1,2,3},{2,4,6}};
    clip.rotations={{0,0,h,h},{h,0,0,h},{0,h,0,h},
                    {0,0,h,h},{h,0,0,h},{0,h,0,h}};
    return clip;
}
Profile AnalyticProfile() {
    const double h=std::sqrt(0.5);
    Profile p;p.id="self-authored-reparented-reflection";p.target="vmd";
    p.basis=Reflection;p.meters_per_unit=0.5;p.root_offset={10,20,30};
    p.joints={{0,-1,"root",{0,h,0,h},{h,0,0,h}},
              {8,0,"child",{0,0,h,h},{0,h,0,h}}};
    return p;
}
void ConversionChecks() {
    const Clip c=AnalyticClip();const Profile p=AnalyticProfile();
    const Converted out=Convert(c,p,30,true);
    // Independent matrix algebra: source root Z, child world Z*X*Y;
    // reflection is B*R*B^T; target frame correction postmultiplies.
    const M rootWorld=Mul(Mul(Mul(Reflection,Z),Reflection),Y);
    const M childWorld=Mul(Mul(Mul(Reflection,Mul(Mul(Z,X),Y)),Reflection),Z);
    const M rootDelta=Mul(Transpose(X),rootWorld);
    const M childDelta=Mul(Transpose(Y),Mul(Transpose(rootWorld),childWorld));
    Check(out.frames==2 && out.joints==2,"dense dimensions");
    Check(Same(out.rotations[0],rootDelta),"reflection + nonidentity root alignment/rest order");
    Check(Same(out.rotations[1],childDelta),"source FK + target reparent + nonidentity child alignment/rest");
    Check(Near(out.roots[0].x,8) && Near(out.roots[0].y,24) && Near(out.roots[0].z,36),"root basis/unit/offset");
    Check(Near(out.roots[1].x,6) && Near(out.roots[1].y,28) && Near(out.roots[1].z,42),"root_scale metadata is not multiplied again");
    Check(out.omitted_hids==std::vector<int>{7},"omitted source track reported");
    Reject([&]{Convert(c,p,30,false);},"source track omission requires body-only acknowledgement");
    auto withFace=c;withFace.morphs={"face"};
    auto complete=p;complete.joints.push_back({7,0,"extra",{}, {}});
    Reject([&]{Convert(withFace,complete,30,false);},"morph omission requires acknowledgement");
    auto withCamera=c;withCamera.has_camera=true;
    Reject([&]{Convert(withCamera,complete,30,false);},"camera omission requires acknowledgement");
    auto invalidParent=c;invalidParent.bones[1].valid=false;
    const auto sparse=Convert(invalidParent,p,30,true);
    Check(sparse.valid==std::vector<std::uint8_t>({1,0}) && sparse.missing_hids==std::vector<int>{8},"unavailable ancestor propagates missing instead of measured identity");
    Check(Same(sparse.rotations[1],I),"missing quaternion is identity sentinel with invalid mask");
    Reject([&]{Convert(invalidParent,p,30,false);},"strict VMD rejects missing target track");
    auto absentParent=c;absentParent.bones.erase(absentParent.bones.begin()+1);
    absentParent.bones[1].parent=-1;
    absentParent.rotations={c.rotations[0],c.rotations[2],c.rotations[3],c.rotations[5]};
    auto sourceRoots=p;sourceRoots.joints[1].alignment={};sourceRoots.joints[1].rest={};
    const auto separate=Convert(absentParent,sourceRoots,30,true);
    const M rootAligned=Mul(Mul(Mul(Reflection,Z),Reflection),Y);
    const M childAsRoot=Mul(Mul(Reflection,Y),Reflection);
    Check(Same(separate.rotations[1],Mul(Transpose(rootAligned),childAsRoot)),"absent retained source parent means chain root");
    auto eye=c;eye.bones[2].head_space_eye=true;
    Reject([&]{Convert(eye,p,30,true);},"Head-space eye delta cannot enter body pose mapping");
    auto bad=p;bad.joints[1].parent=1;
    Reject([&]{Convert(c,bad,30,true);},"target self parent rejected");
    bad=p;bad.joints[1].source_hid=0;
    Reject([&]{Convert(c,bad,30,true);},"duplicate source mapping rejected");
    bad=p;bad.basis[0][0]=2;
    Reject([&]{Convert(c,bad,30,true);},"nonorthogonal basis rejected");
    bad=p;bad.joints[1].alignment={0,0,0,2};
    Reject([&]{Convert(c,bad,30,true);},"nonunit alignment rejected");
    bad=p;bad.root_offset.x=std::numeric_limits<double>::infinity();
    Reject([&]{Convert(c,bad,30,true);},"nonfinite root offset rejected");
    Reject([&]{Convert(c,p,0,true);},"zero output FPS rejected");
    bad=p;bad.min_version=5;
    Reject([&]{Convert(c,bad,30,true);},"profile source-version binding rejected");

    Clip timed;timed.version=2;timed.fps=30;timed.frame_count=2;timed.duration=0.0666667f;
    timed.root_scale=100;timed.bones={{0,"Hips",true,false,-1}};
    timed.roots={{0,0,0},{2,4,6}};
    const float h=std::sqrt(0.5f);timed.rotations={{0,0,0,1},{0,0,h,h}};
    Profile timing;timing.id="timing";timing.target="vmd";timing.joints={{0,-1,"root",{}, {}}};
    auto upsampled=Convert(timed,timing,60,false);
    const double s=std::sqrt(0.5);const M Z45{{{s,-s,0},{s,s,0},{0,0,1}}};
    Check(upsampled.frames==3 && Near(upsampled.roots[1].x,1) && Same(upsampled.rotations[1],Z45),"resampling uses seconds and SLERP");
    timed.fps=120;
    const auto downsampled=Convert(timed,timing,30,false);
    Check(downsampled.frames==2 && Near(downsampled.roots.back().x,2) && Same(downsampled.rotations.back(),Z),"non-grid endpoint held without stretching to duration header");
    Profile smpl;smpl.id="self-authored-smplx-order";smpl.target="smplx";
    constexpr int parents[]{-1,0,0,0,1,2,3,4,5,6,7,8,9,9,9,12,13,14,16,17,18,19};
    for(int j=0;j<22;++j)smpl.joints.push_back({j==0 ? 0:-1,parents[j],"joint"+std::to_string(j),{}, {}});
    Check(Convert(timed,smpl,30,false).valid[0]==1,"SMPL-X fixed topology with explicit missing placeholders accepted");
    smpl.joints[2].parent=1;
    Reject([&]{Convert(timed,smpl,30,false);},"SMPL-X custom topology cannot contradict implicit model parents");
}
using Blob=std::vector<std::uint8_t>;
void SourceRatePrecisionCheck() {
    Clip c;c.version=4;c.fps=120;c.frame_count=4097;c.bones={{0,"Hips",true,false,-1}};
    c.rotations.resize(c.frame_count);c.roots.resize(c.frame_count);
    for(unsigned f=0;f<c.frame_count;++f)c.roots[f]={f%2 ? -100.0f:100.0f,float(f),0};
    Profile p;p.id="source-rate-authored-keys";p.target="vmd";p.joints={{0,-1,"hips",{},{}}};
    const auto out=Convert(c,p,120,false);
    bool exact=out.frames==c.frame_count;
    for(unsigned f=0;f<c.frame_count && exact;++f)exact=out.roots[f].x==c.roots[f].x && out.roots[f].y==c.roots[f].y;
    Check(exact,"source-rate export retains every authored root key without float-seconds drift");
}
void U(Blob& b,std::uint32_t value,unsigned width=4) { for(unsigned i=0;i<width;++i) b.push_back(std::uint8_t(value>>(8*i))); }
void F(Blob& b,float value) { U(b,std::bit_cast<std::uint32_t>(value)); }
void SetU(Blob& b,std::size_t offset,std::uint32_t value) { for(unsigned i=0;i<4;++i) b[offset+i]=std::uint8_t(value>>(8*i)); }
Blob Binary(unsigned version) {
    Blob b{'G','K','C','L','I','P','C','L'};
    U(b,version);F(b,30);U(b,2);F(b,2.0f/30);U(b,2);F(b,0.083f);
    U(b,0,2);U(b,1,1);U(b,1,1);b.push_back('a');
    U(b,7,2);U(b,1,1);U(b,1,1);b.push_back('b');
    for(int frame=0;frame<2;++frame) {
        F(b,float(frame));F(b,0);F(b,0);
        for(int joint=0;joint<2;++joint) { F(b,0);F(b,0);F(b,0);F(b,1); }
    }
    U(b,0,1);
    if(version>=4) {
        U(b,1);U(b,4,2);b.insert(b.end(),{'f','a','c','e'});F(b,0);F(b,1);
    }
    return b;
}
void DecoderChecks(const std::filesystem::path& folder) {
    int number=0;
    const auto read=[&](const Blob& data) {
        const auto path=folder/("fixture-"+std::to_string(number++)+".gkclip");
        { std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size())); }
        return ReadClip(path);
    };
    for(unsigned version=1;version<=5;++version) {
        const auto c=read(Binary(version));
        Check(c.version==version && c.frame_count==2 && c.bones[1].parent==0 && c.roots[1].x==1,"v1-v5 little-endian body decoding");
        Check(c.morphs.size()==(version>=4 ? 1u:0u),"versioned morph table decoding");
    }
    auto eye=Binary(5);eye[32]=10;eye[37]=21;eye[40]=3;
    const auto eyes=read(eye);
    Check(eyes.bones[1].head_space_eye && eyes.bones[1].parent==0,"v5 eye representation flag preserved");
    auto camera=Binary(4);camera[130]=1;Blob cameraFrames;
    for(int frame=0;frame<2;++frame) {for(int axis=0;axis<6;++axis)F(cameraFrames,float(axis));F(cameraFrames,45);}
    camera.insert(camera.begin()+131,cameraFrames.begin(),cameraFrames.end());
    Check(read(camera).has_camera,"present dense camera payload decoded before morphs");
    auto b=Binary(4);b.pop_back();Reject([&]{read(b);},"truncated morph payload rejected");
    b=Binary(2);b[6]=b[7]=0;Check(read(b).version==2,"legacy NUL marker recognized without weakening name validation");
    b=Binary(4);b.push_back(0);Reject([&]{read(b);},"trailing payload rejected");
    b=Binary(4);SetU(b,8,6);Reject([&]{read(b);},"unsupported version rejected");
    b=Binary(4);SetU(b,12,0x7f800000);Reject([&]{read(b);},"nonfinite header rejected");
    b=Binary(4);SetU(b,16,0);Reject([&]{read(b);},"zero frame count rejected");
    b=Binary(4);SetU(b,24,257);Reject([&]{read(b);},"bone budget rejected");
    b=Binary(4);b[35]=2;Reject([&]{read(b);},"v4 eye flag rejected");
    b=Binary(5);b[35]=3;Reject([&]{read(b);},"eye representation on non-eye rejected");
    b=Binary(4);b[37]=0;Reject([&]{read(b);},"duplicate HID rejected");
    b=Binary(4);b[41]='a';Reject([&]{read(b);},"duplicate bone name rejected");
    b=Binary(4);b[36]=0;Reject([&]{read(b);},"embedded NUL bone name rejected");
    b=Binary(4);b[32]=7;b[37]=0;Reject([&]{read(b);},"child before existing parent rejected");
    b=Binary(4);SetU(b,42+12+12,0);Reject([&]{read(b);},"nonunit pose quaternion rejected");
    b=Binary(4);b[130]=2;Reject([&]{read(b);},"camera boolean flag rejected");
    b=Binary(4);SetU(b,b.size()-4,std::bit_cast<std::uint32_t>(1.5f));Reject([&]{read(b);},"morph outside unit interval rejected");
    b=Binary(4);b[130]=1;Reject([&]{read(b);},"truncated camera payload rejected");
    b=Binary(2);b.pop_back();Check(read(b).frame_count==2,"legacy camera-tail absence accepted");
    b=Binary(4);b[37]=99;const auto unknown=read(b);
    Profile p;p.id="unknown";p.target="vmd";p.joints={{0,-1,"root",{},{}},{99,0,"unknown",{},{}}};
    Reject([&]{Convert(unknown,p,30,true);},"unknown HID cannot silently infer source topology");
}
}
int main() {
    const auto token=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto folder=std::filesystem::temp_directory_path()/("kinetexture-adapter-test-"+std::to_string(token));
    std::filesystem::create_directory(folder);
    try {
        ConversionChecks();SourceRatePrecisionCheck();DecoderChecks(folder);
        std::filesystem::remove_all(folder);
        std::printf("gkclip adapter: %d checks passed\n",checks);
        return 0;
    } catch(const std::exception& e) {
        std::filesystem::remove_all(folder);
        std::fprintf(stderr,"gkclip adapter: %s\n",e.what());return 1;
    }
}
