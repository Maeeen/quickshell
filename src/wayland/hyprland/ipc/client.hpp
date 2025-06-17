#pragma once
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qtmetamacros.h>

#include "connection.hpp"
namespace qs::hyprland::ipc {

class HyprlandClient: public QObject {
	Q_OBJECT;
	// clang-format off
	/// The address of the client
	Q_PROPERTY(qint64 address READ default NOTIFY addressChanged BINDABLE bindableAddress);
	/// The title of the client
	Q_PROPERTY(QString title READ default NOTIFY titleChanged BINDABLE bindableTitle);
	/// The workspace of the client (might be null)
	Q_PROPERTY(qs::hyprland::ipc::HyprlandWorkspace* workspace READ default NOTIFY workspaceChanged BINDABLE bindableWorkspace);
	/// Whether the client is currently active or not
	Q_PROPERTY(bool active READ default NOTIFY activeChanged BINDABLE bindableActive);
	/// Whether the client is urgent or not
	Q_PROPERTY(bool urgent READ default NOTIFY urgentChanged BINDABLE bindableUrgent);
	/// The monitor where the client is currently located (might be null)
	Q_PROPERTY(qs::hyprland::ipc::HyprlandMonitor* monitor READ default NOTIFY monitorChanged BINDABLE bindableMonitor);
	// clang-format on
	QML_ELEMENT;
	QML_UNCREATABLE("HyprlandClients must be retrieved from the HyprlandIpc object.");

public:
	explicit HyprlandClient(HyprlandIpc* ipc);

	/// Updates from an `openwindow` event
	void updateInitial(qint64 address, const QString& title, const QString& workspaceName);

	/// Updates the client from a JSON object.
	/// from `j/clients` request`.
	void updateFromObject(QVariantMap object);

	/// Sets the workspace where the client belongs to.
	/// Takes care of removing the client from the previous workspace.
	/// and adds it to the new one.
	void setWorkspace(HyprlandWorkspace* workspace);

	[[nodiscard]] QBindable<qint64> bindableAddress() { return &this->bAddress; }
	[[nodiscard]] QBindable<QString> bindableTitle() { return &this->bTitle; }
	[[nodiscard]] QBindable<HyprlandWorkspace*> bindableWorkspace() { return &this->bWorkspace; }
	[[nodiscard]] QBindable<bool> bindableActive() { return &this->bActive; }
	[[nodiscard]] QBindable<bool> bindableUrgent() { return &this->bUrgent; }
	[[nodiscard]] QBindable<HyprlandMonitor*> bindableMonitor() { return &this->bMonitor; }

signals:
	void addressChanged();
	void titleChanged();
	void workspaceChanged();
	void activeChanged();
	void urgentChanged();
	void monitorChanged();

private:
	HyprlandIpc* ipc;

	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, qint64, bAddress, &HyprlandClient::addressChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, QString, bTitle, &HyprlandClient::titleChanged);
	Q_OBJECT_BINDABLE_PROPERTY(
	    HyprlandClient,
	    HyprlandWorkspace*,
	    bWorkspace,
	    &HyprlandClient::workspaceChanged
	);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, bool, bActive, &HyprlandClient::activeChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, bool, bUrgent, &HyprlandClient::urgentChanged);
	Q_OBJECT_BINDABLE_PROPERTY(
	    HyprlandClient,
	    HyprlandMonitor*,
	    bMonitor,
	    &HyprlandClient::monitorChanged
	);
};

} // namespace qs::hyprland::ipc
