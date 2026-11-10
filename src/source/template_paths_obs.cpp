#include "template_paths.hpp"

#include <obs-module.h>

#include <QDir>
#include <QFileInfo>

namespace setolia {

QString templatesDirectoryPath()
{
	char *templatesDir = obs_module_file("templates");
	if (!templatesDir) {
		return QString();
	}
	const QString path = QString::fromUtf8(templatesDir);
	bfree(templatesDir);
	// obs_module_file may return a path containing "../" (e.g. on Windows); normalize it.
	return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

} // namespace setolia
