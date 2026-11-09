#include "template_manager.hpp"
#include "template_paths.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cstring>

using setolia::templatesDirectoryPath;

namespace {
QString normalizePath(const QString &path)
{
	return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#if defined(_WIN32)
	return Qt::CaseInsensitive;
#else
	return Qt::CaseSensitive;
#endif
}

struct MigrationContext {
	QString templatesDirPrefix; // normalized, ends with '/'
	int converted = 0;
};

bool migrateBrowserSource(void *param, obs_source_t *source)
{
	auto *ctx = static_cast<MigrationContext *>(param);

	const char *sourceId = obs_source_get_unversioned_id(source);
	if (!sourceId || strcmp(sourceId, "browser_source") != 0) {
		return true;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return true;
	}

	if (obs_data_get_bool(settings, "is_local_file")) {
		const QString localFile = normalizePath(QString::fromUtf8(obs_data_get_string(settings, "local_file")));
		if (localFile.startsWith(ctx->templatesDirPrefix, pathCaseSensitivity())) {
			const QString relativePath = localFile.mid(ctx->templatesDirPrefix.size());
			const QString url = setolia::templateUrl(relativePath);

			obs_data_t *update = obs_data_create();
			obs_data_set_bool(update, "is_local_file", false);
			obs_data_set_string(update, "url", url.toUtf8().constData());
			obs_source_update(source, update);
			obs_data_release(update);

			ctx->converted++;
			blog(LOG_INFO, "[SETOLIA] Migrated browser source '%s' from local file '%s' to '%s'",
			     obs_source_get_name(source), localFile.toUtf8().constData(), url.toUtf8().constData());
		}
	}

	obs_data_release(settings);
	return true;
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
	// Load via the plugin's HTTP server: OBS 33 (CEF) blocks fetch/EventSource from local-file pages to 127.0.0.1.
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", setolia::templateUrl(templateName + "/index.html").toUtf8().constData());
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

int TemplateManager::migrateLocalFileSources() const
{
	const QString baseDir = templatesDirectoryPath();
	if (baseDir.isEmpty()) {
		return 0;
	}

	MigrationContext ctx;
	ctx.templatesDirPrefix = normalizePath(baseDir) + "/";
	obs_enum_sources(migrateBrowserSource, &ctx);

	if (ctx.converted > 0) {
		blog(LOG_INFO, "[SETOLIA] Migrated %d template browser source(s) to HTTP URLs", ctx.converted);
	}
	return ctx.converted;
}
