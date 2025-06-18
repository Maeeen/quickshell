import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland

FloatingWindow {
    color: contentItem.palette.window

    ColumnLayout {
        Button {
            text: "Print"
            onClicked: function () {
                console.log("HyprlandToplevels");
                console.log(JSON.stringify(ToplevelManager.toplevels, null, 2));
            }
        }
        Repeater {
            model: Hyprland.toplevels

            ClientItem {}
        }

        Repeater {
            model: ToplevelManager.toplevels

            ToplevelItem {}
        }

        Repeater {
            model: ToplevelManager.toplevels

            ToplevelItem2 {}
        }

        Repeater {
            model: Hyprland.clients

            ClientItem2 {}
        }
    }

    component ToplevelItem2: Text {
        required property Toplevel modelData

        text: "Toplevel (direct) Window " + modelData.title
    }
    component ToplevelItem: Text {
        required property Toplevel modelData
        property HyprlandClient hypr: modelData.ToplevelAttachment.client

        text: "Toplevel (from attached) Window 0x" + hypr.address.toString(16) + " title " + hypr.title
    }
    component ClientItem: Text {
        required property HyprlandToplevel modelData

        text: "Client (from HyprlandIpc.toplevels) window 0x" + modelData.address.toString(16) + "; " + modelData.title
    }

    component ClientItem2: Text {
        required property HyprlandClient modelData

        text: "Client (from HyprlandIpc) window 0x" + modelData.address.toString(16) + "; " + modelData.title
    }
}
