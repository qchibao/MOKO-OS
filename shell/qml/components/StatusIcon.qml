import QtQuick

Item {
    id: root
    property string kind: "wifi"
    property bool active: true
    property int level: 100
    property bool charging: false
    property color iconColor: active ? "#2D3948" : "#98A6B5"

    Canvas {
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            const w = width
            const h = height
            const cx = w / 2
            const cy = h / 2
            ctx.resetTransform()
            ctx.clearRect(0, 0, w, h)
            ctx.strokeStyle = root.iconColor
            ctx.fillStyle = root.iconColor
            ctx.lineWidth = Math.max(1.4, w * .09)
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.kind === "wifi") {
                for (let ring = 0; ring < 3; ++ring) {
                    const radius = w * (.13 + ring * .14)
                    ctx.beginPath()
                    ctx.arc(cx, h * .74, radius, Math.PI * 1.18, Math.PI * 1.82)
                    ctx.stroke()
                }
                ctx.beginPath()
                ctx.arc(cx, h * .73, w * .055, 0, Math.PI * 2)
                ctx.fill()
            } else if (root.kind === "bluetooth") {
                ctx.beginPath()
                ctx.moveTo(cx, h * .12)
                ctx.lineTo(cx, h * .88)
                ctx.moveTo(cx, h * .12)
                ctx.lineTo(w * .74, h * .35)
                ctx.lineTo(w * .28, h * .67)
                ctx.lineTo(cx, h * .88)
                ctx.lineTo(w * .74, h * .65)
                ctx.lineTo(w * .28, h * .33)
                ctx.stroke()
            } else if (root.kind === "sound") {
                ctx.beginPath()
                ctx.moveTo(w * .15, h * .42)
                ctx.lineTo(w * .35, h * .42)
                ctx.lineTo(w * .56, h * .23)
                ctx.lineTo(w * .56, h * .77)
                ctx.lineTo(w * .35, h * .58)
                ctx.lineTo(w * .15, h * .58)
                ctx.closePath()
                ctx.fill()
                if (root.active && root.level > 0) {
                    ctx.beginPath()
                    ctx.arc(w * .54, cy, w * .27, -Math.PI / 3, Math.PI / 3)
                    ctx.stroke()
                } else {
                    ctx.beginPath()
                    ctx.moveTo(w * .68, h * .36)
                    ctx.lineTo(w * .86, h * .64)
                    ctx.moveTo(w * .86, h * .36)
                    ctx.lineTo(w * .68, h * .64)
                    ctx.stroke()
                }
            } else if (root.kind === "battery") {
                ctx.strokeRect(w * .10, h * .27, w * .72, h * .48)
                ctx.fillRect(w * .84, h * .40, w * .08, h * .22)
                const fillWidth = w * .62 * Math.max(0, Math.min(100, root.level)) / 100
                ctx.fillRect(w * .15, h * .34, fillWidth, h * .34)
                if (root.charging) {
                    ctx.fillStyle = "#2F78EA"
                    ctx.beginPath()
                    ctx.moveTo(w * .52, h * .12)
                    ctx.lineTo(w * .40, h * .52)
                    ctx.lineTo(w * .53, h * .52)
                    ctx.lineTo(w * .45, h * .90)
                    ctx.lineTo(w * .68, h * .43)
                    ctx.lineTo(w * .54, h * .43)
                    ctx.closePath()
                    ctx.fill()
                }
            } else if (root.kind === "brightness") {
                ctx.beginPath()
                ctx.arc(cx, cy, w * .18, 0, Math.PI * 2)
                ctx.fill()
                for (let index = 0; index < 8; ++index) {
                    const angle = index * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(angle) * w * .29, cy + Math.sin(angle) * w * .29)
                    ctx.lineTo(cx + Math.cos(angle) * w * .43, cy + Math.sin(angle) * w * .43)
                    ctx.stroke()
                }
            } else if (root.kind === "notification") {
                ctx.beginPath()
                ctx.moveTo(w * .28, h * .68)
                ctx.quadraticCurveTo(w * .35, h * .57, w * .35, h * .40)
                ctx.quadraticCurveTo(w * .35, h * .20, cx, h * .20)
                ctx.quadraticCurveTo(w * .65, h * .20, w * .65, h * .40)
                ctx.quadraticCurveTo(w * .65, h * .57, w * .72, h * .68)
                ctx.moveTo(w * .23, h * .70)
                ctx.lineTo(w * .77, h * .70)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(cx, h * .78, w * .06, 0, Math.PI * 2)
                ctx.fill()
            } else if (root.kind === "screenshot") {
                ctx.strokeRect(w * .17, h * .29, w * .66, h * .48)
                ctx.beginPath()
                ctx.moveTo(w * .32, h * .29)
                ctx.lineTo(w * .39, h * .20)
                ctx.lineTo(w * .59, h * .20)
                ctx.lineTo(w * .67, h * .29)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(cx, h * .53, w * .14, 0, Math.PI * 2)
                ctx.stroke()
            }
        }

        Connections {
            target: root
            function onKindChanged() { parent.requestPaint() }
            function onActiveChanged() { parent.requestPaint() }
            function onLevelChanged() { parent.requestPaint() }
            function onChargingChanged() { parent.requestPaint() }
            function onIconColorChanged() { parent.requestPaint() }
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
}
