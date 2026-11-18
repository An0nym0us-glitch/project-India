#include "MapView.h"
#include "Format.h"
#include "catalog/Catalog.h"
#include "engines/Aqi.h"
#include "engines/ConstructionEngine.h"
#include "gis/GeoMath.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>
#include <cmath>

namespace isim {

static const double kLonScale = std::cos(22.0 * geo::kPi / 180.0);

MapView::MapView(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
    setCursor(Qt::OpenHandCursor);
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(90);
    connect(&m_previewTimer, &QTimer::timeout, this, &MapView::updatePreview);
    m_redrawTimer.setSingleShot(true);
    m_redrawTimer.setInterval(120);
    connect(&m_redrawTimer, &QTimer::timeout, this, [this]() { invalidate(); });
    connect(app, &AppController::worldUpdated, this, [this]() {
        if (m_layers.choropleth != Choropleth::None || m_layers.pollutionGrid) m_cacheValid = false;
        update();
    });
    connect(app, &AppController::worldReset, this, [this]() { invalidate(); });
    connect(app, &AppController::selectionChanged, this, [this]() { update(); });
    connect(app, &AppController::placementChanged, this, [this](const QString &d) {
        m_lineVerts.clear();
        m_previewValid = false;
        setCursor(d.isEmpty() ? Qt::OpenHandCursor : Qt::CrossCursor);
        update();
    });
    connect(app, &AppController::focusRequested, this, &MapView::fitBounds);
}

QString MapView::choroplethName(Choropleth c)
{
    switch (c) {
    case Choropleth::None: return tr("None");
    case Choropleth::PopulationDensity: return tr("Population density");
    case Choropleth::ElectricityServed: return tr("Electricity demand served");
    case Choropleth::WaterServed: return tr("Piped water demand served");
    case Choropleth::Unemployment: return tr("Unemployment rate");
    case Choropleth::HousingShortage: return tr("Housing shortage (share of population)");
    case Choropleth::GdpPerCapita: return tr("GDP per capita");
    case Choropleth::AirQuality: return tr("Air quality (CPCB AQI, monthly mean)");
    case Choropleth::Literacy2011: return tr("Literacy (Census 2011, measured)");
    case Choropleth::UrbanShare: return tr("Urban population share");
    }
    return {};
}

QString MapView::legendTitle() const { return choroplethName(m_layers.choropleth); }

QTransform MapView::transform() const
{
    QTransform t;
    t.translate(width() / 2.0, height() / 2.0);
    t.scale(m_scale * kLonScale, -m_scale);
    t.translate(-m_center.x(), -m_center.y());
    return t;
}

QPointF MapView::toScreen(const QPointF &ll) const { return transform().map(ll); }
QPointF MapView::toLonLat(const QPointF &s) const { return transform().inverted().map(s); }

void MapView::fitBounds(const QRectF &b)
{
    if (b.isEmpty()) return;
    m_center = b.center();
    const double sx = width() / (b.width() * kLonScale), sy = height() / b.height();
    m_scale = std::clamp(std::min(sx, sy) * 0.95, 9.0, 4000.0);
    invalidate();
}

void MapView::fitIndia()
{
    fitBounds(m_app->resources().geo.bounds().adjusted(-1, -0.5, 1, 0.5));
}

void MapView::resizeEvent(QResizeEvent *)
{
    if (!m_userMoved) fitIndia(); // keep India framed until the user pans/zooms
    invalidate();
}

static QColor ramp(double t, bool reverse = false)
{
    // sequential blue -> teal -> yellow (perceptually ordered)
    t = std::clamp(t, 0.0, 1.0);
    if (reverse) t = 1 - t;
    static const QColor stops[] = {QColor(0x2c, 0x3e, 0x7a), QColor(0x23, 0x8a, 0x8d), QColor(0x5c, 0xc8, 0x63), QColor(0xfd, 0xe7, 0x25)};
    const double x = t * 3;
    const int i = std::min(2, int(x));
    const double f = x - i;
    const QColor &a = stops[i], &b = stops[i + 1];
    return QColor(int(a.red() + (b.red() - a.red()) * f), int(a.green() + (b.green() - a.green()) * f), int(a.blue() + (b.blue() - a.blue()) * f));
}

QColor MapView::choroplethColor(int di, double *value) const
{
    const auto &w = m_app->sim().world();
    const auto &d = w.districts[di];
    const auto &g = m_app->resources().geo.districts()[di];
    double v = 0, t = 0;
    bool reverse = false;
    switch (m_layers.choropleth) {
    case Choropleth::None: return {};
    case Choropleth::PopulationDensity: v = d.population / std::max(1.0, g.areaKm2); t = std::log10(std::max(1.0, v)) / 4.5; break;
    case Choropleth::ElectricityServed: v = d.powerDemandMW > 0 ? d.powerServedMW / d.powerDemandMW : 0; t = v; break;
    case Choropleth::WaterServed: v = d.waterDemandMLD > 0 ? d.waterSuppliedMLD / d.waterDemandMLD : 0; t = v; break;
    case Choropleth::Unemployment: v = d.labourForce > 0 ? d.unemployed / d.labourForce : 0; t = v / 0.2; reverse = true; break;
    case Choropleth::HousingShortage: v = d.population > 0 ? std::max(0.0, d.population - d.legacyHousing - d.modernHousing) / d.population : 0; t = v / 0.3; reverse = true; break;
    case Choropleth::GdpPerCapita: v = d.population > 0 ? d.gdpCr * 1e7 / d.population : 0; t = std::log10(std::max(1.0, v) / 20000.0) / 2.0; break;
    case Choropleth::AirQuality: v = d.aqi; if (value) *value = v; return CpcbAqi::colorFor(d.aqi);
    case Choropleth::Literacy2011: v = g.census.population > 0 ? g.census.literate / g.census.population : 0; t = (v - 0.3) / 0.6; break;
    case Choropleth::UrbanShare: v = d.population > 0 ? d.urbanPopulation / d.population : 0; t = v; break;
    }
    if (value) *value = v;
    return ramp(t, reverse);
}

void MapView::renderBase(QPainter &p)
{
    const auto &geo = m_app->resources().geo;
    const QTransform t = transform();
    p.fillRect(rect(), palette().color(QPalette::Base).lightness() < 128 ? QColor(0x1b, 0x2a, 0x38) : QColor(0xc9, 0xdc, 0xe8));
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF viewLL = t.inverted().mapRect(QRectF(rect())).normalized().adjusted(-1, -1, 1, 1);
    const bool dark = palette().color(QPalette::Base).lightness() < 128;
    const QColor land = dark ? QColor(0x2e, 0x33, 0x2c) : QColor(0xf2, 0xef, 0xe6);
    p.setTransform(t);
    if (m_layers.neighbours) {
        p.setPen(QPen(dark ? QColor(0x55, 0x55, 0x55) : QColor(0xaa, 0xaa, 0xaa), 0));
        p.setBrush(dark ? QColor(0x26, 0x28, 0x26) : QColor(0xe3, 0xe1, 0xdb));
        for (const auto &f : geo.neighbours()) if (f.bbox.intersects(viewLL)) p.drawPath(f.path);
    }
    // states / districts fill
    const bool districtDetail = m_layers.districts && m_scale > 14;
    p.setPen(Qt::NoPen);
    if (m_layers.choropleth != Choropleth::None) {
        for (const auto &d : geo.districts()) {
            if (!d.feature->bbox.intersects(viewLL)) continue;
            QColor c = choroplethColor(d.index);
            c.setAlpha(230);
            p.setBrush(c);
            p.drawPath(d.feature->path);
        }
    } else {
        p.setBrush(land);
        for (const auto &s : geo.states()) if (s.feature->bbox.intersects(viewLL)) p.drawPath(s.feature->path);
    }
    if (m_layers.physical) {
        for (const auto &f : geo.physicalRegions()) {
            if (!f.bbox.intersects(viewLL)) continue;
            const QString cls = f.properties.value("featurecla").toString();
            QColor c = cls == "Range/mtn" ? QColor(140, 110, 80, 70) : cls == "Desert" ? QColor(230, 200, 120, 70) : QColor(120, 160, 120, 45);
            p.setBrush(c);
            p.setPen(QPen(c.darker(150), 0, Qt::DotLine));
            p.drawPath(f.path);
        }
    }
    // pollution grid overlay
    if (m_layers.pollutionGrid && m_app->sim().environment().initialised()) {
        const auto &env = m_app->sim().environment();
        QImage img(env.nx, env.ny, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        const auto &aqi = m_app->sim().aqi();
        for (int y = 0; y < env.ny; ++y)
            for (int x = 0; x < env.nx; ++x) {
                const int i = env.idx(x, y);
                const int si = aqi.subIndex("pm25", env.pm25[i]);
                QColor c = CpcbAqi::colorFor(si);
                c.setAlpha(env.cellDistrict[i] >= 0 ? 150 : 70);
                img.setPixelColor(x, y, c); // row 0 = south; the map transform flips y
            }
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(env.lon0, env.lat0, env.nx * env.cell, env.ny * env.cell), img);
        p.restore();
    }
    // lakes, rivers
    if (m_layers.lakes) {
        p.setPen(Qt::NoPen);
        p.setBrush(dark ? QColor(0x2a, 0x4d, 0x6e) : QColor(0x9c, 0xc3, 0xe0));
        for (const auto &f : geo.lakes()) if (f.bbox.intersects(viewLL)) p.drawPath(f.path);
    }
    if (m_layers.rivers) {
        for (const auto &f : geo.rivers()) {
            if (!f.bbox.intersects(viewLL)) continue;
            const int rank = f.properties.value("scalerank").toInt(8);
            const double wpx = std::clamp((9 - rank) * 0.35 * std::sqrt(m_scale / 20.0), 0.6, 3.5);
            QPen pen(dark ? QColor(0x3f, 0x7f, 0xb5) : QColor(0x4a, 0x8e, 0xc7), wpx);
            pen.setCosmetic(true);
            p.setPen(pen);
            for (const auto &l : f.lines) p.drawPolyline(l);
        }
    }
    // borders
    if (districtDetail) {
        QPen dp(dark ? QColor(0x70, 0x70, 0x68) : QColor(0xb4, 0xad, 0xa0), 0.6);
        dp.setCosmetic(true);
        p.setPen(dp);
        p.setBrush(Qt::NoBrush);
        for (const auto &d : geo.districts()) if (d.feature->bbox.intersects(viewLL)) p.drawPath(d.feature->path);
    }
    QPen sp(dark ? QColor(0xc8, 0xc0, 0xa8) : QColor(0x6e, 0x64, 0x55), m_scale > 30 ? 1.6 : 1.1);
    sp.setCosmetic(true);
    p.setPen(sp);
    p.setBrush(Qt::NoBrush);
    for (const auto &s : geo.states()) if (s.feature->bbox.intersects(viewLL)) p.drawPath(s.feature->path);
    if (const auto *o = geo.indiaOutline()) {
        QPen op(dark ? QColor(0xe0, 0xd8, 0xc0) : QColor(0x3d, 0x35, 0x2a), 1.8);
        op.setCosmetic(true);
        p.setPen(op);
        p.drawPath(o->path);
    }
    // labels
    p.resetTransform();
    if (m_layers.labels) {
        QFont f = font();
        f.setPointSizeF(std::clamp(7.0 + m_scale / 25.0, 7.5, 11.0));
        f.setBold(true);
        p.setFont(f);
        const QColor txt = dark ? QColor(0xe8, 0xe4, 0xd8) : QColor(0x3a, 0x33, 0x2a);
        for (const auto &s : geo.states()) {
            if (s.areaKm2 < 3000 && m_scale < 60) continue;
            if (s.areaKm2 < 40000 && m_scale < 24) continue;
            const QPointF pt = t.map(s.centroid);
            if (!rect().contains(pt.toPoint())) continue;
            const QString name = m_scale < 24 ? s.id : s.name;
            QRectF r(pt.x() - 80, pt.y() - 9, 160, 18);
            p.setPen(QPen(dark ? QColor(0, 0, 0, 160) : QColor(255, 255, 255, 190), 3));
            QPainterPath tp; tp.addText(QPointF(pt.x() - p.fontMetrics().horizontalAdvance(name) / 2.0, pt.y() + 4), f, name);
            p.drawPath(tp);
            p.fillPath(tp, txt);
        }
        if (m_scale > 60) {
            QFont df = font();
            df.setPointSizeF(8);
            p.setFont(df);
            p.setPen(dark ? QColor(0xbb, 0xbb, 0xaa) : QColor(0x5a, 0x52, 0x48));
            for (const auto &d : geo.districts()) {
                const QPointF pt = t.map(d.centroid);
                if (rect().contains(pt.toPoint())) p.drawText(QRectF(pt.x() - 60, pt.y() + 6, 120, 14), Qt::AlignCenter, d.name);
            }
        }
    }
}

void MapView::drawFacilities(QPainter &p)
{
    const auto &w = m_app->sim().world();
    const auto &cat = m_app->resources().catalog;
    const QTransform t = transform();
    const QRectF view = QRectF(rect()).adjusted(-50, -50, 50, 50);
    p.setRenderHint(QPainter::Antialiasing, true);
    // lines first
    if (m_layers.networks) {
        for (const auto &f : w.facilities) {
            const auto *d = cat.facility(f.defId);
            if (!d || !d->isLine() || f.path.size() < 2) continue;
            QPolygonF poly;
            for (const auto &v : f.path) poly << t.map(v);
            if (!poly.boundingRect().intersects(view)) continue;
            QColor c; double wpx = 2; Qt::PenStyle style = Qt::SolidLine;
            const QString &n = d->network;
            if (n == "road") {
                if (d->id == "expressway") { c = QColor(0xd9, 0x5f, 0x0e); wpx = 4; }
                else if (d->id == "national_highway") { c = QColor(0xe6, 0x7e, 0x22); wpx = 3.2; }
                else if (d->id == "state_highway") { c = QColor(0xe0, 0xb0, 0x3a); wpx = 2.4; }
                else { c = QColor(0x9a, 0x8f, 0x80); wpx = 1.6; }
            } else if (n == "rail" || n == "metro") { c = n == "metro" ? QColor(0xc0, 0x3a, 0xa0) : QColor(0x30, 0x30, 0x30); wpx = d->id == "freight_corridor" ? 3.5 : 2.5; }
            else if (n == "power") { c = QColor(0x8e, 0x44, 0xad); wpx = d->id == "hvdc_line" ? 2.8 : 1.8; style = Qt::DashLine; }
            else if (n == "water") { c = QColor(0x1f, 0x78, 0xb4); wpx = 2.2; }
            else if (n == "fibre") { c = QColor(0x17, 0xbe, 0xcf); wpx = 1.4; style = Qt::DotLine; }
            else if (n == "waterway") { c = QColor(0x5d, 0xad, 0xe2); wpx = 3; style = Qt::DashDotLine; }
            if (f.status == FacilityStatus::UnderConstruction) { c.setAlpha(120); style = Qt::DashLine; }
            if (n == "rail" && f.isOperating()) {
                QPen under(QColor(255, 255, 255), wpx + 2); under.setCosmetic(true); p.setPen(under); p.drawPolyline(poly);
                QPen top(c, wpx, Qt::DashLine); top.setCosmetic(true); p.setPen(top); p.drawPolyline(poly);
            } else {
                QPen pen(c, wpx, style, Qt::RoundCap, Qt::RoundJoin);
                pen.setCosmetic(true);
                p.setPen(pen);
                p.drawPolyline(poly);
            }
            if (f.networkBroken && f.isOperating()) {
                QPen bp(QColor(220, 30, 30), 1.5, Qt::DotLine); bp.setCosmetic(true); p.setPen(bp); p.drawPolyline(poly);
            }
            if (m_app->selection().type == Selection::Facility && m_app->selection().index == f.uid) {
                QPen hp(QColor(0, 200, 255, 160), wpx + 6); hp.setCosmetic(true); p.setPen(hp); p.drawPolyline(poly);
            }
        }
        if (m_layers.crossings) {
            p.setPen(QPen(QColor(220, 30, 30), 2));
            p.setBrush(Qt::NoBrush);
            for (const auto &c : m_app->sim().network().unbridgedCrossings) {
                const QPointF s = t.map(c.second);
                p.drawLine(s + QPointF(-5, -5), s + QPointF(5, 5));
                p.drawLine(s + QPointF(-5, 5), s + QPointF(5, -5));
            }
        }
    }
    if (!m_layers.facilities) return;
    const double r = std::clamp(3.0 + m_scale / 25.0, 4.0, 11.0);
    QFont lf = font(); lf.setPointSizeF(7.5);
    p.setFont(lf);
    for (const auto &f : w.facilities) {
        const auto *d = cat.facility(f.defId);
        if (!d || d->isLine()) continue;
        const QPointF s = t.map(f.location);
        if (!view.contains(s)) continue;
        const QColor c = fmt::categoryColor(d->category);
        const bool sel = m_app->selection().type == Selection::Facility && m_app->selection().index == f.uid;
        if (sel) { p.setPen(Qt::NoPen); p.setBrush(QColor(0, 200, 255, 120)); p.drawEllipse(s, r + 6, r + 6); }
        if (f.status == FacilityStatus::UnderConstruction) {
            p.setPen(QPen(c, 1.5, Qt::DashLine)); p.setBrush(QColor(255, 255, 255, 140));
            p.drawEllipse(s, r, r);
            p.setPen(QPen(c, 2.5));
            p.drawArc(QRectF(s.x() - r, s.y() - r, 2 * r, 2 * r), 90 * 16, -int(f.progress * 360 * 16));
        } else {
            p.setPen(QPen(f.efficiency < 0.3 ? QColor(220, 30, 30) : QColor(30, 30, 30), f.efficiency < 0.3 ? 2.2 : 1.0));
            p.setBrush(c);
            if (d->tags.contains("power_plant") || d->tags.contains("storage")) {
                QPolygonF tri; tri << s + QPointF(0, -r * 1.2) << s + QPointF(r * 1.1, r * 0.8) << s + QPointF(-r * 1.1, r * 0.8);
                p.drawPolygon(tri);
            } else if (d->tags.contains("manufacturing") || d->tags.contains("mining")) p.drawRect(QRectF(s.x() - r, s.y() - r, 2 * r, 2 * r));
            else if (d->tags.contains("research") || d->tags.contains("education")) {
                QPolygonF dia; dia << s + QPointF(0, -r * 1.2) << s + QPointF(r * 1.2, 0) << s + QPointF(0, r * 1.2) << s + QPointF(-r * 1.2, 0);
                p.drawPolygon(dia);
            } else p.drawEllipse(s, r, r);
        }
        if (m_scale > 90 && m_layers.labels) {
            p.setPen(palette().color(QPalette::Text));
            p.drawText(QRectF(s.x() + r + 2, s.y() - 7, 180, 14), Qt::AlignLeft | Qt::AlignVCenter, f.label.isEmpty() ? d->name : f.label);
        }
    }
}

void MapView::drawPlacement(QPainter &p)
{
    const QString defId = m_app->placementDef();
    if (defId.isEmpty()) return;
    const auto *d = m_app->resources().catalog.facility(defId);
    if (!d) return;
    const QTransform t = transform();
    QColor status = QColor(150, 150, 150);
    if (m_previewValid) {
        switch (m_preview.status) {
        case SuitabilityStatus::Recommended: status = QColor(40, 170, 80); break;
        case SuitabilityStatus::FeasibleWithGaps: status = QColor(230, 170, 30); break;
        case SuitabilityStatus::NotRecommended: status = QColor(230, 100, 30); break;
        case SuitabilityStatus::NotFeasible: status = QColor(210, 30, 30); break;
        }
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    QString caption;
    if (d->isLine()) {
        QPolygonF poly;
        for (const auto &v : m_lineVerts) poly << t.map(v);
        bool snapped = false;
        const QPointF cur = snap(m_mouse, &snapped);
        poly << t.map(cur);
        QPen pen(status, 3, Qt::DashLine); pen.setCosmetic(true);
        p.setPen(pen);
        p.drawPolyline(poly);
        p.setBrush(status);
        for (const auto &v : poly) p.drawEllipse(v, 3, 3);
        if (snapped) { p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(0, 180, 255), 2)); p.drawEllipse(t.map(cur), 8, 8); }
        QVector<QPointF> all = m_lineVerts; all << cur;
        const double km = geo::polylineLengthKm(all);
        auto ctx = m_app->sim().context();
        caption = QStringLiteral("%1 — %2 km — %3   [click: add point · double-click/Enter: finish · Backspace: undo · Esc: cancel]")
                      .arg(d->name).arg(km, 0, 'f', 1).arg(fmt::cr(ConstructionEngine::effectiveCapex(ctx, *d, km)));
    } else {
        const QPointF s = m_mouse;
        p.setPen(QPen(status, 2));
        p.setBrush(QColor(status.red(), status.green(), status.blue(), 90));
        p.drawEllipse(s, 12, 12);
        caption = d->name + (m_previewValid ? " — " + m_preview.statusText + (m_preview.location.isEmpty() ? QString() : " (" + m_preview.location + ")") : QString())
                  + "   [click: place · Esc: cancel]";
    }
    QFont f = font(); f.setBold(true);
    p.setFont(f);
    const QRectF box(10, height() - 34, std::min(width() - 20.0, p.fontMetrics().horizontalAdvance(caption) + 20.0), 24);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 200));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::white);
    p.drawText(box.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, caption);
}

