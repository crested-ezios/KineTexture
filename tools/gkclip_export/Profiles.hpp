#pragma once
#include "Clip.hpp"
#include <iomanip>
#include <sstream>

namespace KineTexture::GkExport {
inline std::string Hex(const std::string& value) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    if(value.empty()) return "-";
    for(unsigned char c:value) {out+=digits[c>>4];out+=digits[c&15];}return out;
}
inline std::string Unhex(const std::string& value) {
    if(value=="-") return {};
    Require(value.size()%2==0,"name hex must have even length");std::string out;
    auto digit=[](char c) { if(c>='0' && c<='9')return c-'0';if(c>='a' && c<='f')return c-'a'+10;if(c>='A' && c<='F')return c-'A'+10;throw std::runtime_error("invalid name hex"); };
    for(std::size_t i=0;i<value.size();i+=2) {const auto c=char(digit(value[i])*16+digit(value[i+1]));Require(c!=0,"name contains NUL");out+=c;}return out;
}
inline std::string SerializeProfile(const Profile& p) {
    std::ostringstream out;out<<std::setprecision(17);
    out<<"kinetexture-gk-export-profile 1\n# Adapter configuration; names are CP932 bytes for VMD, ASCII for SMPL.\n";
    out<<"id "<<std::quoted(p.id)<<"\ntarget "<<p.target<<"\nsource_versions "<<p.min_version<<' '<<p.max_version<<"\napproximate "<<p.approximate<<"\nbasis";
    for(const auto& row:p.basis) for(auto n:row) out<<' '<<n;
    out<<"\nmeters_per_unit "<<p.meters_per_unit<<"\nroot_offset "<<p.root_offset.x<<' '<<p.root_offset.y<<' '<<p.root_offset.z;
    out<<"\nroot_track "<<Hex(p.root_track)<<"\n# joint source_hid parent_index name_hex alignment_xyzw rest_local_xyzw\n";
    for(const auto& j:p.joints) out<<"joint "<<j.source_hid<<' '<<j.parent<<' '<<Hex(j.name)<<' '
        <<j.alignment.x<<' '<<j.alignment.y<<' '<<j.alignment.z<<' '<<j.alignment.w<<' '
        <<j.rest.x<<' '<<j.rest.y<<' '<<j.rest.z<<' '<<j.rest.w<<'\n';
    return out.str();
}
inline Profile ReadProfile(const std::filesystem::path& path) {
    Require(std::filesystem::file_size(path)<=65536,"profile exceeds 64 KiB");
    std::ifstream in(path);Require(bool(in),"cannot open profile");
    std::string line;Require(bool(std::getline(in,line)),"empty profile");
    {std::istringstream row(line);std::string schema,extra;int version{};Require(bool(row>>schema>>version) && schema=="kinetexture-gk-export-profile" && version==1 && !(row>>extra),"invalid profile schema");}
    Profile p;std::set<std::string> fields;
    while(std::getline(in,line)) {
        std::istringstream row(line);std::string field;if(!(row>>field) || field[0]=='#')continue;
        Require(field=="joint" || fields.insert(field).second,"duplicate profile field: "+field);
        std::string text;
        if(field=="id") row>>std::quoted(p.id);
        else if(field=="target")row>>p.target;
        else if(field=="source_versions")row>>p.min_version>>p.max_version;
        else if(field=="approximate") {int v=-1;row>>v;Require(v==0 || v==1,"invalid approximate flag");p.approximate=v!=0;}
        else if(field=="basis") for(auto& r:p.basis) for(auto& n:r) row>>n;
        else if(field=="meters_per_unit")row>>p.meters_per_unit;
        else if(field=="root_offset")row>>p.root_offset.x>>p.root_offset.y>>p.root_offset.z;
        else if(field=="root_track") {row>>text;p.root_track=Unhex(text);}
        else if(field=="joint") {
            Joint j;row>>j.source_hid>>j.parent>>text>>j.alignment.x>>j.alignment.y>>j.alignment.z>>j.alignment.w
                >>j.rest.x>>j.rest.y>>j.rest.z>>j.rest.w;j.name=Unhex(text);p.joints.push_back(j);
            Require(p.joints.size()<=256,"profile joint budget exceeded");
        } else throw std::runtime_error("unknown profile field: "+field);
        Require(!row.fail(),"malformed profile field: "+field);
        Require(!(row>>text),"unexpected profile tokens: "+field);
    }
    for(auto field:{"id","target","source_versions","approximate","basis","meters_per_unit","root_offset","root_track"}) Require(fields.contains(field),std::string("missing profile field: ")+field);
    Require(!p.id.empty(),"empty profile ID");return p;
}
} // namespace KineTexture::GkExport
