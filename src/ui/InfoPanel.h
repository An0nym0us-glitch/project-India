#pragma once
#include "AppController.h"
#include "advisor/Suitability.h"
#include <QWidget>

class QTextBrowser;
class QPushButton;
class QLabel;

namespace isim {

// Right-hand panel: facility information sheets, suitability, placed-facility status, region data.
class InfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit InfoPanel(AppController *app, QWidget *parent = nullptr);
    void showPreview(const SuitabilityReport &rep, const QString &defId);
signals:
    void askAI(const QString &task, const QJsonObject &context, const QString &title);
private:
    void refresh();
    QString definitionHtml(const FacilityDefinition &d, const SuitabilityReport *rep) const;
    QString facilityHtml(const PlacedFacility &f) const;
    QString districtHtml(int di) const;
    QString stateHtml(int si) const;
    AppController *m_app;
    QTextBrowser *m_view;
    QPushButton *m_place, *m_ai, *m_demolish, *m_assess;
    SuitabilityReport m_rep;
    QString m_repDef;
    bool m_hasRep = false;
};

} // namespace isim
