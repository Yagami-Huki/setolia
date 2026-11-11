#include "../src/dock/stream_session.hpp"

#include "test_support.hpp"

namespace {

using setolia::StreamSession;

constexpr std::int64_t kStart = 1'700'000'000'000; // some wall-clock time in ms
constexpr std::int64_t kMinute = 60 * 1000;

void testCanResume()
{
	const StreamSession previous{kStart, kStart + 47 * kMinute};
	// Restarted 30 s / exactly 60 s after the last heartbeat: may continue.
	CHECK(setolia::canResumeStreamSession(previous, previous.lastAliveMs + 30 * 1000));
	CHECK(setolia::canResumeStreamSession(previous, previous.lastAliveMs + kMinute));
	// Too late, no previous stream, clock went backwards, broken record.
	CHECK(!setolia::canResumeStreamSession(previous, previous.lastAliveMs + kMinute + 1));
	CHECK(!setolia::canResumeStreamSession(std::nullopt, kStart));
	CHECK(!setolia::canResumeStreamSession(previous, previous.lastAliveMs - 1));
	CHECK(!setolia::canResumeStreamSession(StreamSession{0, 10}, 20));
	CHECK(!setolia::canResumeStreamSession(StreamSession{kStart + 10, kStart}, kStart + 20));
}

void testResumedElapsedIncludesInterruption()
{
	// Started 21:03, interrupted 21:50, resumed 21:51 -> 48 minutes (the minute of interruption counts).
	const StreamSession previous{kStart, kStart + 47 * kMinute};
	CHECK_EQ(setolia::resumedStreamElapsedMs(previous, kStart + 48 * kMinute), 48 * kMinute);
	CHECK_EQ(setolia::resumedStreamElapsedMs(previous, kStart - 5), std::int64_t{0});
}

void testJsonRoundTrip()
{
	const StreamSession session{kStart, kStart + 123456};
	const auto parsed = setolia::streamSessionFromJson(setolia::streamSessionToJson(session));
	CHECK(parsed.has_value());
	if (parsed) {
		CHECK_EQ(parsed->startMs, session.startMs);
		CHECK_EQ(parsed->lastAliveMs, session.lastAliveMs);
	}
}

void testInvalidJson()
{
	CHECK(!setolia::streamSessionFromJson(""));
	CHECK(!setolia::streamSessionFromJson("[]"));
	CHECK(!setolia::streamSessionFromJson(R"({"startMs":"x","lastAliveMs":"1"})"));
	CHECK(!setolia::streamSessionFromJson(R"({"startMs":"20","lastAliveMs":"10"})"));
	CHECK(!setolia::streamSessionFromJson(R"({"startMs":"0","lastAliveMs":"10"})"));
}

} // namespace

int main()
{
	testCanResume();
	testResumedElapsedIncludesInterruption();
	testJsonRoundTrip();
	testInvalidJson();
	return setolia_test::finish("stream_session_test");
}
