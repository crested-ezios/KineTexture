#include <KineTexture/KineTexture.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>

int main(int argc, char** argv)
{
	std::array<KineTexture::Vector3,2> roots{{{0,0,0},{2,0,0}}};
	std::array<KineTexture::Quaternion,2> rotations{{{},KineTexture::Quaternion::FromEulerDegreesXYZ(0,0,90)}};
	std::array<KineTexture::Quaternion,1> output;
	KineTexture::Vector3 root;
	const KineTexture::DenseClipView clip{30,2,1,roots,rotations};
	if (!KineTexture::ValidateDenseClip(clip) || !KineTexture::SampleDensePose(clip,1.0f/60,root,output)) return 1;
	const auto direction=output[0].Rotate({1,0,0});
	const float expected=std::sqrt(0.5f);
	if (std::abs(root.x-1)>1e-6f || std::abs(direction.x-expected)>1e-5f || std::abs(direction.y-expected)>1e-5f) return 2;
	KineTexture::ScalarTrack scalar;
	scalar.semantics = {"custom:strength", KineTexture::ChannelValueType::Scalar,
		KineTexture::ChannelRepresentation::Absolute, "unitless", "semantic", ""};
	scalar.timebase = {2, 10};
	scalar.provenance = {"synthetic:installed-consumer", "", "strength", "authored"};
	scalar.values = {0, 2, -2};
	if (!scalar.Validate()) return 3;
	float value = 9;
	std::uint8_t valid = 9;
	if (!scalar.Sample(10.25f, value, valid) || value != 1 || valid != 1) return 4;
	std::FILE* trace = argc > 1 ? std::fopen(argv[1], "w") : nullptr;
	if (argc > 1 && !trace) return 5;
	for (int mode = 0; mode != 2; ++mode)
	{
		if (mode) scalar.valid = {1, 0, 1};
		for (float time : {-1.f, 10.f, 10.125f, 10.25f, 10.5f, 10.75f, 11.f, 12.f})
		{
			if (!scalar.Sample(time, value, valid)) return 6;
			if (trace) std::fprintf(trace, "%d,%a,%a,%u\n", mode, double(time), double(value), unsigned(valid));
		}
	}
	if (trace) std::fclose(trace);
	if (!scalar.Sample(10, value, valid) || value != 0 || valid != 1) return 7;
	if (!scalar.Sample(10.25f, value, valid) || valid != 0) return 8;
	std::puts("Installed KineTexture::Core consumer: analytic midpoint PASS");
	std::puts("Installed ScalarTrack consumer: timebase/provenance/zero/missing PASS");
	return 0;
}
