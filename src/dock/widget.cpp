#include "widget.hpp"

#include "action.hpp"
#include "../config/config_manager.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

SetoliaDockWidget::SetoliaDockWidget(QWidget *parent) : QWidget(parent)
{
	buildUI();
	actions = std::make_unique<DockActions>(viewParts);
	wireSignals();
	bootstrap();
}

SetoliaDockWidget::~SetoliaDockWidget()
{
	shutdown();
}

void SetoliaDockWidget::onStreamingStarted()
{
	actions->startStreamingTimer();
}

void SetoliaDockWidget::onStreamingStopped()
{
	actions->stopStreamingTimer();
}

void SetoliaDockWidget::buildUI()
{
	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);

	auto *scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *content = new QWidget(scrollArea);
	auto *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(10, 10, 10, 10);
	contentLayout->setSpacing(5);
	contentLayout->setAlignment(Qt::AlignCenter | Qt::AlignTop);

	viewParts.clockLabel = new QLabel(obs_module_text("CLOCK_LABEL"), content);
	viewParts.clockLabel->setStyleSheet("font-weight: bold;");
	viewParts.clockLabel->setAlignment(Qt::AlignCenter);
	viewParts.clockLabel->setVisible(false);

	viewParts.clockDisplay = new QLineEdit("0:00:00", content);
	viewParts.clockDisplay->setStyleSheet("font-weight: bold; font-size: 28px;");
	viewParts.clockDisplay->setFixedHeight(50);
	viewParts.clockDisplay->setAlignment(Qt::AlignCenter);
	viewParts.clockDisplay->setReadOnly(true);
	viewParts.clockDisplay->setVisible(false);

	viewParts.clockToggle = new QCheckBox(obs_module_text("TOGGLE_CLOCK"), content);
	viewParts.clockToggle->setChecked(false);
	viewParts.clockTimer = new QTimer(this);

	auto *reserveLabel = new QLabel(obs_module_text("RESERVE_LABEL"), content);
	reserveLabel->setStyleSheet("font-weight: bold;");
	viewParts.reserveText = new QTextEdit(content);
	viewParts.reserveText->setAcceptRichText(false);
	viewParts.reserveText->setFixedHeight(200);

	auto *singingLabel = new QLabel(obs_module_text("SINGING_LABEL"), content);
	singingLabel->setStyleSheet("font-weight: bold;");
	viewParts.singingText = new QLineEdit(content);

	auto *setlistLabel = new QLabel(obs_module_text("SETLIST_LABEL"), content);
	setlistLabel->setStyleSheet("font-weight: bold;");
	viewParts.setlistText = new QTextEdit(content);
	viewParts.setlistText->setAcceptRichText(false);
	viewParts.setlistText->setFixedHeight(200);

	viewParts.nextButton = new QPushButton(obs_module_text("NEXT_BUTTON"), content);
	viewParts.nextButton->setCursor(Qt::PointingHandCursor);
	viewParts.nextToggle = new QCheckBox(obs_module_text("TOGGLE_NEXT"), content);
	viewParts.nextToggle->setChecked(true);

	viewParts.timestampDisplay = new QLineEdit("0:00:00", content);
	viewParts.timestampDisplay->setStyleSheet("font-weight: bold; font-size: 14px;");
	viewParts.timestampDisplay->setAlignment(Qt::AlignCenter);
	viewParts.timestampDisplay->setReadOnly(true);
	viewParts.timestampToggle = new QCheckBox(obs_module_text("TOGGLE_TIMESTAMP"), content);
	viewParts.timestampToggle->setChecked(false);
	viewParts.timestampTimer = new QTimer(this);

	auto *lyricLabel = new QLabel(obs_module_text("LYRIC_SEARCH_LABEL"), content);
	lyricLabel->setStyleSheet("font-weight: bold;");
	auto *lyricLayout = new QHBoxLayout();
	viewParts.lyricSearchText = new QLineEdit(content);
	viewParts.lyricSearchText->setPlaceholderText(obs_module_text("LYRIC_SEARCH_PLACEHOLDER"));
	viewParts.lyricSearchButton = new QPushButton(obs_module_text("LYRIC_SEARCH_BUTTON"), content);
	viewParts.lyricSearchButton->setCursor(Qt::PointingHandCursor);
	lyricLayout->addWidget(viewParts.lyricSearchText);
	lyricLayout->addWidget(viewParts.lyricSearchButton);

	viewParts.resetButton = new QPushButton(obs_module_text("RESET_BUTTON"), content);
	viewParts.resetButton->setStyleSheet("color: red;");
	viewParts.resetButton->setCursor(Qt::PointingHandCursor);

	viewParts.templateComboBox = new QComboBox(content);
	viewParts.openTemplateButton = new QPushButton(obs_module_text("OPEN_TEMPLATE_BUTTON"), content);
	viewParts.reloadTemplateButton = new QPushButton(obs_module_text("RELOAD_TEMPLATE_BUTTON"), content);
	viewParts.addButton = new QPushButton(obs_module_text("ADD_BUTTON"), content);

	contentLayout->addWidget(viewParts.clockLabel);
	contentLayout->addWidget(viewParts.clockDisplay);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(reserveLabel);
	contentLayout->addWidget(viewParts.reserveText);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(singingLabel);
	contentLayout->addWidget(viewParts.singingText);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(setlistLabel);
	contentLayout->addWidget(viewParts.setlistText);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(viewParts.nextButton);
	contentLayout->addWidget(viewParts.timestampDisplay);
	contentLayout->addWidget(viewParts.timestampToggle);
	contentLayout->addWidget(lyricLabel);
	contentLayout->addLayout(lyricLayout);
	contentLayout->addWidget(viewParts.resetButton);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(viewParts.templateComboBox);
	contentLayout->addWidget(viewParts.openTemplateButton);
	contentLayout->addWidget(viewParts.reloadTemplateButton);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(viewParts.addButton);
	contentLayout->addSpacing(10);
	contentLayout->addWidget(viewParts.clockToggle);
	contentLayout->addWidget(viewParts.nextToggle);
	contentLayout->addStretch();

	scrollArea->setWidget(content);
	rootLayout->addWidget(scrollArea);
	setLayout(rootLayout);

	setObjectName("SETOLIA_Dock");
	setWindowTitle("SETOLIA");
	setWindowFlags(Qt::Widget | Qt::WindowStaysOnTopHint);
	setMinimumSize(250, 400);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setAttribute(Qt::WA_DeleteOnClose);
}

