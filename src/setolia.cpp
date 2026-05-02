#include "setolia.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>

#include "source/setolia_text_sources.hpp"

#define PLUGIN_NAME "SETOLIA"
#define PLUGIN_VERSION "1.1.0"

OBS_DECLARE_MODULE();
OBS_MODULE_AUTHOR("Yagami Huki");
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US");

SETOLIA_Dock::SETOLIA_Dock(QWidget *parent) : SetoliaDockWidget(parent) {}

void SETOLIA_Dock::StartTimer()
{
	onStreamingStarted();
}

void SETOLIA_Dock::StopTimer()
{
	onStreamingStopped();
}

static SETOLIA_Dock *g_setoliaDock = nullptr;

bool obs_module_load(void)
{
	register_setolia_text_sources();
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
	g_setoliaDock = setoliaDock;
	const auto title = QString::fromUtf8(obs_module_text("DOCK_TITLE"));
	const auto name = "SETOLIA_Dock";
	obs_frontend_add_dock_by_id(name, title.toUtf8().constData(), setoliaDock);

	obs_frontend_add_event_callback(frontend_event_callback, setoliaDock);
}

void obs_module_unload(void)
{
	if (g_setoliaDock) {
		obs_frontend_remove_event_callback(frontend_event_callback, g_setoliaDock);
		obs_frontend_remove_dock("SETOLIA_Dock");
		delete g_setoliaDock;
		g_setoliaDock = nullptr;
	}
}
