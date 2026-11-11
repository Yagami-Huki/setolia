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
#include <QSaveFile>

#include <cstring>

using setolia::templatesDirectoryPath;
using setolia::userTemplatesDirectoryPath;

namespace {
// Names of custom templates already copied from the bundled folder, so a template the user deleted from the
// user folder is not copied again on the next start.
constexpr const char *kCopiedTemplatesFileName = "copied-templates.txt";

const char *const kUserTemplatesReadme =
	"SETOLIA - カスタムテンプレート / Custom templates\n"
	"\n"
	"[日本語]\n"
	"このフォルダにはカスタムテンプレートを置きます。\n"
	"1. ドックの「テンプレートフォルダを開く」で開いたこのフォルダに、同梱テンプレートのフォルダ\n"
	"   (例: light)をコピーし、フォルダ名を変えてください。同梱テンプレートは SETOLIA のインストール先の\n"
	"   data/templates にあります(例: C:\\ProgramData\\obs-studio\\plugins\\setolia\\data\\templates)。\n"
	"2. コピーしたフォルダの index.html / style.css / script.js / template.json を編集してください。\n"
	"3. ドックの「テンプレートを再読み込み」を押すと、一覧に表示されます。\n"
	"同梱テンプレートと同じ名前のフォルダを置くと、こちらが優先されます。\n"
	"このフォルダは SETOLIA をアンインストール・更新しても削除されません。\n"
	"\n"
	"[English]\n"
	"Put your custom templates in this folder.\n"
	"1. Copy a bundled template folder (e.g. light) into this folder and rename it. The bundled templates are in\n"
	"   data/templates of the SETOLIA install folder\n"
	"   (e.g. C:\\ProgramData\\obs-studio\\plugins\\setolia\\data\\templates).\n"
	"2. Edit index.html / style.css / script.js / template.json in the copied folder.\n"
	"3. Press \"Reload Templates\" in the dock to show it in the list.\n"
	"A folder with the same name as a bundled template takes precedence over the bundled one.\n"
	"This folder is kept when SETOLIA is uninstalled or updated.\n";

QStringList templateFolderNames(const QString &baseDir)
{
	if (baseDir.isEmpty() || !QFileInfo::exists(baseDir)) {
		return {};
	}
	return QDir(baseDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}

// Folder of the template that the HTTP server serves for templateName: the user folder wins.
QString templateDirectory(const QString &templateName)
{
	for (const QString &baseDir : {userTemplatesDirectoryPath(), templatesDirectoryPath()}) {
		if (baseDir.isEmpty()) {
			continue;
		}
		const QFileInfo dir(baseDir, templateName);
		if (dir.isDir()) {
			return dir.absoluteFilePath();
		}
	}
	return QString();
}

QString copiedTemplatesFilePath()
{
	char *path = obs_module_config_path(kCopiedTemplatesFileName);
	if (!path) {
		return QString();
	}
	const QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

QStringList readCopiedTemplates()
{
	QFile file(copiedTemplatesFilePath());
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return {};
	}
	return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void writeCopiedTemplates(const QStringList &names)
{
	const QString path = copiedTemplatesFilePath();
	if (path.isEmpty()) {
		return;
	}
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}
	file.write((names.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
	file.commit();
}

struct MigrationContext {
	QString userTemplatesDir;
	QString templatesDir;
	QStringList templateNames;
	int port = setolia::kDefaultHttpServerPort;
	int converted = 0;
};

void applyTemplateUrl(obs_source_t *source, const QString &url)
{
	obs_data_t *update = obs_data_create();
	obs_data_set_bool(update, "is_local_file", false);
	obs_data_set_string(update, "url", url.toUtf8().constData());
	obs_source_update(source, update);
	obs_data_release(update);
}

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
		const QString localFile = QString::fromUtf8(obs_data_get_string(settings, "local_file"));
		// Rule (a) for the user folder first, then rules (a) and (b) for the bundled folder.
		auto match = setolia::templateRelativePathFromLocalFile(localFile, ctx->userTemplatesDir, {});
		if (!match) {
			match = setolia::templateRelativePathFromLocalFile(localFile, ctx->templatesDir,
									   ctx->templateNames);
		}
		if (match) {
			const QString url = setolia::templateUrl(match->relativePath, ctx->port);
			applyTemplateUrl(source, url);
			ctx->converted++;
			blog(LOG_INFO, "[SETOLIA] Migrated browser source '%s' (%s) from local file '%s' to '%s'",
			     obs_source_get_name(source), setolia::templateMigrationRuleName(match->rule),
			     localFile.toUtf8().constData(), url.toUtf8().constData());
		}
	} else {
		const QString oldUrl = QString::fromUtf8(obs_data_get_string(settings, "url"));
		if (const auto url = setolia::rewriteTemplateUrlPort(oldUrl, ctx->port)) {
			applyTemplateUrl(source, *url);
			ctx->converted++;
			blog(LOG_INFO, "[SETOLIA] Migrated browser source '%s' (%s) from '%s' to '%s'",
			     obs_source_get_name(source),
			     setolia::templateMigrationRuleName(setolia::TemplateMigrationRule::PortChange),
			     oldUrl.toUtf8().constData(), url->toUtf8().constData());
		}
	}

	obs_data_release(settings);
	return true;
}
} // namespace

QStringList TemplateManager::listTemplateNames(QStringList *overridden) const
{
	const setolia::MergedTemplateNames merged = setolia::mergeTemplateNames(
		templateFolderNames(userTemplatesDirectoryPath()), templateFolderNames(templatesDirectoryPath()));
	if (overridden) {
		*overridden = merged.overridden;
	}
	return merged.names;
}

bool TemplateManager::openTemplateDirectory() const
{
	const QString baseDir = userTemplatesDirectoryPath();
	if (baseDir.isEmpty() || !QDir().mkpath(baseDir)) {
		return false;
	}

	if (QDir(baseDir).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
		QSaveFile readme(QDir(baseDir).filePath("README.txt"));
		if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
			readme.write(kUserTemplatesReadme);
			readme.commit();
		}
	}

#if defined(_WIN32)
	return QProcess::startDetached("explorer", {QDir::toNativeSeparators(baseDir)});
#elif defined(__APPLE__)
	return QProcess::startDetached("open", {baseDir});
#else
	return QProcess::startDetached("xdg-open", {baseDir});
#endif
}

