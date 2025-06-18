#include "connection.hpp"
#include <algorithm>
#include <functional>
#include <utility>

#include <qbytearrayview.h>
#include <qcontainerfwd.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qlocalsocket.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qobject.h>
#include <qproperty.h>
#include <qqml.h>
#include <qtenvironmentvariables.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>

#include "../../../core/model.hpp"
#include "../../../core/qmlscreen.hpp"
#include "../../toplevel_management/qml.hpp"
#include "client.hpp"
#include "monitor.hpp"
#include "to_wayland_toplevel.hpp"
#include "toplevel_mapping.hpp"
#include "workspace.hpp"

namespace qs::hyprland::ipc {

namespace {
Q_LOGGING_CATEGORY(logHyprlandIpc, "quickshell.hyprland.ipc", QtWarningMsg);
Q_LOGGING_CATEGORY(logHyprlandIpcEvents, "quickshell.hyprland.ipc.events", QtWarningMsg);
} // namespace

HyprlandIpc::HyprlandIpc() {
	auto his = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
	if (his.isEmpty()) {
		qWarning() << "$HYPRLAND_INSTANCE_SIGNATURE is unset. Cannot connect to hyprland.";
		return;
	}

	auto runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
	auto hyprlandDir = runtimeDir + "/hypr/" + his;

	if (!QFileInfo(hyprlandDir).isDir()) {
		hyprlandDir = "/tmp/hypr/" + his;
	}

	if (!QFileInfo(hyprlandDir).isDir()) {
		qWarning() << "Unable to find hyprland socket. Cannot connect to hyprland.";
		return;
	}

	this->bFocusedWorkspace.setBinding([this]() -> HyprlandWorkspace* {
		if (!this->bFocusedMonitor) return nullptr;
		return this->bFocusedMonitor->bindableActiveWorkspace().value();
	});

	this->mRequestSocketPath = hyprlandDir + "/.socket.sock";
	this->mEventSocketPath = hyprlandDir + "/.socket2.sock";

	// clang-format off
	QObject::connect(&this->eventSocket, &QLocalSocket::errorOccurred, this, &HyprlandIpc::eventSocketError);
	QObject::connect(&this->eventSocket, &QLocalSocket::stateChanged, this, &HyprlandIpc::eventSocketStateChanged);
	QObject::connect(&this->eventSocket, &QLocalSocket::readyRead, this, &HyprlandIpc::eventSocketReady);
	// clang-format off
	auto* toplevelMappingManager = HyprlandToplevelMappingManager::instance();
	QObject::connect(toplevelMappingManager, &HyprlandToplevelMappingManager::toplevelAddressed,
	                 this, &HyprlandIpc::toplevelAddressed);
	// clang-format on

	this->eventSocket.connectToServer(this->mEventSocketPath, QLocalSocket::ReadOnly);
	this->refreshMonitors(true);
	this->refreshWorkspaces(true);
	this->refreshClients();
}

QString HyprlandIpc::requestSocketPath() const { return this->mRequestSocketPath; }
QString HyprlandIpc::eventSocketPath() const { return this->mEventSocketPath; }

void HyprlandIpc::eventSocketError(QLocalSocket::LocalSocketError error) const {
	if (!this->valid) {
		qWarning() << "Unable to connect to hyprland event socket:" << error;
	} else {
		qWarning() << "Hyprland event socket error:" << error;
	}
}

void HyprlandIpc::eventSocketStateChanged(QLocalSocket::LocalSocketState state) {
	if (state == QLocalSocket::ConnectedState) {
		qCInfo(logHyprlandIpc) << "Hyprland event socket connected.";
		emit this->connected();
	} else if (state == QLocalSocket::UnconnectedState && this->valid) {
		qCWarning(logHyprlandIpc) << "Hyprland event socket disconnected.";
	}

	this->valid = state == QLocalSocket::ConnectedState;
}

void HyprlandIpc::eventSocketReady() {
	while (true) {
		auto rawEvent = this->eventSocket.readLine();
		if (rawEvent.isEmpty()) break;

		// remove trailing \n
		rawEvent.truncate(rawEvent.length() - 1);
		auto splitIdx = rawEvent.indexOf(">>");
		auto event = QByteArrayView(rawEvent.data(), splitIdx);
		auto data = QByteArrayView(
		    rawEvent.data() + splitIdx + 2,     // NOLINT
		    rawEvent.data() + rawEvent.length() // NOLINT
		);
		qCDebug(logHyprlandIpcEvents) << "Received event:" << rawEvent << "parsed as" << event << data;

		this->event.name = event;
		this->event.data = data;
		this->onEvent(&this->event);
		emit this->rawEvent(&this->event);
	}
}

void HyprlandIpc::toplevelAddressed(
    qs::wayland::toplevel_management::impl::ToplevelHandle* handle,
    quint64 address
) {
	auto* toplevel = qs::wayland::toplevel_management::ToplevelManager::instance()->forImpl(handle);
	if (toplevel == nullptr) return;

	QObject* attachment = qmlAttachedPropertiesObject<ToplevelAttachment>(toplevel, false);
	if (attachment == nullptr) {
		qCWarning(logHyprlandIpc) << "Toplevel" << toplevel->title() << "addressed with" << address
		                          << "but no ToplevelAttachment found.";
		return;
	}
	if (auto* attachment2 = qobject_cast<ToplevelAttachment*>(attachment)) {
		// TODO: change type
		auto* client = this->findClientByAddress((qint64) address, true);
		if (attachment2->bindableClient().value() == client) {
			qCDebug(logHyprlandIpc) << "Toplevel" << toplevel->title() << "already has client"
			                        << "for address" << address;
			return;
		} else {
			qCDebug(logHyprlandIpc) << "Toplevel" << toplevel->title() << "addressed with" << address
			                        << "setting client to"
			                        << "was my time toshine!!";
			attachment2->bindableClient().setValue(client);
		}
	} else {
		qCWarning(logHyprlandIpc) << "Toplevel" << toplevel->title() << "addressed with" << address
		                          << "but ToplevelAttachment is not HyprlandToplevel.";
	}
	auto* x = this->clients();

	qCDebug(logHyprlandIpc) << "Toplevel" << toplevel->title() << "addressed with" << address << x;
}

void HyprlandIpc::makeRequest(
    const QByteArray& request,
    const std::function<void(bool, QByteArray)>& callback
) {
	auto* requestSocket = new QLocalSocket(this);
	qCDebug(logHyprlandIpc) << "Making request:" << request;

	auto connectedCallback = [this, request, requestSocket, callback]() {
		auto responseCallback = [requestSocket, callback]() {
			auto response = requestSocket->readAll();
			callback(true, std::move(response));
			delete requestSocket;
		};

		QObject::connect(requestSocket, &QLocalSocket::readyRead, this, responseCallback);

		requestSocket->write(request);
		requestSocket->flush();
	};

	auto errorCallback = [=](QLocalSocket::LocalSocketError error) {
		qCWarning(logHyprlandIpc) << "Error making request:" << error << "request:" << request;
		requestSocket->deleteLater();
		callback(false, {});
	};

	QObject::connect(requestSocket, &QLocalSocket::connected, this, connectedCallback);
	QObject::connect(requestSocket, &QLocalSocket::errorOccurred, this, errorCallback);

	requestSocket->connectToServer(this->mRequestSocketPath);
}

void HyprlandIpc::dispatch(const QString& request) {
	this->makeRequest(
	    ("dispatch " + request).toUtf8(),
	    [request](bool success, const QByteArray& response) {
		    if (!success) {
			    qCWarning(logHyprlandIpc) << "Failed to request dispatch of" << request;
			    return;
		    }

		    if (response != "ok") {
			    qCWarning(logHyprlandIpc)
			        << "Dispatch request" << request << "failed with error" << response;
		    }
	    }
	);
}

ObjectModel<HyprlandMonitor>* HyprlandIpc::monitors() { return &this->mMonitors; }

ObjectModel<HyprlandWorkspace>* HyprlandIpc::workspaces() { return &this->mWorkspaces; }

ObjectModel<HyprlandClient>* HyprlandIpc::clients() { return &this->mClients; }

QVector<QByteArrayView> HyprlandIpc::parseEventArgs(QByteArrayView event, quint16 count) {
	auto args = QVector<QByteArrayView>();

	for (auto i = 0; i < count - 1; i++) {
		auto splitIdx = event.indexOf(',');
		if (splitIdx == -1) break;
		args.push_back(event.sliced(0, splitIdx));
		event = event.sliced(splitIdx + 1);
	}

	if (!event.isEmpty()) {
		args.push_back(event);
	}

	while (args.length() < count) {
		args.push_back(QByteArrayView());
	}

	return args;
}

QVector<QString> HyprlandIpcEvent::parse(qint32 argumentCount) const {
	auto args = QVector<QString>();

	for (auto arg: this->parseView(argumentCount)) {
		args.push_back(QString::fromUtf8(arg));
	}

	return args;
}

QVector<QByteArrayView> HyprlandIpcEvent::parseView(qint32 argumentCount) const {
	return HyprlandIpc::parseEventArgs(this->data, argumentCount);
}

QString HyprlandIpcEvent::nameStr() const { return QString::fromUtf8(this->name); }
QString HyprlandIpcEvent::dataStr() const { return QString::fromUtf8(this->data); }

HyprlandIpc* HyprlandIpc::instance() {
	static HyprlandIpc* instance = nullptr; // NOLINT

	if (instance == nullptr) {
		instance = new HyprlandIpc();
	}

	return instance;
}

void HyprlandIpc::onEvent(HyprlandIpcEvent* event) {
	if (event->name == "configreloaded") {
		this->refreshMonitors(true);
		this->refreshWorkspaces(true);
		this->refreshClients();
	} else if (event->name == "monitoraddedv2") {
		auto args = event->parseView(3);

		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));

