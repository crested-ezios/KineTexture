#include "../tools/gkclip_export/SmplWriter.hpp"
#include <bit>
#include <cstdio>
#include <fstream>
#include <map>
#include <numbers>

using namespace KineTexture;
using namespace KineTexture::GkExport;

static int failures=0;
static void Check(bool condition,const char* message)
{ if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); ++failures; } }
static bool Near(double a,double b,double tolerance=1e-11) { return std::abs(a-b)<tolerance; }
static SmplMotion Fixture(std::size_t count=24)
{
    SmplMotion result;
    result.fps=60; result.frame_count=2; result.joint_count=count;
    result.local_rotations.resize(2*count);
    result.translations={{1,2,3},{-4,5,6}};
    result.joint_valid.assign(count,1);
    // Input quaternion signs should not affect exported shortest rotation vectors.
    result.local_rotations[0]={0,0,std::sqrt(.5),std::sqrt(.5)};
    result.local_rotations[count]={0,0,-std::sqrt(.5),-std::sqrt(.5)};
    result.local_rotations[1]={-1,0,0,0};
    result.local_rotations[count+1]={1,0,0,0};
    result.local_rotations[2]={0,1e-15,0,1};
    const double beforePi=std::numbers::pi-1e-8, afterPi=std::numbers::pi+1e-8;
    result.local_rotations[3]={0,std::sin(beforePi/2),0,std::cos(beforePi/2)};
    result.local_rotations[4]={0,std::sin(afterPi/2),0,std::cos(afterPi/2)};
    if (count==24) { result.joint_valid[23]=0; result.local_rotations[23]={NAN,NAN,NAN,NAN}; }
    return result;
}

