#include "setolia-dock.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QTimer>
#include <QTime>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QJsonArray>
#include <QDesktopServices>
#include <QMessageBox>

#define PLUGIN_NAME "SETOLIA"
#define PLUGIN_VERSION "1.0.0"
#define PORT 8080

OBS_DECLARE_MODULE();
OBS_MODULE_AUTHOR("Yagami Huki");
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US");

SETOLIA_Dock::SETOLIA_Dock(QWidget *parent) : QWidget(parent)
{
	m_Layout = new QVBoxLayout(this);
	m_Layout->setContentsMargins(0, 0, 0, 0);
	m_ScrollArea = new QScrollArea();
	m_ScrollArea->setWidgetResizable(true);
	m_ScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_ScrollArea->setBackgroundRole(QPalette::Base);

	m_Content = new QWidget();
	m_ContentLayout = new QVBoxLayout(m_Content);
	m_ContentLayout->setContentsMargins(10, 10, 10, 10);
	m_ContentLayout->setSpacing(5);
	m_ContentLayout->setAlignment(Qt::AlignCenter | Qt::AlignTop);

	clock_Label = new QLabel(obs_module_text("CLOCK_LABEL"));
	clock_Label->setStyleSheet("font-weight: bold;");
	clock_Label->setAlignment(Qt::AlignCenter);
	clock_Label->setVisible(false);
	clock_Display = new QLineEdit("0:00:00");
	clock_Display->setStyleSheet("font-weight: bold; font-size: 28px;");
	clock_Display->setFixedHeight(50);
	clock_Display->setFrame(true);
	clock_Display->setAlignment(Qt::AlignCenter);
	clock_Display->setReadOnly(true);
	clock_Display->setVisible(false);
	clock_Toggle = new QCheckBox(obs_module_text("TOGGLE_CLOCK"));
	clock_Toggle->setChecked(false);
	clock_Timer = new QTimer();

	reserve_Label = new QLabel(obs_module_text("RESERVE_LABEL"));
	reserve_Label->setStyleSheet("font-weight: bold;");
	reserve_Text = new QTextEdit();
	reserve_Text->setAcceptRichText(false);
	reserve_Text->setFixedHeight(200);

	singing_Label = new QLabel(obs_module_text("SINGING_LABEL"));
	singing_Label->setStyleSheet("font-weight: bold;");
	singing_Text = new QLineEdit();

	setlist_Label = new QLabel(obs_module_text("SETLIST_LABEL"));
	setlist_Label->setStyleSheet("font-weight: bold;");
	setlist_Text = new QTextEdit();
	setlist_Text->setAcceptRichText(false);
	setlist_Text->setFixedHeight(200);

	next_Button = new QPushButton(obs_module_text("NEXT_BUTTON"));
	next_Button->setCursor(Qt::PointingHandCursor);
	next_Toggle = new QCheckBox(obs_module_text("TOGGLE_NEXT"));
	next_Toggle->setChecked(true);

	timestamp_Display = new QLineEdit("0:00:00");
	timestamp_Display->setStyleSheet("font-weight: bold; font-size: 14px;");
	timestamp_Display->setAlignment(Qt::AlignCenter);
	timestamp_Display->setReadOnly(true);
	timestamp_Toggle = new QCheckBox(obs_module_text("TOGGLE_TIMESTAMP"));
	timestamp_Toggle->setChecked(false);
	timestamp_Timer = new QTimer();

	lyricSearch_Label = new QLabel(obs_module_text("LYRIC_SEARCH_LABEL"));
	lyricSearch_Label->setStyleSheet("font-weight: bold;");
	lyricSearch_Layout = new QHBoxLayout();
	lyricSearch_Text = new QLineEdit();
	lyricSearch_Text->setPlaceholderText(obs_module_text("LYRIC_SEARCH_PLACEHOLDER"));
	lyricSearch_Button = new QPushButton(obs_module_text("LYRIC_SEARCH_BUTTON"));
	lyricSearch_Button->setCursor(Qt::PointingHandCursor);
	lyricSearch_Layout->addWidget(lyricSearch_Text);
	lyricSearch_Layout->addWidget(lyricSearch_Button);

	reset_Button = new QPushButton(obs_module_text("RESET_BUTTON"));
	reset_Button->setStyleSheet("color: red;");
	reset_Button->setCursor(Qt::PointingHandCursor);

	template_ComboBox = new QComboBox();
	loadTemplates();
	open_Template_Button = new QPushButton(obs_module_text("OPEN_TEMPLATE_BUTTON"));
	open_Template_Button->setCursor(Qt::PointingHandCursor);
	reload_Template_Button = new QPushButton(obs_module_text("RELOAD_TEMPLATE_BUTTON"));
	reload_Template_Button->setCursor(Qt::PointingHandCursor);

	add_Button = new QPushButton(obs_module_text("ADD_BUTTON"));
	add_Button->setCursor(Qt::PointingHandCursor);

	m_ContentLayout->addWidget(clock_Label);
	m_ContentLayout->addWidget(clock_Display);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(reserve_Label);
	m_ContentLayout->addWidget(reserve_Text);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(singing_Label);
	m_ContentLayout->addWidget(singing_Text);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(setlist_Label);
	m_ContentLayout->addWidget(setlist_Text);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(next_Button);
	m_ContentLayout->addWidget(timestamp_Display);
	m_ContentLayout->addWidget(timestamp_Toggle);
	m_ContentLayout->addWidget(lyricSearch_Label);
	m_ContentLayout->addLayout(lyricSearch_Layout);
	m_ContentLayout->addWidget(reset_Button);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(template_ComboBox);
	m_ContentLayout->addWidget(open_Template_Button);
	m_ContentLayout->addWidget(reload_Template_Button);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(add_Button);
	m_ContentLayout->addSpacing(10);
	m_ContentLayout->addWidget(clock_Toggle);
	m_ContentLayout->addWidget(next_Toggle);
	m_ContentLayout->addStretch();

	m_ScrollArea->setWidget(m_Content);
	m_Layout->addWidget(m_ScrollArea);
	setLayout(m_Layout);

	setObjectName("SETOLIA_Dock");
	setWindowTitle("SETOLIA");
	setWindowFlags(Qt::Widget | Qt::WindowStaysOnTopHint);
	setMinimumSize(250, 400);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setAttribute(Qt::WA_DeleteOnClose);
	setAcceptDrops(true);

	auto configDir = obs_module_config_path("");
	if (configDir && QFileInfo::exists(QString::fromUtf8(configDir)) &&
	    QFileInfo::exists(QString::fromUtf8(configDir) + "/setolia-config.json")) {
		obs_data_t *data = obs_data_create_from_json_file_safe(
			(QString::fromUtf8(configDir) + "/setolia-config.json").toUtf8().constData(), "bkp");
		if (data) {
			reserve_Text->setPlainText(QString::fromUtf8(obs_data_get_string(data, "reserve")));
			singing_Text->setText(QString::fromUtf8(obs_data_get_string(data, "singing")));
			setlist_Text->setPlainText(QString::fromUtf8(obs_data_get_string(data, "setlist")));
			timestamp_Toggle->setChecked(obs_data_get_bool(data, "timestampToggle"));
			template_ComboBox->setCurrentText(QString::fromUtf8(obs_data_get_string(data, "template")));
			next_Toggle->setChecked(obs_data_get_bool(data, "nextToggle"));
			clock_Toggle->setChecked(obs_data_get_bool(data, "clockToggle"));
			if (clock_Toggle->isChecked()) {
				startClock();
				clock_Label->setVisible(true);
				clock_Display->setVisible(true);
			}
		}
		obs_data_release(data);
		blog(LOG_INFO, "SETOLIA configuration loaded from %s", configDir);
	}
	bfree(configDir);

	updateCachedData();

	startServer();

	connect(reserve_Text, &QTextEdit::textChanged, this, [this]() {
		updateCachedData();
		sendData();
	});

	connect(singing_Text, &QLineEdit::textChanged, this, [this]() {
		updateCachedData();
		sendData();
	});

	connect(setlist_Text, &QTextEdit::textChanged, this, [this]() {
		updateCachedData();
		sendData();
	});

	connect(next_Button, &QPushButton::clicked, this, &SETOLIA_Dock::nextSong);

	connect(reset_Button, &QPushButton::clicked, this, [this]() {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, obs_module_text("RESET_CONFIRM_TITLE"),
					      obs_module_text("RESET_CONFIRM_TEXT"),
					      QMessageBox::Ok | QMessageBox::Cancel);
		if (reply != QMessageBox::Ok)
			return;

		reserve_Text->blockSignals(true);
		singing_Text->blockSignals(true);
		setlist_Text->blockSignals(true);
		reserve_Text->clear();
		singing_Text->clear();
		setlist_Text->clear();
		reserve_Text->blockSignals(false);
		singing_Text->blockSignals(false);
		setlist_Text->blockSignals(false);

		updateCachedData();
		sendData();
	});

	connect(lyricSearch_Button, &QPushButton::clicked, this, [this]() {
		if (lyricSearch_Text->text().trimmed().isEmpty())
			return;
		QString query = "https://utaten.com/search?title=" + lyricSearch_Text->text().trimmed();
		QDesktopServices::openUrl(QUrl(query));
	});

	connect(open_Template_Button, &QPushButton::clicked, this, [this]() {
		auto templatesDir = obs_module_file("templates");
		if (templatesDir && QFileInfo::exists(QString::fromUtf8(templatesDir))) {
#if defined(_WIN32)
			QString path = QString::fromUtf8(templatesDir);
			QProcess::startDetached("explorer", {QDir::toNativeSeparators(path)});
#elif defined(__APPLE__)
			QString path = QString::fromUtf8(templatesDir);
			QProcess::startDetached("open", { path });
#else
			QString path = QString::fromUtf8(templatesDir);
			QProcess::startDetached("xdg-open", { path });
#endif
		}
		bfree(templatesDir);
	});

	connect(reload_Template_Button, &QPushButton::clicked, this, &SETOLIA_Dock::loadTemplates);

	connect(add_Button, &QPushButton::clicked, this, [this]() {
		auto templatesDir = obs_module_file("templates");
		QString selectedTemplate = template_ComboBox->currentText();
		if (templatesDir && QFileInfo::exists(QString::fromUtf8(templatesDir)) && !selectedTemplate.isEmpty()) {
			QString baseDir = QString::fromUtf8(templatesDir);
			QString templateDir = QFileInfo(baseDir, selectedTemplate).absoluteFilePath();
			QString templatePath = templateDir + "/index.html";
			if (!QFileInfo(templateDir, "index.html").isFile() ||
			    !QFileInfo(templateDir, "script.js").isFile() ||
			    !QFileInfo(templateDir, "style.css").isFile()) {
				bfree(templatesDir);
				return;
			}
			int width = 1920;
			int height = 1080;
			QString css = "";
			if (QFileInfo(templateDir, "template.json").isFile()) {
				QFile file(templateDir + "/template.json");
				if (file.open(QIODevice::ReadOnly)) {
					QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll());
					QJsonObject jsonObj = jsonDoc.object();
					if (jsonObj.contains("width")) {
						width = jsonObj["width"].toInt();
					}
					if (jsonObj.contains("height")) {
						height = jsonObj["height"].toInt();
					}
					if (jsonObj.contains("css")) {
						css = jsonObj["css"].toString();
					}
				}
				file.close();
			}

			obs_source_t *browser_source = nullptr;
			obs_data_t *settings = obs_data_create();
			obs_data_set_bool(settings, "is_local_file", true);
			obs_data_set_string(settings, "local_file", templatePath.toUtf8().constData());
			obs_data_set_int(settings, "width", width);
			obs_data_set_int(settings, "height", height);
			obs_data_set_string(settings, "css", css.toUtf8().constData());

			browser_source = obs_source_create("browser_source", selectedTemplate.toUtf8().constData(),
							   settings, nullptr);

			if (browser_source) {
				obs_source_t *current_scene = obs_frontend_get_current_scene();
				if (current_scene) {
					obs_scene_t *scene = obs_scene_from_source(current_scene);
					if (scene) {
						obs_scene_add(scene, browser_source);
					}
					obs_source_release(current_scene);
				}
				obs_source_release(browser_source);
			}
			obs_data_release(settings);
		}
		bfree(templatesDir);
		return;
	});

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
	connect(clock_Toggle, &QCheckBox::stateChanged, this, [this](int state) {
		if (state == Qt::Checked) {
			startClock();
			clock_Label->setVisible(true);
			clock_Display->setVisible(true);
		} else {
			stopClock();
			clock_Label->setVisible(false);
			clock_Display->setVisible(false);
		}
	});