void MapView::drawLegend(QPainter &p)
{
    if (m_layers.choropleth == Choropleth::None && !m_layers.pollutionGrid) return;
    const bool aqi = m_layers.choropleth == Choropleth::AirQuality || (m_layers.choropleth == Choropleth::None && m_layers.pollutionGrid);
    QRectF box(width() - 250, 10, 240, aqi ? 140 : 70);
    p.setPen(QPen(QColor(0, 0, 0, 80)));
    p.setBrush(QColor(255, 255, 255, 225));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::black);
    QFont f = font(); f.setPointSizeF(8.5); f.setBold(true); p.setFont(f);
    p.drawText(box.adjusted(8, 4, -8, 0), Qt::AlignTop | Qt::AlignLeft, aqi ? tr("CPCB AQI category (PM2.5 sub-index for grid)") : legendTitle());
    f.setBold(false); p.setFont(f);
    if (aqi) {
        const int vals[] = {25, 75, 150, 250, 350, 450};
        for (int i = 0; i < 6; ++i) {
            const QRectF sw(box.left() + 8, box.top() + 24 + i * 18, 14, 12);
            p.fillRect(sw, CpcbAqi::colorFor(vals[i]));
            p.drawText(QPointF(sw.right() + 6, sw.bottom()), CpcbAqi::categoryFor(vals[i]));
        }
        p.drawText(QPointF(box.left() + 110, box.bottom() - 8), tr("Simplified model"));
        return;
    }
    for (int i = 0; i < 100; ++i) p.fillRect(QRectF(box.left() + 10 + i * 2.2, box.top() + 26, 2.3, 14), ramp(i / 99.0));
    const bool reverse = m_layers.choropleth == Choropleth::Unemployment || m_layers.choropleth == Choropleth::HousingShortage;
    p.drawText(QPointF(box.left() + 10, box.top() + 58), reverse ? tr("higher / worse") : tr("lower"));
    p.drawText(QRectF(box.left(), box.top() + 46, box.width() - 10, 16), Qt::AlignRight, reverse ? tr("lower / better") : tr("higher"));
}

