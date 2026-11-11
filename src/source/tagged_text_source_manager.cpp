#include "tagged_text_source_manager.hpp"
#include "setolia_text_sources.hpp"

#include <obs-module.h>

namespace {
constexpr const char *kTagPrefix = "[SETOLIA_TAG:";
constexpr const char *kTagSuffix = "]";
constexpr const char *kTextSettingKey = "text";
constexpr const char *kTagSettingKey = "setolia_tag";
constexpr const char *kStateSettingKey = "setolia_text_state";
constexpr const char *kStateReserve = "reserve";
constexpr const char *kStateSinging = "singing";
constexpr const char *kStateSetlist = "setlist";

struct CurrentTexts {
	QString reserve;
	QString singing;
	QString setlist;
};

CurrentTexts g_currentTexts;
QMutex g_currentTextsMutex;

struct SyncContext {
	TaggedTextSourceManager::TextState state = TaggedTextSourceManager::TextState::Reserve;
	QString text;
};

bool syncTaggedSourceCallback(void *param, obs_source_t *source)
{
	auto *context = static_cast<SyncContext *>(param);
	if (!context || !source) {
		return true;
	}

	// Cheap type check first, so the settings of unrelated sources are never copied. Compare the unversioned
	// id: sources saved by older versions use v1 of the SETOLIA types, new ones v2.
	if (!is_setolia_text_source_id(obs_source_get_unversioned_id(source))) {
		return true;
	}

	if (TaggedTextSourceManager::sourceState(source) != context->state) {
		return true;
	}

	if (TaggedTextSourceManager::sourceTag(source).isEmpty()) {
		return true;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return true;
	}

	obs_data_set_string(settings, kTextSettingKey, context->text.toUtf8().constData());
	obs_source_update(source, settings);
	obs_data_release(settings);
	return true;
}
} // namespace

void TaggedTextSourceManager::setCurrentText(TextState state, const QString &text)
{
	QMutexLocker locker(&g_currentTextsMutex);

	switch (state) {
	case TextState::Reserve:
		g_currentTexts.reserve = text;
		break;
	case TextState::Singing:
		g_currentTexts.singing = text;
		break;
	case TextState::Setlist:
		g_currentTexts.setlist = text;
		break;
	}
}

QString TaggedTextSourceManager::currentText(TextState state)
{
	QMutexLocker locker(&g_currentTextsMutex);

	switch (state) {
	case TextState::Reserve:
		return g_currentTexts.reserve;
	case TextState::Singing:
		return g_currentTexts.singing;
	case TextState::Setlist:
		return g_currentTexts.setlist;
	}

	return {};
}

bool TaggedTextSourceManager::setSourceTag(obs_source_t *source, const QString &tag)
{
	if (!source) {
		return false;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return false;
	}

	obs_data_set_string(settings, kTagSettingKey, tag.toUtf8().constData());
	obs_source_update(source, settings);
	obs_data_release(settings);
	return true;
}

bool TaggedTextSourceManager::setSourceState(obs_source_t *source, TextState state)
{
	if (!source) {
		return false;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return false;
	}

	obs_data_set_string(settings, kStateSettingKey, stateToSettingValue(state));
	obs_source_update(source, settings);
	obs_data_release(settings);
	return true;
}

QString TaggedTextSourceManager::extractTagFromSourceName(const char *sourceName)
{
	if (!sourceName) {
		return {};
	}

	const QString sourceNameText = QString::fromUtf8(sourceName);
	const QString prefix = QString::fromUtf8(kTagPrefix);
	const QString suffix = QString::fromUtf8(kTagSuffix);
	if (!sourceNameText.startsWith(prefix)) {
		return {};
	}

	const int suffixIndex = sourceNameText.indexOf(suffix, prefix.length());
	if (suffixIndex < 0) {
		return {};
	}

	return sourceNameText.mid(prefix.length(), suffixIndex - prefix.length()).trimmed();
}

QString TaggedTextSourceManager::sourceTag(obs_source_t *source)
{
	if (!source) {
		return {};
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return {};
	}

	const QString tag = QString::fromUtf8(obs_data_get_string(settings, kTagSettingKey)).trimmed();
	obs_data_release(settings);
	if (!tag.isEmpty()) {
		return tag;
	}

	return extractTagFromSourceName(obs_source_get_name(source));
}

TaggedTextSourceManager::TextState TaggedTextSourceManager::sourceState(obs_source_t *source)
{
	if (!source) {
		return TextState::Reserve;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return TextState::Reserve;
	}

	const QString stateValue = QString::fromUtf8(obs_data_get_string(settings, kStateSettingKey)).trimmed();
	obs_data_release(settings);

	return stateFromSettingValue(stateValue);
}

const char *TaggedTextSourceManager::stateToSettingValue(TextState state)
{
	switch (state) {
	case TextState::Reserve:
		return kStateReserve;
	case TextState::Singing:
		return kStateSinging;
	case TextState::Setlist:
		return kStateSetlist;
	}

	return kStateReserve;
}

TaggedTextSourceManager::TextState TaggedTextSourceManager::stateFromSettingValue(const QString &value)
{
	const QString normalized = value.trimmed().toLower();
	if (normalized == QString::fromUtf8(kStateSinging)) {
		return TextState::Singing;
	}
	if (normalized == QString::fromUtf8(kStateSetlist)) {
		return TextState::Setlist;
	}
	return TextState::Reserve;
}

void TaggedTextSourceManager::syncAllTaggedSources(TextState state, const QString &text) const
{
	SyncContext context{state, text};
	obs_enum_sources(syncTaggedSourceCallback, &context);
}