#else
	connect(clock_Toggle, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
		if (state == Qt::Checked) {
			startClock();
			clock_Label->setVisible(true);
			clock_Display->setVisible(true);
		} else {
			stopClock();
			clock_Label->setVisible(false);
			clock_Display->setVisible(false);
		}
	});
#endif
}

SETOLIA_Dock::~SETOLIA_Dock()
{
	stopServer();
	auto configDir = obs_module_config_path("");
	if (!configDir)
		return;
	if (!QFileInfo::exists(QString::fromUtf8(configDir))) {
		QDir().mkdir(QString::fromUtf8(configDir));
	}
	bfree(configDir);

	auto configPath = obs_module_config_path("setolia-config.json");
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "reserve", reserve_Text->toPlainText().toUtf8().constData());
	obs_data_set_string(data, "singing", singing_Text->text().toUtf8().constData());
	obs_data_set_string(data, "setlist", setlist_Text->toPlainText().toUtf8().constData());
	obs_data_set_bool(data, "timestampToggle", timestamp_Toggle->isChecked());
	obs_data_set_string(data, "template", template_ComboBox->currentText().toUtf8().constData());
	obs_data_set_bool(data, "nextToggle", next_Toggle->isChecked());
	obs_data_set_bool(data, "clockToggle", clock_Toggle->isChecked());
	obs_data_save_json_safe(data, configPath, "tmp", "bkp");
	obs_data_release(data);
	blog(LOG_INFO, "SETOLIA configuration saved to %s", configPath);
	bfree(configPath);
}

