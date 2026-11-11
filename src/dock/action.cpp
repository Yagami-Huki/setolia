#include "action.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "../source/template_manager.hpp"
#include "../source/http_sync_server.hpp"
#include "../source/tagged_text_source_manager.hpp"
#include "../config/config_manager.hpp"
#include "../source/template_paths.hpp"
#include "../source/timestamp.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
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

namespace {
// Name of the hotkey and its key in Config::hotkeys.
constexpr const char *kNextSongHotkeyName = "setolia.next_song";
constexpr const char *kNextSongHotkeyConfigKey = "nextSong";

// obs_data_array <-> JSON array, through a wrapper object (obs_data has no direct array (de)serialization).
QJsonArray hotkeyArrayToJson(obs_data_array_t *array)
{
	if (!array) {
		return {};
	}
	obs_data_t *wrapper = obs_data_create();
	obs_data_set_array(wrapper, "bindings", array);
	const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(obs_data_get_json(wrapper)));
	obs_data_release(wrapper);
	return doc.object().value("bindings").toArray();
}

obs_data_array_t *hotkeyArrayFromJson(const QJsonArray &array)
{
	const QJsonObject wrapperObject{{"bindings", array}};
	obs_data_t *wrapper = obs_data_create_from_json(QJsonDocument(wrapperObject).toJson().constData());
	if (!wrapper) {
		return nullptr;
	}
	obs_data_array_t *result = obs_data_get_array(wrapper, "bindings");
	obs_data_release(wrapper);
	return result;
}

