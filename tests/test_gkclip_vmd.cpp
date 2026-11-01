#include "../tools/gkclip_export/VmdWriter.hpp"
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>

using namespace KineTexture;
using namespace KineTexture::GkExport;
namespace
{
    int checks=0,failures=0;
    void Check(bool value,const char* label)
    {
        ++checks;
        if (!value) { ++failures; std::fprintf(stderr,"FAIL: %s\n",label); }
    }
    void Reject(const std::function<void()>& action,const char* label)
    {
        bool rejected=false;
        try { action(); } catch (const std::invalid_argument&) { rejected=true; }
        Check(rejected,label);
    }
    struct Reader
    {
        const std::vector<std::uint8_t>& data;
        std::size_t offset{};
        std::uint32_t U32()
        {
            const auto p=offset; offset+=4;
            return std::uint32_t(data.at(p)) | std::uint32_t(data.at(p+1))<<8 |
                std::uint32_t(data.at(p+2))<<16 | std::uint32_t(data.at(p+3))<<24;
        }
        float Float() { return std::bit_cast<float>(U32()); }
        std::string Text(std::size_t size)
        {
            const auto p=offset; offset+=size;
            std::string value;
            for (std::size_t i=0;i<size && data.at(p+i);++i) value+=static_cast<char>(data.at(p+i));
            return value;
        }
    };
    bool Near(double a,double b) { return std::abs(a-b)<1e-6; }
    VmdAnimation Fixture()
    {
        constexpr double halfPi=1.57079632679489661923;
        const auto q=Quaterniond{0,0,std::sin(halfPi/2),std::cos(halfPi/2)};
        return {"KineTexture",{
            {"Root",{{0,{0,0,0},{}},{2,{2,-1,0.5},q}}},
            {"Child",{{0,{0,0,0},{}},{2,{0,0,0},{-q.x,-q.y,-q.z,-q.w}}}}},
            {"LegIK"}};
    }
}

int main(int argc,char** argv)
{
    auto fixture=Fixture();
    const auto bytes=SerializeVmd(fixture);
    Check(bytes.size()==548,"exact VMD body/IK-property size");
    Reader reader{bytes};
    Check(reader.Text(30)=="Vocaloid Motion Data 0002","VMD 0002 header");
    Check(reader.Text(20)=="KineTexture","model field");
    Check(reader.U32()==4,"bone key count");
    const char* expectedNames[]{"Root","Root","Child","Child"};
    for (int key=0;key<4;++key)
    {
        Check(reader.Text(15)==expectedNames[key],"fixed bone-name field");
        Check(reader.U32()==static_cast<unsigned>(key%2*2),"frame indices retained");
        const double x=reader.Float(),y=reader.Float(),z=reader.Float();
        Check(Near(x,key==1 ? 2 : 0) && Near(y,key==1 ? -1 : 0) && Near(z,key==1 ? 0.5 : 0),
            "translations serialized little endian without unit inference");
        const double qx=reader.Float(),qy=reader.Float(),qz=reader.Float(),qw=reader.Float();
        Check(Near(qx,0) && Near(qy,0),"quaternion xy components");
        Check(Near(qz,key%2 ? std::sqrt(0.5) : 0) && Near(qw,key%2 ? std::sqrt(0.5) : 1),
            "unit quaternion sign continuity");
        const auto controls=reader.offset;
        for (int axis=0;axis<4;++axis)
        {
            Check(bytes.at(controls+axis)==20 && bytes.at(controls+axis+4)==20 &&
                bytes.at(controls+axis+8)==107 && bytes.at(controls+axis+12)==107,
                "each coordinate and rotation Bezier curve is linear");
        }
        reader.offset+=64;
    }
    for (int section=0;section<4;++section) Check(reader.U32()==0,"no implicit morph/camera/light/shadow data");
    Check(reader.U32()==1,"one explicit property key");
    Check(reader.U32()==0 && bytes.at(reader.offset++)==1,"frame zero model visibility");
    Check(reader.U32()==1 && reader.Text(20)=="LegIK" && bytes.at(reader.offset++)==0,"explicit IK disabled");
    Check(reader.offset==bytes.size(),"reader consumes complete VMD");

    auto absent=fixture;
    absent.disabled_ik_cp932.clear();
    Check(SerializeVmd(absent).size()==518,"IK property absent unless requested");
    auto unicode=absent;
    unicode.bones[0].name_cp932="\x82\xa0"; // valid two-byte CP932 hiragana
    Check(SerializeVmd(unicode).at(54)==0x82 && SerializeVmd(unicode).at(55)==0xa0,"CP932 bytes retained");
    auto limit=absent;
    limit.bones[0].name_cp932=std::string(15,'A');
    Check(!SerializeVmd(limit).empty(),"full 15-byte VMD bone field accepted without truncation");

    auto reject=[&](const std::function<void(VmdAnimation&)>& change,const char* label)
    {
        auto copy=fixture; change(copy);
        Reject([&]{(void)SerializeVmd(copy);},label);
    };
    reject([](auto& x){x.bones[0].name_cp932=std::string(16,'x');},"long name rejected");
    reject([](auto& x){x.bones[0].name_cp932=std::string("A\0B",3);},"embedded NUL rejected");
    reject([](auto& x){x.bones[0].name_cp932="\x82";},"truncated CP932 character rejected");
    reject([](auto& x){x.bones[0].name_cp932="\x82\x20";},"invalid CP932 trail rejected");
    reject([](auto& x){x.bones[0].name_cp932="\x1f";},"control name rejected");
    reject([](auto& x){x.model_name_cp932.clear();},"empty model rejected");
    reject([](auto& x){x.bones[1].name_cp932=x.bones[0].name_cp932;},"duplicate track rejected");
    reject([](auto& x){x.bones[0].keys.clear();},"empty track rejected");
    reject([](auto& x){x.bones[0].keys[1].frame=0;},"duplicate/unsorted frame rejected");
    reject([](auto& x){x.bones[0].keys[0].translation.x=std::numeric_limits<double>::infinity();},"infinite translation rejected");
    reject([](auto& x){x.bones[0].keys[0].translation.y=std::numeric_limits<double>::max();},"float32 translation overflow rejected");
    reject([](auto& x){x.bones[0].keys[0].rotation={0,0,0,0};},"zero quaternion rejected");
    reject([](auto& x){x.bones[0].keys[0].rotation.w=2;},"nonunit quaternion rejected");
    reject([](auto& x){x.bones[0].keys[0].rotation.x=std::numeric_limits<double>::quiet_NaN();},"NaN quaternion rejected");
    reject([](auto& x){x.disabled_ik_cp932.push_back(x.disabled_ik_cp932[0]);},"duplicate IK rejected");
    reject([](auto& x){x.bones.clear();},"empty animation rejected");
    reject([](auto& x){x.bones.resize(257,x.bones[0]);},"bone-track budget enforced");
    reject([](auto& x){x.disabled_ik_cp932.resize(257,"IK");},"IK switch budget enforced");
    if (argc==3 && std::string(argv[1])=="--write-fixture")
    {
        std::ofstream output(argv[2],std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        Check(bool(output),"write fixture for independent parser");
    }
    std::printf("VMD writer: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
