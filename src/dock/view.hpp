#pragma once

class QLabel;
class QLineEdit;
class QCheckBox;
class QTimer;
class QTextEdit;
class QPushButton;
class QComboBox;

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

	// Reset
	QPushButton *resetButton = nullptr;

	// Template
	QComboBox *templateComboBox = nullptr;
	QPushButton *openTemplateButton = nullptr;
	QPushButton *reloadTemplateButton = nullptr;
	QPushButton *addButton = nullptr;

	// Lyric Search
	QLineEdit *lyricSearchText = nullptr;
	QPushButton *lyricSearchButton = nullptr;
};