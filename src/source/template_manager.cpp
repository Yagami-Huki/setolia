#include "template_manager.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace {
QString templatesDirectoryPath()
{
	char *templatesDir = obs_module_file("templates");
	if (!templatesDir) {
		return QString();
	}
	QString path = QString::fromUtf8(templatesDir);
	bfree(templatesDir);
	return path;
}
} // namespace

QStringList TemplateManager::listTemplateNames() const
{
	const QString baseDir = templatesDirectoryPath();
	if (baseDir.isEmpty() || !QFileInfo::exists(baseDir)) {
		return {};
	}

	QDir dir(baseDir);
	return dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}

bool TemplateManager::openTemplateDirectory() const
{
	const QString baseDir = templatesDirectoryPath();
	if (baseDir.isEmpty() || !QFileInfo::exists(baseDir)) {
		return false;
	}

#if defined(_WIN32)
	return QProcess::startDetached("explorer", {QDir::toNativeSeparators(baseDir)});
#elif defined(__APPLE__)
	return QProcess::startDetached("open", {baseDir});
#else
	return QProcess::startDetached("xdg-open", {baseDir});
#endif
}

bool TemplateManager::addTemplateToCurrentScene(const QString &templateName) const
{
	if (templateName.trimmed().isEmpty()) {
		return false;
	}

	const QString baseDir = templatesDirectoryPath();
	if (baseDir.isEmpty() || !QFileInfo::exists(baseDir)) {
		return false;
	}

	const QString templateDir = QFileInfo(baseDir, templateName).absoluteFilePath();
	const QString templatePath = templateDir + "/index.html";

	if (!QFileInfo(templateDir, "index.html").isFile() || !QFileInfo(templateDir, "script.js").isFile() ||
	    !QFileInfo(templateDir, "style.css").isFile()) {
		return false;
	}

	int width = 1920;
	int height = 1080;
	QString css;

	if (QFileInfo(templateDir, "template.json").isFile()) {
		QFile file(templateDir + "/template.json");
		if (file.open(QIODevice::ReadOnly)) {
			const QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll());
			const QJsonObject jsonObj = jsonDoc.object();
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
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", templatePath.toUtf8().constData());
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_string(settings, "css", css.toUtf8().constData());

	obs_source_t *browserSource =
		obs_source_create("browser_source", templateName.toUtf8().constData(), settings, nullptr);
	obs_data_release(settings);

	if (!browserSource) {
		return false;
	}

	obs_source_t *currentSceneSource = obs_frontend_get_current_scene();
	if (!currentSceneSource) {
		obs_source_release(browserSource);
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(currentSceneSource);
	if (!scene) {
		obs_source_release(currentSceneSource);
		obs_source_release(browserSource);
		return false;
	}

	obs_scene_add(scene, browserSource);
	obs_source_release(currentSceneSource);
	obs_source_release(browserSource);
	return true;
}
