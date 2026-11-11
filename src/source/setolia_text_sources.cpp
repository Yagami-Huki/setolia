#include "setolia_text_sources.hpp"
#include "tagged_text_source_manager.hpp"
#include "timestamp.hpp"

#include <obs-module.h>

#include <QString>
#include <QStringList>
#include <QUuid>

#include <cstring>

namespace {
constexpr const char *kTagSettingKey = "setolia_tag";
constexpr const char *kStateSettingKey = "setolia_text_state";
constexpr const char *kTextSettingKey = "text";
constexpr const char *kSongTitleOnlySettingKey = "setolia_song_title_only";

struct SetoliaTextSourceDescriptor {
	const char *sourceId;
	const char *displayNameKey;
	TaggedTextSourceManager::TextState state;
};

// Keep reserve source ID for backward compatibility with existing scenes.
constexpr SetoliaTextSourceDescriptor kDescriptors[] = {
	{"reserve_list_text_source", "RESERVE_TEXT_SOURCE_NAME", TaggedTextSourceManager::TextState::Reserve},
	{"setolia_singing_text_source", "SINGING_TEXT_SOURCE_NAME", TaggedTextSourceManager::TextState::Singing},
	{"setolia_setlist_text_source", "SETLIST_TEXT_SOURCE_NAME", TaggedTextSourceManager::TextState::Setlist},
};

// Each SETOLIA text source type is registered in two versions (same id, OBS stores the versioned id):
//  - v1 (version 0, id "<id>"): wraps the v1 OBS text sources ("text_gdiplus" / "text_ft2_source"), which OBS
//    itself marks obsolete. Hidden from the "Add Source" menu (OBS_SOURCE_CAP_OBSOLETE) but still used to load
//    sources saved by 1.2.x and earlier, so their look does not change.
//  - v2 (version 2, id "<id>_v2"): wraps the current OBS text sources ("text_gdiplus_v3" /
//    "text_ft2_source_v2"). New sources use this version.
struct SetoliaTextSourceVersion {
	const SetoliaTextSourceDescriptor *descriptor;
	uint32_t version;
	const char *wrappedSourceId;
};

#if defined(_WIN32)
constexpr const char *kWrappedSourceIdV1 = "text_gdiplus";
constexpr const char *kWrappedSourceIdV2 = "text_gdiplus_v3";
#else
constexpr const char *kWrappedSourceIdV1 = "text_ft2_source";
constexpr const char *kWrappedSourceIdV2 = "text_ft2_source_v2";
#endif

constexpr SetoliaTextSourceVersion kVersions[] = {
	{&kDescriptors[0], 0, kWrappedSourceIdV1}, {&kDescriptors[0], 2, kWrappedSourceIdV2},
	{&kDescriptors[1], 0, kWrappedSourceIdV1}, {&kDescriptors[1], 2, kWrappedSourceIdV2},
	{&kDescriptors[2], 0, kWrappedSourceIdV1}, {&kDescriptors[2], 2, kWrappedSourceIdV2},
};

struct SetoliaTextSourceData {
	obs_source_t *wrappedSource = nullptr;
	TaggedTextSourceManager::TextState state = TaggedTextSourceManager::TextState::Reserve;
	bool songTitleOnly = true;
};

QString stripTimestampFromLine(const QString &line)
{
	return setolia::splitTimestampSuffix(line).title;
}

QString displayTextForSettings(obs_data_t *settings, bool songTitleOnly)
{
	if (!settings) {
		return {};
	}

	const QString text = QString::fromUtf8(obs_data_get_string(settings, kTextSettingKey));
	if (!songTitleOnly || text.isEmpty()) {
		return text;
	}

	QStringList lines = text.split("\n", Qt::KeepEmptyParts);
	for (QString &line : lines) {
		line = stripTimestampFromLine(line);
	}
	return lines.join("\n");
}

QString displayTextFromRawText(const QString &rawText, bool songTitleOnly)
{
	if (!songTitleOnly || rawText.isEmpty()) {
		return rawText;
	}

	QStringList lines = rawText.split("\n", Qt::KeepEmptyParts);
	for (QString &line : lines) {
		line = stripTimestampFromLine(line);
	}
	return lines.join("\n");
}

const SetoliaTextSourceVersion *versionFromSource(obs_source_t *source)
{
	if (!source) {
		return nullptr;
	}
	return static_cast<const SetoliaTextSourceVersion *>(obs_source_get_type_data(source));
}

const char *setolia_text_source_get_name(void *typeData)
{
	auto *version = static_cast<const SetoliaTextSourceVersion *>(typeData);
	if (!version) {
		return obs_module_text("SETOLIA_TEXT_SOURCE_NAME");
	}
	return obs_module_text(version->descriptor->displayNameKey);
}

void *setolia_text_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new SetoliaTextSourceData();
	const auto *version = versionFromSource(source);
	const auto *descriptor = version ? version->descriptor : nullptr;
	if (descriptor) {
		data->state = descriptor->state;
	}