void SETOLIA_Dock::StartTimer()
{
	timestamp_Timer->start(1000);

	QObject::connect(timestamp_Timer, &QTimer::timeout, this, [this]() {
		timestamp_Count++;
		int hours = timestamp_Count / 3600;
		int minutes = (timestamp_Count % 3600) / 60;
		int seconds = timestamp_Count % 60;
		timestamp_Display->setText(QString("%1:%2:%3")
						   .arg(hours, 2, 10)
						   .arg(minutes, 2, 10, QChar('0'))
						   .arg(seconds, 2, 10, QChar('0')));
	});
}

void SETOLIA_Dock::StopTimer()
{
	timestamp_Timer->stop();
	QObject::disconnect(timestamp_Timer, &QTimer::timeout, this, nullptr);
	timestamp_Count = 0;
	timestamp_Display->setText("0:00:00");
}

void SETOLIA_Dock::nextSong()
{
	reserve_Text->blockSignals(true);
	singing_Text->blockSignals(true);
	setlist_Text->blockSignals(true);
	if (next_Toggle->isChecked()) {
		QString singingText = singing_Text->text().trimmed();
		if (!singingText.isEmpty()) {
			QString currentSetlist = setlist_Text->toPlainText();
			if (!currentSetlist.isEmpty()) {
				setlist_Text->setPlainText(currentSetlist + "\n" + singingText);
			} else {
				setlist_Text->setPlainText(singingText);
			}
			singing_Text->clear();
		} else {
			QStringList reserveLines = reserve_Text->toPlainText().split("\n", Qt::KeepEmptyParts);
			if (!reserveLines.isEmpty()) {
				QString topLine = reserveLines.first().trimmed();
				if (timestamp_Toggle->isChecked() && !topLine.isEmpty()) {
					topLine += " - " + timestamp_Display->text();
				}
				if (!topLine.isEmpty()) {
					singing_Text->setText(topLine);
					reserveLines.removeFirst();
					reserve_Text->setPlainText(reserveLines.join("\n"));
				}
			}
		}
	} else {
		QString singingText = singing_Text->text().trimmed();
		if (!singingText.isEmpty()) {
			QString currentSetlist = setlist_Text->toPlainText();
			if (!currentSetlist.isEmpty()) {
				setlist_Text->setPlainText(currentSetlist + "\n" + singingText);
			} else {
				setlist_Text->setPlainText(singingText);
			}
			singing_Text->clear();
		}

		QStringList reserveLines = reserve_Text->toPlainText().split("\n", Qt::KeepEmptyParts);
		if (!reserveLines.isEmpty()) {
			QString topLine = reserveLines.first().trimmed();
			if (timestamp_Toggle->isChecked() && !topLine.isEmpty()) {
				topLine += " - " + timestamp_Display->text();
			}
			if (!topLine.isEmpty()) {
				singing_Text->setText(topLine);
				reserveLines.removeFirst();
				reserve_Text->setPlainText(reserveLines.join("\n"));
			}
		}
	}

	reserve_Text->blockSignals(false);
	singing_Text->blockSignals(false);
	setlist_Text->blockSignals(false);

	updateCachedData();
	sendData();
}

