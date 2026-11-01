#include "KineTexture/KineTexture.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>

// Prepared-path allocation counter. The core uses ordinary standard containers.
static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n)
{
	if (void* p = std::malloc(n ? n : 1)) { ++allocations; return p; }
	throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace KineTexture;
static int failures = 0, checks = 0;
static void Check(bool condition, const char* description)
{
	++checks;
	if (!condition) { ++failures; std::printf("FAIL: %s\n", description); }
}
static void Check(Status status, const char* description) { Check(static_cast<bool>(status), description); }
static bool Near(float a, float b, float tolerance = 2e-5f) { return std::abs(a - b) <= tolerance; }
static bool Near(const Vector3& a, const Vector3& b)
{
	return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
}
static double AngleDegrees(const Quaternion& a, const Quaternion& b)
{
	const double an = std::sqrt(double(a.x)*a.x + double(a.y)*a.y + double(a.z)*a.z + double(a.w)*a.w);
	const double bn = std::sqrt(double(b.x)*b.x + double(b.y)*b.y + double(b.z)*b.z + double(b.w)*b.w);
	const double av[] = {a.x/an,a.y/an,a.z/an,a.w/an};
	double bv[] = {b.x/bn,b.y/bn,b.z/bn,b.w/bn};
	double dot = 0;
	for (int i=0;i<4;++i) dot += av[i]*bv[i];
	if (dot < 0) for (double& v : bv) v = -v;
	double difference = 0, sum = 0;
	for (int i=0;i<4;++i) { difference += (av[i]-bv[i])*(av[i]-bv[i]); sum += (av[i]+bv[i])*(av[i]+bv[i]); }
	return 4.0 * std::atan2(std::sqrt(difference),std::sqrt(sum)) * 180.0 / 3.14159265358979323846;
}

static void MathAndSampling()
{
	static_assert(std::is_standard_layout_v<Vector3> && std::is_standard_layout_v<Quaternion>);
	static_assert(sizeof(Vector3) == 12 && sizeof(Quaternion) == 16);
	Check(Near(Quaternion::FromEulerDegreesXYZ(0,0,90).Rotate({1,0,0}), {0,1,0}), "Hamilton active z rotation");
	Check(Near(Quaternion::FromEulerDegreesXYZ(90,0,0).Rotate({0,1,0}), {0,0,1}), "x rotation maps y to z");
	Check(AngleDegrees(Quaternion(0,0,0,0).Normalized(),Quaternion::Identity()) < 1e-6, "legacy zero normalization");
	const Quaternion rz = Quaternion::FromEulerDegreesXYZ(0,0,90);
	Check(AngleDegrees(rz * rz.Inverse(), Quaternion::Identity()) < 1e-5, "unit inverse composition");
	const Quaternion negative(-rz.x,-rz.y,-rz.z,-rz.w);
	Check(AngleDegrees(Quaternion::Slerp(rz,negative,0.5f),rz) < 1e-5, "opposite quaternion signs are the same rotation");

	for (const float fps : {30.0f,60.0f,120.0f})
	{
		std::vector<Vector3> roots{{0,0,0},{2,4,6},{4,8,12}};
		std::vector<Quaternion> rotations{Quaternion::Identity(),Quaternion::FromEulerDegreesXYZ(0,0,90),Quaternion::FromEulerDegreesXYZ(0,0,179.9f)};
		DenseClipView clip{fps,3,1,roots,rotations};
		Check(ValidateDenseClip(clip), "finite dense input validates");
		Vector3 root;
		std::vector<Quaternion> out(1);
		Check(SampleDensePose(clip,-100,root,out), "negative time accepted");
		Check(Near(root,{0,0,0}) && AngleDegrees(out[0],Quaternion::Identity()) < 1e-5, "negative time holds first pose");
		Check(SampleDensePose(clip,0.5f/fps,root,out), "half-frame sampling");
		Check(Near(root,{1,2,3}), "root midpoint is analytic linear interpolation");
		Check(AngleDegrees(out[0],Quaternion::FromEulerDegreesXYZ(0,0,45)) < 0.001, "midpoint is analytic 45 degree rotation");
		Check(SampleDensePose(clip,2.0f/fps,root,out), "last frame sampling");
		Check(Near(root,{4,8,12}) && AngleDegrees(out[0],rotations[2]) < 0.001, "last frame maintained");
		Check(SampleDensePose(clip,std::numeric_limits<float>::max(),root,out), "huge finite time clamps before integer conversion");
		Check(Near(root,{4,8,12}), "huge finite time holds final root");
		const Vector3 sentinel{91,92,93};
		root=sentinel;
		out[0]=rz;
		Check(SampleDensePose(clip,std::numeric_limits<float>::quiet_NaN(),root,out).code==ErrorCode::InvalidTime, "NaN time rejected");
		Check(SampleDensePose(clip,std::numeric_limits<float>::infinity(),root,out).code==ErrorCode::InvalidTime, "infinite time rejected");
		Check(Near(root,sentinel) && AngleDegrees(out[0],rz)<1e-6, "failed sampling leaves output unchanged");
		const auto before = allocations.load();
		bool ok = true;
		for (int i=0;i<1000;++i) if (!SampleDensePose(clip,float(i%31)/(30.0f*fps),root,out)) ok=false;
		const auto after = allocations.load();
		Check(ok && before==after,"prepared sampler makes no allocations across 1000 calls");
	}
	std::vector<Vector3> roots{{1,2,3}};
	std::vector<Quaternion> rotations{rz};
	DenseClipView single{60,1,1,roots,rotations};
	Vector3 root;
	std::vector<Quaternion> out(1);
	Check(SampleDensePose(single,99,root,out), "one-frame dense clip samples");
	Check(Near(root,roots[0]) && AngleDegrees(out[0],rz)<0.001, "one-frame output preserved");
	single.fps=0;
	Check(ValidateDenseClip(single).code==ErrorCode::InvalidRate,"zero frame rate rejected");
	single.fps=60;
	single.frame_count=std::numeric_limits<std::size_t>::max();
	single.joint_count=2;
	Check(ValidateDenseClip(single).code==ErrorCode::InvalidSize,"shape multiplication overflow rejected");
	single={60,1,1,roots,rotations};
	roots[0].x=std::numeric_limits<float>::quiet_NaN();
	Check(ValidateDenseClip(single).code==ErrorCode::NonFinite,"nonfinite root rejected at ingestion");
	roots[0]={};
	rotations[0]={0,0,0,0};
	Check(ValidateDenseClip(single).code==ErrorCode::InvalidRotation,"zero quaternion rejected at ingestion");
	rotations[0]={0,0,0,2};
	Check(ValidateDenseClip(single).code==ErrorCode::InvalidRotation,"nonunit quaternion rejected at ingestion");
}

static SkeletonDescriptor SkeletonA()
{
	SkeletonDescriptor s;
	s.id="synthetic-a";
	// Deliberately not parent-first, and external IDs are deliberately sparse.
	s.joints={
		{9001,"middle",2,{2,0,0},Quaternion::FromEulerDegreesXYZ(0,90,0),{0,0,1},{0,1,0}},
		{42,"tip",0,{0,0,3},Quaternion::FromEulerDegreesXYZ(90,0,0),{0,1,0},{1,0,0}},
		{700000,"base",-1,{},Quaternion::FromEulerDegreesXYZ(0,0,90),{1,0,0},{0,1,0}}
	};
	return s;
}
static SkeletonDescriptor SkeletonB()
{
	SkeletonDescriptor s;
	s.id="synthetic-b";
	s.basis.up=Axis::PositiveZ;
	s.basis.forward=Axis::NegativeY;
	s.joints={
		{135,"base",-1,{},Quaternion::FromEulerDegreesXYZ(90,0,0),{0,0,1},{1,0,0}},
		{555,"middle",0,{0,4,0},Quaternion::FromEulerDegreesXYZ(0,0,90),{0,1,0},{0,0,1}},
		{98765,"tip",1,{0,5,0},Quaternion::Identity(),{1,0,0},{0,0,1}}
	};
	return s;
}
static PoseSchema Schema(const SkeletonDescriptor& s,RotationRepresentation representation)
{
	PoseSchema schema;
	schema.skeleton_id=s.id;
	schema.skeleton_revision=s.revision;
	schema.basis=s.basis;
	schema.rotations=representation;
	return schema;
}
static PoseFrame Frame(const PoseSchema& schema,std::size_t count)
{
	PoseFrame f;
	f.schema=schema;
	f.joint_rotations.resize(count);
	return f;
}
struct Output
{
	std::vector<Vector3> positions;
	std::vector<Quaternion> rotations;
	std::vector<std::uint8_t> valid;
	explicit Output(std::size_t n):positions(n),rotations(n),valid(n){}
	FkOutputView View(){return {positions,rotations,valid};}
};

static void FkAndInstances()
{
	const auto a=SkeletonA(),b=SkeletonB();
	const auto sa=Schema(a,RotationRepresentation::RestRelative),sb=Schema(b,RotationRepresentation::RestRelative);
	FkPlan pa,pb;
	Check(pa.Prepare(a,sa),"prepare synthetic rig A with unsorted parents");
	Check(pb.Prepare(b,sb),"prepare independent axes/rest/length rig B");
	Check(pa.EvaluationOrder().size()==3 && pa.EvaluationOrder()[0]==2 && pa.EvaluationOrder()[1]==0,"prepare resolves explicit parent order without external IDs");
	FkContext ca,cb;
	Check(pa.PrepareContext(ca),"prepare independent context A");
	Check(pb.PrepareContext(cb),"prepare independent context B");
	auto fa=Frame(sa,3),fb=Frame(sb,3);
	fa.root_translation={1,2,3};
	fb.root_translation={-1,2,-3};
	Output oa(3),ob(3);
	Check(pa.Evaluate(fa,ca,oa.View()),"evaluate rest-relative rig A");
	Check(Near(oa.positions[2],{1,2,3}) && Near(oa.positions[0],{1,4,3}) && Near(oa.positions[1],{1,7,3}),"rig A FK matches independently calculated right-angle positions");
	Check(Near(oa.rotations[2].Rotate({1,0,0}),{0,1,0}),"nonidentity root rest applied once");
	Check(pb.Evaluate(fb,cb,ob.View()),"evaluate rest-relative rig B");
	Check(Near(ob.positions[0],{-1,2,-3}) && Near(ob.positions[1],{-1,2,1}) && Near(ob.positions[2],{-6,2,1}),"rig B FK matches different analytic geometry");
	const auto before=allocations.load();
	bool ok=true;
	for(int i=0;i<1000;++i)
	{
		if(!pa.Evaluate(fa,ca,oa.View()) || !pb.Evaluate(fb,cb,ob.View()))ok=false;
	}
	const auto after=allocations.load();
	Check(ok && before==after,"interleaved prepared FK has no ordinary heap allocations");
	Check(Near(oa.positions[1],{1,7,3}) && Near(ob.positions[2],{-6,2,1}),"interleaved instances preserve independent results");
	Check(pa.Evaluate(fa,cb,oa.View()).code==ErrorCode::ContextMismatch,"other plan context rejected");

	// LocalAbsolute intentionally ignores nonidentity descriptor rest rotations.
	const auto absolute=Schema(a,RotationRepresentation::LocalAbsolute);
	FkPlan local;
	FkContext context;
	Check(local.Prepare(a,absolute),"prepare absolute local semantics");
	Check(local.PrepareContext(context),"prepare absolute context");
	auto frame=Frame(absolute,3);
	frame.root_translation={10,20,30};
	Check(local.Evaluate(frame,context,oa.View()),"evaluate local absolute");
	Check(Near(oa.positions[1],{12,20,33}),"local absolute never implicitly composes rest");
	Check(AngleDegrees(oa.rotations[1],Quaternion::Identity())<1e-6,"identity local pose remains identity despite rest");
	frame.local_translations={{5,0,0},{0,0,6},{1,0,0}};
	Check(local.Evaluate(frame,context,oa.View()),"explicit local translation overrides offsets");
	Check(Near(oa.positions[1],{16,20,36}),"replacement local translation semantics are explicit");

	const auto world=Schema(a,RotationRepresentation::WorldAbsolute);
	FkPlan wp;
	FkContext wc;
	Check(wp.Prepare(a,world),"prepare world rotations");
	Check(wp.PrepareContext(wc),"prepare world context");
	auto wf=Frame(world,3);
	wf.joint_rotations[2]=Quaternion::FromEulerDegreesXYZ(0,0,90);
	wf.joint_rotations[0]=Quaternion::Identity();
	Check(wp.Evaluate(wf,wc,oa.View()),"evaluate explicit world rotations");
	Check(Near(oa.positions[0],{0,2,0}) && Near(oa.positions[1],{0,2,3}),"world pose uses parent world directly without recomposition");
	Check(AngleDegrees(oa.rotations[0],Quaternion::Identity())<1e-6,"world child is not multiplied by root again");

	// Reprepare invalidates old per-instance scratch; failed Prepare is atomic.
	Check(pa.Prepare(a,sa),"successful reprepare");
	Check(pa.Evaluate(fa,ca,oa.View()).code==ErrorCode::ContextMismatch,"rebind invalidates stale context");
	Check(pa.PrepareContext(ca),"context rebuilt after rebind");
	auto invalid=a;
	invalid.joints[0].parent=100;
	Check(pa.Prepare(invalid,sa).code==ErrorCode::InvalidParent,"invalid reprepare rejected");
	Check(pa.Evaluate(fa,ca,oa.View()),"failed reprepare preserves prior valid plan and context");
}

static void ValidationAndAtomicOutput()
{
	auto a=SkeletonA();
	auto schema=Schema(a,RotationRepresentation::LocalAbsolute);
	FkPlan plan;
	FkContext context;
	Check(plan.PrepareContext(context).code==ErrorCode::NotPrepared,"context before plan rejected");
	Check(plan.Prepare(a,schema,2).code==ErrorCode::BudgetExceeded,"joint prepare budget enforced");
	auto invalid=a;
	invalid.joints[0].id=invalid.joints[1].id;
	Check(plan.Prepare(invalid,schema).code==ErrorCode::DuplicateId,"duplicate external ID rejected");
	invalid=a;invalid.joints[0].name=invalid.joints[1].name;
	Check(plan.Prepare(invalid,schema).code==ErrorCode::DuplicateName,"duplicate nonempty name rejected");
	invalid=a;invalid.joints[0].parent=1;invalid.joints[1].parent=0;
	Check(plan.Prepare(invalid,schema).code==ErrorCode::Cycle,"cycle rejected without recursive stack use");
	invalid=a;invalid.joints[0].secondary_axis=invalid.joints[0].primary_axis;
	Check(plan.Prepare(invalid,schema).code==ErrorCode::InvalidAxes,"parallel reference axes rejected");
	invalid=a;invalid.joints[1].rest_rotation={0,0,0,0};
	Check(plan.Prepare(invalid,schema).code==ErrorCode::InvalidRotation,"invalid rest quaternion rejected");
	invalid=a;invalid.joints[0].parent=-1;
	Check(plan.Prepare(invalid,schema).code==ErrorCode::InvalidParent,"multiple roots require explicit permission");
	invalid.allow_multiple_roots=true;
	Check(plan.Prepare(invalid,schema),"declared multiple roots accepted");
	Check(plan.Prepare(a,schema),"restore valid test plan");
	Check(plan.PrepareContext(context),"prepare validation context");
	auto frame=Frame(schema,3);
	Output output(3);
	for(auto& v:output.positions)v={51,52,53};
	for(auto& v:output.rotations)v=Quaternion::FromEulerDegreesXYZ(30,40,50);
	for(auto& v:output.valid)v=9;
	frame.valid={1,1,0};
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::MissingParent,"valid child cannot inherit missing root");
	Check(Near(output.positions[0],{51,52,53}) && output.valid[0]==9,"failed evaluate is output-atomic");
	frame.valid={1,0,1};
	frame.joint_rotations[1]={std::numeric_limits<float>::quiet_NaN(),0,0,0};
	Check(plan.Evaluate(frame,context,output.View()),"missing leaf explicitly distinguished from identity");
	Check(output.valid[1]==0 && Near(output.positions[1],{}),"missing leaf propagated as invalid mask");
	frame.valid={2,0,1};
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::InvalidMask,"invalid mask value rejected");
	frame.valid.clear();frame.joint_rotations[1]=Quaternion::Identity();
	frame.schema.skeleton_revision=7;
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::SchemaMismatch,"schema revision mismatch rejected");
	frame.schema=schema;
	frame.root_translation.x=std::numeric_limits<float>::infinity();
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::NonFinite,"nonfinite FK root rejected");
	frame.root_translation={};
	frame.time_seconds=std::numeric_limits<double>::quiet_NaN();
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::InvalidTime,"nonfinite frame timestamp rejected");
	frame.time_seconds=0;
	frame.joint_rotations[0]={0,0,0,0};
	Check(plan.Evaluate(frame,context,output.View()).code==ErrorCode::InvalidRotation,"invalid valid-joint quaternion rejected");
}

int main()
{
	MathAndSampling();
	FkAndInstances();
	ValidationAndAtomicOutput();
	std::printf("KineTexture core: %d checks, %d failures; fixtures are self-authored synthetic only.\n",checks,failures);
	return failures ? 1 : 0;
}