void MapView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const bool same = m_cacheValid && m_cacheCenter == m_center && std::abs(m_cacheScale - m_scale) < 1e-9 && m_cache.size() == size() * devicePixelRatioF();
    if (m_dragging && m_cacheValid && std::abs(m_cacheScale - m_scale) < 1e-9) {
        // fast pan: shift the cached base map
        const QPointF off = toScreen(m_cacheCenter) - QPointF(width() / 2.0, height() / 2.0);
        p.fillRect(rect(), palette().color(QPalette::Base).lightness() < 128 ? QColor(0x1b, 0x2a, 0x38) : QColor(0xc9, 0xdc, 0xe8));
        p.drawPixmap(off, m_cache);
    } else {
        if (!same) {
            m_cache = QPixmap(size() * devicePixelRatioF());
            m_cache.setDevicePixelRatio(devicePixelRatioF());
            QPainter cp(&m_cache);
            renderBase(cp);
            m_cacheValid = true;
            m_cacheCenter = m_center;
            m_cacheScale = m_scale;
        }
        p.drawPixmap(0, 0, m_cache);
    }
    // hover / selection outlines
    const auto &geo = m_app->resources().geo;
    p.setTransform(transform());
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto &sel = m_app->selection();
    if (sel.type == Selection::State && sel.index >= 0) {
        QPen pen(QColor(0, 160, 255), 3); pen.setCosmetic(true); p.setPen(pen); p.setBrush(QColor(0, 160, 255, 30));
        p.drawPath(geo.states()[sel.index].feature->path);
    } else if (sel.type == Selection::District && sel.index >= 0) {
        QPen pen(QColor(0, 160, 255), 2.5); pen.setCosmetic(true); p.setPen(pen); p.setBrush(QColor(0, 160, 255, 40));
        p.drawPath(geo.districts()[sel.index].feature->path);
    }
    if (m_hoverDistrict >= 0 && !m_dragging) {
        QPen pen(QColor(255, 140, 0, 200), 1.5); pen.setCosmetic(true); p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawPath(geo.districts()[m_hoverDistrict].feature->path);
    }
    p.resetTransform();
    drawFacilities(p);
    drawPlacement(p);
    drawLegend(p);
    // scale bar
    const double kmPerPx = 110.57 / m_scale;
    double km = 1; for (double c : {1., 2., 5., 10., 20., 50., 100., 200., 500., 1000.}) if (c / kmPerPx < 140) km = c;
    const double px = km / kmPerPx;
    p.setPen(QPen(palette().color(QPalette::Text), 2));
    p.drawLine(QPointF(12, 18), QPointF(12 + px, 18));
    p.drawText(QPointF(14 + px, 22), QStringLiteral("%1 km").arg(km));
}

