#pragma once
#include <qobject.h>
#include <qtmetamacros.h>

#include "connection.hpp"
namespace qs::hyprland::ipc {

class HyprlandClient: public QObject {
	Q_OBJECT;
	/// The address of the client
	Q_PROPERTY(qint64 address READ default BINDABLE bindableAddress);
	/// The title of the client
	Q_PROPERTY(QString title READ default NOTIFY titleChanged BINDABLE bindableTitle);
	/// The workspace identifier of the client
	Q_PROPERTY(
	    qint32 workspaceId READ default NOTIFY workspaceIdChanged BINDABLE bindableWorkspaceId
	);
	/// Whether the client is currently active or not
	Q_PROPERTY(bool active READ default NOTIFY activeChanged BINDABLE bindableActive);

public:
	explicit HyprlandClient(HyprlandIpc* ipc);

	/// Updates from an `openwindow` event
	void updateInitial(qint64 address, const QString& title, qint32 workspaceId);

	/// Updates the client from a JSON object.
	/// from `j/clients` request`.
	void updateFromObject(QVariantMap object);

	[[nodiscard]] QBindable<qint64> bindableAddress() { return &this->bAddress; }
	[[nodiscard]] QBindable<QString> bindableTitle() { return &this->bTitle; }
	[[nodiscard]] QBindable<qint32> bindableWorkspaceId() { return &this->bWorkspaceId; }
	[[nodiscard]] QBindable<bool> bindableActive() { return &this->bActive; }

signals:
	void addressChanged();
	void titleChanged();
	void workspaceIdChanged();
	void activeChanged();

private:
	HyprlandIpc* ipc;

	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, qint64, bAddress, &HyprlandClient::addressChanged);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, QString, bTitle, &HyprlandClient::titleChanged);
	Q_OBJECT_BINDABLE_PROPERTY(
	    HyprlandClient,
	    qint32,
	    bWorkspaceId,
	    &HyprlandClient::workspaceIdChanged
	);
	Q_OBJECT_BINDABLE_PROPERTY(HyprlandClient, bool, bActive, &HyprlandClient::activeChanged);
};

} // namespace qs::hyprland::ipc
