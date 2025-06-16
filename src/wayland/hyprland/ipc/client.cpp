#include "client.hpp"

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qproperty.h>
#include <qtypes.h>

#include "connection.hpp"
#include "workspace.hpp"

namespace qs::hyprland::ipc {

HyprlandClient::HyprlandClient(HyprlandIpc* ipc): QObject(ipc), ipc(ipc) {}

void HyprlandClient::updateInitial(
    qint64 address,
    const QString& title,
    const QString& workspaceName
) {
	HyprlandWorkspace* workspace = this->ipc->findWorkspaceByName(workspaceName, true);
	Qt::beginPropertyUpdateGroup();
	this->bAddress = address;
	this->bTitle = title;
	if (workspace) {
		this->setWorkspace(workspace);
	}
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

	this->setWorkspace(workspace);
}

void HyprlandClient::setWorkspace(HyprlandWorkspace* workspace) {
	if (this->bWorkspace == workspace) return;
	Qt::beginPropertyUpdateGroup();
	if (this->bWorkspace) {
		// Remove from the previous workspace
		this->bWorkspace->clients()->removeObject(this);
	}
	this->bWorkspace = workspace;
	this->bWorkspace->clients()->insertObject(this, -1);
	emit this->workspaceChanged();
	Qt::endPropertyUpdateGroup();
}

} // namespace qs::hyprland::ipc