void MapView::wheelEvent(QWheelEvent *e)
{
    const QPointF before = toLonLat(e->position());
    const double f = std::pow(1.0015, e->angleDelta().y());
    m_scale = std::clamp(m_scale * f, 9.0, 4000.0);
    m_userMoved = true;
    const QPointF after = toLonLat(e->position());
    m_center += before - after;
    m_cacheValid = false;
    update();
}

void MapView::mousePressEvent(QMouseEvent *e)
{
    setFocus();
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragMoved = false;
        m_dragStart = e->pos();
        m_dragCenter = m_center;
        if (m_app->placementDef().isEmpty()) setCursor(Qt::ClosedHandCursor);
    }
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->pos();
    m_mouseLL = toLonLat(e->pos());
    if (m_dragging) {
        const QPoint d = e->pos() - m_dragStart;
        if (d.manhattanLength() > 3) m_dragMoved = true;
        if (m_dragMoved) {
            m_center = m_dragCenter - QPointF(d.x() / (m_scale * kLonScale), -d.y() / m_scale);
            m_userMoved = true;
            update();
            return;
        }
    }
    const int di = m_app->resources().geo.districtAt(m_mouseLL);
    if (di != m_hoverDistrict) m_hoverDistrict = di;
    QString info;
    if (di >= 0) {
        const auto &d = m_app->resources().geo.districts()[di];
        const auto &ds = m_app->sim().world().districts[di];
        info = QStringLiteral("%1, %2 — pop. %3").arg(d.name, d.stateName, fmt::people(ds.population));
        if (m_layers.choropleth != Choropleth::None) { double v = 0; choroplethColor(di, &v); info += QStringLiteral(" — %1: %2").arg(legendTitle()).arg(v, 0, 'g', 3); }
    }
    info += QStringLiteral("   (%1°E, %2°N)").arg(m_mouseLL.x(), 0, 'f', 3).arg(m_mouseLL.y(), 0, 'f', 3);
    emit hoverInfo(info);
    if (!m_app->placementDef().isEmpty()) m_previewTimer.start();
    update();
}

