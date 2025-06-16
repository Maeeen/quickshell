import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Hyprland

FloatingWindow {
    color: contentItem.palette.window

    ColumnLayout {
        Repeater {
            model: Hyprland.clients

            ClientItem {}
        }
    }

    component ClientItem: Text {
        required property HyprlandClient modelData

        text: "Window 0x" + modelData.address.toString(16) + "; " + modelData.title + " (workspace: " + modelData.workspace.id + ")"
    }
}
