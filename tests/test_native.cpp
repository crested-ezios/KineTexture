#include <KineTexture/KineTexture.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static std::size_t allocationCount=0;
void* operator new(std::size_t size)
{
    ++allocationCount;
    if (void* p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace KineTexture;
using namespace KineTexture::NativeDetail;

static bool Near(double a,double b,double tolerance=1e-10) { return std::abs(a-b)<=tolerance; }
static bool Near(Vector3d a,Vector3d b,double tolerance=1e-10) { return Length(Sub(a,b))<=tolerance; }
static bool SameRotation(Quaterniond a,Quaterniond b,double tolerance=1e-10)
{
    Quaterniond x,y;
    if (!Normalize(a,x) || !Normalize(b,y)) return false;
    const double sign=x.x*y.x+x.y*y.y+x.z*y.z+x.w*y.w<0 ? -1 : 1;
    return std::hypot(std::hypot(x.x-sign*y.x,x.y-sign*y.y),std::hypot(x.z-sign*y.z,x.w-sign*y.w))<=tolerance;
}
static Quaterniond AxisAngle(Vector3d axis,double angle)
{
    axis=Scale(axis,1/Length(axis));
    const double sine=std::sin(angle/2);
    return {axis.x*sine,axis.y*sine,axis.z*sine,std::cos(angle/2)};
}
static TwoBoneInput Fixture(int i)
{
    const double t=0.17*i;
    TwoBoneInput in;
    in.root_position={std::sin(t),0.1*i,std::cos(t)};
    in.goal_position=Add(in.root_position,{1.2*std::cos(t),0.7*std::sin(2*t),0.4*std::sin(t)});
    in.upper_offset={0.1,0.7+0.1*std::cos(t),0.2};
    in.lower_offset={-0.1,0.6,0.08};
    in.upper_world_hint=AxisAngle({1,2,-1},t);
    in.lower_world_hint=AxisAngle({-2,1,3},0.7*t);
    if (i%5==0) in.previous_pole=Vector3d{0.2,-0.4,0.8};
    if (i%3==0) in.pole_hint=Vector3d{std::sin(t),1,std::cos(t)};
    if (i%11==0) in.goal_position=in.root_position;
    if (i%13==0) in.goal_position=Add(in.root_position,{5,-2,1});
    return in;
}
static void V(Vector3d v) { std::printf("[%.17g,%.17g,%.17g]",v.x,v.y,v.z); }
static void Q(Quaterniond q) { std::printf("[%.17g,%.17g,%.17g,%.17g]",q.x,q.y,q.z,q.w); }
static void Optional(const std::optional<Vector3d>& v) { if (v) V(*v); else std::printf("null"); }
static int DumpOracle()
{
    for (int i=0;i<128;++i)
    {
        const auto in=Fixture(i); TwoBoneSolution out;
        if (!SolveTwoBone(in,out)) return 10;
        std::printf("{\"kind\":\"ik\",\"root\":"); V(in.root_position);
        std::printf(",\"goal\":"); V(in.goal_position);
        std::printf(",\"upper_offset\":"); V(in.upper_offset);
        std::printf(",\"lower_offset\":"); V(in.lower_offset);
        std::printf(",\"upper_hint\":"); Q(in.upper_world_hint);
        std::printf(",\"lower_hint\":"); Q(in.lower_world_hint);
        std::printf(",\"previous\":"); Optional(in.previous_pole);
        std::printf(",\"explicit\":"); Optional(in.pole_hint);
        std::printf(",\"upper\":"); Q(out.upper_world);
        std::printf(",\"lower\":"); Q(out.lower_world);
        std::printf(",\"joint\":"); V(out.joint_position);
        std::printf(",\"end\":"); V(out.end_position);
        std::printf(",\"pole\":"); V(out.pole);
        std::printf(",\"requested\":%.17g,\"distance\":%.17g,\"residual\":%.17g,\"reachable\":%s}\n",
            out.requested_distance,out.reachable_distance,out.residual,out.reachable ? "true" : "false");
        const double t=0.07*i,c=std::cos(t),s=std::sin(t);
        const Matrix3d basis{{{c,-s,0},{s,c,0},{0,0,(i%2) ? -1.0 : 1.0}}};
        CoordinateTransform transform;
        if (!transform.Prepare(basis,0.08)) return 11;
        Vector3d vector; Quaterniond rotation,k,absolute,delta,local,parent;
        const Vector3d sp{1,0.3*std::cos(t),0.2},sh{0.1,1,0.3*std::sin(t)},tp{0.4,1,0.1},th{0,0.2,1};
        if (!transform.Vector(in.goal_position,vector) || !transform.Rotation(in.upper_world_hint,rotation) ||
            !AlignBoneFrames(sp,sh,tp,th,k) || !RebaseAbsoluteRotation(in.upper_world_hint,k,absolute) ||
            !RebaseRotationDelta(in.upper_world_hint,k,delta) ||
            !ComposeRotationDelta(in.upper_world_hint,in.lower_world_hint,DeltaSpace::Local,local) ||
            !ComposeRotationDelta(in.upper_world_hint,in.lower_world_hint,DeltaSpace::Parent,parent)) return 12;
        std::printf("{\"kind\":\"retarget\",\"basis\":[[%.17g,%.17g,0],[%.17g,%.17g,0],[0,0,%.17g]],\"input_vector\":",c,-s,s,c,basis[2][2]); V(in.goal_position);
        std::printf(",\"input_rotation\":"); Q(in.upper_world_hint);
        std::printf(",\"change\":"); Q(in.lower_world_hint);
        std::printf(",\"vector\":"); V(vector);
        std::printf(",\"rotation\":"); Q(rotation);
        std::printf(",\"source_primary\":"); V(sp);
        std::printf(",\"source_hinge\":"); V(sh);
        std::printf(",\"target_primary\":"); V(tp);
        std::printf(",\"target_hinge\":"); V(th);
        std::printf(",\"k\":"); Q(k);
        std::printf(",\"absolute\":"); Q(absolute);
        std::printf(",\"delta\":"); Q(delta);
        std::printf(",\"local\":"); Q(local);
        std::printf(",\"parent\":"); Q(parent);
        std::printf("}\n");
        RotationHierarchy hierarchy;
        const std::array<std::int32_t,3> parents{{2,-1,1}};
        const std::array<Quaterniond,3> locals{{in.upper_world_hint,in.lower_world_hint,k}};
        std::array<Quaterniond,3> worlds,roundtrip;
        if (!hierarchy.Prepare(parents) || !hierarchy.LocalToWorld(locals,worlds) || !hierarchy.WorldToLocal(worlds,roundtrip)) return 13;
        std::printf("{\"kind\":\"hierarchy\",\"locals\":[");
        for (int j=0;j<3;++j) { if (j) std::printf(","); Q(locals[j]); }
        std::printf("],\"worlds\":[");
        for (int j=0;j<3;++j) { if (j) std::printf(","); Q(worlds[j]); }
        std::printf("],\"roundtrip\":[");
        for (int j=0;j<3;++j) { if (j) std::printf(","); Q(roundtrip[j]); }
        std::printf("]}\n");
    }
    return 0;
}

int main(int argc,char** argv)
{
    if (argc==2 && std::strcmp(argv[1],"--oracle")==0) return DumpOracle();
    int checks=0;
    #define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(false)
    const double pi=std::numbers::pi;
    Quaterniond k,absolute,delta,local,parent;
    CHECK(AlignBoneFrames({1,0,0},{0,1,0},{0,1,0},{-1,0,0},k));
    CHECK(Near(Rotate(k,{0,1,0}),{1,0,0}));
    CHECK(Near(Rotate(k,{-1,0,0}),{0,1,0}));
    const auto x90=AxisAngle({1,0,0},pi/2),z90=AxisAngle({0,0,1},pi/2);
    CHECK(RebaseAbsoluteRotation(x90,z90,absolute));
    CHECK(RebaseRotationDelta(x90,z90,delta));
    CHECK(Near(Rotate(absolute,{1,0,0}),{0,0,1}));
    CHECK(Near(Rotate(delta,{0,0,1}),{-1,0,0}));
    CHECK(ComposeRotationDelta(z90,x90,DeltaSpace::Local,local));
    CHECK(ComposeRotationDelta(z90,x90,DeltaSpace::Parent,parent));
    CHECK(Near(Rotate(local,{0,1,0}),{0,0,1}));
    CHECK(Near(Rotate(parent,{0,1,0}),{-1,0,0}));
    const auto saved=k;
    CHECK(!AlignBoneFrames({1,0,0},{2,0,0},{1,0,0},{0,1,0},k));
    CHECK(SameRotation(k,saved));
    CHECK(!RebaseAbsoluteRotation({0,0,0,0},z90,k));
    CHECK(SameRotation(k,saved));
    CHECK(!ComposeRotationDelta(x90,z90,static_cast<DeltaSpace>(99),k));

    CoordinateTransform transform;
    Vector3d vector{9,8,7}; Quaterniond rotation;
    CHECK(!transform.Vector({1,2,3},vector) && Near(vector,{9,8,7}));
    const Matrix3d reflection{{{1,0,0},{0,1,0},{0,0,-1}}};
    CHECK(transform.Prepare(reflection,0.08));
    CHECK(transform.Vector({10,20,30},vector) && Near(vector,{0.8,1.6,-2.4}));
    const Quaterniond doubled{2*x90.x,0,0,2*x90.w};
    CHECK(transform.Rotation(doubled,rotation));
    CHECK(Near(rotation.x,-doubled.x) && Near(rotation.w,doubled.w));
    CHECK(transform.Rotation({0.0,-0.0,0.0,1},rotation));
    CHECK(std::signbit(rotation.x) && !std::signbit(rotation.y));
    CoordinateTransform inverse;
    CHECK(transform.Inverse(inverse));
    CHECK(inverse.Vector(vector,vector) && Near(vector,{10,20,30}));
    Matrix3d bad=reflection; bad[0][0]=2;
    CHECK(!transform.Prepare(bad,1));
    CHECK(transform.Vector({10,20,30},vector) && Near(vector,{0.8,1.6,-2.4}));
    const auto oldVector=vector;
    CHECK(!transform.Vector({std::numeric_limits<double>::infinity(),0,0},vector) && Near(vector,oldVector));

    RotationHierarchy hierarchy;
    const std::array<std::int32_t,3> parents{{2,-1,1}},cycle{{1,2,0}},invalid{{3,-1,1}};
    const std::array<Quaterniond,3> locals{{x90,z90,{}}};
    std::array<Quaterniond,3> worlds,restored;
    CHECK(hierarchy.Prepare(parents));
    const auto before=allocationCount;
    CHECK(hierarchy.LocalToWorld(locals,worlds));
    CHECK(hierarchy.WorldToLocal(worlds,restored));
    CHECK(allocationCount==before);
    CHECK(SameRotation(worlds[0],Multiply(z90,x90)));
    for (int i=0;i<3;++i) CHECK(SameRotation(restored[i],locals[i]));
    CHECK(!hierarchy.Prepare(cycle)); CHECK(!hierarchy.Prepare(invalid));
    CHECK(hierarchy.LocalToWorld(locals,worlds));
    CHECK(!hierarchy.WorldToLocal(worlds,worlds));
    auto badLocals=locals; badLocals[2]={0,0,0,0};
    const auto prior=worlds;
    CHECK(!hierarchy.LocalToWorld(badLocals,worlds));
    CHECK(std::memcmp(worlds.data(),prior.data(),sizeof(worlds))==0);

    TwoBoneInput in;
    in.upper_offset={1,0,0}; in.lower_offset={1,0,0};
    in.goal_position={1,1,0}; in.pole_hint=Vector3d{0,1,0};
    TwoBoneSolution solution;
    CHECK(SolveTwoBone(in,solution));
    CHECK(solution.reachable && Near(solution.joint_position,{0,1,0}) && Near(solution.end_position,{1,1,0}));
    CHECK(Near(solution.residual,0));
    in.goal_position={4,0,0};
    CHECK(SolveTwoBone(in,solution));
    CHECK(!solution.reachable && Near(solution.end_position,{2,0,0}) && Near(solution.residual,2));
    in.goal_position={0,0,0};
    CHECK(SolveTwoBone(in,solution));
    CHECK(solution.reachable && Near(solution.joint_position,{1,0,0}) && Near(solution.end_position,{0,0,0}));
    in.upper_offset={2,0,0};
    CHECK(SolveTwoBone(in,solution));
    CHECK(!solution.reachable && Near(solution.reachable_distance,1) && Near(solution.residual,1));
    in.upper_offset={1,0,0}; in.goal_position={-2,0,0};
    CHECK(SolveTwoBone(in,solution));
    CHECK(Near(solution.end_position,{-2,0,0}) && Near(solution.residual,0));
    in.goal_position={1,0,0}; in.pole_hint=Vector3d{1,0,0}; in.previous_pole=Vector3d{0,0,-1};
    CHECK(SolveTwoBone(in,solution) && Near(solution.pole,{0,0,-1}));
    in.pole_hint=Vector3d{0,1,0};
    CHECK(SolveTwoBone(in,solution) && Near(solution.pole,{0,1,0}));
    std::array<unsigned char,sizeof(TwoBoneSolution)> bytes;
    std::memcpy(bytes.data(),&solution,sizeof(solution));
    in.upper_offset={0,0,0};
    CHECK(!SolveTwoBone(in,solution) && std::memcmp(bytes.data(),&solution,sizeof(solution))==0);
    in=Fixture(1); in.upper_world_hint={0,0,0,0};
    CHECK(!SolveTwoBone(in,solution) && std::memcmp(bytes.data(),&solution,sizeof(solution))==0);
    in=Fixture(1); in.previous_pole=Vector3d{0,std::numeric_limits<double>::quiet_NaN(),0};
    CHECK(!SolveTwoBone(in,solution) && std::memcmp(bytes.data(),&solution,sizeof(solution))==0);

    const auto rigid=AxisAngle({1,-2,3},0.83);
    const Vector3d translation{2,-1,0.7};
    const auto rigidPoint=[&](Vector3d p) { return Add(Rotate(rigid,p),translation); };
    const auto allocationBaseline=allocationCount;
    for (int i=0;i<128;++i)
    {
        in=Fixture(i); TwoBoneSolution base,moved;
        CHECK(SolveTwoBone(in,base));
        CHECK(Near(base.joint_position,Add(in.root_position,Rotate(base.upper_world,in.upper_offset))));
        CHECK(Near(base.end_position,Add(base.joint_position,Rotate(base.lower_world,in.lower_offset))));
        CHECK(Near(base.residual,Length(Sub(in.goal_position,base.end_position))));
        CHECK(Near(Length(Sub(base.joint_position,in.root_position)),Length(in.upper_offset)));
        CHECK(Near(Length(Sub(base.end_position,base.joint_position)),Length(in.lower_offset)));
        CHECK(Near(Dot(base.pole,base.pole),1));
        in.root_position=rigidPoint(in.root_position); in.goal_position=rigidPoint(in.goal_position);
        in.upper_world_hint=Multiply(rigid,in.upper_world_hint); in.lower_world_hint=Multiply(rigid,in.lower_world_hint);
        if (in.previous_pole) in.previous_pole=Rotate(rigid,*in.previous_pole);
        if (in.pole_hint) in.pole_hint=Rotate(rigid,*in.pole_hint);
        CHECK(SolveTwoBone(in,moved));
        CHECK(Near(moved.joint_position,rigidPoint(base.joint_position),2e-9));
        CHECK(Near(moved.end_position,rigidPoint(base.end_position),2e-9));
        CHECK(SameRotation(moved.upper_world,Multiply(rigid,base.upper_world),2e-9));
        CHECK(SameRotation(moved.lower_world,Multiply(rigid,base.lower_world),2e-9));
        CHECK(Near(moved.residual,base.residual,2e-9));
    }
    CHECK(allocationCount==allocationBaseline);
    std::printf("KineTexture native retarget/IK: %d checks PASS\n",checks);
    return 0;
}