void MapView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const bool moved = m_dragMoved;
    m_dragging = false;
    m_dragMoved = false;
    setCursor(m_app->placementDef().isEmpty() ? Qt::OpenHandCursor : Qt::CrossCursor);
    if (moved) { invalidate(); return; }
    const QPointF ll = toLonLat(e->pos());
    const QString defId = m_app->placementDef();
    if (!defId.isEmpty()) {
        const auto *d = m_app->resources().catalog.facility(defId);
        if (!d) return;
        if (d->isLine()) { m_lineVerts << snap(e->pos()); update(); return; }
        auto r = m_app->place({defId, ll, {}, {}});
        if (!r.ok) QMessageBox::warning(this, tr("Cannot place facility"), r.error);
        else if (!(e->modifiers() & Qt::ShiftModifier)) m_app->cancelPlacement(); // Shift keeps placing
        return;
    }
    const int fu = facilityAt(e->pos());
    if (fu >= 0) { m_app->select({Selection::Facility, fu, {}}); return; }
    const auto &geo = m_app->resources().geo;
    const int di = geo.districtAt(ll);
    if (di < 0) { m_app->select({}); return; }
    if (m_scale > 14 && m_layers.districts && !(e->modifiers() & Qt::AltModifier)) m_app->select({Selection::District, di, {}});
    else m_app->select({Selection::State, geo.districts()[di].stateIndex, {}});
}

