#include "action.hpp"

#include <obs-module.h>

#include "../source/template_manager.hpp"
#include "../source/http_sync_server.hpp"
#include "../source/tagged_text_source_manager.hpp"
#include "../config/config_manager.hpp"
#include "../source/template_paths.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
#include <QUrl>

#include <regex>

namespace {
QStringList splitTimestamp(const QString &text)
{
	const std::string input = text.toStdString();
	const std::regex re(R"(^(.*) - (\d{1,2}:\d{2}:\d{2})$)");
	std::smatch matches;
	if (std::regex_match(input, matches, re)) {
		return QStringList({QString::fromStdString(matches[1]), QString::fromStdString(matches[2])});
	}
	return QStringList({text});
}

std::string buildCachedData(const DockViewParts &viewParts)
{
	QJsonObject jsonObj = QJsonObject();
	jsonObj["reserve"] =
		QJsonArray::fromStringList(viewParts.reserveText->toPlainText().split("\n", Qt::SkipEmptyParts));
	jsonObj["singing"] = QJsonArray::fromStringList(splitTimestamp(viewParts.singingText->text()));

	QJsonArray setlist;
	for (const QString &line : viewParts.setlistText->toPlainText().split("\n", Qt::SkipEmptyParts)) {
		setlist.append(QJsonArray::fromStringList(splitTimestamp(line)));
	}
	jsonObj["setlist"] = setlist;

	return QJsonDocument(jsonObj).toJson(QJsonDocument::Compact).constData();
}
} // namespace

DockActions::DockActions(DockViewParts &viewParts_) : viewParts(viewParts_)
{
	templateManager = std::make_unique<TemplateManager>();
	httpSyncServer = std::make_unique<HTTPSyncServer>();
	taggedTextSourceManager = std::make_unique<TaggedTextSourceManager>();
	configManager = std::make_unique<ConfigManager>();

	// Setup callback for external HTTP /next command
	httpSyncServer->setNextSongCallback([this]() {
		QObject *contextObject = viewParts.nextButton;
		if (!contextObject) {
			blog(LOG_WARNING, "DockActions: Cannot dispatch nextSong, UI context is null");
			return;
		}

		QMetaObject::invokeMethod(contextObject, [this]() { nextSong(); }, Qt::QueuedConnection);
	});
}

DockActions::~DockActions()
{
	if (httpSyncServer) {
		httpSyncServer->stopServer();
	}
}

void DockActions::nextSong()
{
	viewParts.reserveText->blockSignals(true);
	viewParts.singingText->blockSignals(true);
	viewParts.setlistText->blockSignals(true);

	if (viewParts.nextToggle->isChecked()) {
		const QString singingText = viewParts.singingText->text().trimmed();
		if (!singingText.isEmpty()) {
			const QString currentSetlist = viewParts.setlistText->toPlainText();
			if (!currentSetlist.isEmpty()) {
				viewParts.setlistText->setPlainText(currentSetlist + "\n" + singingText);
			} else {
				viewParts.setlistText->setPlainText(singingText);
			}
			viewParts.singingText->clear();
		} else {
			QStringList reserveLines = viewParts.reserveText->toPlainText().split("\n", Qt::KeepEmptyParts);
			if (!reserveLines.isEmpty()) {
				QString topLine = reserveLines.first().trimmed();
				if (viewParts.timestampToggle->isChecked() && !topLine.isEmpty()) {
					topLine += " - " + viewParts.timestampDisplay->text();
				}
				if (!topLine.isEmpty()) {
					viewParts.singingText->setText(topLine);
					reserveLines.removeFirst();
					viewParts.reserveText->setPlainText(reserveLines.join("\n"));
				}
			}
		}
	} else {
		const QString singingText = viewParts.singingText->text().trimmed();
		if (!singingText.isEmpty()) {
			const QString currentSetlist = viewParts.setlistText->toPlainText();
			if (!currentSetlist.isEmpty()) {
				viewParts.setlistText->setPlainText(currentSetlist + "\n" + singingText);
			} else {
				viewParts.setlistText->setPlainText(singingText);
			}
			viewParts.singingText->clear();
		}

		QStringList reserveLines = viewParts.reserveText->toPlainText().split("\n", Qt::KeepEmptyParts);
		if (!reserveLines.isEmpty()) {
			QString topLine = reserveLines.first().trimmed();
			if (viewParts.timestampToggle->isChecked() && !topLine.isEmpty()) {
				topLine += " - " + viewParts.timestampDisplay->text();
			}
			if (!topLine.isEmpty()) {
				viewParts.singingText->setText(topLine);
				reserveLines.removeFirst();
				viewParts.reserveText->setPlainText(reserveLines.join("\n"));
			}
		}
	}

	viewParts.reserveText->blockSignals(false);
	viewParts.singingText->blockSignals(false);
	viewParts.setlistText->blockSignals(false);

	notifyStateChanged();
}

