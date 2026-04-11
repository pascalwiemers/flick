import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtCore

ApplicationWindow {
    id: root
    visible: true
    width: 520
    height: 400
    color: "#1a1a1a"
    flags: Qt.FramelessWindowHint | Qt.Window
    title: "Flick"

    Settings {
        id: windowSettings
        property alias x: root.x
        property alias y: root.y
        property alias width: root.width
        property alias height: root.height
    }

    // --- Font helper ---
    readonly property string monoFont: {
        var candidates = ["JetBrains Mono", "Fira Code", "Cascadia Code"]
        for (var i = 0; i < candidates.length; i++) {
            var test = Qt.font({family: candidates[i], pixelSize: 14})
            if (test.family === candidates[i])
                return candidates[i]
        }
        return "monospace"
    }

    // --- Drag to move ---
    MouseArea {
        id: dragArea
        anchors.fill: parent
        z: 0
        property point dragStart
        property point windowStart

        onPressed: function(mouse) {
            dragStart = Qt.point(mouse.x, mouse.y)
            windowStart = Qt.point(root.x, root.y)
        }
        onPositionChanged: function(mouse) {
            root.x = windowStart.x + (mouse.x - dragStart.x)
            root.y = windowStart.y + (mouse.y - dragStart.y)
        }
    }

    // --- Resize handles ---
    component ResizeHandle: MouseArea {
        property int edges: 0
        cursorShape: {
            if (edges === (Qt.LeftEdge | Qt.TopEdge) || edges === (Qt.RightEdge | Qt.BottomEdge))
                return Qt.SizeFDiagCursor
            if (edges === (Qt.RightEdge | Qt.TopEdge) || edges === (Qt.LeftEdge | Qt.BottomEdge))
                return Qt.SizeBDiagCursor
            if (edges & (Qt.LeftEdge | Qt.RightEdge))
                return Qt.SizeHorCursor
            return Qt.SizeVerCursor
        }
        onPressed: root.startSystemResize(edges)
        z: 3
    }
    ResizeHandle { edges: Qt.LeftEdge; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6 }
    ResizeHandle { edges: Qt.RightEdge; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6 }
    ResizeHandle { edges: Qt.TopEdge; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 6 }
    ResizeHandle { edges: Qt.BottomEdge; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 6 }
    ResizeHandle { edges: Qt.LeftEdge | Qt.TopEdge; anchors.left: parent.left; anchors.top: parent.top; width: 12; height: 12 }
    ResizeHandle { edges: Qt.RightEdge | Qt.TopEdge; anchors.right: parent.right; anchors.top: parent.top; width: 12; height: 12 }
    ResizeHandle { edges: Qt.LeftEdge | Qt.BottomEdge; anchors.left: parent.left; anchors.bottom: parent.bottom; width: 12; height: 12 }
    ResizeHandle { edges: Qt.RightEdge | Qt.BottomEdge; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 12; height: 12 }

    // --- Two-panel swipe: old text exits, new text enters simultaneously ---

    property bool _syncing: false
    property bool _animating: false

    // Clip container so panels sliding off-screen are hidden
    Item {
        id: viewport
        anchors.fill: parent
        z: 1
        clip: true

        // Active panel — the one the user types in
        Item {
            id: panelA
            width: parent.width
            height: parent.height

            Flickable {
                id: flickableA
                anchors.fill: parent
                contentWidth: width
                contentHeight: textArea.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

                TextArea {
                    id: textArea
                    width: flickableA.width
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    color: "#e0e0e0"
                    selectionColor: "#404040"
                    selectedTextColor: "#ffffff"
                    padding: 32
                    font.family: root.monoFont
                    font.pixelSize: 14
                    background: Rectangle { color: "transparent" }
                    focus: true

                    Component.onCompleted: {
                        text = noteStore.currentText
                        forceActiveFocus()
                    }

                    onTextChanged: {
                        if (!root._syncing)
                            noteStore.currentText = text
                    }
                }
            }
        }

        // Ghost panel — shows incoming note text during transition
        Item {
            id: panelB
            width: parent.width
            height: parent.height
            visible: false

            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: ghostText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: ghostText
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#e0e0e0"
                    padding: 32
                    font.family: root.monoFont
                    font.pixelSize: 14
                }
            }
        }
    }

    // Both panels slide together in parallel
    ParallelAnimation {
        id: swipeAnim
        property bool direction: true  // true = slide left (next), false = slide right (prev)

        NumberAnimation {
            target: panelA
            property: "x"
            from: 0
            to: swipeAnim.direction ? -root.width : root.width
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: panelB
            property: "x"
            from: swipeAnim.direction ? root.width : -root.width
            to: 0
            duration: 200
            easing.type: Easing.OutCubic
        }

        onStarted: panelB.visible = true
        onFinished: {
            // Swap: put new text into the real TextArea, hide ghost
            root._syncing = true
            noteStore.currentIndex = root._pendingIndex
            textArea.text = noteStore.currentText
            root._syncing = false
            panelA.x = 0
            panelB.visible = false
            textArea.forceActiveFocus()
            root._animating = false
        }
    }

    property int _pendingIndex: -1

    function navigateTo(newIndex) {
        if (root._animating)
            return
        if (noteStore.noteCount <= 1)
            return

        // Wrap around
        var direction
        if (newIndex < 0) {
            newIndex = noteStore.noteCount - 1
            direction = false
        } else if (newIndex >= noteStore.noteCount) {
            newIndex = 0
            direction = true
        } else {
            direction = newIndex > noteStore.currentIndex
        }

        if (newIndex === noteStore.currentIndex)
            return

        root._animating = true
        root._pendingIndex = newIndex

        // Load incoming text into ghost panel
        ghostText.text = noteStore.getText(newIndex)

        swipeAnim.direction = direction
        swipeAnim.start()
    }

    // Sync text from backend when it changes externally (e.g., after delete)
    Connections {
        target: noteStore
        function onCurrentTextChanged() {
            if (!root._syncing && textArea.text !== noteStore.currentText) {
                root._syncing = true
                textArea.text = noteStore.currentText
                root._syncing = false
            }
        }
        function onNoteCountChanged() {
            root._syncing = true
            textArea.text = noteStore.currentText
            root._syncing = false
            textArea.forceActiveFocus()
        }
    }

    // --- Grid overview (Shift+Tab) ---
    property bool gridVisible: false
    property int _gridRevision: 0  // bump to force delegate refresh

    Rectangle {
        id: gridOverlay
        anchors.fill: parent
        z: 10
        color: "#1a1a1a"
        visible: gridVisible
        opacity: gridVisible ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 150 } }

        GridView {
            id: gridView
            anchors.fill: parent
            anchors.margins: 16
            cellWidth: Math.max(160, (width - 16) / Math.min(3, noteStore.noteCount))
            cellHeight: cellWidth * 0.75
            model: noteStore.noteCount
            clip: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

            delegate: Item {
                width: gridView.cellWidth
                height: gridView.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    color: index === noteStore.currentIndex ? "#2a2a2a" : "#222222"
                    border.color: index === noteStore.currentIndex ? "#444444" : "#2f2f2f"
                    border.width: 1
                    radius: 4
                    clip: true

                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        readonly property string noteText: root._gridRevision, noteStore.getText(index)
                        text: noteText || "(empty)"
                        color: noteText ? "#e0e0e0" : "#555555"
                        font.family: root.monoFont
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        maximumLineCount: Math.max(1, Math.floor((parent.height - 24) / 16))
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root._syncing = true
                            noteStore.currentIndex = index
                            textArea.text = noteStore.currentText
                            root._syncing = false
                            textArea.forceActiveFocus()
                            root.gridVisible = false
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Shift+Tab"
        onActivated: {
            root.gridVisible = !root.gridVisible
            if (root.gridVisible)
                root._gridRevision++
            else
                textArea.forceActiveFocus()
        }
    }

    // --- Ctrl+Wheel to navigate notes ---
    MouseArea {
        anchors.fill: parent
        z: 2
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true

        onWheel: function(wheel) {
            if (!(wheel.modifiers & Qt.ControlModifier)) {
                wheel.accepted = false
                return
            }
            if (wheel.angleDelta.y > 0)
                navigateTo(noteStore.currentIndex - 1)
            else if (wheel.angleDelta.y < 0)
                navigateTo(noteStore.currentIndex + 1)
        }
    }

    // --- Keyboard shortcuts ---
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            if (root._animating) return
            root._animating = true
            // Load empty ghost for the new note
            ghostText.text = ""
            noteStore.createNote()
            // createNote sets index to 0 — text is already synced
            // Animate: new note slides in from the left
            swipeAnim.direction = false
            root._pendingIndex = 0
            swipeAnim.start()
        }
    }
    Shortcut {
        sequence: "Ctrl+W"
        onActivated: noteStore.deleteNote(noteStore.currentIndex)
    }
    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: navigateTo(noteStore.currentIndex - 1)
    }
    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: navigateTo(noteStore.currentIndex + 1)
    }
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
}
