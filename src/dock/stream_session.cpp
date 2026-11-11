#include "stream_session.hpp"

#include <QJsonDocument>
#include <QJsonObject>

namespace setolia {

bool canResumeStreamSession(const std::optional<StreamSession> &previous, std::int64_t nowMs, std::int64_t windowMs)
{
	if (!previous) {
		return false;
	}
	const std::int64_t sinceStop = nowMs - previous->lastAliveMs;
	return previous->startMs > 0 && previous->startMs <= previous->lastAliveMs && sinceStop >= 0 &&
	       sinceStop <= windowMs;
}

std::int64_t resumedStreamElapsedMs(const StreamSession &previous, std::int64_t nowMs)
{
	const std::int64_t elapsed = nowMs - previous.startMs;
	return elapsed > 0 ? elapsed : 0;
}

QByteArray streamSessionToJson(const StreamSession &session)
{
	QJsonObject object;
	// Stored as strings: JSON numbers are doubles in Qt, and milliseconds since the epoch fit, but strings keep
	// the file readable and exact.
	object["startMs"] = QString::number(session.startMs);
	object["lastAliveMs"] = QString::number(session.lastAliveMs);
	return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<StreamSession> streamSessionFromJson(const QByteArray &json)
{
	const QJsonDocument doc = QJsonDocument::fromJson(json);
	if (!doc.isObject()) {
		return std::nullopt;
	}
	bool okStart = false;
	bool okAlive = false;
	StreamSession session;
	session.startMs = doc.object().value("startMs").toString().toLongLong(&okStart);
	session.lastAliveMs = doc.object().value("lastAliveMs").toString().toLongLong(&okAlive);
	if (!okStart || !okAlive || session.startMs <= 0 || session.startMs > session.lastAliveMs) {
		return std::nullopt;
	}
	return session;
}

} // namespace setolia
