import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtCore

ApplicationWindow {
    id: root
    visible: true
    width: 520
    height: 400
    color: bgColor
    flags: Qt.FramelessWindowHint | Qt.Window
    title: "Flick"

    onActiveChanged: if (!active) noteStore.commitHistory()

    property int fontSize: 14
    property bool markdownPreview: false
    property bool darkMode: true

    // --- Theme colors ---
    readonly property color bgColor:        darkMode ? "#1a1a1a" : "#f5f5f5"
    readonly property color textColor:      darkMode ? "#e0e0e0" : "#1a1a1a"
    readonly property color dimTextColor:   darkMode ? "#555555" : "#aaaaaa"
    readonly property color selectionColor: darkMode ? "#404040" : "#b0d0ff"
    readonly property color borderColor:    darkMode ? "#333333" : "#d0d0d0"
    readonly property color surfaceColor:   darkMode ? "#222222" : "#e8e8e8"
    readonly property color hoverColor:     darkMode ? "#333333" : "#d8d8d8"
    readonly property color activeSurface:  darkMode ? "#2a2a2a" : "#e0e0e0"
    readonly property color activeBorder:   darkMode ? "#444444" : "#bbbbbb"
    readonly property color inactiveBorder: darkMode ? "#2f2f2f" : "#d5d5d5"
    readonly property color accentColor:    "#4a9eff"

    FontMetrics {
        id: editorFontMetrics
        font.family: root.monoFont
        font.pixelSize: root.fontSize
    }

    Settings {
        id: windowSettings
        property alias x: root.x
        property alias y: root.y
        property alias width: root.width
        property alias height: root.height
        property alias fontSize: root.fontSize
        property alias darkMode: root.darkMode
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

    // --- Drag to move (invisible strip at top) ---
    MouseArea {
        id: dragArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28
        z: 9  // above content but below grid overlay and auth overlay

        onPressed: windowDragger.startDrag(root)
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
                    color: root.textColor
                    selectionColor: root.selectionColor
                    selectedTextColor: darkMode ? "#ffffff" : "#000000"
                    padding: 32
                    leftPadding: Math.max(32, Math.round(32 * root.fontSize / 14))
                    font.family: root.monoFont
                    font.pixelSize: root.fontSize
                    background: Rectangle { color: "transparent" }
                    focus: true

                    Component.onCompleted: {
                        text = noteStore.currentText
                        forceActiveFocus()
                        syntaxHighlighter.document = textArea.textDocument
                    }

                    // Intercept Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y directly here so
                    // TextArea's built-in single-document undo never runs —
                    // we route to core per-note history instead.
                    Keys.onPressed: (event) => {
                        if (event.modifiers & Qt.ControlModifier) {
                            if (event.key === Qt.Key_Z && !(event.modifiers & Qt.ShiftModifier)) {
                                event.accepted = true
                                noteStore.commitHistory()
                                if (noteStore.undo()) {
                                    root._syncing = true
                                    textArea.text = noteStore.currentText
                                    root._syncing = false
                                }
                                return
                            }
                            if ((event.key === Qt.Key_Z && (event.modifiers & Qt.ShiftModifier))
                                    || event.key === Qt.Key_Y) {
                                event.accepted = true
                                if (noteStore.redo()) {
                                    root._syncing = true
                                    textArea.text = noteStore.currentText
                                    root._syncing = false
                                }
                                return
                            }
                        }
                    }

                    onTextChanged: {
                        if (!root._syncing)
                            noteStore.currentText = text
                        var hasMath = text.indexOf("math:") >= 0
                        syntaxHighlighter.mathMode = hasMath
                        mathDebounce.restart()
                        listDebounce.restart()
                        statsDebounce.restart()
                    }

                    // Helper: get character position of start of line N
                    function lineStartPos(lineIndex) {
                        var t = text
                        var pos = 0
                        for (var i = 0; i < lineIndex; i++) {
                            var nl = t.indexOf('\n', pos)
                            if (nl < 0) return t.length
                            pos = nl + 1
                        }
                        return pos
                    }

                    // Helper: get character position of end of line N (before \n)
                    function lineEndPos(lineIndex) {
                        var t = text
                        var pos = 0
                        for (var i = 0; i < lineIndex; i++) {
                            var nl = t.indexOf('\n', pos)
                            if (nl < 0) return t.length
                            pos = nl + 1
                        }
                        var nl = t.indexOf('\n', pos)
                        return nl < 0 ? t.length : nl
                    }

                    // Helper: stable visual rect for line N.
                    function visualLineRect(lineIndex) {
                        var startPos = lineStartPos(lineIndex)
                        var startRect = positionToRectangle(startPos)
                        var nextStartPos = lineStartPos(lineIndex + 1)
                        var nextRect = positionToRectangle(nextStartPos)
                        var h = nextStartPos > startPos ? (nextRect.y - startRect.y) : startRect.height
                        if (h <= 0)
                            h = startRect.height
                        return Qt.rect(startRect.x, startRect.y, startRect.width, h)
                    }
                }

                // List checkbox overlay in same Flickable content layer as text.
                // This keeps text + checkboxes on same scroll/zoom geometry.
                Item {
                    visible: !root.markdownPreview && listEngine.active
                    width: textArea.width
                    height: flickableA.contentHeight
                    z: 2

                    Repeater {
                        model: listEngine.items

                        Item {
                            visible: modelData.type === "item"
                            property int lineIdx: modelData.line
                            property rect startRect: {
                                var _f = root.fontSize
                                var _p = textArea.leftPadding
                                var _h = textArea.contentHeight
                                var _w = textArea.contentWidth
                                return textArea.positionToRectangle(textArea.lineStartPos(lineIdx))
                            }
                            property rect lineRect: {
                                var _f = root.fontSize
                                var _p = textArea.leftPadding
                                var _h = textArea.contentHeight
                                var _w = textArea.contentWidth
                                return textArea.visualLineRect(lineIdx)
                            }
                            property bool isChecked: modelData.checked === true

                            Rectangle {
                                id: checkBox
                                property real boxSize: Math.round(parent.startRect.height * 0.72)
                                property real pad: Math.max(6, boxSize * 0.35)
                                x: Math.max(4, Math.round(textArea.leftPadding - boxSize - pad))
                                y: Math.round(parent.startRect.y + (parent.startRect.height - boxSize) / 2)
                                width: boxSize
                                height: boxSize
                                radius: 3
                                color: parent.isChecked ? root.accentColor : "transparent"
                                border.color: parent.isChecked ? root.accentColor : root.dimTextColor
                                border.width: 1.5

                                Text {
                                    visible: parent.parent.isChecked
                                    anchors.centerIn: parent
                                    text: "\u2713"
                                    color: "#ffffff"
                                    font.pixelSize: Math.round(checkBox.boxSize * 0.72)
                                    font.bold: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -Math.max(4, checkBox.boxSize * 0.25)
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var newText = listEngine.toggleCheck(textArea.text, modelData.line)
                                        if (newText !== textArea.text) {
                                            var pos = textArea.cursorPosition
                                            textArea.text = newText
                                            textArea.cursorPosition = Math.min(pos, textArea.text.length)
                                        }
                                    }
                                    z: 100
                                }
                            }

                            Rectangle {
                                visible: parent.isChecked
                                property rect endRect: textArea.positionToRectangle(textArea.lineEndPos(parent.lineIdx))
                                property real strikeStartX: textArea.leftPadding
                                x: strikeStartX
                                y: Math.round(parent.startRect.y + parent.startRect.height / 2)
                                width: Math.max(0, endRect.x - strikeStartX)
                                height: 1
                                color: root.dimTextColor
                                opacity: 0.6
                            }
                        }
                    }
                }
            }

            // --- Math result overlays (outside Flickable, clipped by panelA) ---
            Item {
                visible: !root.markdownPreview
                anchors.fill: parent
                clip: true

                Repeater {
                    model: mathEngine.results

                    Item {
                        visible: !modelData.isComment
                        property int lineIdx: modelData.line
                        property rect lineRect: {
                            var _f = root.fontSize
                            var _p = textArea.leftPadding
                            return textArea.positionToRectangle(textArea.lineEndPos(lineIdx))
                        }
                        property real scrollY: lineRect.y - flickableA.contentY

                        // Separator line for running totals
                        Rectangle {
                            visible: modelData.isSeparator
                            width: 80
                            height: 1
                            color: root.borderColor
                            x: panelA.width - width - 32
                            y: parent.scrollY + parent.lineRect.height + 2
                        }

                        // Result text — inline after the line content
                        Text {
                            visible: modelData.text !== "" && !modelData.isSeparator
                            text: " " + modelData.text
                            color: modelData.color
                            font.family: root.monoFont
                            font.pixelSize: root.fontSize
                            font.italic: modelData.isTotal

                            x: {
                                if (modelData.isTotal)
                                    return panelA.width - width - 32
                                // Position right after the last character of the line
                                return parent.lineRect.x - flickableA.contentX
                            }
                            y: {
                                if (modelData.isTotal)
                                    return parent.scrollY + parent.lineRect.height + 6
                                return parent.scrollY
                            }
                        }
                    }
                }

                // Comment dimming overlays
                Repeater {
                    model: mathEngine.results

                    Rectangle {
                        visible: modelData.isComment
                        property int lineIdx: modelData.line
                        property rect startRect: {
                            var _f = root.fontSize
                            var _p = textArea.leftPadding
                            return textArea.positionToRectangle(textArea.lineStartPos(lineIdx))
                        }
                        property rect endRect: {
                            var _f = root.fontSize
                            var _p = textArea.leftPadding
                            return textArea.positionToRectangle(textArea.lineEndPos(lineIdx))
                        }

                        x: 0
                        y: startRect.y - flickableA.contentY
                        width: panelA.width
                        height: Math.max(endRect.y + endRect.height - startRect.y, startRect.height)
                        color: root.bgColor
                        opacity: 0.6
                    }
                }
            }
        }

        // List checkbox overlay moved into Flickable content layer above.

        // --- Stats overlay (below text content) ---
        Item {
            visible: !root.markdownPreview && statsEngine.active
            anchors.fill: parent
            clip: true

            Column {
                property int lastLine: {
                    var lines = textArea.text.split('\n')
                    return lines.length - 1
                }
                property rect lastLineRect: {
                    var _f = root.fontSize
                    var _p = textArea.leftPadding
                    return textArea.positionToRectangle(textArea.lineEndPos(lastLine))
                }

                x: 24
                y: lastLineRect.y + lastLineRect.height + 24 - flickableA.contentY
                spacing: 2

                Repeater {
                    model: [
                        { label: "Items:", value: statsEngine.items },
                        { label: "Words:", value: statsEngine.words },
                        { label: "Characters:", value: statsEngine.characters },
                        { label: "Sentences:", value: statsEngine.sentences },
                        { label: "Flesch Reading Ease Score:", value: statsEngine.fleschReadingEase.toFixed(2) },
                        { label: "Flesch-Kincaid Grade Level:", value: statsEngine.fleschKincaidGrade.toFixed(2) }
                    ]

                    Row {
                        spacing: 8
                        Text {
                            text: modelData.label
                            color: root.dimTextColor
                            font.family: root.monoFont
                            font.pixelSize: 11
                        }
                        Text {
                            text: String(modelData.value)
                            color: root.textColor
                            font.family: root.monoFont
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // --- Markdown preview overlay ---
        Item {
            id: previewPanel
            width: parent.width
            height: parent.height
            visible: root.markdownPreview
            z: 2

            Rectangle {
                anchors.fill: parent
                color: root.bgColor
            }

            Flickable {
                id: flickablePreview
                anchors.fill: parent
                contentWidth: width
                contentHeight: previewText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

                TextArea {
                    id: previewText
                    width: flickablePreview.width
                    textFormat: TextEdit.MarkdownText
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    color: root.textColor
                    selectionColor: root.selectionColor
                    selectedTextColor: darkMode ? "#ffffff" : "#000000"
                    padding: 32
                    font.family: root.monoFont
                    font.pixelSize: root.fontSize
                    background: Rectangle { color: "transparent" }

                    // Blockquote background + left bar overlays
                    Repeater {
                        model: markdownStyler.regions

                        Item {
                            // Left accent bar
                            Rectangle {
                                x: previewText.padding + (modelData.level - 1) * 16
                                y: modelData.y + previewText.padding
                                width: 3
                                height: modelData.height
                                color: root.accentColor
                                radius: 1
                            }

                            // Background tint
                            Rectangle {
                                x: previewText.padding + (modelData.level - 1) * 16 + 6
                                y: modelData.y + previewText.padding
                                width: previewText.width - previewText.padding * 2 - (modelData.level - 1) * 16 - 6
                                height: modelData.height
                                color: root.accentColor
                                opacity: 0.06
                                radius: 2
                            }
                        }
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
                    color: root.textColor
                    padding: 32
                    font.family: root.monoFont
                    font.pixelSize: root.fontSize
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
            mathDebounce.restart()
            listDebounce.restart()
            statsDebounce.restart()
        }
    }

    property int _pendingIndex: -1

    function navigateTo(newIndex) {
        if (root._animating)
            return
        if (root.markdownPreview)
            root.markdownPreview = false
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

    Connections {
        target: mathEngine
        function onVariableNamesChanged() {
            syntaxHighlighter.variableNames = mathEngine.variableNames
        }
    }

    // --- Math debounce timer ---
    Timer {
        id: mathDebounce
        interval: 50
        onTriggered: {
            var t = textArea.text
            if (t.indexOf("math:") >= 0 || t.indexOf("total:") === 0 || t.indexOf("Total:") === 0 || t.indexOf("avg:") === 0 || t.indexOf("Avg:") === 0)
                mathEngine.evaluate(t)
            else
                mathEngine.evaluate("")
        }
    }

    // --- List debounce timer ---
    Timer {
        id: listDebounce
        interval: 50
        onTriggered: listEngine.evaluate(textArea.text)
    }

    // --- Stats debounce timer ---
    Timer {
        id: statsDebounce
        interval: 50
        onTriggered: statsEngine.evaluate(textArea.text)
    }

    // Sync text from backend when it changes externally (e.g., after delete or AutoPaste)
    Connections {
        target: noteStore
        function onCurrentTextChanged() {
            if (!root._syncing && textArea.text !== noteStore.currentText) {
                var pos = textArea.cursorPosition
                root._syncing = true
                textArea.text = noteStore.currentText
                root._syncing = false
                textArea.cursorPosition = Math.min(pos, textArea.text.length)
                mathDebounce.restart()
                listDebounce.restart()
                statsDebounce.restart()
            }
        }
        function onNoteCountChanged() {
            root._syncing = true
            textArea.text = noteStore.currentText
            root._syncing = false
            textArea.forceActiveFocus()
            mathDebounce.restart()
            listDebounce.restart()
            statsDebounce.restart()
        }
    }

    // --- Grid overview (Shift+Tab) ---
    property bool gridVisible: false
    property int _gridRevision: 0  // bump to force delegate refresh

    Rectangle {
        id: gridOverlay
        anchors.fill: parent
        z: 10
        color: root.bgColor
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
                    color: index === noteStore.currentIndex ? root.activeSurface : root.surfaceColor
                    border.color: index === noteStore.currentIndex ? root.activeBorder : root.inactiveBorder
                    border.width: 1
                    radius: 4
                    clip: true

                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        readonly property string noteText: root._gridRevision, noteStore.getText(index)
                        text: noteText || "(empty)"
                        color: noteText ? root.textColor : root.dimTextColor
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
        onActivated: root.toggleGridOverview()
    }

    // --- Swipe / Ctrl+Wheel to navigate notes ---
    // On macOS, two-finger horizontal swipe on trackpad sends horizontal scroll.
    // On all platforms, Ctrl+vertical scroll also navigates.
    property real _swipeAccumulator: 0
    readonly property real _swipeThreshold: 200

    MouseArea {
        anchors.fill: parent
        z: 2
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true

        onWheel: function(wheel) {
            // Ctrl+vertical scroll — works everywhere
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y > 0)
                    navigateTo(noteStore.currentIndex - 1)
                else if (wheel.angleDelta.y < 0)
                    navigateTo(noteStore.currentIndex + 1)
                return
            }

            // Two-finger horizontal swipe (macOS trackpad)
            var dominated = Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y)
            if ((Qt.platform.os === "osx" || Qt.platform.os === "macos") && dominated) {
                // Swallow all horizontal scroll during animation + cooldown
                if (root._animating || swipeCooldownTimer.running)
                    return

                root._swipeAccumulator += wheel.angleDelta.x
                if (root._swipeAccumulator > root._swipeThreshold) {
                    root._swipeAccumulator = 0
                    swipeCooldownTimer.restart()
                    navigateTo(noteStore.currentIndex - 1)
                } else if (root._swipeAccumulator < -root._swipeThreshold) {
                    root._swipeAccumulator = 0
                    swipeCooldownTimer.restart()
                    navigateTo(noteStore.currentIndex + 1)
                }
                return
            }

            wheel.accepted = false
        }
    }

    // Cooldown: ignore trackpad inertia after a swipe triggers navigation
    Timer {
        id: swipeCooldownTimer
        interval: 800
        onTriggered: root._swipeAccumulator = 0
    }

    // --- Helper functions ---
    function createNewNote() {
        if (root._animating) return
        if (root.markdownPreview) root.markdownPreview = false
        root._animating = true
        ghostText.text = ""
        noteStore.createNote()
        swipeAnim.direction = false
        root._pendingIndex = 0
        swipeAnim.start()
    }

    function toggleMarkdownPreview() {
        if (root.gridVisible) return
        root.markdownPreview = !root.markdownPreview
        if (root.markdownPreview) {
            previewText.text = textArea.text
            markdownStyler.styleDocument(previewText.textDocument)
        } else {
            textArea.forceActiveFocus()
        }
    }

    function toggleGridOverview() {
        if (root.markdownPreview) root.markdownPreview = false
        root.gridVisible = !root.gridVisible
        if (root.gridVisible)
            root._gridRevision++
        else
            textArea.forceActiveFocus()
    }

    // --- Right-click context menu ---
    component StyledMenuItem: MenuItem {
        id: smi
        contentItem: Text {
            text: smi.text
            color: root.textColor
            font.family: root.monoFont
            font.pixelSize: 13
            leftPadding: smi.checkable ? 24 : 8
            rightPadding: 16
            verticalAlignment: Text.AlignVCenter
        }
        indicator: Rectangle {
            visible: smi.checkable
            x: 6
            y: (smi.height - height) / 2
            width: 12; height: 12; radius: 2
            color: "transparent"
            border.color: root.dimTextColor
            Rectangle {
                anchors.centerIn: parent
                width: 8; height: 8; radius: 1
                color: root.accentColor
                visible: smi.checked
            }
        }
        background: Rectangle {
            color: smi.highlighted ? root.hoverColor : "transparent"
        }
    }

    component StyledMenuSeparator: MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: root.borderColor
        }
    }

    Menu {
        id: contextMenu

        background: Rectangle {
            implicitWidth: 180
            color: root.surfaceColor
            border.color: root.borderColor
            border.width: 1
            radius: 4
        }

        StyledMenuItem {
            text: "New Note"
            onTriggered: root.createNewNote()
        }
        StyledMenuItem {
            text: "Delete Note"
            onTriggered: noteStore.deleteNote(noteStore.currentIndex)
        }
        StyledMenuItem {
            text: "Trash…"
            onTriggered: trashDialog.open()
        }
        StyledMenuItem {
            text: "Grid Overview"
            onTriggered: root.toggleGridOverview()
        }
        StyledMenuSeparator {}
        StyledMenuItem {
            text: "Markdown Preview"
            checkable: true
            checked: root.markdownPreview
            onTriggered: root.toggleMarkdownPreview()
        }
        StyledMenuItem {
            text: "AutoPaste"
            checkable: true
            checked: autoPaste.active
            onTriggered: autoPaste.active = !autoPaste.active
        }
        StyledMenuSeparator {}
        StyledMenuItem {
            text: "Increase Font Size"
            onTriggered: root.fontSize = Math.min(48, root.fontSize + 2)
        }
        StyledMenuItem {
            text: "Decrease Font Size"
            onTriggered: root.fontSize = Math.max(8, root.fontSize - 2)
        }
        StyledMenuSeparator {}
        StyledMenuItem {
            text: githubSync.authenticated ? "Sync Now" : "Connect GitHub"
            onTriggered: {
                if (githubSync.authenticated)
                    githubSync.sync()
                else
                    githubSync.startAuth()
            }
        }
        StyledMenuItem {
            visible: githubSync.authenticated
            height: visible ? implicitHeight : 0
            text: "Disconnect GitHub"
            onTriggered: githubSync.logout()
        }
        StyledMenuSeparator {}
        StyledMenuItem {
            text: "Dark Mode"
            checkable: true
            checked: root.darkMode
            onTriggered: root.darkMode = !root.darkMode
        }
        StyledMenuSeparator {}
        StyledMenuItem {
            text: "Quit"
            onTriggered: Qt.quit()
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 5
        acceptedButtons: Qt.RightButton
        onClicked: function(mouse) {
            contextMenu.popup()
        }
    }

    // --- Keyboard shortcuts ---
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: root.createNewNote()
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
    Shortcut {
        sequence: "Ctrl+="
        onActivated: root.fontSize = Math.min(48, root.fontSize + 2)
    }
    Shortcut {
        sequence: "Ctrl+-"
        onActivated: root.fontSize = Math.max(8, root.fontSize - 2)
    }
    Shortcut {
        sequence: "Ctrl+Shift+V"
        onActivated: autoPaste.active = !autoPaste.active
    }

    Shortcut {
        sequence: "Ctrl+M"
        onActivated: root.toggleMarkdownPreview()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: root.darkMode = !root.darkMode
    }
    Shortcut {
        sequence: "Ctrl+G"
        onActivated: {
            if (githubSync.authenticated)
                githubSync.sync()
            else
                githubSync.startAuth()
        }
    }
    Shortcut {
        sequence: "Ctrl+Alt+Z"
        onActivated: {
            var entries = noteStore.trashEntries()
            if (entries.length > 0)
                noteStore.restoreFromTrash(entries[0].id)
        }
    }

    Popup {
        id: trashDialog
        modal: true
        focus: true
        anchors.centerIn: parent
        width: Math.min(520, root.width - 80)
        height: Math.min(480, root.height - 80)
        padding: 0

        property var entries: []

        function refresh() {
            entries = noteStore.trashEntries()
        }

        Connections {
            target: noteStore
            function onTrashChanged() { trashDialog.refresh() }
        }

        onAboutToShow: refresh()

        background: Rectangle {
            color: root.surfaceColor
            border.color: root.borderColor
            border.width: 1
            radius: 4
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 36
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Trash (" + trashDialog.entries.length + ")"
                    color: root.textColor
                    font.family: root.monoFont
                    font.pixelSize: 13
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Empty Trash"
                    color: trashDialog.entries.length > 0 ? root.accentColor : root.dimTextColor
                    font.family: root.monoFont
                    font.pixelSize: 12
                    MouseArea {
                        anchors.fill: parent
                        enabled: trashDialog.entries.length > 0
                        cursorShape: Qt.PointingHandCursor
                        onClicked: noteStore.emptyTrash()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.borderColor
            }

            ListView {
                id: trashList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: trashDialog.entries
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 48
                    color: ma.containsMouse ? root.hoverColor : "transparent"

                    MouseArea {
                        id: ma
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: modelData.preview && modelData.preview.length > 0
                                  ? modelData.preview : "(empty)"
                            color: root.textColor
                            font.family: root.monoFont
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Text {
                            text: {
                                var age = Math.floor(Date.now() / 1000) - modelData.deletedAt
                                if (age < 60) return age + "s"
                                if (age < 3600) return Math.floor(age / 60) + "m"
                                if (age < 86400) return Math.floor(age / 3600) + "h"
                                return Math.floor(age / 86400) + "d"
                            }
                            color: root.dimTextColor
                            font.family: root.monoFont
                            font.pixelSize: 11
                        }

                        Text {
                            text: "Restore"
                            color: root.accentColor
                            font.family: root.monoFont
                            font.pixelSize: 12
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: noteStore.restoreFromTrash(modelData.id)
                            }
                        }

                        Text {
                            text: "Delete"
                            color: root.dimTextColor
                            font.family: root.monoFont
                            font.pixelSize: 12
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: noteStore.purgeFromTrash(modelData.id)
                            }
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: root.borderColor
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: trashDialog.entries.length === 0
                    text: "Trash empty"
                    color: root.dimTextColor
                    font.family: root.monoFont
                    font.pixelSize: 12
                }
            }
        }
    }

    // --- AutoPaste indicator dot ---
    Rectangle {
        visible: autoPaste.active
        width: 6; height: 6; radius: 3
        color: root.accentColor
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        z: 20
    }

    // --- List mode indicator dot ---
    Rectangle {
        visible: listEngine.active
        width: 6; height: 6; radius: 3
        color: "#ff9f4a"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: autoPaste.active ? 20 : 8
        anchors.topMargin: 8
        z: 20
    }

    // --- Markdown preview indicator dot ---
    Rectangle {
        visible: root.markdownPreview
        width: 6; height: 6; radius: 3
        color: "#4aff7f"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: {
            var offset = 8
            if (autoPaste.active) offset += 12
            if (listEngine.active) offset += 12
            return offset
        }
        anchors.topMargin: 8
        z: 20
    }

    // --- GitHub sync indicator dot ---
    Rectangle {
        visible: githubSync.authenticated
        width: 6; height: 6; radius: 3
        color: githubSync.status === "syncing" ? "#ffaa4a" : "#4aff7f"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: {
            var offset = 8
            if (autoPaste.active) offset += 12
            if (listEngine.active) offset += 12
            if (root.markdownPreview) offset += 12
            return offset
        }
        anchors.topMargin: 8
        z: 20
    }

    // --- GitHub sync debounce timer ---
    Timer {
        id: syncDebounce
        interval: 30000
        onTriggered: {
            if (githubSync.authenticated)
                githubSync.sync()
        }
    }

    Connections {
        target: noteStore
        function onCurrentTextChanged() {
            if (githubSync.authenticated)
                syncDebounce.restart()
        }
        function onNoteCountChanged() {
            if (githubSync.authenticated)
                syncDebounce.restart()
        }
    }

    // --- GitHub auth overlay ---
    Rectangle {
        id: authOverlay
        anchors.fill: parent
        z: 50
        visible: githubSync.status === "authenticating" && githubSync.userCode !== ""
        color: Qt.rgba(0, 0, 0, 0.8)

        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: 320
            height: 200
            radius: 12
            color: root.surfaceColor
            border.color: root.borderColor
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 16

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Enter this code on GitHub:"
                    color: root.dimTextColor
                    font.family: root.monoFont
                    font.pixelSize: 13
                }

                Rectangle {
                    id: codeBox
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: codeText.implicitWidth + 24
                    height: codeText.implicitHeight + 12
                    radius: 6
                    color: root.hoverColor
                    border.color: root.borderColor

                    property bool copied: false

                    Text {
                        id: codeText
                        anchors.centerIn: parent
                        text: githubSync.userCode
                        color: root.accentColor
                        font.family: root.monoFont
                        font.pixelSize: 32
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            githubSync.copyToClipboard(githubSync.userCode)
                            codeBox.copied = true
                            copiedResetTimer.restart()
                        }
                    }

                    Timer {
                        id: copiedResetTimer
                        interval: 2000
                        onTriggered: codeBox.copied = false
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: codeBox.copied ? "Copied!" : "Click to copy"
                    color: codeBox.copied ? root.accentColor : root.dimTextColor
                    font.family: root.monoFont
                    font.pixelSize: 11
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Waiting for authorization..."
                    color: root.dimTextColor
                    font.family: root.monoFont
                    font.pixelSize: 12
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 80
                    height: 28
                    radius: 4
                    color: root.hoverColor
                    border.color: root.borderColor

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: root.textColor
                        font.family: root.monoFont
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: githubSync.cancelAuth()
                    }
                }
            }
        }
    }
}
