#include "timestamp.hpp"

#include <QRegularExpression>

namespace setolia {

TimestampSplit splitTimestampSuffix(const QString &line)
{
	static const QRegularExpression kTimestampSuffix(
		QStringLiteral(R"(^\s*(.*?)\s+-\s*([0-9]{1,2}:[0-9]{2}:[0-9]{2})\s*$)"));

	const QRegularExpressionMatch match = kTimestampSuffix.match(line);
	if (!match.hasMatch()) {
		return {line, std::nullopt};
	}
	return {match.captured(1).trimmed(), match.captured(2)};
}

QString formatElapsedTimestamp(std::int64_t elapsedSeconds)
{
	if (elapsedSeconds < 0) {
		elapsedSeconds = 0;
	}
	const qint64 hours = elapsedSeconds / 3600;
	const qint64 minutes = (elapsedSeconds % 3600) / 60;
	const qint64 seconds = elapsedSeconds % 60;
	return QStringLiteral("%1:%2:%3")
		.arg(hours, 2, 10)
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace setolia
