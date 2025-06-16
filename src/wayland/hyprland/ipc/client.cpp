#include "client.hpp"

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qtypes.h>

#include "connection.hpp"

namespace qs::hyprland::ipc {

HyprlandClient::HyprlandClient(HyprlandIpc* ipc): QObject(ipc), ipc(ipc) {}

void HyprlandClient::updateInitial(
    qint64 address,
    const QString& title,
    const QString& workspaceName
) {
	Qt::beginPropertyUpdateGroup();
	this->bAddress = address;
	this->bTitle = title;
	this->bWorkspace = this->ipc->findWorkspaceByName(workspaceName, false);
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

	auto workspaceMap = object.value("workspace").toMap();
	auto workspaceName = workspaceMap.value("name").value<QString>();

	HyprlandWorkspace* workspace = this->ipc->findWorkspaceByName(workspaceName, false);
	if (!workspace) return;

	this->bWorkspace = workspace;
}

} // namespace qs::hyprland::ipc
