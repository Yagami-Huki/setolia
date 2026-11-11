#pragma once

class QLabel;
class QLineEdit;
class QCheckBox;
class QTimer;
class QTextEdit;
class QPushButton;
class QComboBox;
class QSpinBox;

struct DockViewParts {
	// Realtime Clock
	QLabel *clockLabel = nullptr;
	QLineEdit *clockDisplay = nullptr;
	QCheckBox *clockToggle = nullptr;
	QTimer *clockTimer = nullptr;

	// Reserve, Singing, Setlist
	QTextEdit *reserveText = nullptr;
	QLineEdit *singingText = nullptr;
	QTextEdit *setlistText = nullptr;

	// Next Song
	QPushButton *nextButton = nullptr;
	QCheckBox *nextToggle = nullptr;

	// Timestamp
	QLineEdit *timestampDisplay = nullptr;
	QCheckBox *timestampToggle = nullptr;
	QTimer *timestampTimer = nullptr;
	// Shown after a restart within setolia::kStreamResumeWindowMs of the previous stream.
	QPushButton *resumeTimestampButton = nullptr;

	// Reset
	QPushButton *resetButton = nullptr;

	// Template
	QComboBox *templateComboBox = nullptr;
	QPushButton *openTemplateButton = nullptr;
	QPushButton *reloadTemplateButton = nullptr;
	QPushButton *addButton = nullptr;

	// HTTP server
	QLabel *serverStatusLabel = nullptr;
	QSpinBox *serverPortSpinBox = nullptr;
	QPushButton *applyServerPortButton = nullptr;
	QCheckBox *restrictNextOriginToggle = nullptr;

	// Debounced settings save
	QTimer *saveTimer = nullptr;

	// Lyric Search
	QLineEdit *lyricSearchText = nullptr;
	QPushButton *lyricSearchButton = nullptr;
};
