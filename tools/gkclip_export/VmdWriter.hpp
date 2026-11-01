#pragma once

// Development-only VMD container adapter. The caller supplies already baked
// target-local channels and encoded CP932 names. No skeleton, coordinate,
// timebase, IK, append or facial policy is inferred here.
#include <KineTexture/Retarget.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace KineTexture::GkExport
{
    struct VmdBoneKey
    {
        std::uint32_t frame{};
        Vector3d translation{};
        Quaterniond rotation{};
    };
    struct VmdBoneTrack
    {
        std::string name_cp932;
        std::vector<VmdBoneKey> keys;
    };
    struct VmdAnimation
    {
        std::string model_name_cp932;
        std::vector<VmdBoneTrack> bones;
        std::vector<std::string> disabled_ik_cp932;
    };

    namespace VmdDetail
    {
        // Conservative CP932 syntax validation. Conversion from UTF-8 belongs
        // to the CLI/profile reader; names are never truncated or substituted.
        inline void Name(std::string_view text, std::size_t limit)
        {
            if (text.empty() || text.size()>limit)
                throw std::invalid_argument("VMD CP932 name is empty or exceeds its byte field");
            for (std::size_t i=0;i<text.size();++i)
            {
                const auto value=static_cast<unsigned char>(text[i]);
                if ((value>=0x20 && value<=0x7e) || (value>=0xa1 && value<=0xdf)) continue;
                if (!((value>=0x81 && value<=0x9f) || (value>=0xe0 && value<=0xfc)) || ++i==text.size())
                    throw std::invalid_argument("invalid/control/embedded-NUL VMD CP932 name");
                const auto trail=static_cast<unsigned char>(text[i]);
                if (!((trail>=0x40 && trail<=0x7e) || (trail>=0x80 && trail<=0xfc)))
                    throw std::invalid_argument("invalid VMD CP932 trail byte");
            }
        }
        inline void U32(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (unsigned i=0;i<4;++i) out.push_back(static_cast<std::uint8_t>(value>>(8*i)));
        }
        inline void Float(std::vector<std::uint8_t>& out, double value)
        {
            if (!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
                throw std::invalid_argument("VMD scalar exceeds finite float32 range");
            U32(out,std::bit_cast<std::uint32_t>(static_cast<float>(value)));
        }
        inline void Field(std::vector<std::uint8_t>& out, std::string_view text, std::size_t width)
        {
            out.insert(out.end(),text.begin(),text.end());
            out.insert(out.end(),width-text.size(),0);
        }
    }

    // Dense keys retain their supplied frame indices. VMD has no FPS field;
    // callers must explicitly resample into the intended viewer timebase.
    // A new vector is returned only after successful serialization.
    inline std::vector<std::uint8_t> SerializeVmd(const VmdAnimation& animation)
    {
        constexpr std::size_t MaxKeys=2'000'000;
        constexpr std::size_t MaxTracks=256;
        VmdDetail::Name(animation.model_name_cp932,20);
        if (animation.bones.empty() || animation.bones.size()>MaxTracks)
            throw std::invalid_argument("VMD requires 1..256 explicit bone tracks");
        if (animation.disabled_ik_cp932.size()>256)
            throw std::invalid_argument("VMD IK switch budget exceeded");
        std::set<std::string> names;
        std::size_t count=0;
        for (const auto& track:animation.bones)
        {
            VmdDetail::Name(track.name_cp932,15);
            if (!names.insert(track.name_cp932).second || track.keys.empty())
                throw std::invalid_argument("duplicate or empty VMD bone track");
            if (track.keys.size()>MaxKeys-count)
                throw std::invalid_argument("VMD bone-key budget exceeded");
            count+=track.keys.size();
            bool first=true;
            std::uint32_t previous=0;
            for (const auto& key:track.keys)
            {
                if (!first && key.frame<=previous)
                    throw std::invalid_argument("VMD bone keys must have strictly increasing frame indices");
                first=false; previous=key.frame;
                const auto& q=key.rotation;
                const double norm=std::hypot(std::hypot(q.x,q.y),std::hypot(q.z,q.w));
                if (!IsFinite(q) || !std::isfinite(norm) || std::abs(norm-1)>0.001)
                    throw std::invalid_argument("VMD quaternion must be finite and near unit length");
                for (double value:{key.translation.x,key.translation.y,key.translation.z})
                    if (!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
                        throw std::invalid_argument("VMD position exceeds finite float32 range");
            }
        }
        names.clear();
        for (const auto& name:animation.disabled_ik_cp932)
        {
            VmdDetail::Name(name,20);
            if (!names.insert(name).second) throw std::invalid_argument("duplicate VMD IK switch name");
        }
        std::vector<std::uint8_t> out;
        out.reserve(74+111*count+(animation.disabled_ik_cp932.empty() ? 0 : 9+21*animation.disabled_ik_cp932.size()));
        VmdDetail::Field(out,"Vocaloid Motion Data 0002",30);
        VmdDetail::Field(out,animation.model_name_cp932,20);
        VmdDetail::U32(out,static_cast<std::uint32_t>(count));
        // Four coordinate/rotation curves, all linear. The redundant VMD
        // blocks follow the same diagonal controls as the first block.
        constexpr std::array<std::uint8_t,64> linear{{
            20,20,20,20,20,20,20,20,107,107,107,107,107,107,107,107,
            20,20,20,20,20,20,20,107,107,107,107,107,107,107,0,0,
            20,20,20,20,20,20,107,107,107,107,107,107,0,0,0,0,
            20,20,20,20,20,107,107,107,107,107,0,0,0,0,0,0}};
        for (const auto& track:animation.bones)
        {
            Quaterniond previous{};
            bool first=true;
            for (const auto& key:track.keys)
            {
                VmdDetail::Field(out,track.name_cp932,15);
                VmdDetail::U32(out,key.frame);
                VmdDetail::Float(out,key.translation.x);
                VmdDetail::Float(out,key.translation.y);
                VmdDetail::Float(out,key.translation.z);
                Quaterniond q;
                NativeDetail::Normalize(key.rotation,q);
                // q and -q are equivalent; continuity avoids a viewer taking
                // an unnecessary long arc between adjacent exported keys.
                if (!first && previous.x*q.x+previous.y*q.y+previous.z*q.z+previous.w*q.w<0)
                    q={-q.x,-q.y,-q.z,-q.w};
                previous=q; first=false;
                VmdDetail::Float(out,q.x); VmdDetail::Float(out,q.y);
                VmdDetail::Float(out,q.z); VmdDetail::Float(out,q.w);
                out.insert(out.end(),linear.begin(),linear.end());
            }
        }
        VmdDetail::U32(out,0); // morphs: absent in this explicit body contract
        VmdDetail::U32(out,0); // cameras
        VmdDetail::U32(out,0); // lights
        VmdDetail::U32(out,0); // shadows
        VmdDetail::U32(out,animation.disabled_ik_cp932.empty() ? 0 : 1);
        if (!animation.disabled_ik_cp932.empty())
        {
            VmdDetail::U32(out,0); // frame zero
            out.push_back(1);     // visible
            VmdDetail::U32(out,static_cast<std::uint32_t>(animation.disabled_ik_cp932.size()));
            for (const auto& name:animation.disabled_ik_cp932)
            {
                VmdDetail::Field(out,name,20);
                out.push_back(0); // FK export: disable only caller-specified IKs
            }
        }
        return out;
    }
}