void DockActions::resetAll()
{
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(nullptr, QString::fromUtf8(obs_module_text("RESET_CONFIRM_TITLE")),
				      QString::fromUtf8(obs_module_text("RESET_CONFIRM_TEXT")),
				      QMessageBox::Ok | QMessageBox::Cancel);
	if (reply != QMessageBox::Ok) {
		return;
	}

	viewParts.reserveText->blockSignals(true);
	viewParts.singingText->blockSignals(true);
	viewParts.setlistText->blockSignals(true);

	viewParts.reserveText->clear();
	viewParts.singingText->clear();
	viewParts.setlistText->clear();

	viewParts.reserveText->blockSignals(false);
	viewParts.singingText->blockSignals(false);
	viewParts.setlistText->blockSignals(false);

	notifyStateChanged();
}

void DockActions::lyricSearch() const
{
	if (viewParts.lyricSearchText->text().trimmed().isEmpty()) {
		return;
	}

	const QString encoded = QUrl::toPercentEncoding(viewParts.lyricSearchText->text().trimmed());
	const QUrl url("https://utaten.com/search?title=" + encoded);
	QDesktopServices::openUrl(url);
}

void DockActions::startClock()
{
	viewParts.clockDisplay->setText(QTime::currentTime().toString("hh:mm:ss"));
	QObject::disconnect(viewParts.clockTimer, &QTimer::timeout, nullptr, nullptr);
	QObject::connect(viewParts.clockTimer, &QTimer::timeout,
			 [this]() { viewParts.clockDisplay->setText(QTime::currentTime().toString("hh:mm:ss")); });
	viewParts.clockTimer->start(1000);
}

void DockActions::stopClock()
{
	viewParts.clockTimer->stop();
	QObject::disconnect(viewParts.clockTimer, &QTimer::timeout, nullptr, nullptr);
}

void DockActions::startStreamingTimer()
{
	timestampCount = 0;
	viewParts.timestampDisplay->setText("0:00:00");
	QObject::disconnect(viewParts.timestampTimer, &QTimer::timeout, nullptr, nullptr);
	QObject::connect(viewParts.timestampTimer, &QTimer::timeout, viewParts.timestampTimer, [this]() {
		timestampCount++;
		const int hours = timestampCount / 3600;
		const int minutes = (timestampCount % 3600) / 60;
		const int seconds = timestampCount % 60;
		viewParts.timestampDisplay->setText(QString("%1:%2:%3")
							    .arg(hours, 2, 10)
							    .arg(minutes, 2, 10, QChar('0'))
							    .arg(seconds, 2, 10, QChar('0')));
	});
	viewParts.timestampTimer->start(1000);
}

void DockActions::stopStreamingTimer()
{
	viewParts.timestampTimer->stop();
	QObject::disconnect(viewParts.timestampTimer, &QTimer::timeout, nullptr, nullptr);
	timestampCount = 0;
	viewParts.timestampDisplay->setText("0:00:00");
}

void DockActions::prepareUserTemplates()
{
	if (templateManager) {
		templateManager->prepareUserTemplateDirectory();
	}
}

