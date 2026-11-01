#pragma once

#include "Clip.hpp"

namespace KineTexture::GkExport
{
    namespace PresetDetail
    {
        // These development presets use self-authored cardinal axes. They do
        // not contain extracted rest joints, fitted MMD source constants,
        // character geometry or SMPL model weights. A measured profile is
        // required for a particular rig; these presets are approximations.
        inline Quaterniond Alignment(const Matrix3d& basis,Vector3d sourcePrimary,
            Vector3d targetPrimary,Vector3d targetHinge)
        {
            const auto primary=NativeDetail::Transform(basis,sourcePrimary);
            const double determinant=basis[0][0]*basis[1][1]*basis[2][2];
            // Hinge directions are axial vectors, unlike segment directions.
            const auto hinge=NativeDetail::Scale(NativeDetail::Transform(basis,{0,0,1}),determinant);
            Quaterniond result;
            Require(bool(AlignBoneFrames(primary,hinge,targetPrimary,targetHinge,result)),
                "invalid built-in development bone frame");
            return result;
        }
        inline Quaterniond Frame(int hid,const Matrix3d& basis,bool mmd)
        {
            // The forward MMD adapter's hinge is +X in GK coordinates.
            // Its inverse basis diag(-1,+1,-1) sends that axial direction
            // to -X in MMD. The reflected SMPL basis instead sends it to +X.
            if(hid>=1 && hid<=4)
                return Alignment(basis,{hid%2 ? -1.0:1.0,0,0},{0,-1,0},{mmd ? -1.0:1.0,0,0});
            if(hid==5 || hid==6 || hid==19 || hid==20)
                return Alignment(basis,{(hid==5 || hid==19) ? -1.0:1.0,0,0},
                    {0,0,mmd ? -1.0:1.0},{mmd ? -1.0:1.0,0,0});
            // The mirrored right arm frame uses an X half-turn. The same
            // world correction applies to its shoulder, arm, wrist/fingers.
            const bool right=(hid==12 || hid==14 || hid==16 || hid==18 || (hid>=39 && hid<=53));
            Quaterniond frame=right ? Quaterniond{1,0,0,0}:Quaterniond{};
            if((hid>=24 && hid<=26) || (hid>=39 && hid<=41))
            {
                // Ideal thumb bend-plane quarter-turn, no fitted spread.
                constexpr double rootHalf=0.7071067811865475244;
                frame=Product(frame,Quaterniond{mmd ? rootHalf:-rootHalf,0,0,rootHalf});
            }
            return frame;
        }
        inline int Index(const std::vector<Joint>& joints,int hid)
        {
            for(std::size_t i=0;i<joints.size();++i) if(joints[i].source_hid==hid) return int(i);
            return -1;
        }
    }

    inline Profile MakePreset(const std::string& name)
    {
        Require(name=="canonical-mmd" || name=="canonical-smpl" || name=="canonical-smplx",
            "unknown preset (expected canonical-mmd, canonical-smpl or canonical-smplx)");
        Profile p;
        p.approximate=true;
        p.min_version=2;
        const bool mmd=name=="canonical-mmd";
        p.id=name+(mmd ? "-approx/2":"-approx/1");
        p.target=mmd ? "vmd":(name=="canonical-smpl" ? "smpl":"smplx");
        p.basis={{{-1,0,0},{0,1,0},{0,0,mmd ? -1.0:1.0}}};
        auto add=[&](int hid,int parent,const std::string& jointName)
        {
            p.joints.push_back({hid,parent,jointName,PresetDetail::Frame(hid,p.basis,mmd),{}});
        };
        if(!mmd)
        {
            // SMPL-X's 22 body joints share this order with the first 22 of
            // SMPL. SMPL's two terminal hands have no corresponding GK joint;
            // explicit missing slots preserve that fact in the NPZ mask.
            constexpr int ids[]{0,1,2,7,3,4,8,5,6,54,19,20,9,11,12,10,13,14,15,16,17,18};
            constexpr int parents[]{-1,0,0,0,1,2,3,4,5,6,7,8,9,9,9,12,13,14,16,17,18,19};
            constexpr const char* names[]{"pelvis","left_hip","right_hip","spine1","left_knee","right_knee",
                "spine2","left_ankle","right_ankle","spine3","left_foot","right_foot","neck","left_collar",
                "right_collar","head","left_shoulder","right_shoulder","left_elbow","right_elbow","left_wrist","right_wrist"};
            for(int i=0;i<22;++i) add(ids[i],parents[i],names[i]);
            if(p.target=="smpl")
            {
                add(-1,20,"left_hand");
                add(-1,21,"right_hand");
            }
            return p;
        }
        // Explicit approximate MMD units, independent of the gkclip header's
        // root_scale provenance. Change the profile for a different model.
        p.meters_per_unit=0.083;
        p.root_track="\x91\x53\x82\xc4\x82\xcc\x90\x65"; // full-parent
        // Names are exact CP932 byte fields, never locale-dependent literals.
        // The upper and lower torso are siblings beneath the root controller.
        add(0,-1,"\x89\xba\x94\xbc\x90\x67");
        add(7,-1,"\x8f\xe3\x94\xbc\x90\x67");
        add(8,1,"\x8f\xe3\x94\xbc\x90\x67" "2");
        add(54,2,"\x8f\xe3\x94\xbc\x90\x67" "3");
        add(9,3,"\x8e\xf1");
        add(10,4,"\x93\xaa");
        add(11,3,"\x8d\xb6\x8c\xa8");
        add(12,3,"\x89\x45\x8c\xa8");
        add(13,6,"\x8d\xb6\x98\x72");
        add(14,7,"\x89\x45\x98\x72");
        add(15,8,"\x8d\xb6\x82\xd0\x82\xb6");
        add(16,9,"\x89\x45\x82\xd0\x82\xb6");
        add(17,10,"\x8d\xb6\x8e\xe8\x8e\xf1");
        add(18,11,"\x89\x45\x8e\xe8\x8e\xf1");
        add(1,0,"\x8d\xb6\x91\xab");
        add(2,0,"\x89\x45\x91\xab");
        add(3,14,"\x8d\xb6\x82\xd0\x82\xb4");
        add(4,15,"\x89\x45\x82\xd0\x82\xb4");
        add(5,16,"\x8d\xb6\x91\xab\x8e\xf1");
        add(6,17,"\x89\x45\x91\xab\x8e\xf1");
        add(19,18,"\x8d\xb6\x82\xc2\x82\xdc\x90\xe6");
        add(20,19,"\x89\x45\x82\xc2\x82\xdc\x90\xe6");
        const std::string stems[]{"\x90\x65\x8e\x77","\x90\x6c\x8e\x77","\x92\x86\x8e\x77",
            "\x96\xf2\x8e\x77","\x8f\xac\x8e\x77"};
        const std::string digits[]{"\x82\x4f","\x82\x50","\x82\x51","\x82\x52"};
        for(int side=0;side<2;++side)
        {
            const std::string prefix=side ? "\x89\x45":"\x8d\xb6";
            for(int finger=0;finger<5;++finger)
            {
                const int first=(side ? 39:24)+3*finger;
                int parent=PresetDetail::Index(p.joints,side ? 18:17);
                for(int part=0;part<3;++part)
                {
                    add(first+part,parent,prefix+stems[finger]+digits[part+(finger ? 1:0)]);
                    parent=int(p.joints.size()-1);
                }
            }
        }
        return p;
    }
}
