#pragma once

#include <QString>

#include <cstdint>
#include <optional>

namespace setolia {

struct TimestampSplit {
	QString title;                    // the line without the timestamp suffix
	std::optional<QString> timestamp; // "h:mm:ss" (or "hh:mm:ss"), without surrounding spaces
};

// Splits "<title> - <h:mm:ss>" into title and timestamp. Whitespace around the dash and at both ends is
// tolerated (the dock writes "<title> -  0:12:34" for single-digit hours), but at least one space is
// required before the dash so that titles such as "A-1:23:45" are not split. Lines without a timestamp
// suffix are returned unchanged with no timestamp.
TimestampSplit splitTimestampSuffix(const QString &line);

// Formats an elapsed time the way the dock always has: hours right-aligned to two characters (so a single
// digit gets a leading space, e.g. " 0:01:05"), minutes and seconds zero-padded. Negative values count as 0.
QString formatElapsedTimestamp(std::int64_t elapsedSeconds);

} // namespace setolia