void DockActions::reloadTemplates()
{
	if (!viewParts.templateComboBox || !templateManager) {
		return;
	}

	const QString current = viewParts.templateComboBox->currentText();
	viewParts.templateComboBox->clear();
	QStringList overridden;
	const QStringList names = templateManager->listTemplateNames(&overridden);
	for (const QString &name : overridden) {
		blog(LOG_INFO, "[SETOLIA] User template '%s' overrides the bundled template of the same name",
		     name.toUtf8().constData());
	}
	viewParts.templateComboBox->addItems(names);

	if (!current.isEmpty()) {
		const int index = viewParts.templateComboBox->findText(current);
		if (index >= 0) {
			viewParts.templateComboBox->setCurrentIndex(index);
		}
	}
}

void DockActions::openTemplateDirectory()
{
	if (!templateManager) {
		return;
	}
	templateManager->openTemplateDirectory();
}

void DockActions::addSelectedTemplateToScene()
{
	if (!viewParts.templateComboBox || !templateManager) {
		return;
	}
	templateManager->addTemplateToCurrentScene(viewParts.templateComboBox->currentText());
}

void DockActions::onTextStateChanged()
{
	notifyStateChanged();
}

void DockActions::onClockTogglechanged(bool checked)
{
	if (checked) {
		startClock();
		viewParts.clockLabel->setVisible(true);
		viewParts.clockDisplay->setVisible(true);
	} else {
		stopClock();
		viewParts.clockLabel->setVisible(false);
		viewParts.clockDisplay->setVisible(false);
	}
}

const std::string &DockActions::cacheData() const
{
	return cachedData;
}

void DockActions::notifyStateChanged()
{
	cachedData = buildCachedData(viewParts);
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Reserve,
						viewParts.reserveText->toPlainText());
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Singing,
						viewParts.singingText->text());
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Setlist,
						viewParts.setlistText->toPlainText());
	if (taggedTextSourceManager) {
		taggedTextSourceManager->syncAllTaggedSources(TaggedTextSourceManager::TextState::Reserve,
							      viewParts.reserveText->toPlainText());
		taggedTextSourceManager->syncAllTaggedSources(TaggedTextSourceManager::TextState::Singing,
							      viewParts.singingText->text());
		taggedTextSourceManager->syncAllTaggedSources(TaggedTextSourceManager::TextState::Setlist,
							      viewParts.setlistText->toPlainText());
	}
	if (httpSyncServer) {
		httpSyncServer->updateCachedData(cachedData);
	}
}

void DockActions::startStreaming()
{
	startServer();
}

bool DockActions::startServer()
{
	if (!httpSyncServer) {
		return false;
	}
	const int port = setolia::httpServerPort();
	const bool running = httpSyncServer->startServer(port);
	updateServerStatus(running, port);
	return running;
}

void DockActions::updateServerStatus(bool running, int port)
{
	if (!viewParts.serverStatusLabel) {
		return;
	}
	if (running) {
		viewParts.serverStatusLabel->setText(
			QString::fromUtf8(obs_module_text("SERVER_STATUS_RUNNING")).arg(port));
		viewParts.serverStatusLabel->setStyleSheet(QString());
	} else {
		viewParts.serverStatusLabel->setText(
			QString::fromUtf8(obs_module_text("SERVER_STATUS_PORT_UNAVAILABLE")).arg(port));
		viewParts.serverStatusLabel->setStyleSheet("color: red;");
	}
}

void DockActions::applyServerPort()
{
	if (!viewParts.serverPortSpinBox || !httpSyncServer) {
		return;
	}

	const int port = viewParts.serverPortSpinBox->value();
	if (!setolia::isValidHttpServerPort(port)) {
		return;
	}
	const int previousPort = setolia::httpServerPort();
	const bool wasRunning = httpSyncServer->isRunning();
	if (port == previousPort && wasRunning) {
		return; // nothing to do
	}

	// The new port is bound before the running server is stopped, so a port in use leaves the server
	// running on the previous port. The port is committed (and saved) only after the new server is up.
	const bool started = (port == previousPort) ? httpSyncServer->startServer(port)
						    : httpSyncServer->switchPort(port);
	if (started) {
		setolia::setHttpServerPort(port);
		saveConfig();
		updateServerStatus(true, port);
		// Rewrite existing template sources only once the new port actually serves them.
		if (templateManager) {
			templateManager->migrateTemplateSources();
		}
		return;
	}

	// Keep the previous port. If the server was not running (the configured port failed at startup), try
	// it once more so the dock shows its current state.
	viewParts.serverPortSpinBox->setValue(previousPort);
	const bool previousRunning = wasRunning || httpSyncServer->startServer(previousPort);
	if (previousRunning) {
		viewParts.serverStatusLabel->setText(
			QString::fromUtf8(obs_module_text("SERVER_STATUS_PORT_CHANGE_FAILED"))
				.arg(port)
				.arg(previousPort));
		viewParts.serverStatusLabel->setStyleSheet("color: red;");
	} else {
		updateServerStatus(false, port);
	}
}

