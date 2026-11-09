#pragma once

#include <QString>

namespace setolia {

// Port of the plugin's local HTTP server (templates, /data, /events, /ws, /next).
constexpr int kHttpServerPort = 8080;

// Mount point under which the template folder is served.
inline constexpr const char *kTemplatesMountPoint = "/templates";

// Absolute, cleaned path of the bundled template folder (obs_module_file("templates")).
// Returns an empty string if the module data path cannot be resolved.
QString templatesDirectoryPath();

// http://127.0.0.1:<port>/templates/<percent-encoded relative path>
// relativePath uses '/' as separator, e.g. "light/index.html".
QString templateUrl(const QString &relativePath);

} // namespace setolia
