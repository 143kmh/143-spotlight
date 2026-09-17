#include "replay-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMetaObject>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("143-spotlight", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "143 Spotlight - a gaming-first replay buffer frontend for OBS Studio";
}

namespace {
QPointer<ReplayDock> g_dock;

void frontendEvent(enum obs_frontend_event event, void *)
{
	if (!g_dock)
		return;

	QMetaObject::invokeMethod(g_dock, [event]() {
		if (g_dock)
			g_dock->handleFrontendEvent(static_cast<int>(event));
	}, Qt::QueuedConnection);
}
}

bool obs_module_load(void)
{
	g_dock = new ReplayDock();
	if (!obs_frontend_add_dock_by_id("143Spotlight", "143 Spotlight", g_dock)) {
		blog(LOG_ERROR, "[143 Spotlight] Failed to register dock");
		delete g_dock;
		g_dock = nullptr;
		return false;
	}

	obs_frontend_add_event_callback(frontendEvent, nullptr);
	blog(LOG_INFO, "[143 Spotlight] Loaded");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	obs_frontend_remove_dock("143Spotlight");
	g_dock = nullptr;
	blog(LOG_INFO, "[143 Spotlight] Unloaded");
}
