#pragma once

#include <QElapsedTimer>
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

	// Starts the stream timestamp at alreadyElapsedMs (0 when streaming just started).
	void startStreamingTimer(qint64 alreadyElapsedMs = 0);
	void stopStreamingTimer();
	void prepareUserTemplates();
	// Starts the timestamp if OBS is streaming but the timer is not running yet.
	void resumeStreamingTimerIfActive();
	void reloadTemplates();
	void openTemplateDirectory();
	void addSelectedTemplateToScene();

	void onTextStateChanged();
	void onClockTogglechanged(bool checked);
	void onRestrictNextOriginChanged(bool checked);
	// Pushes the dock state to text sources and the HTTP server without scheduling a save.
	void refreshState();

	// Streaming/HTTP server lifecycle
	void startStreaming();
	void stopStreaming();
	// Restarts the HTTP server on the port entered in the dock and rewrites template sources to it.
	void applyServerPort();

	// Configuration lifecycle
	void applyConfig(const Config &config);
	void saveConfig() const;
	// Debounced save: (re)starts a short single-shot timer, so a burst of edits is written once.
	void scheduleSave();
	// Writes a pending save right away (used on exit).
	void flushPendingSave();
	void setAutosaveEnabled(bool enabled) { autosaveEnabled = enabled; }
	// After this, signals from the dock no longer reach OBS (no source sync, no saves). Used on exit, since
	// libobs may be shut down before the widgets are destroyed.
	void deactivate();

	const std::string &cacheData() const;

private:
	void notifyStateChanged();
	bool startServer();
	void updateServerStatus(bool running, int port);
	Config getConfigForSave() const;
	// Timestamp text inserted by nextSong() and shown in the dock.
	QString currentTimestampText() const;
	void updateTimestampDisplay();

	DockViewParts &viewParts;
	std::unique_ptr<TemplateManager> templateManager;
	std::unique_ptr<HTTPSyncServer> httpSyncServer;
	std::unique_ptr<TaggedTextSourceManager> taggedTextSourceManager;
	std::unique_ptr<ConfigManager> configManager;
	// Stream time = streamClock.elapsed() + streamOffsetMs, computed on demand so it does not drift.
	QElapsedTimer streamClock;
	qint64 streamOffsetMs = 0;
	bool autosaveEnabled = false;
	bool active = true;
	std::string cachedData;
};
