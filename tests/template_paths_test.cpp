#include "../src/source/template_paths.hpp"

#include <QString>
#include <QStringList>

#include <ostream>

std::ostream &operator<<(std::ostream &os, const QString &s)
{
	return os << '"' << s.toStdString() << '"';
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
	return setolia_test::finish("template_paths_test");
}