		// hyprland will often reference the monitor before creation, in which case
		// it will already exist.
		auto* monitor = this->findMonitorByName(name, false);
		auto existed = monitor != nullptr;

		if (monitor == nullptr) {
			monitor = new HyprlandMonitor(this);
		}

		qCDebug(logHyprlandIpc) << "Monitor added with id" << id << "name" << name
		                        << "preemptively created:" << existed;

		monitor->updateInitial(id, name, QString::fromUtf8(args.at(2)));

		if (!existed) {
			this->mMonitors.insertObject(monitor);
		}

		// refresh even if it already existed because workspace focus might have changed.
		this->refreshMonitors(false);
	} else if (event->name == "monitorremoved") {
		const auto& mList = this->mMonitors.valueList();
		auto name = QString::fromUtf8(event->data);

		auto monitorIter = std::ranges::find_if(mList, [name](HyprlandMonitor* m) {
			return m->bindableName().value() == name;
		});

		if (monitorIter == mList.end()) {
			qCWarning(logHyprlandIpc) << "Got removal for monitor" << name
			                          << "which was not previously tracked.";
			return;
		}

		auto index = monitorIter - mList.begin();
		auto* monitor = *monitorIter;

		qCDebug(logHyprlandIpc) << "Monitor removed with id" << monitor->bindableId().value() << "name"
		                        << monitor->bindableName().value();
		this->mMonitors.removeAt(index);

		// delete the monitor object in the next event loop cycle so it's likely to
		// still exist when future events reference it after destruction.
		// If we get to the next cycle and things still reference it (unlikely), nulls
		// can make it to the frontend.
		monitor->deleteLater();
	} else if (event->name == "createworkspacev2") {
		auto args = event->parseView(2);

		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));

		qCDebug(logHyprlandIpc) << "Workspace created with id" << id << "name" << name;

		auto* workspace = this->findWorkspaceByName(name, false);
		auto existed = workspace != nullptr;

		if (workspace == nullptr) {
			workspace = new HyprlandWorkspace(this);
		}

		workspace->updateInitial(id, name);

		if (!existed) {
			this->refreshWorkspaces(false);
			this->mWorkspaces.insertObjectSorted(workspace, &HyprlandIpc::compareWorkspaces);
		}
	} else if (event->name == "destroyworkspacev2") {
		auto args = event->parseView(2);

		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));

		const auto& mList = this->mWorkspaces.valueList();

		auto workspaceIter = std::ranges::find_if(mList, [id](HyprlandWorkspace* m) {
			return m->bindableId().value() == id;
		});

		if (workspaceIter == mList.end()) {
			qCWarning(logHyprlandIpc) << "Got removal for workspace id" << id << "name" << name
			                          << "which was not previously tracked.";
			return;
		}

		auto index = workspaceIter - mList.begin();
		auto* workspace = *workspaceIter;

		qCDebug(logHyprlandIpc) << "Workspace removed with id" << id << "name" << name;
		this->mWorkspaces.removeAt(index);

		// workspaces have not been observed to be referenced after deletion
		delete workspace;

		for (auto* monitor: this->mMonitors.valueList()) {
			if (monitor->bindableActiveWorkspace().value() == nullptr) {
				// removing a monitor will cause a new workspace to be created and destroyed after removal,
				// but it won't go back to a real workspace afterwards and just leaves a null, so we
				// re-query monitors if this appears to be the case.
				this->refreshMonitors(false);
				break;
			}
		}
	} else if (event->name == "focusedmon") {
		auto args = event->parseView(2);
		auto name = QString::fromUtf8(args.at(0));
		auto workspaceName = QString::fromUtf8(args.at(1));

		HyprlandWorkspace* workspace = nullptr;
		if (workspaceName != "?") { // what the fuck
			workspace = this->findWorkspaceByName(workspaceName, false);
		}

		auto* monitor = this->findMonitorByName(name, true);
		this->setFocusedMonitor(monitor);
		monitor->setActiveWorkspace(workspace);
		qCDebug(logHyprlandIpc) << "Monitor" << name << "focused with workspace"
		                        << (workspace ? workspace->bindableId().value() : -1);
	} else if (event->name == "workspacev2") {
		auto args = event->parseView(2);
		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));

		if (this->bFocusedMonitor != nullptr) {
			auto* workspace = this->findWorkspaceByName(name, true, id);
			this->bFocusedMonitor->setActiveWorkspace(workspace);
			// Workspace has been visited, can dismiss the "urgent" notification
			qCDebug(logHyprlandIpc) << "Workspace" << id << "activated on"
			                        << this->bFocusedMonitor->bindableName().value();
		}
	} else if (event->name == "moveworkspacev2") {
		auto args = event->parseView(3);
		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));
		auto monitorName = QString::fromUtf8(args.at(2));

		auto* workspace = this->findWorkspaceByName(name, true, id);
		auto* monitor = this->findMonitorByName(monitorName, true);

		qCDebug(logHyprlandIpc) << "Workspace" << id << "moved to monitor" << monitorName;
		workspace->setMonitor(monitor);
	} else if (event->name == "renameworkspace") {
		auto args = event->parseView(2);
		auto id = args.at(0).toInt();
		auto name = QString::fromUtf8(args.at(1));

		const auto& mList = this->mWorkspaces.valueList();

		auto workspaceIter = std::ranges::find_if(mList, [id](HyprlandWorkspace* m) {
			return m->bindableId().value() == id;
		});

		if (workspaceIter == mList.end()) return;

		qCDebug(logHyprlandIpc) << "Workspace with id" << id << "renamed from"
		                        << (*workspaceIter)->bindableName().value() << "to" << name;

		(*workspaceIter)->bindableName().setValue(name);
	} else if (event->name == "fullscreen") {
		if (auto* workspace = this->bFocusedWorkspace.value()) {
			workspace->bindableHasFullscreen().setValue(event->data == "1");
		}

		// In theory knowing the current workspace would be enough to determine where
		// the fullscreen state changed, but this falls apart if you move a fullscreen
		// window between workspaces.
		this->refreshWorkspaces(false);
	} else if (event->name == "openwindow") {
		// new window opened
		auto args = event->parseView(4);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);

		if (!ok) return;

		auto workspaceName = QString::fromUtf8(args.at(1));
		auto windowTitle = QString::fromUtf8(args.at(2));
		auto windowClass = QString::fromUtf8(args.at(3));

		HyprlandWorkspace* workspace = this->findWorkspaceByName(workspaceName, false);
		if (!workspace) {
			qCWarning(logHyprlandIpc) << "Got openwindow event for workspace" << workspaceName
			                          << "which was not previously tracked.";
			return;
		}

		HyprlandClient* client = this->findClientByAddress(windowAddress, false);
		const bool existed = client != nullptr;
		if (!client) {
			client = new HyprlandClient(this);
		}
		client->updateInitial(windowAddress, windowTitle, workspace->bindableName().value());

		if (!existed) {
			this->refreshClients();
			this->mClients.insertObject(client, -1);
			qCDebug(logHyprlandIpc) << "New client created with address" << windowAddress << "title"
			                        << windowTitle << "workspace id" << workspace->bindableId().value();
		}
	} else if (event->name == "activewindowv2") {
		auto args = event->parseView(1);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);

		if (!ok) return;

		// Iterate through all clients and set if they are active or not
		const auto& mList = this->mClients.valueList();
		Qt::beginPropertyUpdateGroup();
		for (auto* client: mList) {
			if (client->bindableAddress().value() == windowAddress) {
				client->bindableActive().setValue(true);
				qCDebug(logHyprlandIpc) << "Client with address" << windowAddress << "is now active.";
			} else {
				client->bindableActive().setValue(false);
			}
		}
		Qt::endPropertyUpdateGroup();
	} else if (event->name == "closewindow") {
		auto args = event->parseView(1);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);

		if (!ok) return;

		const auto& mList = this->mClients.valueList();
		auto clientIter = std::ranges::find_if(mList, [windowAddress](HyprlandClient* c) {
			return c->bindableAddress().value() == windowAddress;
		});
		if (clientIter == mList.end()) {
			qCWarning(logHyprlandIpc) << "Got closewindow event for client with address" << windowAddress
			                          << "which was not previously tracked.";
			return;
		}
		HyprlandClient* client = *clientIter;
		auto index = clientIter - mList.begin();
		this->mClients.removeAt(index);

		// Remove from the workspace as well
		HyprlandWorkspace* workspace = client->bindableWorkspace().value();
		if (workspace) {
			workspace->clients()->removeObject(client);
		}

		delete client;
	} else if (event->name == "windowtitlev2") {
		auto args = event->parseView(2);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);
		auto windowTitle = QString::fromUtf8(args.at(1));

		if (!ok) return;

		// It happens that Hyprland sends windowtitlev2 events before event
		// "openwindow" is emitted, so let's preemptively create it
		HyprlandClient* client = this->findClientByAddress(windowAddress, true);
		if (!client) {
			qCWarning(logHyprlandIpc) << "Got windowtitlev2 event for client with address"
			                          << windowAddress << "which was not previously tracked.";
			return;
		}
		client->bindableTitle().setValue(windowTitle);
	} else if (event->name == "movewindowv2") {
		auto args = event->parseView(3);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);
		auto workspaceName = QString::fromUtf8(args.at(2));

		HyprlandClient* client = this->findClientByAddress(windowAddress, false);
		if (!client) {
			qCWarning(logHyprlandIpc) << "Got movewindowv2 event for client with address" << windowAddress
			                          << "which was not previously tracked.";
			return;
		}

		HyprlandWorkspace* workspace = this->findWorkspaceByName(workspaceName, false);
		if (!workspace) {
			qCWarning(logHyprlandIpc) << "Got movewindowv2 event for workspace" << args.at(2)
			                          << "which was not previously tracked.";
			return;
		}

		client->setWorkspace(workspace);
	} else if (event->name == "urgent") {
		auto args = event->parseView(1);
		bool ok = false;
		auto windowAddress = args.at(0).toLongLong(&ok, 16);

		// It happens that Hyprland sends urgent events before event
		// "openwindow" is emitted, so let's preemptively create it
		HyprlandClient* client = this->findClientByAddress(windowAddress, true);

		if (!client) {
			qCWarning(logHyprlandIpc) << "Got urgent event for client with address" << windowAddress
			                          << "which was not previously tracked.";
			return;
		}

		client->bindableUrgent().setValue(true);
	}
}

