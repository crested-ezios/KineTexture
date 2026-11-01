#pragma once

// Development-only SMPL parameter serialization. Retargeting, source skeleton
// semantics and model geometry are supplied by the caller, never inferred here.
#include <KineTexture/Retarget.hpp>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace KineTexture::GkExport
{
    struct SmplMotion
    {
        double fps{30};
        std::size_t frame_count{};
        std::size_t joint_count{24};
        // Frame-major LOCAL rest-relative rotations in the SMPL target basis.
        // Standard SMPL order (24) or SMPL-X body order (22). The latter does
        // not include the separate 30-finger, face or expression parameters.
        std::vector<Quaterniond> local_rotations;
        std::vector<Vector3d> translations; // SMPL model-origin displacement, meters.
        std::vector<double> betas; // Optional caller-supplied shape. Empty => omitted.
        std::vector<std::uint8_t> joint_valid; // joint_count explicit availability flags.
    };

    namespace SmplDetail
    {
        using Bytes = std::vector<std::uint8_t>;
        inline void U16(Bytes& out, std::uint16_t value)
        { out.push_back(static_cast<std::uint8_t>(value)); out.push_back(static_cast<std::uint8_t>(value >> 8)); }
        inline void U32(Bytes& out, std::uint32_t value)
        { for (int i=0;i<4;++i) out.push_back(static_cast<std::uint8_t>(value >> (i*8))); }
        inline void F64(Bytes& out, double value)
        {
            static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
            const auto bits=std::bit_cast<std::uint64_t>(value);
            for (int i=0;i<8;++i) out.push_back(static_cast<std::uint8_t>(bits >> (i*8)));
        }
        inline std::uint32_t Crc32(std::span<const std::uint8_t> data)
        {
            std::uint32_t crc=0xffffffffU;
            for (const auto value:data)
            {
                crc ^= value;
                for (int bit=0;bit<8;++bit) crc=(crc>>1) ^ ((crc&1U) ? 0xedb88320U : 0U);
            }
            return crc ^ 0xffffffffU;
        }
        inline Bytes Npy(std::string descriptor, const std::vector<std::size_t>& shape, Bytes data)
        {
            std::string dimensions="(";
            for (std::size_t i=0;i<shape.size();++i)
            { if (i) dimensions+=", "; dimensions+=std::to_string(shape[i]); }
            if (shape.size()==1) dimensions+=",";
            dimensions+=")";
            std::string header="{'descr': '"+descriptor+"', 'fortran_order': False, 'shape': "+dimensions+", }";
            // NPY v1: 10-byte prefix, newline-terminated Python dict, aligned to 64.
            header.append((64-((10+header.size()+1)%64))%64,' ');
            header+='\n';
            if (header.size()>65535) throw std::runtime_error("NPY header exceeds v1 budget");
            Bytes result{0x93,'N','U','M','P','Y',1,0};
            U16(result,static_cast<std::uint16_t>(header.size()));
            result.insert(result.end(),header.begin(),header.end());
            result.insert(result.end(),data.begin(),data.end());
            return result;
        }
        inline Bytes Doubles(const std::vector<std::size_t>& shape, const std::vector<double>& values)
        {
            Bytes data; data.reserve(values.size()*8);
            for (double value:values) F64(data,value);
            return Npy("<f8",shape,std::move(data));
        }
        struct Member { std::string name; Bytes data; };
        inline Bytes Zip(const std::vector<Member>& members)
        {
            Bytes result,central;
            constexpr auto limit=std::numeric_limits<std::uint32_t>::max();
            for (const auto& member:members)
            {
                if (member.name.size()>65535 || member.data.size()>limit || result.size()>limit ||
                    member.data.size()+member.name.size()+30ULL>limit-result.size())
                    throw std::runtime_error("SMPL NPZ exceeds ZIP32 budget");
                const auto offset=static_cast<std::uint32_t>(result.size());
                const auto size=static_cast<std::uint32_t>(member.data.size());
                const auto nameSize=static_cast<std::uint16_t>(member.name.size());
                const auto crc=Crc32(member.data);
                U32(result,0x04034b50); U16(result,20); U16(result,0); U16(result,0);
                U16(result,0); U16(result,0x21); U32(result,crc); U32(result,size); U32(result,size);
                U16(result,nameSize); U16(result,0);
                result.insert(result.end(),member.name.begin(),member.name.end());
                result.insert(result.end(),member.data.begin(),member.data.end());
                U32(central,0x02014b50); U16(central,20); U16(central,20); U16(central,0); U16(central,0);
                U16(central,0); U16(central,0x21); U32(central,crc); U32(central,size); U32(central,size);
                U16(central,nameSize); U16(central,0); U16(central,0); U16(central,0); U16(central,0);
                U32(central,0); U32(central,offset);
                central.insert(central.end(),member.name.begin(),member.name.end());
            }
            if (members.size()>65535 || result.size()+central.size()+22ULL>limit)
                throw std::runtime_error("SMPL NPZ central directory exceeds ZIP32 budget");
            const auto offset=static_cast<std::uint32_t>(result.size());
            result.insert(result.end(),central.begin(),central.end());
            U32(result,0x06054b50); U16(result,0); U16(result,0);
            U16(result,static_cast<std::uint16_t>(members.size())); U16(result,static_cast<std::uint16_t>(members.size()));
            U32(result,static_cast<std::uint32_t>(central.size())); U32(result,offset); U16(result,0);
            return result;
        }
        inline Vector3d AxisAngle(Quaterniond input)
        {
            Quaterniond q;
            if (!NativeDetail::Normalize(input,q)) throw std::runtime_error("SMPL rotation is nonfinite or zero");
            // q/-q have a deterministic shortest-arc representation, including pi.
            bool flip=q.w<0;
            if (std::abs(q.w)<=1e-15)
            {
                const double components[3]{q.x,q.y,q.z};
                for (double value:components) if (std::abs(value)>1e-15) { flip=value<0; break; }
            }
            if (flip) q={-q.x,-q.y,-q.z,-q.w};
            const double sine=std::hypot(q.x,q.y,q.z);
            if (sine<1e-12) return {2*q.x,2*q.y,2*q.z};
            const double angle=2*std::atan2(sine,std::max(0.0,q.w));
            return {q.x*angle/sine,q.y*angle/sine,q.z*angle/sine};
        }
    }

    // Stored (uncompressed) ZIP of numeric NPY arrays, loadable with
    // numpy.load(..., allow_pickle=False). No Python, zlib or body model needed.
    inline std::vector<std::uint8_t> SerializeSmplNpz(const SmplMotion& motion)
    {
        if (!std::isfinite(motion.fps) || motion.fps<=0 || motion.fps>1000)
            throw std::runtime_error("SMPL FPS must be finite and in (0,1000]");
        // Bounds total serialization memory; callers should window long research clips.
        if ((motion.joint_count!=24 && motion.joint_count!=22) || !motion.frame_count || motion.frame_count>1'000'000 ||
            motion.local_rotations.size()!=motion.frame_count*motion.joint_count || motion.translations.size()!=motion.frame_count ||
            motion.joint_valid.size()!=motion.joint_count)
            throw std::runtime_error("SMPL requires 1..1000000 frames, 24 SMPL or 22 SMPL-X body rotations per frame, root translations and a matching joint mask");
        if (!std::isfinite(static_cast<double>(motion.frame_count-1)/motion.fps))
            throw std::runtime_error("SMPL seconds timebase exceeds finite range");
        for (auto valid:motion.joint_valid) if (valid>1) throw std::runtime_error("SMPL joint_valid values must be 0 or 1");
        if (!motion.joint_valid[0]) throw std::runtime_error("SMPL pelvis rotation must be available");
        if (motion.betas.size()>300) throw std::runtime_error("SMPL shape exceeds 300-component budget");
        for (double beta:motion.betas) if (!std::isfinite(beta)) throw std::runtime_error("SMPL shape contains nonfinite values");
        std::vector<double> poses,global,body,transl,times;
        const auto poseWidth=motion.joint_count*3, bodyWidth=(motion.joint_count-1)*3;
        poses.reserve(motion.frame_count*poseWidth); global.reserve(motion.frame_count*3);
        body.reserve(motion.frame_count*bodyWidth); transl.reserve(motion.frame_count*3); times.reserve(motion.frame_count);
        for (std::size_t frame=0;frame<motion.frame_count;++frame)
        {
            if (!IsFinite(motion.translations[frame])) throw std::runtime_error("SMPL translation contains nonfinite values");
            const auto t=motion.translations[frame];
            transl.insert(transl.end(),{t.x,t.y,t.z}); times.push_back(static_cast<double>(frame)/motion.fps);
            for (std::size_t joint=0;joint<motion.joint_count;++joint)
            {
                // Unavailable channels are explicit neutral placeholders; the mask
                // must travel with the arrays. They are not measured identity poses.
                const auto aa=motion.joint_valid[joint] ? SmplDetail::AxisAngle(motion.local_rotations[frame*motion.joint_count+joint]) : Vector3d{};
                poses.insert(poses.end(),{aa.x,aa.y,aa.z});
                auto& subset=joint==0 ? global : body;
                subset.insert(subset.end(),{aa.x,aa.y,aa.z});
            }
        }
        using namespace SmplDetail;
        std::vector<Member> members;
        members.push_back({"poses.npy",Doubles({motion.frame_count,poseWidth},poses)});
        members.push_back({"global_orient.npy",Doubles({motion.frame_count,3},global)});
        members.push_back({"body_pose.npy",Doubles({motion.frame_count,bodyWidth},body)});
        members.push_back({"transl.npy",Doubles({motion.frame_count,3},transl)});
        members.push_back({"fps.npy",Doubles({}, {motion.fps})});
        members.push_back({"mocap_framerate.npy",Doubles({}, {motion.fps})});
        members.push_back({"time_seconds.npy",Doubles({motion.frame_count},times)});
        members.push_back({"joint_valid.npy",Npy("|u1",{motion.joint_count},motion.joint_valid)});
        const std::string model=motion.joint_count==24 ? "smpl" : "smplx";
        members.push_back({"model_type.npy",Npy("|S"+std::to_string(model.size()),{},Bytes(model.begin(),model.end()))});
        if (!motion.betas.empty()) members.push_back({"betas.npy",Doubles({1,motion.betas.size()},motion.betas)});
        return Zip(members);
    }
}
