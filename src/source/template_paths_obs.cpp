#include "template_paths.hpp"

#include "builtin_templates.hpp"

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

QString userTemplatesDirectoryPath()
{
	char *templatesDir = obs_module_config_path("templates");
	if (!templatesDir) {
		return QString();
	}
	const QString path = QString::fromUtf8(templatesDir);
	bfree(templatesDir);
	return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QStringList builtinTemplateNames()
{
	QStringList names;
	for (const char *name : kBuiltinTemplateNames) {
		if (name) {
			names.append(QString::fromUtf8(name));
		}
	}
	return names;
}

} // namespace setolia
