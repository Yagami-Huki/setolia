#pragma once

#include <obs-module.h>

#include <QString>
#include <QMutex>

class TaggedTextSourceManager {
public:
	enum class TextState {
		Reserve,
		Singing,
		Setlist,
	};

	TaggedTextSourceManager() = default;

	static void setCurrentText(TextState state, const QString &text);
	static QString currentText(TextState state);

	bool createTaggedTextSource(const QString &sourceName, const QString &tag, const QString &text) const;
	void syncTextToTaggedSources(TextState state, const QString &tag, const QString &text) const;
	void syncAllTaggedSources(TextState state, const QString &text) const;
	static bool setSourceTag(obs_source_t *source, const QString &tag);
	static bool setSourceState(obs_source_t *source, TextState state);

	static QString buildTaggedSourceName(const QString &sourceName, const QString &tag);
	static QString extractTagFromSourceName(const char *sourceName);
	static const char *sourceIdForCurrentPlatform();
	static bool isTaggedTextSourceName(const char *sourceName);
	static QString sourceTag(obs_source_t *source);
	static TextState sourceState(obs_source_t *source);
	static const char *stateToSettingValue(TextState state);
	static TextState stateFromSettingValue(const QString &value);
};