QString streamSessionFilePath()
{
	char *path = obs_module_config_path("stream-session.json");
	if (!path) {
		return QString();
	}
	const QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

std::optional<setolia::StreamSession> readStreamSession()
{
	QFile file(streamSessionFilePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	return setolia::streamSessionFromJson(file.readAll());
}

// Delay of the debounced settings save.
constexpr int kSaveDelayMs = 2000;
// Refresh rate of the timestamp display. The value itself is computed from the stream start time.
constexpr int kTimestampRefreshMs = 250;

QStringList splitTimestamp(const QString &text)
{
	const setolia::TimestampSplit split = setolia::splitTimestampSuffix(text);
	if (split.timestamp) {
		return QStringList({split.title, *split.timestamp});
	}
	return QStringList({text});
}

// How long the current stream has been running according to the streaming output, or -1 if unknown.
// Used when the dock is created while OBS is already streaming. The output's frame count times the video frame
// interval gives the stream time without depending on when the plugin was loaded.
qint64 streamingOutputElapsedMs()
{
	obs_output_t *output = obs_frontend_get_streaming_output();
	if (!output) {
		return -1;
	}
	qint64 elapsedMs = -1;
	const int frames = obs_output_get_total_frames(output);
	video_t *video = obs_output_video(output);
	const uint64_t frameTimeNs = video ? video_output_get_frame_time(video) : 0;
	if (frames > 0 && frameTimeNs > 0) {
		elapsedMs = static_cast<qint64>(static_cast<uint64_t>(frames) * frameTimeNs / 1000000ULL);
	}
	obs_output_release(output);
	return elapsedMs;
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

QString DockActions::currentTimestampText() const
{
	if (!streamClock.isValid()) {
		return viewParts.timestampDisplay->text();
	}
	return setolia::formatElapsedTimestamp((streamClock.elapsed() + streamOffsetMs) / 1000);
}

void DockActions::updateTimestampDisplay()
{
	const QString text = currentTimestampText();
	if (viewParts.timestampDisplay->text() != text) {
		viewParts.timestampDisplay->setText(text);
	}
}

bool DockActions::moveSingingToSetlist()
{
	const QString singingText = viewParts.singingText->text().trimmed();
	if (singingText.isEmpty()) {
		return false;
	}
	const QString currentSetlist = viewParts.setlistText->toPlainText();
	if (!currentSetlist.isEmpty()) {
		viewParts.setlistText->setPlainText(currentSetlist + "\n" + singingText);
	} else {
		viewParts.setlistText->setPlainText(singingText);
	}
	viewParts.singingText->clear();
	return true;
}

void DockActions::takeNextFromReserve()
{
	QStringList reserveLines = viewParts.reserveText->toPlainText().split("\n", Qt::KeepEmptyParts);
	if (reserveLines.isEmpty()) {
		return;
	}
	QString topLine = reserveLines.first().trimmed();
	if (topLine.isEmpty()) {
		return;
	}
	if (viewParts.timestampToggle->isChecked()) {
		topLine += " - " + currentTimestampText();
	}
	viewParts.singingText->setText(topLine);
	reserveLines.removeFirst();
	viewParts.reserveText->setPlainText(reserveLines.join("\n"));
}

void DockActions::nextSong()
{
	viewParts.reserveText->blockSignals(true);
	viewParts.singingText->blockSignals(true);
	viewParts.setlistText->blockSignals(true);

	// "Wait before moving on" (nextToggle): a press only finishes the current song; the next press takes the
	// next one. Otherwise one press does both.
	const bool finishedSong = moveSingingToSetlist();
	if (!viewParts.nextToggle->isChecked() || !finishedSong) {
		takeNextFromReserve();
	}

	viewParts.reserveText->blockSignals(false);
	viewParts.singingText->blockSignals(false);
	viewParts.setlistText->blockSignals(false);

	notifyStateChanged();
	scheduleSave();
}

void DockActions::registerHotkeys(const QJsonObject &savedBindings)
{
	loadedHotkeyBindings = savedBindings;
	if (nextSongHotkeyId != static_cast<size_t>(OBS_INVALID_HOTKEY_ID)) {
		return;
	}
	nextSongHotkeyId = obs_hotkey_register_frontend(
		kNextSongHotkeyName, obs_module_text("HOTKEY_NEXT_SONG"),
		[](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
			if (!pressed) {
				return;
			}
			// Called on the hotkey thread: run nextSong() on the UI thread, like POST /next.
			auto *self = static_cast<DockActions *>(data);
			QMetaObject::invokeMethod(
				self->viewParts.nextButton, [self]() { self->nextSong(); }, Qt::QueuedConnection);
		},
		this);

	const QJsonValue bindings = savedBindings.value(kNextSongHotkeyConfigKey);
	if (bindings.isArray()) {
		obs_data_array_t *array = hotkeyArrayFromJson(bindings.toArray());
		if (array) {
			obs_hotkey_load(nextSongHotkeyId, array);
			obs_data_array_release(array);
		}
	}
}

void DockActions::unregisterHotkeys()
{
	if (nextSongHotkeyId == static_cast<size_t>(OBS_INVALID_HOTKEY_ID)) {
		return;
	}
	loadedHotkeyBindings = hotkeyBindingsForSave();
	obs_hotkey_unregister(nextSongHotkeyId);
	nextSongHotkeyId = static_cast<size_t>(OBS_INVALID_HOTKEY_ID);
}

QJsonObject DockActions::hotkeyBindingsForSave() const
{
	if (nextSongHotkeyId == static_cast<size_t>(OBS_INVALID_HOTKEY_ID)) {
		return loadedHotkeyBindings;
	}
	QJsonObject bindings = loadedHotkeyBindings;
	obs_data_array_t *array = obs_hotkey_save(nextSongHotkeyId);
	bindings[kNextSongHotkeyConfigKey] = hotkeyArrayToJson(array);
	obs_data_array_release(array);
	return bindings;
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
	scheduleSave();
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

void DockActions::startStreamingTimer(qint64 alreadyElapsedMs)
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	streamOffsetMs = alreadyElapsedMs > 0 ? alreadyElapsedMs : 0;
	streamClock.start();

	// Remember the previous stream before this one overwrites the record, and offer to continue it if it
	// stopped moments ago. Counting starts from 0 either way, as before, until the button is pressed.
	previousSession = readStreamSession();
	if (!setolia::canResumeStreamSession(previousSession, now)) {
		previousSession.reset();
	}
	currentSession = {now - streamOffsetMs, now};
	writeStreamSession();
	if (viewParts.resumeTimestampButton) {
		if (previousSession) {
			const QString startTime =
				QDateTime::fromMSecsSinceEpoch(previousSession->startMs).toString("HH:mm");
			viewParts.resumeTimestampButton->setText(
				QString::fromUtf8(obs_module_text("RESUME_TIMESTAMP_BUTTON")).arg(startTime));
		}
		viewParts.resumeTimestampButton->setVisible(previousSession.has_value());
	}

	updateTimestampDisplay();
	QObject::disconnect(viewParts.timestampTimer, &QTimer::timeout, nullptr, nullptr);
	QObject::connect(viewParts.timestampTimer, &QTimer::timeout, viewParts.timestampTimer,
			 [this]() { onTimestampTick(); });
	viewParts.timestampTimer->start(kTimestampRefreshMs);
}

void DockActions::stopStreamingTimer()
{
	viewParts.timestampTimer->stop();
	QObject::disconnect(viewParts.timestampTimer, &QTimer::timeout, nullptr, nullptr);
	if (streamClock.isValid()) {
		// Record when the stream stopped, so a restart within a minute can continue it.
		currentSession.lastAliveMs = QDateTime::currentMSecsSinceEpoch();
		writeStreamSession();
	}
	streamClock.invalidate();
	streamOffsetMs = 0;
	previousSession.reset();
	if (viewParts.resumeTimestampButton) {
		viewParts.resumeTimestampButton->setVisible(false);
	}
	viewParts.timestampDisplay->setText("0:00:00");
}

void DockActions::onTimestampTick()
{
	updateTimestampDisplay();
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	if (now - lastSessionWriteMs >= setolia::kStreamSessionHeartbeatMs) {
		// Heartbeat: if OBS crashes, the last write tells when the stream was still running.
		currentSession.lastAliveMs = now;
		writeStreamSession();
	}
}

void DockActions::writeStreamSession()
{
	lastSessionWriteMs = QDateTime::currentMSecsSinceEpoch();
	const QString path = streamSessionFilePath();
	if (path.isEmpty()) {
		return;
	}
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(setolia::streamSessionToJson(currentSession)) < 0 ||
	    !file.commit()) {
		blog(LOG_WARNING, "[SETOLIA] Failed to write the stream session file: %s", path.toUtf8().constData());
	}
}

void DockActions::continueFromPreviousStream()
{
	if (!previousSession || !streamClock.isValid()) {
		return;
	}
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	streamOffsetMs = setolia::resumedStreamElapsedMs(*previousSession, now);
	streamClock.start();
	// This stream now continues the previous one; a further restart chains to the same start.
	currentSession = {previousSession->startMs, now};
	writeStreamSession();
	blog(LOG_INFO, "[SETOLIA] Timestamp continues the previous stream (started %s)",
	     QDateTime::fromMSecsSinceEpoch(previousSession->startMs).toString(Qt::ISODate).toUtf8().constData());
	previousSession.reset();
	if (viewParts.resumeTimestampButton) {
		viewParts.resumeTimestampButton->setVisible(false);
	}
	updateTimestampDisplay();
}

void DockActions::prepareUserTemplates()
{
	if (templateManager) {
		templateManager->prepareUserTemplateDirectory();
	}
}

void DockActions::resumeStreamingTimerIfActive()
{
	if (streamClock.isValid() || !obs_frontend_streaming_active()) {
		return;
	}
	// Prefer the stream time reported by the streaming output. If it cannot be determined (no output yet,
	// no frames), count from now instead.
	const qint64 elapsedMs = streamingOutputElapsedMs();
	blog(LOG_INFO, "[SETOLIA] Already streaming; timestamp starts at %s",
	     elapsedMs >= 0 ? "the streaming output's elapsed time" : "0 (stream time unknown)");
	startStreamingTimer(elapsedMs);
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
	scheduleSave();
}

void DockActions::refreshState()
{
	notifyStateChanged();
}

void DockActions::onRestrictNextOriginChanged(bool checked)
{
	if (httpSyncServer) {
		httpSyncServer->setRestrictNextOrigin(checked);
	}
	scheduleSave();
}

void DockActions::scheduleSave()
{
	if (!autosaveEnabled || !viewParts.saveTimer) {
		return;
	}
	viewParts.saveTimer->start(kSaveDelayMs); // restarting extends the delay while edits keep coming
}

void DockActions::flushPendingSave()
{
	if (viewParts.saveTimer) {
		viewParts.saveTimer->stop();
	}
	// Always save on exit, like earlier versions did, even if no edit is pending.
	saveConfig();
}

void DockActions::onClockTogglechanged(bool checked)
{
	scheduleSave();
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

void DockActions::deactivate()
{
	active = false;
	autosaveEnabled = false;
	if (viewParts.saveTimer) {
		viewParts.saveTimer->stop();
	}
}

void DockActions::notifyStateChanged()
{
	if (!active) {
		return;
	}
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
	config.restrictNextOrigin = viewParts.restrictNextOriginToggle->isChecked();
	config.hotkeys = hotkeyBindingsForSave();
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

	// Restore the /next restriction (applied to the server, which reads it per request)
	viewParts.restrictNextOriginToggle->blockSignals(true);
	viewParts.restrictNextOriginToggle->setChecked(config.restrictNextOrigin);
	viewParts.restrictNextOriginToggle->blockSignals(false);
	if (httpSyncServer) {
		httpSyncServer->setRestrictNextOrigin(config.restrictNextOrigin);
	}

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
