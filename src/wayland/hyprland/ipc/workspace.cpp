#include "workspace.hpp"
#include <algorithm>
#include <utility>

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qproperty.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "../../../core/model.hpp"
#include "client.hpp"
#include "connection.hpp"
#include "monitor.hpp"

namespace qs::hyprland::ipc {

QVariantMap HyprlandWorkspace::lastIpcObject() const { return this->mLastIpcObject; }

HyprlandWorkspace::HyprlandWorkspace(HyprlandIpc* ipc): QObject(ipc), ipc(ipc) {
	Qt::beginPropertyUpdateGroup();

	this->bActive.setBinding([this]() {
		return this->bMonitor.value() && this->bMonitor->bindableActiveWorkspace().value() == this;
	});

	this->bFocused.setBinding([this]() {
		return this->ipc->bindableFocusedWorkspace().value() == this;
	});

	Qt::endPropertyUpdateGroup();

	// The signals `onClientAdded` and `onClientRemoved` determine
	// whether the workspace is urgent or not, based on the current
	// clients. Therefore, we need to act after the clients have been
	// removed/added from the workspace.
	QObject::connect(
	    &this->mClients,
	    &UntypedObjectModel::objectInsertedPost,
	    this,
	    &HyprlandWorkspace::onClientAdded
	);

	QObject::connect(
	    &this->mClients,
	    &UntypedObjectModel::objectRemovedPost,
	    this,
	    &HyprlandWorkspace::onClientRemoved
	);

	QObject::connect(
	    this,
	    &HyprlandWorkspace::focusedChanged,
	    this,
	    &HyprlandWorkspace::onFocusedChanged
	);
}

void HyprlandWorkspace::updateInitial(qint32 id, const QString& name) {
	Qt::beginPropertyUpdateGroup();
	this->bId = id;
	this->bName = name;
	Qt::endPropertyUpdateGroup();
}

void HyprlandWorkspace::updateFromObject(QVariantMap object) {
	auto monitorId = object.value("monitorID").value<qint32>();
	auto monitorName = object.value("monitor").value<QString>();
	auto hasFullscreen = object.value("hasfullscreen").value<bool>();

	auto initial = this->bId == -1;

	// ID cannot be updated after creation
	if (initial) {
		this->bId = object.value("id").value<qint32>();
	}

	// No events we currently handle give a workspace id but not a name,
	// so we shouldn't set this if it isn't an initial query
	if (initial) {
		this->bName = object.value("name").value<QString>();
	}

	if (!monitorName.isEmpty()
	    && (this->bMonitor == nullptr || this->bMonitor->bindableName().value() != monitorName))
	{
		auto* monitor = this->ipc->findMonitorByName(monitorName, true, monitorId);
		this->setMonitor(monitor);
	}

	this->bHasFullscreen = hasFullscreen;
	this->mLastIpcObject = std::move(object);
	emit this->lastIpcObjectChanged();
}

void HyprlandWorkspace::setMonitor(HyprlandMonitor* monitor) {
	auto* oldMonitor = this->bMonitor.value();
	if (monitor == oldMonitor) return;

	if (oldMonitor != nullptr) {
		QObject::disconnect(oldMonitor, nullptr, this, nullptr);
	}

	if (monitor != nullptr) {
		QObject::connect(monitor, &QObject::destroyed, this, &HyprlandWorkspace::onMonitorDestroyed);
	}

	this->bMonitor = monitor;
}

void HyprlandWorkspace::onClientAdded(QObject* object) {
	QObject::connect(object, &QObject::destroyed, this, &HyprlandWorkspace::onClientRemoved);

	auto* client = dynamic_cast<HyprlandClient*>(object);
	QObject::connect(
	    client,
	    &HyprlandClient::urgentChanged,
	    this,
	    &HyprlandWorkspace::onClientUrgent
	);

	// If the workspace is focused, and the window is urgent, let's set it to false.
	if (client->bindableUrgent().value() && this->bFocused) {
		client->bindableUrgent().setValue(false);
	}
	emit this->onClientUrgent();
}

void HyprlandWorkspace::onClientRemoved(QObject* object) {
	QObject::disconnect(object, nullptr, this, nullptr);
	emit this->onClientUrgent();
}

void HyprlandWorkspace::onClientUrgent() {
	// If the workspace is focused, the urgent state is not relevant.
	if (this->bFocused) return;
	auto& clients = this->mClients.valueList();
	this->bUrgent = std::ranges::any_of(clients, [](HyprlandClient* client) {
		return client->bindableUrgent().value();
	});
}

void HyprlandWorkspace::clearUrgent() {
	if (!this->bUrgent) return;

	this->bUrgent = false;
	for (auto* client: this->mClients.valueList()) {
		client->bindableUrgent().setValue(false);
	}
}

void HyprlandWorkspace::onFocusedChanged() { this->clearUrgent(); }

void HyprlandWorkspace::onMonitorDestroyed() { this->bMonitor = nullptr; }

void HyprlandWorkspace::activate() {
	this->ipc->dispatch(QString("workspace %1").arg(this->bId.value()));
}

} // namespace qs::hyprland::ipc
