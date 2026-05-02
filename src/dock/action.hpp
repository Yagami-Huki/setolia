#pragma once

#include <QString>
#include <memory>
#include <string>
#include "view.hpp"

class TemplateManager;
class HTTPSyncServer;
class TaggedTextSourceManager;
class ConfigManager;
class Config;

class DockActions {
public:
	explicit DockActions(DockViewParts &viewParts);
	~DockActions();

	void nextSong();
	void resetAll();
	void lyricSearch() const;

	void startClock();
	void stopClock();

	void startStreamingTimer();
	void stopStreamingTimer();
	void reloadTemplates();
	void openTemplateDirectory();
	void addSelectedTemplateToScene();

	void onTextStateChanged();
	void onClockTogglechanged(bool checked);

	// Streaming/HTTP server lifecycle
	void startStreaming();
	void stopStreaming();

	// Configuration lifecycle
	void applyConfig(const Config &config);
	void saveConfig() const;

	const std::string &cacheData() const;

private:
	void notifyStateChanged();
	Config getConfigForSave() const;

	DockViewParts &viewParts;
	std::unique_ptr<TemplateManager> templateManager;
	std::unique_ptr<HTTPSyncServer> httpSyncServer;
	std::unique_ptr<TaggedTextSourceManager> taggedTextSourceManager;
	std::unique_ptr<ConfigManager> configManager;
	int timestampCount = 0;
	std::string cachedData;
};
