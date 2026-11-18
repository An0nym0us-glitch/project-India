#pragma once
#include "AppController.h"
#include "advisor/Suitability.h"
#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace isim {

enum class Choropleth { None, PopulationDensity, ElectricityServed, WaterServed, Unemployment, HousingShortage, GdpPerCapita, AirQuality, Literacy2011, UrbanShare };

struct MapLayers {
    bool districts = true, rivers = true, lakes = true, physical = false, neighbours = true, labels = true;
    bool facilities = true, networks = true, pollutionGrid = false, crossings = true;
    Choropleth choropleth = Choropleth::None;
};

class MapView : public QWidget {
    Q_OBJECT
public:
    explicit MapView(AppController *app, QWidget *parent = nullptr);
    MapLayers &layers() { return m_layers; }
    void setLayers(const MapLayers &l) { m_layers = l; invalidate(); }
    void fitBounds(const QRectF &lonlat);
    void fitIndia();
    QPointF toLonLat(const QPointF &screen) const;
    QPointF toScreen(const QPointF &lonlat) const;
    void invalidate() { m_cacheValid = false; update(); }
    static QString choroplethName(Choropleth c);
    QString legendTitle() const;

signals:
    void hoverInfo(const QString &text);
    void suitabilityPreview(const isim::SuitabilityReport &rep, const QString &defId);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void leaveEvent(QEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;

private:
    QTransform transform() const;
    void renderBase(QPainter &p);
    void drawFacilities(QPainter &p);
    void drawPlacement(QPainter &p);
    void drawLegend(QPainter &p);
    QColor choroplethColor(int district, double *value = nullptr) const;
    int facilityAt(const QPointF &screen) const;
    QPointF snap(const QPointF &screen, bool *snapped = nullptr) const;
    void finishLine();
    void updatePreview();
    double degScale() const { return m_scale; }

    AppController *m_app;
    MapLayers m_layers;
    QPointF m_center{82.8, 22.5};
    double m_scale = 20; // pixels per degree latitude
    QPixmap m_cache;
    bool m_cacheValid = false;
    QPointF m_cacheCenter;
    double m_cacheScale = 0;
    bool m_dragging = false, m_dragMoved = false, m_userMoved = false;
    QPoint m_dragStart;
    QPointF m_dragCenter;
    QPointF m_mouseLL;
    QPoint m_mouse;
    int m_hoverDistrict = -1;
    QVector<QPointF> m_lineVerts;
    QTimer m_previewTimer, m_redrawTimer;
    SuitabilityReport m_preview;
    bool m_previewValid = false;
};

} // namespace isim
