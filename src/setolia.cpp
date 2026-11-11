#include "setolia.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QPointer>

#include "source/setolia_text_sources.hpp"
#include "source/template_manager.hpp"

#define PLUGIN_NAME "SETOLIA"

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

// The dock widget is owned by OBS (its dock). QPointer turns null if OBS deletes it, so the event callback
// never touches a deleted widget.
static QPointer<SETOLIA_Dock> g_setoliaDock;
static bool g_eventCallbackRegistered = false;

bool obs_module_load(void)
{
	register_setolia_text_sources();
	return true;
}

static void frontend_event_callback(obs_frontend_event event, void *)
{
	SETOLIA_Dock *setoliaDock = g_setoliaDock.data();
	if (!setoliaDock)
		return;
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		setoliaDock->StartTimer();
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		setoliaDock->StopTimer();
	} else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING ||
		   event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		// Convert template browser sources saved as local files (or for another port) to the HTTP URL
		// form (OBS 33 / CEF LNA).
		TemplateManager().migrateTemplateSources();
		if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			setoliaDock->onFinishedLoading();
		}
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		// Last point at which libobs and the frontend API are guaranteed to be usable: save settings and
		// stop the HTTP server and timers here instead of in the dock destructor / obs_module_unload.
		setoliaDock->finalize();
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

	obs_frontend_add_event_callback(frontend_event_callback, nullptr);
	g_eventCallbackRegistered = true;
}

void obs_module_unload(void)
{
	// Settings are saved and the HTTP server/timers are stopped on OBS_FRONTEND_EVENT_EXIT (see
	// SetoliaDockWidget::finalize), so nothing here touches the dock's state or libobs data. The two frontend
	// calls are kept as before: OBS 32 detaches the frontend API right after EXIT, which makes them no-ops,
	// and if a frontend still processes them, destroying the dock is safe because its destructor no longer
	// saves or calls into libobs once finalize() has run.
	if (g_eventCallbackRegistered) {
		obs_frontend_remove_event_callback(frontend_event_callback, nullptr);
		obs_frontend_remove_dock("SETOLIA_Dock");
		g_eventCallbackRegistered = false;
	}
	g_setoliaDock = nullptr;
}
