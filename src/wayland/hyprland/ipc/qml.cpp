#include "qml.hpp"

#include <qobject.h>
#include <qproperty.h>

#include "../../../core/model.hpp"
#include "../../../core/qmlscreen.hpp"
#include "connection.hpp"
#include "monitor.hpp"

namespace qs::hyprland::ipc {

HyprlandIpcQml::HyprlandIpcQml() {
	auto* instance = HyprlandIpc::instance();

	QObject::connect(instance, &HyprlandIpc::rawEvent, this, &HyprlandIpcQml::rawEvent);

	QObject::connect(
	    instance,
	    &HyprlandIpc::focusedMonitorChanged,
	    this,
	    &HyprlandIpcQml::focusedMonitorChanged
	);

	QObject::connect(
	    instance,
	    &HyprlandIpc::focusedMonitorChanged,
	    this,
	    &HyprlandIpcQml::focusedMonitorChanged
	);
}

void HyprlandIpcQml::dispatch(const QString& request) {
	HyprlandIpc::instance()->dispatch(request);
}

HyprlandMonitor* HyprlandIpcQml::monitorFor(QuickshellScreenInfo* screen) {
	return HyprlandIpc::instance()->monitorFor(screen);
}

void HyprlandIpcQml::refreshMonitors() { HyprlandIpc::instance()->refreshMonitors(false); }
void HyprlandIpcQml::refreshWorkspaces() { HyprlandIpc::instance()->refreshWorkspaces(false); }
QString HyprlandIpcQml::requestSocketPath() { return HyprlandIpc::instance()->requestSocketPath(); }
QString HyprlandIpcQml::eventSocketPath() { return HyprlandIpc::instance()->eventSocketPath(); }

QBindable<HyprlandMonitor*> HyprlandIpcQml::bindableFocusedMonitor() {
	return HyprlandIpc::instance()->bindableFocusedMonitor();
}

QBindable<HyprlandWorkspace*> HyprlandIpcQml::bindableFocusedWorkspace() {
	return HyprlandIpc::instance()->bindableFocusedWorkspace();
}

ObjectModel<HyprlandMonitor>* HyprlandIpcQml::monitors() {
	return HyprlandIpc::instance()->monitors();
}

ObjectModel<HyprlandWorkspace>* HyprlandIpcQml::workspaces() {
	return HyprlandIpc::instance()->workspaces();
}

ObjectModel<HyprlandClient>* HyprlandIpcQml::clients() {
	return HyprlandIpc::instance()->clients();
}

} // namespace qs::hyprland::ipc