HyprlandWorkspace*
HyprlandIpc::findWorkspaceByName(const QString& name, bool createIfMissing, qint32 id) {
	const auto& mList = this->mWorkspaces.valueList();
	HyprlandWorkspace* workspace = nullptr;

	if (id != -1) {
		auto workspaceIter = std::ranges::find_if(mList, [&](HyprlandWorkspace* m) {
			return m->bindableId().value() == id;
		});

		workspace = workspaceIter == mList.end() ? nullptr : *workspaceIter;
	}

	if (!workspace) {
		auto workspaceIter = std::ranges::find_if(mList, [&](HyprlandWorkspace* m) {
			return m->bindableName().value() == name;
		});

		workspace = workspaceIter == mList.end() ? nullptr : *workspaceIter;
	}

	if (workspace) {
		return workspace;
	} else if (createIfMissing) {
		qCDebug(logHyprlandIpc) << "Workspace" << name
		                        << "requested before creation, performing early init with id" << id;

		auto* workspace = new HyprlandWorkspace(this);
		workspace->updateInitial(id, name);
		this->mWorkspaces.insertObjectSorted(workspace, &HyprlandIpc::compareWorkspaces);
		return workspace;
	} else {
		return nullptr;
	}
}

void HyprlandIpc::refreshWorkspaces(bool canCreate) {
	if (this->requestingWorkspaces) return;
	this->requestingWorkspaces = true;

	this->makeRequest("j/workspaces", [this, canCreate](bool success, const QByteArray& resp) {
		this->requestingWorkspaces = false;
		if (!success) return;

		qCDebug(logHyprlandIpc) << "Parsing workspaces response";
		auto json = QJsonDocument::fromJson(resp).array();

		const auto& mList = this->mWorkspaces.valueList();
		auto ids = QVector<quint32>();

		for (auto entry: json) {
			auto object = entry.toObject().toVariantMap();

			auto id = object.value("id").toInt();

			auto workspaceIter = std::ranges::find_if(mList, [&](HyprlandWorkspace* m) {
				return m->bindableId().value() == id;
			});

			// Only fall back to name-based filtering as a last resort, for workspaces where
			// no ID has been determined yet.
			if (workspaceIter == mList.end()) {
				auto name = object.value("name").toString();

				workspaceIter = std::ranges::find_if(mList, [&](HyprlandWorkspace* m) {
					return m->bindableId().value() == -1 && m->bindableName().value() == name;
				});
			}

			auto* workspace = workspaceIter == mList.end() ? nullptr : *workspaceIter;
			auto existed = workspace != nullptr;

			if (!existed) {
				if (!canCreate) continue;
				workspace = new HyprlandWorkspace(this);
			}

			workspace->updateFromObject(object);

			if (!existed) {
				this->mWorkspaces.insertObjectSorted(workspace, &HyprlandIpc::compareWorkspaces);
			}

			ids.push_back(id);
		}

		if (canCreate) {
			auto removedWorkspaces = QVector<HyprlandWorkspace*>();

			for (auto* workspace: mList) {
				if (!ids.contains(workspace->bindableId().value())) {
					removedWorkspaces.push_back(workspace);
				}
			}

			for (auto* workspace: removedWorkspaces) {
				this->mWorkspaces.removeObject(workspace);
				delete workspace;
			}
		}
	});
}