	if (!settings || !source || !descriptor) {
		return data;
	}

	QString tag = QString::fromUtf8(obs_data_get_string(settings, kTagSettingKey)).trimmed();
	if (tag.isEmpty()) {
		tag = QUuid::createUuid().toString(QUuid::WithoutBraces);
		obs_data_set_string(settings, kTagSettingKey, tag.toUtf8().constData());
	}

	if (!obs_data_has_user_value(settings, kSongTitleOnlySettingKey)) {
		obs_data_set_bool(settings, kSongTitleOnlySettingKey, true);
	}
	data->songTitleOnly = obs_data_get_bool(settings, kSongTitleOnlySettingKey);

	obs_data_set_string(settings, kStateSettingKey,
			    TaggedTextSourceManager::stateToSettingValue(descriptor->state));

	const QString currentText = TaggedTextSourceManager::currentText(descriptor->state);
	if (!currentText.isEmpty()) {
		obs_data_set_string(settings, kTextSettingKey, currentText.toUtf8().constData());
		const QString displayText = displayTextForSettings(settings, data->songTitleOnly);
		obs_data_set_string(settings, kTextSettingKey, displayText.toUtf8().constData());
	}

	const char *wrappedSourceId = version->wrappedSourceId;
	const char *sourceName = obs_source_get_name(source);

	data->wrappedSource = obs_source_create_private(wrappedSourceId, sourceName, settings);
	if (!data->wrappedSource) {
		blog(LOG_ERROR, "setolia_text_source: Failed to create wrapped text source");
		return data;
	}

	TaggedTextSourceManager::setSourceTag(data->wrappedSource, tag);
	TaggedTextSourceManager::setSourceState(data->wrappedSource, descriptor->state);
	return data;
}

void setolia_text_source_destroy(void *dataPtr)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data) {
		return;
	}

	if (data->wrappedSource) {
		obs_source_release(data->wrappedSource);
	}

	delete data;
}

void setolia_text_source_get_defaults(obs_data_t *settings)
{
	if (!settings) {
		return;
	}

	obs_data_set_default_string(settings, kTextSettingKey, "");
	obs_data_set_default_string(settings, kTagSettingKey, "");
	obs_data_set_default_bool(settings, kSongTitleOnlySettingKey, true);
	obs_data_set_default_string(
		settings, kStateSettingKey,
		TaggedTextSourceManager::stateToSettingValue(TaggedTextSourceManager::TextState::Reserve));
}

obs_properties_t *setolia_text_source_get_properties(void *dataPtr)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data || !data->wrappedSource) {
		return nullptr;
	}

	obs_properties_t *properties = obs_source_properties(data->wrappedSource);
	if (!properties) {
		return nullptr;
	}

	if (!obs_properties_get(properties, kSongTitleOnlySettingKey)) {
		obs_properties_add_bool(properties, kSongTitleOnlySettingKey,
					obs_module_text("SETOLIA_TEXT_SOURCE_SONG_TITLE_ONLY"));
	}
	return properties;
}

