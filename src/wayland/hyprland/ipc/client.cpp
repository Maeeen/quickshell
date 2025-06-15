#include "client.hpp"

#include <qobject.h>
#include <qtypes.h>

#include "connection.hpp"

namespace qs::hyprland::ipc {

HyprlandClient::HyprlandClient(HyprlandIpc* ipc): QObject(ipc), ipc(ipc) {}

void HyprlandClient::updateInitial(qint64 address, const QString& title, qint32 workspaceId) {
	Qt::beginPropertyUpdateGroup();
	this->bAddress = address;
	this->bTitle = title;
	this->bWorkspaceId = workspaceId;
	Qt::endPropertyUpdateGroup();
}

void HyprlandClient::updateFromObject(QVariantMap object) {
	auto addressStr = object.value("address").value<QString>();
	auto title = object.value("title").value<QString>();

	bool ok = false;
	this->bAddress = addressStr.toLongLong(&ok, 16);
	if (!ok) {
		this->bAddress = 0;
		return;
	}
	this->bTitle = title;

	auto workspace = object.value("workspace").toMap();
	auto workspaceId = workspace.value("id").value<qint32>();

	this->bWorkspaceId = workspaceId;
}

} // namespace qs::hyprland::ipc
