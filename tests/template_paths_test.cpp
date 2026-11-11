#include "../src/source/template_paths.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <ostream>

std::ostream &operator<<(std::ostream &os, const QString &s)
{
	return os << '"' << s.toStdString() << '"';
}

std::ostream &operator<<(std::ostream &os, const QByteArray &b)
{
	return os << '"' << b.toStdString() << '"';
}

#include "test_support.hpp"

namespace {

using setolia::TemplateMigrationRule;

const QStringList kNames = {QStringLiteral("dark"), QStringLiteral("light"), QStringLiteral("light-rst")};

// Expects a match with the given relative path and rule.
void expectMatch(int line, const QString &localFile, const QString &templatesDir, Qt::CaseSensitivity cs,
		 const QString &expectedPath, TemplateMigrationRule expectedRule)
{
	const auto match = setolia::templateRelativePathFromLocalFile(localFile, templatesDir, kNames, cs);
	if (!match) {
		setolia_test::reportFailure(__FILE__, line, "no match for " + localFile.toStdString());
		return;
	}
	if (match->relativePath != expectedPath) {
		setolia_test::reportFailure(__FILE__, line,
					    "relativePath " + match->relativePath.toStdString() +
						    " != " + expectedPath.toStdString());
	}
	if (match->rule != expectedRule) {
		setolia_test::reportFailure(__FILE__, line,
					    std::string("rule ") + setolia::templateMigrationRuleName(match->rule) +
						    " != " + setolia::templateMigrationRuleName(expectedRule));
	}
}

void expectNoMatch(int line, const QString &localFile, const QString &templatesDir, Qt::CaseSensitivity cs)
{
	const auto match = setolia::templateRelativePathFromLocalFile(localFile, templatesDir, kNames, cs);
	if (match) {
		setolia_test::reportFailure(__FILE__, line,
					    "unexpected match for " + localFile.toStdString() + " -> " +
						    match->relativePath.toStdString());
	}
}

const QString kWinTemplatesDir = QStringLiteral("C:/ProgramData/obs-studio/plugins/setolia/data/templates");
const QString kLinuxTemplatesDir = QStringLiteral("/usr/share/obs/obs-plugins/setolia/templates");

void testCurrentTemplatesDir()
{
	expectMatch(__LINE__, kLinuxTemplatesDir + "/light/index.html", kLinuxTemplatesDir, Qt::CaseSensitive,
		    "light/index.html", TemplateMigrationRule::CurrentTemplatesDir);
	// Not-yet-cleaned paths ("..") are normalized.
	expectMatch(__LINE__, "/usr/share/obs/obs-plugins/setolia/../setolia/templates/dark/index.html",
		    kLinuxTemplatesDir, Qt::CaseSensitive, "dark/index.html",
		    TemplateMigrationRule::CurrentTemplatesDir);
	// Rule (a) keeps converting user templates placed in the current folder, even if not listed.
	expectMatch(__LINE__, kLinuxTemplatesDir + "/my custom/index.html", kLinuxTemplatesDir, Qt::CaseSensitive,
		    "my custom/index.html", TemplateMigrationRule::CurrentTemplatesDir);
	expectMatch(__LINE__, kWinTemplatesDir + "/light-rst/index.html", kWinTemplatesDir, Qt::CaseInsensitive,
		    "light-rst/index.html", TemplateMigrationRule::CurrentTemplatesDir);
}

void testOldProgramFilesPath()
{
	expectMatch(__LINE__, "C:/Program Files/obs-studio/data/obs-plugins/setolia/templates/light/index.html",
		    kWinTemplatesDir, Qt::CaseInsensitive, "light/index.html",
		    TemplateMigrationRule::LegacyTemplatesPath);
}

void testPortableObs32Path()
{
	expectMatch(__LINE__, "D:/obs-portable/data/obs-plugins/setolia/templates/dark/index.html",
		    "D:/obs-portable/plugins/setolia/data/templates", Qt::CaseInsensitive, "dark/index.html",
		    TemplateMigrationRule::LegacyTemplatesPath);
	// The OBS 33 layout of another install location is recognized as well.
	expectMatch(__LINE__, "E:/other-obs/plugins/setolia/data/templates/dark/index.html",
		    "D:/obs-portable/plugins/setolia/data/templates", Qt::CaseInsensitive, "dark/index.html",
		    TemplateMigrationRule::LegacyTemplatesPath);
}

void testBackslashesAndCase()
{
	// Backslashes, current folder.
	expectMatch(__LINE__, "C:\\ProgramData\\obs-studio\\plugins\\setolia\\data\\templates\\light\\index.html",
		    kWinTemplatesDir, Qt::CaseInsensitive, "light/index.html",
		    TemplateMigrationRule::CurrentTemplatesDir);
	// Different case, current folder (Windows).
	expectMatch(__LINE__, "c:/programdata/OBS-Studio/plugins/SETOLIA/data/templates/light/index.html",
		    kWinTemplatesDir, Qt::CaseInsensitive, "light/index.html",
		    TemplateMigrationRule::CurrentTemplatesDir);
	// Backslashes + different case, legacy path; template name takes the listed spelling.
	expectMatch(__LINE__,
		    "C:\\PROGRAM FILES\\obs-studio\\data\\obs-plugins\\Setolia\\Templates\\Light-RST\\index.html",
		    kWinTemplatesDir, Qt::CaseInsensitive, "light-rst/index.html",
		    TemplateMigrationRule::LegacyTemplatesPath);
	// Case-sensitive platforms do not fold case.
	expectNoMatch(__LINE__, "/opt/obs/data/obs-plugins/setolia/templates/Light/index.html", kLinuxTemplatesDir,
		      Qt::CaseSensitive);
}

void testUnknownTemplateName()
{
	expectNoMatch(__LINE__, "C:/Program Files/obs-studio/data/obs-plugins/setolia/templates/removed/index.html",
		      kWinTemplatesDir, Qt::CaseInsensitive);
}

void testUnrelatedPaths()
{
	expectNoMatch(__LINE__, "C:/Users/me/overlays/light/index.html", kWinTemplatesDir, Qt::CaseInsensitive);
	expectNoMatch(__LINE__, "C:/Users/me/setolia-templates/light/index.html", kWinTemplatesDir,
		      Qt::CaseInsensitive);
	expectNoMatch(__LINE__, "", kWinTemplatesDir, Qt::CaseInsensitive);
	// "<name>/<file>" is required.
	expectNoMatch(__LINE__, "C:/old/setolia/templates/light", kWinTemplatesDir, Qt::CaseInsensitive);
	expectNoMatch(__LINE__, "C:/old/setolia/templates/light/", kWinTemplatesDir, Qt::CaseInsensitive);
	// The template folder itself is not a template file.
	expectNoMatch(__LINE__, kWinTemplatesDir, kWinTemplatesDir, Qt::CaseInsensitive);
	// Sibling folder sharing the prefix string.
	expectNoMatch(__LINE__, kLinuxTemplatesDir + "-backup/foo/index.html", kLinuxTemplatesDir, Qt::CaseSensitive);
}

void testOldPortUrl()
{
	const auto a = setolia::rewriteTemplateUrlPort("http://127.0.0.1:8080/templates/light/index.html", 18080);
	CHECK(a.has_value());
	if (a) {
		CHECK_EQ(*a, QStringLiteral("http://127.0.0.1:18080/templates/light/index.html"));
	}

	const auto b = setolia::rewriteTemplateUrlPort("http://localhost:9000/templates/light%20x/index.html", 8080);
	CHECK(b.has_value());
	if (b) {
		CHECK_EQ(*b, QStringLiteral("http://localhost:8080/templates/light%20x/index.html"));
	}

	const auto c = setolia::rewriteTemplateUrlPort("HTTP://LocalHost:9000/templates/dark/index.html?x=1", 8080);
	CHECK(c.has_value());
	if (c) {
		CHECK_EQ(*c, QStringLiteral("HTTP://LocalHost:8080/templates/dark/index.html?x=1"));
	}
}

void testCurrentPortUrlUnchanged()
{
	CHECK(!setolia::rewriteTemplateUrlPort("http://127.0.0.1:8080/templates/light/index.html", 8080));
	CHECK(!setolia::rewriteTemplateUrlPort("http://localhost:8080/templates/light/index.html", 8080));
}

void testUnrelatedUrlsUnchanged()
{
	CHECK(!setolia::rewriteTemplateUrlPort("http://127.0.0.1:8080/other/index.html", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("http://192.168.0.2:8080/templates/light/index.html", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("https://127.0.0.1:8080/templates/light/index.html", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("http://127.0.0.1/templates/light/index.html", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("http://127.0.0.1:8080/templatesX/index.html", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("https://example.com/", 9000));
	CHECK(!setolia::rewriteTemplateUrlPort("", 9000));
}

void testTemplateUrl()
{
	CHECK_EQ(setolia::templateUrl("light/index.html", 8080),
		 QStringLiteral("http://127.0.0.1:8080/templates/light/index.html"));
	CHECK_EQ(setolia::templateUrl("my template/index.html", 12345),
		 QStringLiteral("http://127.0.0.1:12345/templates/my%20template/index.html"));
}

void testPortSetting()
{
	CHECK_EQ(setolia::httpServerPort(), setolia::kDefaultHttpServerPort);
	setolia::setHttpServerPort(18080);
	CHECK_EQ(setolia::httpServerPort(), 18080);
	CHECK_EQ(setolia::templateUrl("dark/index.html"),
		 QStringLiteral("http://127.0.0.1:18080/templates/dark/index.html"));
	setolia::setHttpServerPort(80);    // below range: ignored
	setolia::setHttpServerPort(70000); // above range: ignored
	CHECK_EQ(setolia::httpServerPort(), 18080);
	CHECK(setolia::isValidHttpServerPort(1024));
	CHECK(setolia::isValidHttpServerPort(65535));
	CHECK(!setolia::isValidHttpServerPort(1023));
	CHECK(!setolia::isValidHttpServerPort(65536));
	setolia::setHttpServerPort(setolia::kDefaultHttpServerPort);
}

QString joined(const QStringList &list)
{
	return list.join(QLatin1Char(','));
}

void testMergeTemplateNames()
{
	const QStringList bundled = {"dark", "light", "simple"};

	// Only bundled templates: unchanged list, nothing overridden.
	auto merged = setolia::mergeTemplateNames({}, bundled, Qt::CaseSensitive);
	CHECK_EQ(joined(merged.names), QStringLiteral("dark,light,simple"));
	CHECK(merged.overridden.isEmpty());

	// User templates are added, sorted ignoring case; a user "light" overrides the bundled one.
	merged = setolia::mergeTemplateNames({"light", "My Custom", "aqua"}, bundled, Qt::CaseSensitive);
	CHECK_EQ(joined(merged.names), QStringLiteral("aqua,dark,light,My Custom,simple"));
	CHECK_EQ(joined(merged.overridden), QStringLiteral("light"));

	// Case-insensitive platforms treat "LIGHT" and "light" as the same template (the user spelling is kept).
	merged = setolia::mergeTemplateNames({"LIGHT"}, bundled, Qt::CaseInsensitive);
	CHECK_EQ(joined(merged.names), QStringLiteral("dark,LIGHT,simple"));
	CHECK_EQ(joined(merged.overridden), QStringLiteral("LIGHT"));
	merged = setolia::mergeTemplateNames({"LIGHT"}, bundled, Qt::CaseSensitive);
	CHECK_EQ(merged.names.size(), 4);
	CHECK(merged.names.contains("LIGHT") && merged.names.contains("light"));
	CHECK(merged.overridden.isEmpty());
}

void testCustomTemplatesToCopy()
{
	const QStringList builtin = {"dark", "light"};
	CHECK(setolia::customTemplatesToCopy({"dark", "light"}, builtin, {}, Qt::CaseSensitive).isEmpty());
	CHECK_EQ(joined(setolia::customTemplatesToCopy({"dark", "mine", "light", "other"}, builtin, {},
						       Qt::CaseSensitive)),
		 QStringLiteral("mine,other"));
	// Already in the user folder: skipped.
	CHECK_EQ(joined(setolia::customTemplatesToCopy({"dark", "mine", "other"}, builtin, {"other"},
						       Qt::CaseSensitive)),
		 QStringLiteral("mine"));
	CHECK(setolia::customTemplatesToCopy({"Mine"}, builtin, {"mine"}, Qt::CaseInsensitive).isEmpty());
	CHECK(setolia::customTemplatesToCopy({"DARK"}, builtin, {}, Qt::CaseInsensitive).isEmpty());
}

void writeFile(const QString &path, const QByteArray &content)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	CHECK(file.open(QIODevice::WriteOnly));
	file.write(content);
}

QByteArray readFile(const QString &path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}

void testCopyDirectoryRecursively()
{
	QTemporaryDir tmp;
	CHECK(tmp.isValid());
	const QString source = tmp.filePath("bundled/mine");
	writeFile(source + "/index.html", "index");
	writeFile(source + "/img/bg.png", "png");
	QDir().mkpath(source + "/empty");

	const QString destination = tmp.filePath("user/mine");
	writeFile(destination + "/style.css", "user style"); // existing files are kept
	writeFile(source + "/style.css", "bundled style");

	CHECK(setolia::copyDirectoryRecursively(source, destination));
	CHECK_EQ(readFile(destination + "/index.html"), QByteArray("index"));
	CHECK_EQ(readFile(destination + "/img/bg.png"), QByteArray("png"));
	CHECK_EQ(readFile(destination + "/style.css"), QByteArray("user style"));
	CHECK(QFileInfo(destination + "/empty").isDir());

	CHECK(!setolia::copyDirectoryRecursively(tmp.filePath("does-not-exist"), tmp.filePath("x")));
}

void testLegacyPathWithUserTemplateNames()
{
	// A custom template kept in an old bundled folder is converted once it exists in the user folder: the
	// migration passes the names of both folders to rule (b).
	const QStringList userNames = {"my-custom"};
	const QStringList names = setolia::mergeTemplateNames(userNames, kNames, Qt::CaseInsensitive).names;
	const auto match = setolia::templateRelativePathFromLocalFile(
		"C:/Program Files/obs-studio/data/obs-plugins/setolia/templates/my-custom/index.html", kWinTemplatesDir,
		names, Qt::CaseInsensitive);
	CHECK(match.has_value());
	if (match) {
		CHECK_EQ(match->relativePath, QStringLiteral("my-custom/index.html"));
		CHECK(match->rule == TemplateMigrationRule::LegacyTemplatesPath);
	}
	// Without the user names it is not recognized.
	CHECK(!setolia::templateRelativePathFromLocalFile(
		"C:/Program Files/obs-studio/data/obs-plugins/setolia/templates/my-custom/index.html", kWinTemplatesDir,
		kNames, Qt::CaseInsensitive));

	// Rule (a) for the user folder (no names needed).
	const QString userDir =
		QStringLiteral("C:/Users/me/AppData/Roaming/obs-studio/plugin_config/setolia/templates");
	const auto userMatch = setolia::templateRelativePathFromLocalFile(userDir + "/my-custom/index.html", userDir,
									  {}, Qt::CaseInsensitive);
	CHECK(userMatch.has_value());
	if (userMatch) {
		CHECK_EQ(userMatch->relativePath, QStringLiteral("my-custom/index.html"));
	}
}

} // namespace

int main()
{
	testCurrentTemplatesDir();
	testOldProgramFilesPath();
	testPortableObs32Path();
	testBackslashesAndCase();
	testUnknownTemplateName();
	testUnrelatedPaths();
	testOldPortUrl();
	testCurrentPortUrlUnchanged();
	testUnrelatedUrlsUnchanged();
	testTemplateUrl();
	testPortSetting();
	testMergeTemplateNames();
	testCustomTemplatesToCopy();
	testCopyDirectoryRecursively();
	testLegacyPathWithUserTemplateNames();
	return setolia_test::finish("template_paths_test");
}