void setolia_text_source_update(void *dataPtr, obs_data_t *settings)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data || !data->wrappedSource || !settings) {
		return;
	}

	QString tag = QString::fromUtf8(obs_data_get_string(settings, kTagSettingKey)).trimmed();
	if (tag.isEmpty()) {
		tag = TaggedTextSourceManager::sourceTag(data->wrappedSource);
	}
	if (!tag.isEmpty()) {
		obs_data_set_string(settings, kTagSettingKey, tag.toUtf8().constData());
	}

	if (!obs_data_has_user_value(settings, kSongTitleOnlySettingKey)) {
		obs_data_set_bool(settings, kSongTitleOnlySettingKey, true);
	}
	data->songTitleOnly = obs_data_get_bool(settings, kSongTitleOnlySettingKey);

	QString canonicalText = TaggedTextSourceManager::currentText(data->state);
	if (canonicalText.isEmpty()) {
		canonicalText = QString::fromUtf8(obs_data_get_string(settings, kTextSettingKey));
	}
	obs_data_set_string(settings, kTextSettingKey, canonicalText.toUtf8().constData());

	const QString displayText = displayTextFromRawText(canonicalText, data->songTitleOnly);
	obs_data_set_string(settings, kTextSettingKey, displayText.toUtf8().constData());

	obs_data_set_string(settings, kStateSettingKey, TaggedTextSourceManager::stateToSettingValue(data->state));
	obs_source_update(data->wrappedSource, settings);
}

void setolia_text_source_video_render(void *dataPtr, gs_effect_t *effect)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data || !data->wrappedSource) {
		return;
	}

	obs_source_video_render(data->wrappedSource);
	(void)effect;
}

uint32_t setolia_text_source_get_width(void *dataPtr)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data || !data->wrappedSource) {
		return 0;
	}

	return obs_source_get_width(data->wrappedSource);
}

uint32_t setolia_text_source_get_height(void *dataPtr)
{
	auto *data = static_cast<SetoliaTextSourceData *>(dataPtr);
	if (!data || !data->wrappedSource) {
		return 0;
	}

	return obs_source_get_height(data->wrappedSource);
}
} // namespace

void register_setolia_text_sources()
{
	for (const auto &version : kVersions) {
		struct obs_source_info sourceInfo = {};
		sourceInfo.id = version.descriptor->sourceId;
		sourceInfo.version = version.version;
		sourceInfo.type = OBS_SOURCE_TYPE_INPUT;
		sourceInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
		if (version.version == 0) {
			sourceInfo.output_flags |= OBS_SOURCE_CAP_OBSOLETE;
		}
		sourceInfo.get_name = setolia_text_source_get_name;
		sourceInfo.create = setolia_text_source_create;
		sourceInfo.destroy = setolia_text_source_destroy;
		sourceInfo.get_defaults = setolia_text_source_get_defaults;
		sourceInfo.get_properties = setolia_text_source_get_properties;
		sourceInfo.update = setolia_text_source_update;
		sourceInfo.video_render = setolia_text_source_video_render;
		sourceInfo.get_width = setolia_text_source_get_width;
		sourceInfo.get_height = setolia_text_source_get_height;
		sourceInfo.enum_active_sources = nullptr;
		sourceInfo.icon_type = OBS_ICON_TYPE_TEXT;
		sourceInfo.type_data = const_cast<SetoliaTextSourceVersion *>(&version);
		obs_register_source(&sourceInfo);
	}
}

bool is_setolia_text_source_id(const char *unversionedId)
{
	if (!unversionedId) {
		return false;
	}
	for (const auto &descriptor : kDescriptors) {
		if (strcmp(descriptor.sourceId, unversionedId) == 0) {
			return true;
		}
	}
	return false;
}
