#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace setolia {

// Port of the plugin's local HTTP server (templates, /data, /events, /ws, /next).
constexpr int kDefaultHttpServerPort = 8080;
constexpr int kMinHttpServerPort = 1024;
constexpr int kMaxHttpServerPort = 65535;

// Mount point under which the template folder is served.
inline constexpr const char *kTemplatesMountPoint = "/templates";

bool isValidHttpServerPort(int port);

// Port currently configured for the HTTP server. Used for every URL the plugin writes.
int httpServerPort();
// Invalid ports are ignored.
void setHttpServerPort(int port);

// Absolute, cleaned path of the bundled template folder (obs_module_file("templates")).
// Returns an empty string if the module data path cannot be resolved.
// Defined in template_paths_obs.cpp: everything else in this header is independent of OBS.
QString templatesDirectoryPath();

// http://127.0.0.1:<port>/templates/<percent-encoded relative path>
// relativePath uses '/' as separator, e.g. "light/index.html".
QString templateUrl(const QString &relativePath, int port);
// Same, with the current httpServerPort().
QString templateUrl(const QString &relativePath);

// Windows paths are case-insensitive; other platforms are case-sensitive.
Qt::CaseSensitivity platformPathCaseSensitivity();

// Rule used to rewrite a template browser source (logged on migration).
enum class TemplateMigrationRule {
	CurrentTemplatesDir, // local_file under the current template folder
	LegacyTemplatesPath, // local_file ending in /setolia/templates/<name>/<file> of a listed template
	PortChange,          // http URL of a template served on another port
};
const char *templateMigrationRuleName(TemplateMigrationRule rule);

struct TemplateLocalFileMatch {
	QString relativePath; // e.g. "light/index.html", '/' separated, not percent-encoded
	TemplateMigrationRule rule;
};

// Decides whether a browser source's local_file points at a SETOLIA template.
//  (a) under templatesDir -> relative path below it
//  (b) otherwise, the path ends in /setolia/templates/<name>/<file> (or /setolia/data/templates/...) and
//      <name> is in templateNames (old Program Files install, portable OBS 32 layout, ...)
//      -> "<name>/<file>" using the spelling from templateNames
// Backslashes are accepted as separators. Returns nullopt for unrelated files.
std::optional<TemplateLocalFileMatch>
templateRelativePathFromLocalFile(const QString &localFile, const QString &templatesDir,
				  const QStringList &templateNames,
				  Qt::CaseSensitivity caseSensitivity = platformPathCaseSensitivity());

// If url is http://127.0.0.1:<p>/templates/... or http://localhost:<p>/templates/... with p != port,
// returns the same URL with only the port replaced. Otherwise nullopt.
std::optional<QString> rewriteTemplateUrlPort(const QString &url, int port);

} // namespace setolia
