#pragma once

#include <QByteArray>

#include <cstdint>
#include <optional>

namespace setolia {

// Stream timing that survives an OBS restart, so a stream that YouTube kept waiting for can be continued.
// Times are wall-clock milliseconds since the epoch.
struct StreamSession {
	std::int64_t startMs = 0;     // when the stream (as seen by viewers / the archive) started
	std::int64_t lastAliveMs = 0; // last time the plugin saw the stream running (heartbeat or stop)
};

// While streaming, lastAliveMs is refreshed this often.
constexpr std::int64_t kStreamSessionHeartbeatMs = 10 * 1000;
// A new stream started within this time after the previous one stopped may continue its timestamps
// (YouTube ends a broadcast with auto-stop about a minute after the input stops).
constexpr std::int64_t kStreamResumeWindowMs = 60 * 1000;

// True if a stream starting at nowMs may continue `previous`: it stopped at most windowMs ago and the clock did
// not go backwards.
bool canResumeStreamSession(const std::optional<StreamSession> &previous, std::int64_t nowMs,
			    std::int64_t windowMs = kStreamResumeWindowMs);

// Stream time at nowMs when continuing `previous`. The interruption is included: the archive keeps running
// while the broadcast waits for the input.
std::int64_t resumedStreamElapsedMs(const StreamSession &previous, std::int64_t nowMs);

QByteArray streamSessionToJson(const StreamSession &session);
// nullopt for invalid or inconsistent data.
std::optional<StreamSession> streamSessionFromJson(const QByteArray &json);

} // namespace setolia