HyprlandClient* HyprlandIpc::findClientByAddress(qint64 address, bool createIfMissing) {
	const auto& mList = this->mClients.valueList();
	HyprlandClient* client = nullptr;

	auto clientIter = std::ranges::find_if(mList, [&](HyprlandClient* m) {
		return m->bindableAddress().value() == address;
	});

	client = clientIter == mList.end() ? nullptr : *clientIter;

	if (!client && createIfMissing) {
		qCDebug(logHyprlandIpc) << "Client with address" << address
		                        << "requested before creation, performing early init";
		client = new HyprlandClient(this);
		client->updateInitial(address, "", "");
		this->mClients.insertObject(client, -1);
	}

	return client;
}

void HyprlandIpc::refreshClients() {
	if (this->requestingClients) return;
	this->requestingClients = true;

	this->makeRequest("j/clients", [this](bool success, const QByteArray& resp) {
		this->requestingClients = false;
		if (!success) return;

		qCDebug(logHyprlandIpc) << "Parsing clients response";
		auto json = QJsonDocument::fromJson(resp).array();

		const auto& mList = this->mClients.valueList();

		for (auto entry: json) {
			auto object = entry.toObject().toVariantMap();

			bool ok = false;
			auto address = object.value("address").toString().toLongLong(&ok, 16);

			if (!ok) {
				qCDebug(logHyprlandIpc) << "Invalid address in client entry, skipping:" << object;
				continue;
			}

			auto clientsIter = std::ranges::find_if(mList, [&](HyprlandClient* c) {
				return c->bindableAddress().value() == address;
			});

			auto* client = clientsIter == mList.end() ? nullptr : *clientsIter;
			auto exists = client != nullptr;

			if (!exists) {
				client = new HyprlandClient(this);
			}

			client->updateFromObject(object);

			if (!exists) {
				qCDebug(logHyprlandIpc) << "New client created with address" << address << "title"
				                        << client->bindableTitle().value();
				this->mClients.insertObject(client, -1);
			}
		}
	});
}

