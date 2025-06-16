#pragma once

#include <qbytearrayview.h>
#include <qjsonobject.h>
#include <qobject.h>
#include <qproperty.h>
#include <qqmlintegration.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "connection.hpp"

namespace qs::hyprland::ipc {

class HyprlandMonitor;

class HyprlandWorkspace: public QObject {
	Q_OBJECT;
	// clang-format off
	Q_PROPERTY(qint32 id READ default NOTIFY idChanged BINDABLE bindableId);
	Q_PROPERTY(QString name READ default NOTIFY nameChanged BINDABLE bindableName);
	/// If this workspace is currently active on its monitor. See also @@focused.
	Q_PROPERTY(bool active READ default NOTIFY activeChanged BINDABLE bindableActive);
	/// If this workspace is currently active on a monitor and that monitor is currently
	/// focused. See also @@active.
	Q_PROPERTY(bool focused READ default NOTIFY focusedChanged BINDABLE bindableFocused);
	/// If this workspace currently has a fullscreen client.
	Q_PROPERTY(bool hasFullscreen READ default NOTIFY focusedChanged BINDABLE bindableHasFullscreen);
	/// If this workspace has a client which has emitted a urgent state.
	Q_PROPERTY(bool urgent READ default NOTIFY urgentChanged BINDABLE bindableUrgent);
	/// All hyprland clients (windows) that are part of the workspace.
	QSDOC_TYPE_OVERRIDE(ObjectModel<qs::hyprland::ipc::HyprlandClient>*);
	Q_PROPERTY(UntypedObjectModel* clients READ clients CONSTANT);
	/// Last json returned for this workspace, as a javascript object.
	///
	/// > [!WARNING] This is *not* updated unless the workspace object is fetched again from
	/// > Hyprland. If you need a value that is subject to change and does not have a dedicated
	/// > property, run @@Hyprland.refreshWorkspaces() and wait for this property to update.
	Q_PROPERTY(QVariantMap lastIpcObject READ lastIpcObject NOTIFY lastIpcObjectChanged);
	Q_PROPERTY(qs::hyprland::ipc::HyprlandMonitor* monitor READ default NOTIFY monitorChanged BINDABLE bindableMonitor);
	// clang-format on
	QML_ELEMENT;
	QML_UNCREATABLE("HyprlandWorkspaces must be retrieved from the HyprlandIpc object.");

public:
	explicit HyprlandWorkspace(HyprlandIpc* ipc);

	void updateInitial(qint32 id, const QString& name);
	void updateFromObject(QVariantMap object);

	/// Activate the workspace.
	///
	/// > [!NOTE] This is equivalent to running
	/// > ```qml
	/// > HyprlandIpc.dispatch(`workspace ${workspace.id}`);
	/// > ```
	Q_INVOKABLE void activate();

	[[nodiscard]] QBindable<qint32> bindableId() { return &this->bId; }
	[[nodiscard]] QBindable<QString> bindableName() { return &this->bName; }
	[[nodiscard]] QBindable<bool> bindableActive() { return &this->bActive; }
	[[nodiscard]] QBindable<bool> bindableFocused() { return &this->bFocused; }
	[[nodiscard]] QBindable<bool> bindableHasFullscreen() { return &this->bHasFullscreen; }
	[[nodiscard]] QBindable<bool> bindableUrgent() { return &this->bUrgent; }
	[[nodiscard]] QBindable<HyprlandMonitor*> bindableMonitor() { return &this->bMonitor; }

	[[nodiscard]] ObjectModel<HyprlandClient>* clients() { return &this->mClients; };

	[[nodiscard]] QVariantMap lastIpcObject() const;

	void setMonitor(HyprlandMonitor* monitor);
	/// Removes the urgent status from this workspace, and consequently
	/// from all clients.
	void clearUrgent();

signals:
	void idChanged();
	void nameChanged();
	void activeChanged();
	void focusedChanged();
	void hasFullscreenChanged();
	void urgentChanged();
	void lastIpcObjectChanged();
	void monitorChanged();

private slots:
	void onMonitorDestroyed();
	void onClientAdded(QObject* object);
	void onClientRemoved(QObject* object);
	void onClientUrgent();
	void onFocusedChanged();

private:
	HyprlandIpc* ipc;

	QVariantMap mLastIpcObject;

	ObjectModel<HyprlandClient> mClients {this};

	// clang-format off
	Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(HyprlandWorkspace, qint32, bId, -1, &HyprlandWorkspace::idChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, QString, bName, &HyprlandWorkspace::nameChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, bool, bActive, &HyprlandWorkspace::activeChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, bool, bFocused, &HyprlandWorkspace::focusedChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, bool, bHasFullscreen, &HyprlandWorkspace::hasFullscreenChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, bool, bUrgent, &HyprlandWorkspace::urgentChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandWorkspace, HyprlandMonitor*, bMonitor, &HyprlandWorkspace::monitorChanged);
	// clang-format on
};

} // namespace qs::hyprland::ipc
