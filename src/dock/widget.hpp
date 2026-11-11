#pragma once

#include <QWidget>
#include <memory>
#include "view.hpp"

class DockActions;
class Config;

class SetoliaDockWidget : public QWidget {
	Q_OBJECT
public:
	explicit SetoliaDockWidget(QWidget *parent = nullptr);
	~SetoliaDockWidget() override;

	void onStreamingStarted();
	void onStreamingStopped();
	// Called on OBS_FRONTEND_EVENT_FINISHED_LOADING.
	void onFinishedLoading();
	// Saves pending settings and stops the HTTP server and timers. Called on OBS_FRONTEND_EVENT_EXIT, while
	// libobs is still alive; runs once (the destructor calls it only if EXIT was never received).
	void finalize();

private:
	void buildUI();
	void wireSignals();
	void bootstrap();
	void shutdown();

	bool finalized = false;

	DockViewParts viewParts;
	std::unique_ptr<DockActions> actions;
};
