#pragma once
#include <qcontainerfwd.h>
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
	/// The workspace of the client
	Q_PROPERTY(
	    qs::hyprland::ipc::HyprlandWorkspace* workspace READ default NOTIFY workspaceChanged BINDABLE
	        bindableWorkspace
	)
	/// Whether the client is currently active or not
	Q_PROPERTY(bool active READ default NOTIFY activeChanged BINDABLE bindableActive);

	QML_ELEMENT;
	QML_UNCREATABLE("HyprlandClients must be retrieved from the HyprlandIpc object.");

public:
	explicit HyprlandClient(HyprlandIpc* ipc);

	/// Updates from an `openwindow` event
	void updateInitial(qint64 address, const QString& title, const QString& workspaceName);

	/// Updates the client from a JSON object.
	/// from `j/clients` request`.
	void updateFromObject(QVariantMap object);

	[[nodiscard]] QBindable<qint64> bindableAddress() { return &this->bAddress; }
	[[nodiscard]] QBindable<QString> bindableTitle() { return &this->bTitle; }
	[[nodiscard]] QBindable<HyprlandWorkspace*> bindableWorkspace() { return &this->bWorkspace; }
	[[nodiscard]] QBindable<bool> bindableActive() { return &this->bActive; }

signals:
	void addressChanged();
	void titleChanged();
	void workspaceChanged();
	void activeChanged();

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
};

} // namespace qs::hyprland::ipc
