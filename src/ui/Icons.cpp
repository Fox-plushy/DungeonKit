#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace dk {
namespace Icons {

QPixmap pixmap(Kind kind, const QColor& c, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 64.0, size / 64.0);          // design grid is 64 x 64
    p.setPen(Qt::NoPen);
    p.setBrush(c);

    switch (kind) {
    case Home: {
        QPainterPath path;
        path.addPolygon(QPolygonF({QPointF(32, 7), QPointF(60, 32), QPointF(4, 32)}));
        path.addRect(QRectF(13, 32, 38, 24));
        path.addRect(QRectF(26, 38, 12, 18));    // door (odd-even fill leaves a hole)
        path.setFillRule(Qt::OddEvenFill);
        p.drawPath(path);
        break;
    }
    case Items: {                                // sword, pointing up and to the right
        p.translate(32, 32);
        p.rotate(45);
        p.drawPolygon(QPolygonF({QPointF(0, -30), QPointF(4, -24), QPointF(4, 8), QPointF(-4, 8), QPointF(-4, -24)}));
        p.drawRoundedRect(QRectF(-13, 8, 26, 5), 2, 2);
        p.drawRect(QRectF(-2.5, 13, 5, 11));
        p.drawEllipse(QPointF(0, 27), 4, 4);
        break;
    }
    case Teleports: {                            // blocky player
        p.drawRect(QRectF(22, 5, 20, 19));
        p.drawRect(QRectF(18, 26, 28, 20));
        p.drawRect(QRectF(7, 26, 9, 19));
        p.drawRect(QRectF(48, 26, 9, 19));
        p.drawRect(QRectF(18, 48, 12, 12));
        p.drawRect(QRectF(34, 48, 12, 12));
        break;
    }
    case Player: {                               // up arrow over a ground line (fly)
        p.drawPolygon(QPolygonF({QPointF(32, 5), QPointF(54, 29), QPointF(40, 29), QPointF(40, 47),
                                 QPointF(24, 47), QPointF(24, 29), QPointF(10, 29)}));
        p.drawRoundedRect(QRectF(12, 53, 40, 6), 3, 3);
        break;
    }
    case Eye: {
        QPainterPath eye;
        eye.moveTo(4, 32);
        eye.quadTo(32, 2, 60, 32);
        eye.quadTo(32, 62, 4, 32);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(eye);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(32, 32), 8.5, 8.5);
        break;
    }
    case Tools: {                                // stopwatch
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 5));
        p.drawEllipse(QRectF(10, 15, 44, 44));
        p.setPen(QPen(c, 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(32, 37), QPointF(32, 25));
        p.drawLine(QPointF(32, 37), QPointF(41, 42));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(26, 4, 12, 6), 2, 2);
        p.drawRect(QRectF(30, 9, 4, 6));
        break;
    }
    case Heart: {
        QPainterPath hp;
        hp.moveTo(32, 56);
        hp.cubicTo(6, 38, 6, 12, 22, 10);
        hp.cubicTo(28, 9, 31, 13, 32, 17);
        hp.cubicTo(33, 13, 36, 9, 42, 10);
        hp.cubicTo(58, 12, 58, 38, 32, 56);
        p.drawPath(hp);
        break;
    }
    case Account: {                              // head and shoulders
        p.drawEllipse(QPointF(32, 21), 12, 12);
        p.drawChord(QRectF(9, 38, 46, 46), 0, 180 * 16);
        break;
    }
    case Customize: {                            // three sliders
        const double ys[] = {16, 32, 48};
        const double xs[] = {22, 42, 28};
        for (int i = 0; i < 3; ++i) {
            p.drawRoundedRect(QRectF(7, ys[i] - 2, 50, 4), 2, 2);
            p.drawEllipse(QPointF(xs[i], ys[i]), 7, 7);
        }
        break;
    }
    case Information: {                          // circled "i"
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 5));
        p.drawEllipse(QRectF(7, 7, 50, 50));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(32, 20), 3.8, 3.8);
        p.drawRoundedRect(QRectF(29, 28, 6, 19), 2, 2);
        break;
    }
    case Folder: {                               // folder with a tab
        QPainterPath path;
        path.moveTo(6, 16); path.lineTo(24, 16); path.lineTo(30, 23); path.lineTo(58, 23);
        path.lineTo(58, 52); path.lineTo(6, 52); path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Save: {                                 // floppy disk
        QPainterPath path;
        path.addRoundedRect(QRectF(8, 8, 48, 48), 5, 5);
        QPainterPath hole;
        hole.addRect(QRectF(17, 8, 26, 16));     // shutter (cut out)
        hole.addRect(QRectF(16, 36, 32, 20));    // label (cut out)
        p.drawPath(path.subtracted(hole));
        break;
    }
    case Book: {                                 // page with text lines
        QPainterPath path;
        path.addRoundedRect(QRectF(13, 6, 38, 52), 4, 4);
        QPainterPath lines;
        for (int i = 0; i < 4; ++i) lines.addRoundedRect(QRectF(20, 17 + i * 10, 24, 4), 2, 2);
        p.drawPath(path.subtracted(lines));
        break;
    }
    case Warning: {                              // warning triangle with an exclamation mark
        QPainterPath tri;
        tri.moveTo(32, 5); tri.lineTo(60, 56); tri.lineTo(4, 56); tri.closeSubpath();
        QPainterPath mark;
        mark.addRoundedRect(QRectF(29, 22, 6, 19), 2.5, 2.5);
        mark.addEllipse(QPointF(32, 47.5), 3.6, 3.6);
        p.drawPath(tri.subtracted(mark));
        break;
    }
    case Camera: {                               // camera body with a lens
        QPainterPath body;
        body.addRoundedRect(QRectF(5, 17, 54, 38), 6, 6);
        QPainterPath bump;
        bump.addRoundedRect(QRectF(21, 9, 22, 12), 3, 3);
        QPainterPath lens;
        lens.addEllipse(QPointF(32, 36), 12, 12);
        QPainterPath core;
        core.addEllipse(QPointF(32, 36), 6.5, 6.5);
        p.drawPath(body.united(bump).subtracted(lens).united(core));
        break;
    }
    case Pin: {                                  // map pin
        QPainterPath pin;
        pin.addEllipse(QPointF(32, 26), 19, 19);
        QPainterPath tip;
        tip.moveTo(16.5, 36); tip.lineTo(32, 61); tip.lineTo(47.5, 36); tip.closeSubpath();
        QPainterPath hole;
        hole.addEllipse(QPointF(32, 26), 7, 7);
        p.drawPath(pin.united(tip).subtracted(hole));
        break;
    }
    case Chest: {                                // chest with a lid line and a latch
        QPainterPath box;
        box.addRoundedRect(QRectF(6, 14, 52, 42), 6, 6);
        QPainterPath cut;
        cut.addRect(QRectF(6, 30, 52, 4));
        QPainterPath latch;
        latch.addRoundedRect(QRectF(27, 27, 10, 14), 2, 2);
        p.drawPath(box.subtracted(cut).united(latch));
        break;
    }
    case Pencil: {                               // pencil
        QPainterPath pen;
        pen.moveTo(9, 55); pen.lineTo(14, 39); pen.lineTo(41, 12); pen.lineTo(52, 23); pen.lineTo(25, 50); pen.closeSubpath();
        QPainterPath tipCut;
        tipCut.moveTo(14, 39); tipCut.lineTo(25, 50); tipCut.lineTo(22, 47); tipCut.closeSubpath();
        p.drawPath(pen.subtracted(tipCut));
        break;
    }
    case Globe: {                                // circle with meridians
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 4));
        p.drawEllipse(QRectF(8, 8, 48, 48));
        p.drawEllipse(QRectF(21, 8, 22, 48));
        p.drawLine(QPointF(8, 32), QPointF(56, 32));
        p.drawLine(QPointF(12, 20), QPointF(52, 20));
        p.drawLine(QPointF(12, 44), QPointF(52, 44));
        break;
    }
    }
    return pm;
}

QIcon navIcon(Kind kind, const QColor& normal, const QColor& checked) {
    QIcon icon;
    icon.addPixmap(pixmap(kind, normal), QIcon::Normal, QIcon::Off);
    icon.addPixmap(pixmap(kind, checked), QIcon::Normal, QIcon::On);
    return icon;
}

} // namespace Icons
} // namespace dk