void DockActions::stopStreaming()
{
	if (httpSyncServer) {
		httpSyncServer->stopServer();
	}
}

Config DockActions::getConfigForSave() const
{
	Config config;
	config.reserveText = viewParts.reserveText->toPlainText();
	config.singingText = viewParts.singingText->text();
	config.setlistText = viewParts.setlistText->toPlainText();
	config.clockToggled = viewParts.clockToggle->isChecked();
	config.timestampToggled = viewParts.timestampToggle->isChecked();
	config.nextToggled = viewParts.nextToggle->isChecked();
	config.selectedTemplate = viewParts.templateComboBox->currentText();
	config.port = setolia::httpServerPort(); // the applied port, not an unapplied spin box edit
	return config;
}

void DockActions::applyConfig(const Config &config)
{
	// Block signals to prevent unnecessary cache updates during restoration
	viewParts.reserveText->blockSignals(true);
	viewParts.singingText->blockSignals(true);
	viewParts.setlistText->blockSignals(true);
	viewParts.clockToggle->blockSignals(true);
	viewParts.timestampToggle->blockSignals(true);
	viewParts.nextToggle->blockSignals(true);
	viewParts.templateComboBox->blockSignals(true);

	// Restore text content
	viewParts.reserveText->setPlainText(config.reserveText);
	viewParts.singingText->setText(config.singingText);
	viewParts.setlistText->setPlainText(config.setlistText);

	// Restore toggles
	viewParts.nextToggle->setChecked(config.nextToggled);
	viewParts.timestampToggle->setChecked(config.timestampToggled);

	// Restore server port (the server is started afterwards with this port)
	setolia::setHttpServerPort(config.port);
	viewParts.serverPortSpinBox->setValue(setolia::httpServerPort());

	// Restore template selection
	const int templateIndex = viewParts.templateComboBox->findText(config.selectedTemplate);
	if (templateIndex >= 0) {
		viewParts.templateComboBox->setCurrentIndex(templateIndex);
	}

	// Restore clock state (must be done after signals are unblocked)
	viewParts.clockToggle->blockSignals(false);
	viewParts.clockToggle->setChecked(config.clockToggled);
	if (config.clockToggled) {
		startClock();
		viewParts.clockLabel->setVisible(true);
		viewParts.clockDisplay->setVisible(true);
	}

	// Unblock all signals
	viewParts.reserveText->blockSignals(false);
	viewParts.singingText->blockSignals(false);
	viewParts.setlistText->blockSignals(false);
	viewParts.timestampToggle->blockSignals(false);
	viewParts.nextToggle->blockSignals(false);
	viewParts.templateComboBox->blockSignals(false);

	// Update cache with restored state
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Reserve,
						viewParts.reserveText->toPlainText());
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Singing,
						viewParts.singingText->text());
	TaggedTextSourceManager::setCurrentText(TaggedTextSourceManager::TextState::Setlist,
						viewParts.setlistText->toPlainText());
	notifyStateChanged();
}

void DockActions::saveConfig() const
{
	if (!configManager) {
		blog(LOG_ERROR, "DockActions: ConfigManager not initialized");
		return;
	}

	const Config config = getConfigForSave();
	if (!configManager->saveConfig(config)) {
		blog(LOG_ERROR, "DockActions: Failed to save configuration");
	}
}
