#pragma once

#include "dock/widget.hpp"

class SETOLIA_Dock : public SetoliaDockWidget {
	Q_OBJECT
public:
	explicit SETOLIA_Dock(QWidget *parent = nullptr);

	void StartTimer();
	void StopTimer();
};
