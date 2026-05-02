#include "setolia_text_sources.hpp"
#include "tagged_text_source_manager.hpp"

#include <obs-module.h>

#include <QString>
#include <QUuid>
#include <memory>

namespace {
constexpr const char *kTagSettingKey = "setolia_tag";
constexpr const char *kStateSettingKey = "setolia_text_state";
constexpr const char *kTextSettingKey = "text";

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

struct SetoliaTextSourceData {
	obs_source_t *wrappedSource = nullptr;
	TaggedTextSourceManager::TextState state = TaggedTextSourceManager::TextState::Reserve;
};

const SetoliaTextSourceDescriptor *descriptorById(const char *sourceId)
{
	if (!sourceId) {
		return nullptr;
	}

	for (const auto &descriptor : kDescriptors) {
		if (QString::fromUtf8(descriptor.sourceId) == QString::fromUtf8(sourceId)) {
			return &descriptor;
		}
	}

	return nullptr;
}

const SetoliaTextSourceDescriptor *descriptorFromSource(obs_source_t *source)
{
	if (!source) {
		return nullptr;
	}

	return descriptorById(obs_source_get_id(source));
}

const char *setolia_text_source_get_name(void *typeData)
{
	auto *descriptor = static_cast<const SetoliaTextSourceDescriptor *>(typeData);
	if (!descriptor) {
		return obs_module_text("SETOLIA_TEXT_SOURCE_NAME");
	}
	return obs_module_text(descriptor->displayNameKey);
}

void *setolia_text_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new SetoliaTextSourceData();
	const auto *descriptor = descriptorFromSource(source);
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

	obs_data_set_string(settings, kStateSettingKey,
			    TaggedTextSourceManager::stateToSettingValue(descriptor->state));

	const QString currentText = TaggedTextSourceManager::currentText(descriptor->state);
	if (!currentText.isEmpty()) {
		obs_data_set_string(settings, kTextSettingKey, currentText.toUtf8().constData());
	}

	const char *wrappedSourceId = TaggedTextSourceManager::sourceIdForCurrentPlatform();
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

	return obs_source_properties(data->wrappedSource);
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
	for (const auto &descriptor : kDescriptors) {
		struct obs_source_info sourceInfo = {};
		sourceInfo.id = descriptor.sourceId;
		sourceInfo.type = OBS_SOURCE_TYPE_INPUT;
		sourceInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
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
		sourceInfo.type_data = const_cast<SetoliaTextSourceDescriptor *>(&descriptor);
		obs_register_source(&sourceInfo);
	}
}