void TemplateManager::prepareUserTemplateDirectory() const
{
	const QString userDir = userTemplatesDirectoryPath();
	if (userDir.isEmpty() || !QDir().mkpath(userDir)) {
		blog(LOG_WARNING, "[SETOLIA] Failed to create the user template folder: %s",
		     userDir.toUtf8().constData());
		return;
	}

	const QString bundledDir = templatesDirectoryPath();
	QStringList alreadyCopied = readCopiedTemplates();
	QStringList userNames = templateFolderNames(userDir);
	userNames.append(alreadyCopied); // deleted by the user after an earlier copy: do not bring it back
	const QStringList toCopy = setolia::customTemplatesToCopy(templateFolderNames(bundledDir),
								  setolia::builtinTemplateNames(), userNames);
	if (toCopy.isEmpty()) {
		return;
	}

	for (const QString &name : toCopy) {
		const QString source = QDir(bundledDir).filePath(name);
		const QString destination = QDir(userDir).filePath(name);
		if (setolia::copyDirectoryRecursively(source, destination)) {
			alreadyCopied.append(name);
			blog(LOG_INFO, "[SETOLIA] Copied custom template '%s' from '%s' to '%s'",
			     name.toUtf8().constData(), source.toUtf8().constData(), destination.toUtf8().constData());
		} else {
			blog(LOG_WARNING, "[SETOLIA] Failed to copy custom template '%s' from '%s' to '%s'",
			     name.toUtf8().constData(), source.toUtf8().constData(), destination.toUtf8().constData());
		}
	}
	writeCopiedTemplates(alreadyCopied);
}

bool TemplateManager::addTemplateToCurrentScene(const QString &templateName) const
{
	if (templateName.trimmed().isEmpty()) {
		return false;
	}

	const QString templateDir = templateDirectory(templateName);
	if (templateDir.isEmpty()) {
		return false;
	}

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

int TemplateManager::migrateTemplateSources() const
{
	MigrationContext ctx;
	ctx.userTemplatesDir = userTemplatesDirectoryPath();
	ctx.templatesDir = templatesDirectoryPath(); // may be empty: rule (b) and port rewrites still apply
	// Rule (b) accepts names of both folders, so custom templates that were copied from an old bundled
	// folder to the user folder are recognized as well.
	ctx.templateNames = listTemplateNames();
	ctx.port = setolia::httpServerPort();
	obs_enum_sources(migrateBrowserSource, &ctx);

	if (ctx.converted > 0) {
		blog(LOG_INFO, "[SETOLIA] Migrated %d template browser source(s) to %s", ctx.converted,
		     setolia::templateUrl(QString(), ctx.port).toUtf8().constData());
	}
	return ctx.converted;
}
