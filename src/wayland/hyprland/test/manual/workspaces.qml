import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Hyprland

FloatingWindow {
    color: contentItem.palette.window

    ColumnLayout {
        Repeater {
            model: Hyprland.workspaces

            ColumnLayout {
                Text {
                    text: "Workspace " + modelData.id
                }
                Repeater {
                    model: modelData.clients
                    Text {
                        text: "Window 0x" + modelData.address.toString(16) + "; " + modelData.title + " (workspace: " + modelData.workspace.id + "," + "urgent: " + modelData.urgent + ")"
                    }
                }
            }
        }
    }
}