void MapView::mouseDoubleClickEvent(QMouseEvent *e)
{
    const auto *d = m_app->resources().catalog.facility(m_app->placementDef());
    if (d && d->isLine()) { finishLine(); return; }
    // double-click on a district selects its state
    const int di = m_app->resources().geo.districtAt(toLonLat(e->pos()));
    if (di >= 0 && m_app->placementDef().isEmpty()) m_app->select({Selection::State, m_app->resources().geo.districts()[di].stateIndex, {}});
}

void MapView::finishLine()
{
    const QString defId = m_app->placementDef();
    // the double-click also produced a release-click vertex; remove exact duplicates
    QVector<QPointF> pts;
    for (const auto &v : m_lineVerts) if (pts.isEmpty() || geo::haversineKm(pts.last(), v) > 0.05) pts << v;
    if (pts.size() < 2) return;
    auto r = m_app->place({defId, {}, pts, {}});
    m_lineVerts.clear();
    if (!r.ok) QMessageBox::warning(this, tr("Cannot build line"), r.error);
    update();
}

void MapView::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) { m_lineVerts.clear(); m_app->cancelPlacement(); update(); return; }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !m_lineVerts.isEmpty()) { finishLine(); return; }
    if (e->key() == Qt::Key_Backspace && !m_lineVerts.isEmpty()) { m_lineVerts.removeLast(); update(); return; }
    if (e->key() == Qt::Key_Delete && m_app->selection().type == Selection::Facility) { m_app->demolish(m_app->selection().index); return; }
    if (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal) { m_scale *= 1.25; invalidate(); return; }
    if (e->key() == Qt::Key_Minus) { m_scale = std::max(5.0, m_scale / 1.25); invalidate(); return; }
    QWidget::keyPressEvent(e);
}