void SetoliaDockWidget::wireSignals()
{
	connect(viewParts.reserveText, &QTextEdit::textChanged, this, [this]() { actions->onTextStateChanged(); });
	connect(viewParts.singingText, &QLineEdit::textChanged, this, [this]() { actions->onTextStateChanged(); });
	connect(viewParts.setlistText, &QTextEdit::textChanged, this, [this]() { actions->onTextStateChanged(); });

	connect(viewParts.nextButton, &QPushButton::clicked, this, [this]() { actions->nextSong(); });
	connect(viewParts.resetButton, &QPushButton::clicked, this, [this]() { actions->resetAll(); });
	connect(viewParts.lyricSearchButton, &QPushButton::clicked, this, [this]() { actions->lyricSearch(); });
	connect(viewParts.openTemplateButton, &QPushButton::clicked, this,
		[this]() { actions->openTemplateDirectory(); });
	connect(viewParts.reloadTemplateButton, &QPushButton::clicked, this, [this]() { actions->reloadTemplates(); });
	connect(viewParts.addButton, &QPushButton::clicked, this, [this]() { actions->addSelectedTemplateToScene(); });

	connect(viewParts.clockToggle, &QCheckBox::toggled, this,
		[this](bool checked) { actions->onClockTogglechanged(checked); });
}

void SetoliaDockWidget::bootstrap()
{
	actions->reloadTemplates();

	// Load saved configuration
	Config config;
	auto configManager = std::make_unique<ConfigManager>();
	if (configManager->loadConfig(config)) {
		actions->applyConfig(config);
	}
	actions->onTextStateChanged();
	actions->startStreaming();
}

void SetoliaDockWidget::shutdown()
{
	actions->saveConfig();
	actions->stopStreaming();
	actions->stopClock();
	actions->stopStreamingTimer();
}
