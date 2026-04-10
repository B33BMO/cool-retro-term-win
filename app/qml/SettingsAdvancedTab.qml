/*******************************************************************************
* Copyright (c) 2013-2021 "Filippo Scognamiglio"
* https://github.com/Swordfish90/cool-retro-term
*
* This file is part of cool-retro-term.
*
* cool-retro-term is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/
import QtQuick 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1
import QtQml 2.0

import "Components"

ColumnLayout {
    GroupBox {
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: qsTr("Miscellaneous")
        padding: appSettings.defaultMargin

        ColumnLayout {
            anchors.fill: parent

            // Windows shell preset selector
            RowLayout {
                visible: appSettings.isWindows
                Layout.fillWidth: true
                Label {
                    text: qsTr("Shell preset:")
                }
                ComboBox {
                    id: shellPreset
                    Layout.fillWidth: true
                    model: [
                        { label: qsTr("Default"),        cmd: "" },
                        { label: "cmd.exe",              cmd: "cmd.exe" },
                        { label: "PowerShell",           cmd: "powershell.exe" },
                        { label: "PowerShell Core",      cmd: "pwsh.exe" },
                        { label: "WSL (default)",        cmd: "wsl.exe" },
                        { label: "WSL — Ubuntu-24.04",   cmd: "wsl.exe -d Ubuntu-24.04" },
                        { label: "WSL — Ubuntu",         cmd: "wsl.exe -d Ubuntu" },
                        { label: "Git Bash",             cmd: "\"C:\\Program Files\\Git\\bin\\bash.exe\" --login -i" },
                        { label: qsTr("Custom..."),      cmd: null }
                    ]
                    textRole: "label"
                    // Pick the current index based on the saved settings
                    Component.onCompleted: {
                        if (!appSettings.useCustomCommand) {
                            currentIndex = 0
                            return
                        }
                        for (var i = 1; i < model.length - 1; i++) {
                            if (model[i].cmd === appSettings.customCommand) {
                                currentIndex = i
                                return
                            }
                        }
                        currentIndex = model.length - 1  // "Custom..."
                    }
                    onActivated: function(index) {
                        var item = model[index]
                        if (index === 0) {
                            // Default — use auto-detected shell
                            appSettings.useCustomCommand = false
                            useCustomCommand.checked = false
                        } else if (item.cmd !== null) {
                            // Preset
                            appSettings.customCommand = item.cmd
                            customCommand.text = item.cmd
                            appSettings.useCustomCommand = true
                            useCustomCommand.checked = true
                        } else {
                            // Custom... just enable the field
                            appSettings.useCustomCommand = true
                            useCustomCommand.checked = true
                        }
                    }
                }
            }

            CheckBox {
                id: useCustomCommand
                text: qsTr("Use custom command instead of shell at startup")
                checked: appSettings.useCustomCommand
                onCheckedChanged: appSettings.useCustomCommand = checked
            }
            TextField {
                id: customCommand
                Layout.fillWidth: true
                text: appSettings.customCommand
                enabled: useCustomCommand.checked
                onEditingFinished: appSettings.customCommand = text

                // Save text even if user forgets to press enter or unfocus
                function saveSetting() {
                    appSettings.customCommand = text
                }
                Component.onCompleted: settings_window.closing.connect(
                                           saveSetting)
            }
            CheckBox {
                id: blinkingCursor
                text: qsTr("Blinking Cursor")
                checked: appSettings.blinkingCursor
                onCheckedChanged: appSettings.blinkingCursor = checked
            }
            CheckBox {
                id: showMenubar
                text: qsTr("Show Menubar")
                enabled: !appSettings.isMacOS
                checked: appSettings.showMenubar
                onCheckedChanged: appSettings.showMenubar = checked
            }
        }
    }

    GroupBox {
        title: qsTr("Performance")
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: appSettings.defaultMargin
        GridLayout {
            anchors.fill: parent
            columns: 4

            Label {
                text: qsTr("Effects FPS")
            }
            Slider {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                id: effectsFpsSlider
                onValueChanged: appSettings.effectsFrameSkip = Math.round(value)
                stepSize: 1
                enabled: true
                from: 5
                to: 1
                value: appSettings.effectsFrameSkip
            }

            SizedLabel {
                text: Math.round(100 / Math.max(1, Math.round(effectsFpsSlider.value))) + "%"
            }
            Label {
                text: qsTr("Texture Quality")
            }
            Slider {
                id: txtslider
                Layout.fillWidth: true
                Layout.columnSpan: 2
                onValueChanged: appSettings.windowScaling = value
                stepSize: 0.05
                enabled: true
                from: 0.25
                value: appSettings.windowScaling
            }
            SizedLabel {
                text: Math.round(txtslider.value * 100) + "%"
            }

            Label {
                text: qsTr("Bloom Quality")
            }
            Slider {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                id: bloomSlider
                onValueChanged: appSettings.bloomQuality = value
                stepSize: 0.05
                enabled: true
                from: 0.25
                value: appSettings.bloomQuality
            }
            SizedLabel {
                text: Math.round(bloomSlider.value * 100) + "%"
            }

            Label {
                text: qsTr("BurnIn Quality")
            }
            Slider {
                Layout.fillWidth: true
                id: burnInSlider
                Layout.columnSpan: 2
                onValueChanged: appSettings.burnInQuality = value
                stepSize: 0.05
                enabled: true
                from: 0.25
                value: appSettings.burnInQuality
            }
            SizedLabel {
                text: Math.round(burnInSlider.value * 100) + "%"
            }
        }
    }
}