void MapView::leaveEvent(QEvent *) { m_hoverDistrict = -1; update(); }

void MapView::contextMenuEvent(QContextMenuEvent *e)
{
    if (!m_app->placementDef().isEmpty()) { m_lineVerts.clear(); m_app->cancelPlacement(); return; }
    const int fu = facilityAt(e->pos());
    QMenu m(this);
    if (fu >= 0) {
        const auto *f = m_app->sim().world().facilityByUid(fu);
        const auto *d = f ? m_app->resources().catalog.facility(f->defId) : nullptr;
        m.addAction(tr("Select %1").arg(d ? d->name : QString()), [this, fu]() { m_app->select({Selection::Facility, fu, {}}); });
        m.addAction(tr("Demolish (costs are not recovered)"), [this, fu]() {
            if (QMessageBox::question(this, tr("Demolish facility"), tr("Remove this facility? Capital spent is not recovered.")) == QMessageBox::Yes) m_app->demolish(fu);
        });
    }
    const int di = m_app->resources().geo.districtAt(toLonLat(e->pos()));
    if (di >= 0) {
        m.addAction(tr("District details"), [this, di]() { m_app->select({Selection::District, di, {}}); });
        m.addAction(tr("State details"), [this, di]() { m_app->select({Selection::State, m_app->resources().geo.districts()[di].stateIndex, {}}); });
    }
    m.addAction(tr("Zoom to India"), this, &MapView::fitIndia);
    m.exec(e->globalPos());
}

