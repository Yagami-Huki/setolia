#include "../src/source/timestamp.hpp"

#include <QString>

#include <ostream>

std::ostream &operator<<(std::ostream &os, const QString &s)
{
	return os << '"' << s.toStdString() << '"';
}

#include "test_support.hpp"

namespace {

void expectSplit(int line, const QString &input, const QString &title, const QString &timestamp)
{
	const setolia::TimestampSplit split = setolia::splitTimestampSuffix(input);
	if (split.title != title || !split.timestamp || *split.timestamp != timestamp) {
		setolia_test::reportFailure(__FILE__, line,
					    "split of " + input.toStdString() + " -> " + split.title.toStdString() +
						    " / " +
						    (split.timestamp ? split.timestamp->toStdString() : "<none>"));
	}
}

void expectNoSplit(int line, const QString &input)
{
	const setolia::TimestampSplit split = setolia::splitTimestampSuffix(input);
	if (split.timestamp || split.title != input) {
		setolia_test::reportFailure(__FILE__, line, "unexpected split of " + input.toStdString());
	}
}

void testDockFormat()
{
	// What the dock writes: "<title> - " + display text, which has a leading space for single-digit hours.
	expectSplit(__LINE__, "Song -  0:01:05", "Song", "0:01:05");
	expectSplit(__LINE__, "Song - 12:34:56", "Song", "12:34:56");
	expectSplit(__LINE__, "Song - 0:00:00", "Song", "0:00:00");
}

void testStrictFormatStillSplits()
{
	// Lines that the old strict std::regex split are split the same way.
	expectSplit(__LINE__, "Song - 1:02:03", "Song", "1:02:03");
	expectSplit(__LINE__, "A - B - 1:02:03", "A - B", "1:02:03");
	expectSplit(__LINE__, "曲名 - 1:02:03", "曲名", "1:02:03");
}

void testTolerantWhitespace()
{
	expectSplit(__LINE__, "  Song   -   1:02:03  ", "Song", "1:02:03");
	expectSplit(__LINE__, "Song\t-\t1:02:03", "Song", "1:02:03");
	expectSplit(__LINE__, "Song -1:02:03", "Song", "1:02:03");
	expectSplit(__LINE__, " - 1:02:03", "", "1:02:03");
}

void testNotSplit()
{
	expectNoSplit(__LINE__, "Song");
	expectNoSplit(__LINE__, "");
	expectNoSplit(__LINE__, "Song-1:02:03"); // no space before the dash
	expectNoSplit(__LINE__, "Song - 1:2:03");
	expectNoSplit(__LINE__, "Song - 123:00:00");
	expectNoSplit(__LINE__, "Song - 1:02:03 extra");
	expectNoSplit(__LINE__, "Song 1:02:03");
}

void testFormatElapsedTimestamp()
{
	CHECK_EQ(setolia::formatElapsedTimestamp(0), QStringLiteral(" 0:00:00"));
	CHECK_EQ(setolia::formatElapsedTimestamp(65), QStringLiteral(" 0:01:05"));
	CHECK_EQ(setolia::formatElapsedTimestamp(3600 + 23 * 60 + 45), QStringLiteral(" 1:23:45"));
	CHECK_EQ(setolia::formatElapsedTimestamp(12 * 3600 + 5), QStringLiteral("12:00:05"));
	CHECK_EQ(setolia::formatElapsedTimestamp(123 * 3600), QStringLiteral("123:00:00"));
	CHECK_EQ(setolia::formatElapsedTimestamp(-5), QStringLiteral(" 0:00:00"));
	// Round trip through the splitter.
	expectSplit(__LINE__, "Song - " + setolia::formatElapsedTimestamp(65), "Song", "0:01:05");
}

} // namespace

int main()
{
	testDockFormat();
	testStrictFormatStillSplits();
	testTolerantWhitespace();
	testNotSplit();
	testFormatElapsedTimestamp();
	return setolia_test::finish("timestamp_test");
}