// Independent minimal test reader: local ZIP entries + NPY header/data. Does not
// call writer internals, nor infer offsets from an expected serializer layout.
struct Array { std::string header; std::vector<std::uint8_t> data; };
static std::uint32_t Read(const std::vector<std::uint8_t>& bytes,std::size_t offset,int width)
{
    if (offset+width>bytes.size()) throw std::runtime_error("truncated test archive");
    std::uint32_t result=0;
    for (int i=0;i<width;++i) result |= static_cast<std::uint32_t>(bytes[offset+i])<<(8*i);
    return result;
}
static std::map<std::string,Array> ReadArchive(const std::vector<std::uint8_t>& bytes)
{
    std::map<std::string,Array> result;
    std::size_t offset=0;
    while (Read(bytes,offset,4)==0x04034b50)
    {
        Check(Read(bytes,offset+8,2)==0,"NPZ method must be stored");
        const std::size_t size=Read(bytes,offset+18,4), nameLength=Read(bytes,offset+26,2), extra=Read(bytes,offset+28,2);
        std::string name(bytes.begin()+offset+30,bytes.begin()+offset+30+nameLength);
        const auto start=offset+30+nameLength+extra;
        if (start+size>bytes.size()) throw std::runtime_error("test ZIP entry size");
        Check(bytes[start]==0x93 && std::string(bytes.begin()+start+1,bytes.begin()+start+6)=="NUMPY","NPY magic");
        const auto headerLength=Read(bytes,start+8,2);
        const auto dataStart=start+10+headerLength;
        if (dataStart>start+size) throw std::runtime_error("test NPY header size");
        result.emplace(name,Array{std::string(bytes.begin()+start+10,bytes.begin()+dataStart),
                                 {bytes.begin()+dataStart,bytes.begin()+start+size}});
        offset=start+size;
    }
    Check(Read(bytes,offset,4)==0x02014b50,"ZIP central directory exists");
    Check(Read(bytes,bytes.size()-22,4)==0x06054b50,"ZIP end directory exists");
    Check(Read(bytes,bytes.size()-6,4)==offset,"ZIP directory offset");
    return result;
}
static std::vector<double> Values(const Array& value)
{
    Check(value.header.find("'<f8'")!=std::string::npos,"NPY doubles have explicit little-endian descriptor");
    if (value.data.size()%8) throw std::runtime_error("test NPY scalar width");
    std::vector<double> result;
    for (std::size_t i=0;i<value.data.size();i+=8)
    {
        std::uint64_t bits=0;
        for (int b=0;b<8;++b) bits |= static_cast<std::uint64_t>(value.data[i+b])<<(8*b);
        result.push_back(std::bit_cast<double>(bits));
    }
    return result;
}
template<class F> static void Reject(F operation,const char* message)
{
    bool threw=false; try { operation(); } catch (const std::runtime_error&) { threw=true; }
    Check(threw,message);
}
int main(int argc,char** argv)
{
    const auto motion=Fixture();
    const auto npz=SerializeSmplNpz(motion);
    const auto arrays=ReadArchive(npz);
    Check(arrays.size()==9,"unknown betas are absent");
    Check(arrays.at("poses.npy").header.find("(2, 72)")!=std::string::npos,"SMPL pose dimensions");
    Check(arrays.at("body_pose.npy").header.find("(2, 69)")!=std::string::npos,"SMPL body dimensions");
    const auto poses=Values(arrays.at("poses.npy"));
    Check(poses.size()==144,"SMPL pose element count");
    Check(Near(poses[2],std::numbers::pi/2) && Near(poses[74],poses[2]),"q versus negative q yields same 90-degree root");
    Check(Near(poses[3],std::numbers::pi) && Near(poses[75],poses[3]),"pi rotation is sign deterministic");
    Check(Near(poses[7],2e-15,1e-18),"small angle retains finite displacement");
    Check(Near(poses[10],std::numbers::pi-1e-8),"rotation just below pi");
    Check(Near(poses[13],-std::numbers::pi+1e-8),"rotation just above pi takes equivalent shortest arc");
    Check(poses[69]==0 && poses[70]==0 && poses[71]==0,"unavailable joint stored neutral");
    Check(arrays.at("joint_valid.npy").data.size()==24 && arrays.at("joint_valid.npy").data[23]==0,"missing is separately masked");
    const auto global=Values(arrays.at("global_orient.npy")), body=Values(arrays.at("body_pose.npy"));
    Check(global.size()==6 && body.size()==138,"global/body split element count");
    Check(Near(global[2],poses[2]) && Near(body[0],poses[3]) && Near(body[69],poses[75]),"global/body arrays match full poses");
    const auto translations=Values(arrays.at("transl.npy"));
    Check(translations==std::vector<double>({1,2,3,-4,5,6}),"translations retained in meters");
    Check(Values(arrays.at("fps.npy"))[0]==60 && Values(arrays.at("mocap_framerate.npy"))[0]==60,"FPS aliases");
    Check(Near(Values(arrays.at("time_seconds.npy"))[1],1./60),"seconds timebase");
    auto withShape=motion; withShape.betas={.1,-.2,.3};
    const auto shapeArrays=ReadArchive(SerializeSmplNpz(withShape));
    Check(shapeArrays.at("betas.npy").header.find("(1, 3)")!=std::string::npos,"supplied shape dimensions");
    Check(Values(shapeArrays.at("betas.npy"))==withShape.betas,"supplied shape values");
    const auto smplx=ReadArchive(SerializeSmplNpz(Fixture(22)));
    Check(smplx.at("poses.npy").header.find("(2, 66)")!=std::string::npos,"SMPL-X body pose dimensions");
    Check(smplx.at("body_pose.npy").header.find("(2, 63)")!=std::string::npos,"SMPL-X body dimensions");
    Check(std::string(smplx.at("model_type.npy").data.begin(),smplx.at("model_type.npy").data.end())=="smplx","model type explicit");
    auto invalid=motion; invalid.fps=NAN; Reject([&]{SerializeSmplNpz(invalid);},"reject NaN FPS");
    invalid=motion; invalid.fps=0; Reject([&]{SerializeSmplNpz(invalid);},"reject zero FPS");
    invalid=motion; invalid.fps=std::numeric_limits<double>::denorm_min(); Reject([&]{SerializeSmplNpz(invalid);},"reject nonfinite seconds timebase");
    invalid=motion; invalid.local_rotations.pop_back(); Reject([&]{SerializeSmplNpz(invalid);},"reject wrong rotation count");
    invalid=motion; invalid.local_rotations[0]={0,0,0,0}; Reject([&]{SerializeSmplNpz(invalid);},"reject valid zero quaternion");
    invalid=motion; invalid.translations[0].x=INFINITY; Reject([&]{SerializeSmplNpz(invalid);},"reject nonfinite translation");
    invalid=motion; invalid.joint_valid[0]=0; Reject([&]{SerializeSmplNpz(invalid);},"reject unavailable root");
    invalid=motion; invalid.joint_valid[3]=2; Reject([&]{SerializeSmplNpz(invalid);},"reject non-binary mask");
    invalid=motion; invalid.betas={NAN}; Reject([&]{SerializeSmplNpz(invalid);},"reject nonfinite supplied shape");
    invalid=motion; invalid.joint_count=23; Reject([&]{SerializeSmplNpz(invalid);},"reject unsupported model joint count");
    if (argc==3 && std::string(argv[1])=="--write-fixture")
    {
        std::ofstream out(argv[2],std::ios::binary);
        out.write(reinterpret_cast<const char*>(npz.data()),static_cast<std::streamsize>(npz.size()));
        Check(static_cast<bool>(out),"fixture written");
    }
    std::printf("SMPL export checks: %s\n",failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
