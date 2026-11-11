#include "../src/source/ws_frame.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <string>

namespace {

unsigned byteAt(const std::string &s, size_t i)
{
	return static_cast<unsigned char>(s.at(i));
}

void testShortPayload()
{
	const std::string frame = setolia::buildWsTextFrame("hello");
	CHECK_EQ(frame.size(), size_t(2 + 5));
	CHECK_EQ(byteAt(frame, 0), 0x81u); // FIN + text
	CHECK_EQ(byteAt(frame, 1), 5u);    // no mask bit, 7-bit length
	CHECK(frame.substr(2) == "hello");

	const std::string empty = setolia::buildWsTextFrame("");
	CHECK_EQ(empty.size(), size_t(2));
	CHECK_EQ(byteAt(empty, 1), 0u);

	const std::string max7 = setolia::buildWsTextFrame(std::string(125, 'a'));
	CHECK_EQ(max7.size(), size_t(2 + 125));
	CHECK_EQ(byteAt(max7, 1), 125u);
}

void testMediumPayload()
{
	const std::string min16 = setolia::buildWsTextFrame(std::string(126, 'b'));
	CHECK_EQ(min16.size(), size_t(4 + 126));
	CHECK_EQ(byteAt(min16, 0), 0x81u);
	CHECK_EQ(byteAt(min16, 1), 126u);
	CHECK_EQ(byteAt(min16, 2), 0x00u);
	CHECK_EQ(byteAt(min16, 3), 126u);
	CHECK(min16.substr(4) == std::string(126, 'b'));

	const std::string mid = setolia::buildWsTextFrame(std::string(0x1234, 'c'));
	CHECK_EQ(byteAt(mid, 1), 126u);
	CHECK_EQ(byteAt(mid, 2), 0x12u);
	CHECK_EQ(byteAt(mid, 3), 0x34u);

	const std::string max16 = setolia::buildWsTextFrame(std::string(65535, 'd'));
	CHECK_EQ(max16.size(), size_t(4 + 65535));
	CHECK_EQ(byteAt(max16, 1), 126u);
	CHECK_EQ(byteAt(max16, 2), 0xFFu);
	CHECK_EQ(byteAt(max16, 3), 0xFFu);
}

void testLongPayload()
{
	const std::string min64 = setolia::buildWsTextFrame(std::string(65536, 'e'));
	CHECK_EQ(min64.size(), size_t(10 + 65536));
	CHECK_EQ(byteAt(min64, 0), 0x81u);
	CHECK_EQ(byteAt(min64, 1), 127u);
	// 64-bit big-endian length 0x0000000000010000
	const unsigned expected[8] = {0, 0, 0, 0, 0, 0x01, 0x00, 0x00};
	for (size_t i = 0; i < 8; i++) {
		CHECK_EQ(byteAt(min64, 2 + i), expected[i]);
	}
	CHECK(min64.substr(10) == std::string(65536, 'e'));

	const std::string big = setolia::buildWsTextFrame(std::string(0x123456, 'f'));
	CHECK_EQ(byteAt(big, 1), 127u);
	CHECK_EQ(byteAt(big, 7), 0x12u);
	CHECK_EQ(byteAt(big, 8), 0x34u);
	CHECK_EQ(byteAt(big, 9), 0x56u);
	CHECK_EQ(big.size(), size_t(10 + 0x123456));
}

void testUtf8PayloadLengthIsInBytes()
{
	const std::string text = "\xE6\xAD\x8C"; // "歌" in UTF-8 (3 bytes)
	const std::string frame = setolia::buildWsTextFrame(text);
	CHECK_EQ(byteAt(frame, 1), 3u);
	CHECK(frame.substr(2) == text);
}

void testPingFrame()
{
	const std::string ping = setolia::buildWsPingFrame();
	CHECK_EQ(ping.size(), size_t(2));
	CHECK_EQ(byteAt(ping, 0), 0x89u);
	CHECK_EQ(byteAt(ping, 1), 0x00u);
}

} // namespace

int main()
{
	testShortPayload();
	testMediumPayload();
	testLongPayload();
	testUtf8PayloadLengthIsInBytes();
	testPingFrame();
	return setolia_test::finish("ws_frame_test");
}
