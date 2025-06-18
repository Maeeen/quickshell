
#pragma once

#include <iostream>

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qproperty.h>
#include <qqmlintegration.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "../../toplevel_management/qml.hpp"
#include "client.hpp"
#include "connection.hpp"
#include "toplevel_mapping.hpp"

namespace qs::hyprland::ipc {

//! Exposes Hyprland window address for a Toplevel
/// Attached object of @@Quickshell.Wayland.Toplevel which exposes
/// a Hyprland window address for the window.
class ToplevelAttachment: public QObject {
	Q_OBJECT;
	QML_ELEMENT;
	QML_UNCREATABLE("");
	QML_ATTACHED(ToplevelAttachment);
	// clang-format off
	// TODO : format
	Q_PROPERTY(HyprlandClient* client READ default NOTIFY clientChanged BINDABLE bindableClient);
	// clang-format on

public:
	explicit ToplevelAttachment() { std::cout << "I exist lol" << std::endl; }
	static ToplevelAttachment* qmlAttachedProperties(QObject* object) {
		if (auto* toplevel = qobject_cast<qs::wayland::toplevel_management::Toplevel*>(object)) {
			auto* attachment = new ToplevelAttachment();
			if (auto* implHandle = toplevel->implHandle()) {
				quint64 addr = HyprlandToplevelMappingManager::instance()->getToplevelAddress(implHandle);
				if (auto* client = HyprlandIpc::instance()->findClientByAddress((qint64) addr, false)) {
					attachment->bindableClient().setValue(client);
				} else {
					std::cout << "No client found for address: " << addr << std::endl;
				}
			} else {
				std::cout << "No impl handle for toplevel" << std::endl;
			}
			return attachment;
		} else {
			return nullptr;
		}
	}

	[[nodiscard]] QBindable<HyprlandClient*> bindableClient() { return &this->bClient; }

signals:
	void clientChanged();

private slots:
	// void onToplevelAddressed(
	//     qs::wayland::toplevel_management::impl::ToplevelHandle* handle,
	//     quint64 address
	// );

private:
	// clang-format off
	Q_OBJECT_BINDABLE_PROPERTY(ToplevelAttachment,HyprlandClient*, bClient, &ToplevelAttachment::bindableClient);
	// clang-format on
};

} // namespace qs::hyprland::ipc
