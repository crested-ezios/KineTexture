#include "../tools/gkclip_export/Presets.hpp"
#include "../tools/gkclip_export/VmdWriter.hpp"
#include <cstdio>
#include <set>

using namespace KineTexture;
using namespace KineTexture::GkExport;
namespace
{
    int checks=0,failures=0;
    void Check(bool value,const char* label)
    {
        ++checks;
        if(!value) { ++failures;std::fprintf(stderr,"FAIL: %s\n",label); }
    }
    bool Near(Vector3d a,Vector3d b)
    { return NativeDetail::Length(NativeDetail::Sub(a,b))<1e-10; }
    const Joint& Hid(const Profile& p,int hid)
    {
        for(const auto& joint:p.joints) if(joint.source_hid==hid) return joint;
        throw std::runtime_error("missing test HID");
    }
    using Matrix=std::array<std::array<double,3>,3>;
    Matrix Multiply(const Matrix& a,const Matrix& b)
    {
        Matrix result{};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k)
            result[i][j]+=a[i][k]*b[k][j];
        return result;
    }
    Matrix Transpose(const Matrix& a)
    {
        Matrix result{};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) result[i][j]=a[j][i];
        return result;
    }
    Matrix RotationMatrix(Quaterniond q)
    {
        return {{{1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y-q.z*q.w),2*(q.x*q.z+q.y*q.w)},
                 {2*(q.x*q.y+q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z-q.x*q.w)},
                 {2*(q.x*q.z-q.y*q.w),2*(q.y*q.z+q.x*q.w),1-2*(q.x*q.x+q.y*q.y)}}};
    }
    bool Identity(const Matrix& a)
    {
        for(int i=0;i<3;++i) for(int j=0;j<3;++j)
            if(std::abs(a[i][j]-(i==j ? 1.0:0.0))>1e-12) return false;
        return true;
    }
    Matrix IdealForwardK(int hid)
    {
        // Independent signed-axis reference for the EXISTING forward adapter:
        // columns are [primary, hinge, primary cross hinge]. Source hinge is
        // +X in GK coordinates, target game hinge +Z. No AlignBoneFrames or
        // preset helper is used to construct this oracle.
        constexpr Matrix sourceLeg{{{0,1,0},{-1,0,0},{0,0,1}}}; // -Y,+X,+Z
        constexpr Matrix sourceFoot{{{0,1,0},{0,0,1},{1,0,0}}}; // +Z,+X,+Y
        constexpr Matrix gameLeft{{{-1,0,0},{0,0,1},{0,1,0}}}; // -X,+Z,+Y
        constexpr Matrix gameRight{{{1,0,0},{0,0,-1},{0,1,0}}}; // +X,+Z,-Y
        const bool left=hid==1 || hid==3 || hid==5 || hid==19;
        return Multiply(hid<=4 ? sourceLeg:sourceFoot,Transpose(left ? gameLeft:gameRight));
    }
}
int main()
{
    for(const char* name:{"canonical-mmd","canonical-smpl","canonical-smplx"})
    {
        const auto p=MakePreset(name);
        const bool mmd=p.target=="vmd";
        Check(p.approximate,"built-in profile explicitly approximate");
        Check(p.min_version==2,"legacy v1 frame semantics not silently accepted");
        std::set<int> ids;
        std::set<std::string> names;
        for(std::size_t i=0;i<p.joints.size();++i)
        {
            const auto& joint=p.joints[i];
            Check(joint.source_hid<0 || ids.insert(joint.source_hid).second,"source mappings unique");
            Check(names.insert(joint.name).second,"target names unique");
            Check(joint.parent<int(i) && joint.parent>=-1,"parents precede children");
        }
        // Expected vectors are independent analytic frame statements. Both
        // bases reflect GK's left -X into target +X. An axial GK hinge +Z
        // becomes target -Z. The forward converter's source hinge +X_GK
        // becomes -X_MMD under its proper basis rotation; under the reflected
        // SMPL basis the axial-vector parity leaves it +X_SMPL.
        for(int hid:{1,2,3,4})
        {
            const auto q=Hid(p,hid).alignment;
            Check(Near(NativeDetail::Rotate(q,{0,-1,0}),{hid%2 ? 1.0:-1.0,0,0}),
                "leg primary axis maps into basis-converted GK axis");
            Check(Near(NativeDetail::Rotate(q,{mmd ? -1.0:1.0,0,0}),{0,0,-1}),
                "leg bend plane includes axial reflection parity");
        }
        for(int hid:{5,6,19,20})
        {
            const auto q=Hid(p,hid).alignment;
            Check(Near(NativeDetail::Rotate(q,{0,0,mmd ? -1.0:1.0}),
                {(hid==5 || hid==19) ? 1.0:-1.0,0,0}),"foot primary axis corrected");
            Check(Near(NativeDetail::Rotate(q,{mmd ? -1.0:1.0,0,0}),{0,0,-1}),"foot bend plane corrected");
        }
        for(int hid:{1,2,3,4,5,6,19,20})
        {
            // Ideal source-neutral world I is emitted by the forward path as
            // K. Reverse export must recover I, including roll. The original
            // MMD +X-hinge preset failed this with exactly 180 degrees on all
            // eight leg/foot/toe joints despite passing axis-only tests.
            const auto emitted=IdealForwardK(hid);
            const auto converted=Multiply(Multiply(p.basis,emitted),Transpose(p.basis));
            Check(Identity(Multiply(converted,RotationMatrix(Hid(p,hid).alignment))),
                "independent ideal source neutral forward-K then preset inverse recovers identity");
        }
        for(int hid:{11,13,15,17})
            Check(Near(NativeDetail::Rotate(Hid(p,hid).alignment,{0,1,0}),{0,1,0}),"ideal left arm frame");
        for(int hid:{12,14,16,18})
            Check(Near(NativeDetail::Rotate(Hid(p,hid).alignment,{0,1,0}),{0,-1,0}),"mirrored right arm roll removed");
        if(mmd)
        {
            Check(p.joints.size()==52,"MMD maps complete 52-bone body/finger subset");
            Check(Hid(p,0).parent==-1 && Hid(p,7).parent==-1,"MMD upper and lower torsos are siblings");
            Check(Hid(p,8).parent==PresetDetail::Index(p.joints,7),"MMD upper torso chain");
            Check(Near({p.meters_per_unit,0,0},{0.083,0,0}),"MMD approximate unit scale explicit");
            VmdAnimation animation;animation.model_name_cp932="KineTexture";
            animation.bones.push_back({p.root_track,{{0,{},{}}}});
            for(const auto& joint:p.joints) animation.bones.push_back({joint.name,{{0,{},{}}}});
            Check(!SerializeVmd(animation).empty(),"MMD CP932 names accepted without truncation/collisions");
            Check(Hid(p,24).name==std::string("\x8d\xb6\x90\x65\x8e\x77\x82\x4f"),"MMD thumb uses fullwidth zero");
            Check(Hid(p,29).name==std::string("\x8d\xb6\x90\x6c\x8e\x77\x82\x52"),"MMD index uses fullwidth third segment");
        }
        else
        {
            constexpr int expectedIds[]{0,1,2,7,3,4,8,5,6,54,19,20,9,11,12,10,13,14,15,16,17,18};
            for(int i=0;i<22;++i) Check(p.joints[i].source_hid==expectedIds[i],"standard SMPL body order");
            Check(p.joints.size()==(p.target=="smpl" ? 24u:22u),"SMPL target parameter joint count");
            Check(p.meters_per_unit==1,"SMPL translation unit is meters");
            if(p.target=="smpl")
            {
                Check(p.joints[22].source_hid==-1 && p.joints[23].source_hid==-1,"terminal hands explicitly missing");
                Check(p.joints[22].parent==20 && p.joints[23].parent==21,"terminal hand target parents");
            }
        }
    }
    bool rejected=false;
    try { (void)MakePreset("unknown"); } catch(const std::runtime_error&) { rejected=true; }
    Check(rejected,"unknown preset rejected");
    std::printf("Export presets: %d checks, %d failures\n",checks,failures);
    return failures ? 1:0;
}