void SETOLIA_Dock::loadTemplates()
{
	template_ComboBox->clear();
	auto templatesDir = obs_module_file("templates");
	if (templatesDir && QFileInfo::exists(QString::fromUtf8(templatesDir))) {
		QDir dir(QString::fromUtf8(templatesDir));
		QStringList templatesList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QString &templateName : templatesList) {
			template_ComboBox->addItem(templateName);
		}
	}
	bfree(templatesDir);
}

QStringList SETOLIA_Dock::splitTimastamp(QString text)
{
	std::string std = text.toStdString();
	std::regex re(R"(^(.*) - (\d{1,2}:\d{2}:\d{2})$)");
	std::smatch matches;
	if (std::regex_match(std, matches, re)) {
		return QStringList({QString::fromStdString(matches[1]), QString::fromStdString(matches[2])});
	} else {
		return QStringList({text});
	}
}

void SETOLIA_Dock::updateCachedData()
{
	QJsonObject jsonObj = QJsonObject();
	jsonObj["reserve"] = QJsonArray::fromStringList(reserve_Text->toPlainText().split("\n", Qt::SkipEmptyParts));
	jsonObj["singing"] = QJsonArray::fromStringList(splitTimastamp(singing_Text->text()));
	QJsonArray setlist = QJsonArray();
	for (const QString &line : setlist_Text->toPlainText().split("\n", Qt::SkipEmptyParts)) {
		setlist.append(QJsonArray::fromStringList(splitTimastamp(line)));
	}
	jsonObj["setlist"] = setlist;
	QJsonDocument doc(jsonObj);
	{
		std::lock_guard<std::mutex> lock(data_mutex);
		cachedData = doc.toJson(QJsonDocument::Compact).constData();
	}
};