HyprlandMonitor*
HyprlandIpc::findMonitorByName(const QString& name, bool createIfMissing, qint32 id) {
	const auto& mList = this->mMonitors.valueList();

	auto monitorIter = std::ranges::find_if(mList, [name](HyprlandMonitor* m) {
		return m->bindableName().value() == name;
	});

	if (monitorIter != mList.end()) {
		return *monitorIter;
	} else if (createIfMissing) {
		qCDebug(logHyprlandIpc) << "Monitor" << name
		                        << "requested before creation, performing early init";
		auto* monitor = new HyprlandMonitor(this);
		monitor->updateInitial(id, name, "");
		this->mMonitors.insertObject(monitor);
		return monitor;
	} else {
		return nullptr;
	}
}

HyprlandMonitor* HyprlandIpc::monitorFor(QuickshellScreenInfo* screen) {
	// Wayland monitors appear after hyprland ones are created and disappear after destruction
	// so simply not doing any preemptive creation is enough, however if this call creates
	// the HyprlandIpc singleton then monitors won't be initialized, in which case we
	// preemptively create one.

	if (screen == nullptr) return nullptr;
	return this->findMonitorByName(screen->name(), !this->monitorsRequested);
}

void HyprlandIpc::setFocusedMonitor(HyprlandMonitor* monitor) {
	auto* oldMonitor = this->bFocusedMonitor.value();
	if (monitor == oldMonitor) return;

	if (this->bFocusedMonitor != nullptr) {
		QObject::disconnect(this->bFocusedMonitor, nullptr, this, nullptr);
	}

	if (monitor != nullptr) {
		QObject::connect(monitor, &QObject::destroyed, this, &HyprlandIpc::onFocusedMonitorDestroyed);
	}

	this->bFocusedMonitor = monitor;
}

