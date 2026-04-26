#pragma once

#include <QWidget>
#include <memory>
#include "view.hpp"

class DockActions;
struct Config;

class SetoliaDockWidget : public QWidget {
	Q_OBJECT
public:
	explicit SetoliaDockWidget(QWidget *parent = nullptr);
	~SetoliaDockWidget() override;

	void onStreamingStarted();
	void onStreamingStopped();

private:
	void buildUI();
	void wireSignals();
	void bootstrap();
	void shutdown();

	DockViewParts viewParts;
	int timestampCount = 0;
	std::unique_ptr<DockActions> actions;
};