void SETOLIA_Dock::startServer()
{
	srv.Get("/data", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		std::string data = "";
		{
			std::lock_guard<std::mutex> lock(data_mutex);
			data = cachedData;
		}
		res.set_content(data, "application/json");
	});

	srv.Get("/events", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");

		res.set_content_provider("text/event-stream", [this](size_t, httplib::DataSink &sink) {
			std::string lastSentData = "";
			std::unique_lock<std::mutex> lock(srv_mutex);

			while (true) {
				srv_cond.wait(lock, [this, &lastSentData] {
					return srv_stopped || (latestData != lastSentData);
				});

				if (srv_stopped)
					break;

				std::string payload = "data: " + latestData + "\n\n";

				if (!sink.write(payload.data(), payload.size())) {
					break;
				}

				lastSentData = latestData;
			}
			return true;
		});
	});

	srv.Post("/next", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		QMetaObject::invokeMethod(this, [this]() { this->nextSong(); }, Qt::QueuedConnection);
		res.set_content("OK", "text/plain");
	});

	srv_stopped = false;
	srv_thread = std::thread([this]() { srv.listen("127.0.0.1", PORT); });
}

void SETOLIA_Dock::stopServer()
{
	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		srv_stopped = true;
	}
	srv_cond.notify_all();

	if (srv.is_running()) {
		srv.stop();
	}
	if (srv_thread.joinable()) {
		srv_thread.join();
	}
}

void SETOLIA_Dock::sendData()
{
	std::string newData = "";
	{
		std::lock_guard<std::mutex> lock(data_mutex);
		newData = cachedData;
	}
	if (latestData == newData)
		return;
	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		latestData = newData;
	}
	srv_cond.notify_all();
}

void SETOLIA_Dock::startClock()
{
	clock_Display->setText(QTime::currentTime().toString("hh:mm:ss"));
	clock_Timer->start(1000);

	connect(clock_Timer, &QTimer::timeout, this,
		[this]() { clock_Display->setText(QTime::currentTime().toString("hh:mm:ss")); });
}

void SETOLIA_Dock::stopClock()
{
	clock_Timer->stop();
	QObject::disconnect(clock_Timer, &QTimer::timeout, this, nullptr);
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "%s loaded successfully (version %s)", PLUGIN_NAME, PLUGIN_VERSION);
	return true;
}

static void frontend_event_callback(obs_frontend_event event, void *priv_data)
{
	SETOLIA_Dock *setoliaDock = static_cast<SETOLIA_Dock *>(priv_data);
	if (!setoliaDock)
		return;
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		setoliaDock->StartTimer();
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		setoliaDock->StopTimer();
	}
}

void obs_module_post_load(void)
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	SETOLIA_Dock *setoliaDock = new SETOLIA_Dock(main_window);
	const auto title = QString::fromUtf8(obs_module_text("DOCK_TITLE"));
	const auto name = "SETOLIA_Dock";
	obs_frontend_add_dock_by_id(name, title.toUtf8().constData(), setoliaDock);

	obs_frontend_add_event_callback(frontend_event_callback, setoliaDock);
}
