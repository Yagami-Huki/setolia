#pragma once

// Minimal server-to-client WebSocket frame builders (RFC 6455).
// Header-only and independent of OBS / Qt so it can be unit tested on its own.

#include <cstddef>
#include <cstdint>
#include <string>

namespace setolia {

namespace ws_detail {

// Builds a single unmasked frame with FIN set. Server-to-client frames must not be masked.
inline std::string buildWsFrame(std::uint8_t opcode, const std::string &payload)
{
	const std::uint64_t len = payload.size();
	std::string frame;
	frame.reserve(payload.size() + 10);
	frame.push_back(static_cast<char>(0x80 | (opcode & 0x0F))); // FIN + opcode

	if (len <= 125) {
		frame.push_back(static_cast<char>(len));
	} else if (len <= 0xFFFF) {
		frame.push_back(static_cast<char>(126));
		frame.push_back(static_cast<char>((len >> 8) & 0xFF));
		frame.push_back(static_cast<char>(len & 0xFF));
	} else {
		frame.push_back(static_cast<char>(127));
		for (int i = 7; i >= 0; i--) {
			frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
		}
	}

	frame += payload;
	return frame;
}

} // namespace ws_detail

constexpr std::uint8_t kWsOpcodeText = 0x1;
constexpr std::uint8_t kWsOpcodePing = 0x9;

// Text frame (opcode 0x1) carrying `text` (expected to be UTF-8).
inline std::string buildWsTextFrame(const std::string &text)
{
	return ws_detail::buildWsFrame(kWsOpcodeText, text);
}

// Ping frame (opcode 0x9) with an empty payload: 0x89 0x00.
inline std::string buildWsPingFrame()
{
	return ws_detail::buildWsFrame(kWsOpcodePing, std::string());
}

} // namespace setolia
