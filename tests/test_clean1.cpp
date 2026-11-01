#include <KineTexture/KineTexture.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

int main()
{
	using namespace KineTexture;
	int checks = 0;
	#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (false)
	ScalarTrack track;
	track.semantics = {"performer:intent", ChannelValueType::Scalar, ChannelRepresentation::Absolute, "unitless", "semantic", ""};
	track.timebase = {2, 10};
	track.provenance = {"synthetic:clean1", "", "test", "authored"};
	track.values = {0, 2, -2};
	CHECK(track.Validate());
	float value = 99;
	std::uint8_t valid = 9;
	CHECK(track.Sample(10.25f, value, valid) && value == 1 && valid == 1);
	CHECK(track.Sample(-1, value, valid) && value == 0 && valid == 1);
	CHECK(track.Sample(std::numeric_limits<float>::max(), value, valid) && value == -2 && valid == 1);
	track.valid = {1, 0, 1};
	track.values[1] = std::numeric_limits<float>::quiet_NaN();
	CHECK(track.Validate());
	CHECK(track.Sample(10, value, valid) && value == 0 && valid == 1);
	CHECK(track.Sample(10.25f, value, valid) && value == 0 && valid == 0);
	CHECK(track.Sample(10.5f, value, valid) && value == 0 && valid == 0);
	CHECK(track.Sample(11, value, valid) && value == -2 && valid == 1);
	track.semantics.representation = ChannelRepresentation::Delta;
	CHECK(!track.Validate());
	track.semantics.reference_id = "neutral:v1";
	CHECK(track.Validate());
	track.values.clear(); track.valid.clear();
	CHECK(track.Sample(10, value, valid) && value == 0 && valid == 0);
	track.provenance.source_id.clear();
	CHECK(!track.Validate());
	std::array<float, 4> values{0, 1, 2, -1};
	std::array<float, 2> out{9, 9};
	std::array<std::uint8_t, 2> available{9, 9};
	DenseScalarTracksView view{2, 2, 2, values, {}};
	CHECK(ValidateDenseScalarTracks(view));
	CHECK(SampleDenseScalarTracks(view, .25f, out, available) && out[0] == 1 && out[1] == 0 && available[1] == 1);
	const auto saved = out;
	CHECK(!SampleDenseScalarTracks(view, std::numeric_limits<float>::infinity(), out, available));
	CHECK(out == saved && available[0] == 1 && available[1] == 1);
	values[3] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!SampleDenseScalarTracks(view, .25f, out, available));
	CHECK(out == saved && available[0] == 1 && available[1] == 1);
	values = {-std::numeric_limits<float>::max(), 0, std::numeric_limits<float>::max(), 0};
	CHECK(!SampleDenseScalarTracks(view, .25f, out, available));
	CHECK(out == saved);
	CHECK(SampleDenseScalarTracks(view, 0, out) && out[0] == values[0]);
	const auto unmasked = out;
	std::array<std::uint8_t, 4> allPresent{1, 1, 1, 1};
	view.valid = allPresent;
	CHECK(SampleDenseScalarTracks(view, 0, out, available) && out == unmasked);
	std::printf("KineTexture Clean-1: %d checks PASS\n", checks);
	return 0;
}
