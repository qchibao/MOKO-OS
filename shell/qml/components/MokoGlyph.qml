import QtQuick

Item {
    id: root
    property string kind: "system"
    property color primary: "#3F7CFF"
    property color secondary: "#7AC8FF"
    property color tertiary: "#765BFF"

    Canvas {
        id: canvas
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
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            function stroke(c, widthPx) { ctx.strokeStyle = c; ctx.lineWidth = widthPx }
            function fill(c) { ctx.fillStyle = c }
            function roundRect(x,y,rw,rh,r) {
                ctx.beginPath()
                ctx.moveTo(x+r,y); ctx.lineTo(x+rw-r,y); ctx.quadraticCurveTo(x+rw,y,x+rw,y+r)
                ctx.lineTo(x+rw,y+rh-r); ctx.quadraticCurveTo(x+rw,y+rh,x+rw-r,y+rh)
                ctx.lineTo(x+r,y+rh); ctx.quadraticCurveTo(x,y+rh,x,y+rh-r)
                ctx.lineTo(x,y+r); ctx.quadraticCurveTo(x,y,x+r,y); ctx.closePath()
            }

            if (kind === "files") {
                fill(primary); ctx.beginPath(); ctx.moveTo(w*.18,h*.28); ctx.lineTo(w*.55,h*.28); ctx.lineTo(w*.68,h*.42); ctx.lineTo(w*.82,h*.42); ctx.lineTo(w*.68,h*.75); ctx.lineTo(w*.31,h*.75); ctx.closePath(); ctx.fill()
                fill(secondary); ctx.beginPath(); ctx.moveTo(w*.23,h*.36); ctx.lineTo(w*.58,h*.36); ctx.lineTo(w*.72,h*.50); ctx.lineTo(w*.63,h*.61); ctx.lineTo(w*.28,h*.61); ctx.closePath(); ctx.fill()
            } else if (kind === "browser") {
                stroke(primary, w*.07); ctx.beginPath(); ctx.arc(cx,cy,w*.27,0,Math.PI*2); ctx.stroke()
                stroke(secondary,w*.035); ctx.beginPath(); ctx.moveTo(w*.23,h*.67); ctx.quadraticCurveTo(cx,h*.18,w*.77,h*.34); ctx.stroke()
                fill(tertiary); ctx.beginPath(); ctx.arc(cx+w*.16,cy-h*.13,w*.055,0,Math.PI*2); ctx.fill()
            } else if (kind === "ai" || kind === "music") {
                const bars = kind === "ai" ? [0.30,0.55,0.78,0.46,0.68,0.40,0.25] : [0.24,0.42,0.64,0.82,0.62,0.46,0.29]
                for (let i=0;i<bars.length;i++) {
                    const x = w*(.20+i*.10); const bh = h*bars[i]*.62
                    stroke(i%2===0 ? primary : (kind === "ai" ? tertiary : "#FF6E8E"), w*.055)
                    ctx.beginPath(); ctx.moveTo(x,cy-bh/2); ctx.lineTo(x,cy+bh/2); ctx.stroke()
                }
            } else if (kind === "mail") {
                fill(primary); roundRect(w*.17,h*.28,w*.66,h*.46,w*.07); ctx.fill()
                stroke("#EAF5FF",w*.045); ctx.beginPath(); ctx.moveTo(w*.21,h*.34); ctx.lineTo(cx,h*.57); ctx.lineTo(w*.79,h*.34); ctx.stroke()
            } else if (kind === "camera") {
                fill("#44546A"); roundRect(w*.18,h*.27,w*.64,h*.48,w*.10); ctx.fill()
                fill(primary); ctx.beginPath(); ctx.arc(cx,cy,w*.18,0,Math.PI*2); ctx.fill()
                fill("#EAF5FF"); ctx.beginPath(); ctx.arc(cx,cy,w*.095,0,Math.PI*2); ctx.fill()
                fill("#FF8A5B"); ctx.beginPath(); ctx.arc(w*.72,h*.35,w*.035,0,Math.PI*2); ctx.fill()
            } else if (kind === "terminal") {
                fill("#26354A"); roundRect(w*.13,h*.18,w*.74,h*.64,w*.13); ctx.fill()
                stroke("#FFFFFF",w*.055); ctx.beginPath(); ctx.moveTo(w*.28,h*.36); ctx.lineTo(w*.43,h*.50); ctx.lineTo(w*.28,h*.64); ctx.stroke()
                stroke(secondary,w*.045); ctx.beginPath(); ctx.moveTo(w*.49,h*.65); ctx.lineTo(w*.68,h*.65); ctx.stroke()
            } else if (kind === "settings") {
                stroke("#53637A",w*.085); ctx.beginPath(); ctx.arc(cx,cy,w*.25,0,Math.PI*2); ctx.stroke()
                stroke(primary,w*.055); ctx.beginPath(); ctx.arc(cx,cy,w*.11,0,Math.PI*2); ctx.stroke()
                for (let i=0;i<6;i++) { const a=i*Math.PI/3; stroke("#53637A",w*.055); ctx.beginPath(); ctx.moveTo(cx+Math.cos(a)*w*.28,cy+Math.sin(a)*w*.28); ctx.lineTo(cx+Math.cos(a)*w*.37,cy+Math.sin(a)*w*.37); ctx.stroke() }
            } else if (kind === "diagnostics") {
                stroke("#52647B",w*.045); roundRect(w*.19,h*.20,w*.62,h*.60,w*.08); ctx.stroke()
                stroke(primary,w*.055); ctx.beginPath(); ctx.moveTo(w*.28,h*.54); ctx.lineTo(w*.39,h*.54); ctx.lineTo(w*.46,h*.37); ctx.lineTo(w*.55,h*.66); ctx.lineTo(w*.63,h*.47); ctx.lineTo(w*.73,h*.47); ctx.stroke()
                fill(secondary); ctx.beginPath(); ctx.arc(w*.29,h*.32,w*.045,0,Math.PI*2); ctx.fill()
            } else if (kind === "store") {
                const g = ctx.createLinearGradient(0,h*.2,w,h*.8); g.addColorStop(0,"#40E0D0"); g.addColorStop(1,"#4B75FF"); fill(g); roundRect(w*.20,h*.30,w*.60,h*.48,w*.09); ctx.fill()
                stroke("#FFFFFF",w*.045); ctx.beginPath(); ctx.arc(cx,h*.34,w*.17,Math.PI,0); ctx.stroke()
            } else if (kind === "security") {
                const g = ctx.createLinearGradient(0,0,w,h); g.addColorStop(0,secondary); g.addColorStop(1,tertiary); fill(g)
                ctx.beginPath(); ctx.moveTo(cx,h*.16); ctx.lineTo(w*.76,h*.28); ctx.lineTo(w*.70,h*.62); ctx.quadraticCurveTo(cx,h*.82,w*.30,h*.62); ctx.lineTo(w*.24,h*.28); ctx.closePath(); ctx.fill()
                stroke("#FFFFFF",w*.045); ctx.beginPath(); ctx.moveTo(cx,h*.34); ctx.lineTo(cx,h*.58); ctx.stroke()
            } else if (kind === "cloud") {
                fill(primary); ctx.beginPath(); ctx.arc(w*.40,h*.55,w*.17,Math.PI,0); ctx.arc(w*.56,h*.48,w*.21,Math.PI,0); ctx.arc(w*.68,h*.58,w*.13,Math.PI,0); ctx.lineTo(w*.27,h*.68); ctx.closePath(); ctx.fill()
            } else if (kind === "trash") {
                stroke("#7A8AA0",w*.05); roundRect(w*.31,h*.31,w*.38,h*.44,w*.04); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w*.27,h*.28); ctx.lineTo(w*.73,h*.28); ctx.stroke(); ctx.beginPath(); ctx.moveTo(w*.42,h*.21); ctx.lineTo(w*.58,h*.21); ctx.stroke()
                for (let i=0;i<3;i++){ ctx.beginPath(); const x=w*(.40+i*.10); ctx.moveTo(x,h*.38); ctx.lineTo(x,h*.66); ctx.stroke() }
            } else if (kind === "calendar") {
                fill("#FFFFFF"); roundRect(w*.17,h*.17,w*.66,h*.66,w*.10); ctx.fill()
                fill("#FF5E67"); ctx.fillRect(w*.17,h*.17,w*.66,h*.16)
                fill("#172033"); ctx.font = `${Math.floor(w*.34)}px sans-serif`; ctx.textAlign="center"; ctx.textBaseline="middle"; ctx.fillText("28",cx,h*.56)
            } else if (kind === "notification") {
                stroke(primary,w*.06); ctx.beginPath(); ctx.moveTo(w*.30,h*.64); ctx.quadraticCurveTo(w*.34,h*.54,w*.34,h*.40); ctx.quadraticCurveTo(w*.34,h*.22,cx,h*.22); ctx.quadraticCurveTo(w*.66,h*.22,w*.66,h*.40); ctx.quadraticCurveTo(w*.66,h*.54,w*.70,h*.64); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w*.25,h*.67); ctx.lineTo(w*.75,h*.67); ctx.stroke()
                fill(secondary); ctx.beginPath(); ctx.arc(cx,h*.75,w*.07,0,Math.PI*2); ctx.fill()
            } else {
                stroke(primary,w*.065); ctx.beginPath(); ctx.arc(cx,cy,w*.27,0,Math.PI*2); ctx.stroke()
                fill(tertiary); ctx.beginPath(); ctx.arc(cx,cy,w*.09,0,Math.PI*2); ctx.fill()
            }
        }

        Connections {
            target: root
            function onKindChanged() { canvas.requestPaint() }
            function onPrimaryChanged() { canvas.requestPaint() }
            function onSecondaryChanged() { canvas.requestPaint() }
            function onTertiaryChanged() { canvas.requestPaint() }
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
}