int MapView::facilityAt(const QPointF &s) const
{
    const auto &w = m_app->sim().world();
    const QTransform t = transform();
    int best = -1; double bd = 12;
    for (const auto &f : w.facilities) {
        const auto *d = m_app->resources().catalog.facility(f.defId);
        if (!d) continue;
        double dist;
        if (d->isLine()) {
            dist = 1e9;
            for (int i = 1; i < f.path.size(); ++i) {
                const QPointF a = t.map(f.path[i - 1]), b = t.map(f.path[i]);
                const QPointF ab = b - a, ap = s - a;
                const double l2 = QPointF::dotProduct(ab, ab);
                const double u = l2 > 0 ? std::clamp(QPointF::dotProduct(ap, ab) / l2, 0.0, 1.0) : 0;
                dist = std::min(dist, QLineF(s, a + ab * u).length());
            }
            dist += 3; // prefer points over lines
        } else dist = QLineF(s, t.map(f.location)).length();
        if (dist < bd) { bd = dist; best = f.uid; }
    }
    return best;
}

QPointF MapView::snap(const QPointF &s, bool *snapped) const
{
    // snap to vertices of existing lines of the same network, or to network node facilities
    if (snapped) *snapped = false;
    const auto *d = m_app->resources().catalog.facility(m_app->placementDef());
    QPointF best = toLonLat(s);
    if (!d) return best;
    const QTransform t = transform();
    double bd = 12;
    for (const auto &f : m_app->sim().world().facilities) {
        const auto *fd = m_app->resources().catalog.facility(f.defId);
        if (!fd) continue;
        const bool sameNet = fd->isLine() && fd->network == d->network;
        const bool node = !fd->isLine() && (fd->nodeOf.contains(d->network) || (d->network == "power" && (fd->generationMW > 0 || fd->distributionMW > 0)) ||
                                            (d->network == "water" && fd->waterSupplyMLD > 0) || fd->tags.contains("bridge"));
        if (sameNet) for (const auto &v : f.path) { const double dd = QLineF(s, t.map(v)).length(); if (dd < bd) { bd = dd; best = v; if (snapped) *snapped = true; } }
        if (node) { const double dd = QLineF(s, t.map(f.location)).length(); if (dd < bd) { bd = dd; best = f.location; if (snapped) *snapped = true; } }
    }
    return best;
}

void MapView::updatePreview()
{
    const auto *d = m_app->resources().catalog.facility(m_app->placementDef());
    if (!d) return;
    auto ctx = m_app->sim().context();
    QVector<QPointF> path;
    if (d->isLine()) { path = m_lineVerts; path << snap(m_mouse); if (path.size() < 2) path << path.first() + QPointF(0.01, 0); }
    m_preview = SuitabilityEngine::assess(ctx, *d, m_mouseLL, path);
    m_previewValid = true;
    emit suitabilityPreview(m_preview, d->id);
    update();
}

} // namespace isim