void HyprlandIpc::onFocusedMonitorDestroyed() {
	this->bFocusedMonitor = nullptr;
	emit this->focusedMonitorChanged();
}

void HyprlandIpc::refreshMonitors(bool canCreate) {
	if (this->requestingMonitors) return;
	this->requestingMonitors = true;

	this->makeRequest("j/monitors", [this, canCreate](bool success, const QByteArray& resp) {
		this->requestingMonitors = false;
		if (!success) return;

		this->monitorsRequested = true;

		qCDebug(logHyprlandIpc) << "parsing monitors response";
		auto json = QJsonDocument::fromJson(resp).array();

		const auto& mList = this->mMonitors.valueList();
		auto names = QVector<QString>();

		for (auto entry: json) {
			auto object = entry.toObject().toVariantMap();
			auto name = object.value("name").toString();

			auto monitorIter = std::ranges::find_if(mList, [name](HyprlandMonitor* m) {
				return m->bindableName().value() == name;
			});

			auto* monitor = monitorIter == mList.end() ? nullptr : *monitorIter;
			auto existed = monitor != nullptr;

			if (monitor == nullptr) {
				if (!canCreate) continue;
				monitor = new HyprlandMonitor(this);
			}

			monitor->updateFromObject(object);

			if (!existed) {
				this->mMonitors.insertObject(monitor);
			}

			names.push_back(name);
		}

		auto removedMonitors = QVector<HyprlandMonitor*>();

		for (auto* monitor: mList) {
			if (!names.contains(monitor->bindableName().value())) {
				removedMonitors.push_back(monitor);
			}
		}

		for (auto* monitor: removedMonitors) {
			this->mMonitors.removeObject(monitor);
			// see comment in onEvent
			monitor->deleteLater();
		}
	});
}

bool HyprlandIpc::compareWorkspaces(HyprlandWorkspace* a, HyprlandWorkspace* b) {
	return a->bindableId().value() > b->bindableId().value();
}

} // namespace qs::hyprland::ipc
