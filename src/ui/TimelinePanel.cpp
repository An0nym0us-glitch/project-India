#include "TimelinePanel.h"
#include "Format.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include <QHeaderView>
#include <QListWidget>
#include <QProgressBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace isim {

TimelinePanel::TimelinePanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    m_tabs = new QTabWidget;
    m_all = new QListWidget; m_econ = new QListWidget; m_env = new QListWidget;
    m_construction = new QTableWidget(0, 5);
    m_construction->setHorizontalHeaderLabels({tr("Project"), tr("Location"), tr("Progress"), tr("Cost"), tr("Status")});
    m_construction->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_construction->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_construction->verticalHeader()->hide();
    m_construction->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_construction->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabs->addTab(m_all, tr("Notifications"));
    m_tabs->addTab(m_construction, tr("Construction progress"));
    m_tabs->addTab(m_econ, tr("Economic events"));
    m_tabs->addTab(m_env, tr("Environmental alerts"));
    lay->addWidget(m_tabs);
    connect(app, &AppController::worldUpdated, this, &TimelinePanel::refresh);
    connect(app, &AppController::worldReset, this, [this]() { m_logSize = -1; refresh(); });
    connect(m_construction, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        const int uid = m_construction->item(r, 0)->data(Qt::UserRole).toInt();
        m_app->select({Selection::Facility, uid, {}});
        if (const auto *f = m_app->sim().world().facilityByUid(uid)) m_app->focusDistrict(f->district);
    });
    refresh();
}

void TimelinePanel::refresh()
{
    const auto &w = m_app->sim().world();
    if (w.log.size() != m_logSize) {
        if (m_logSize < 0 || w.log.size() < m_logSize) { m_all->clear(); m_econ->clear(); m_env->clear(); m_logSize = 0; }
        for (int i = m_logSize; i < w.log.size(); ++i) {
            const auto &n = w.log[i];
            const QString date = QLocale(QLocale::English).monthName(n.month % 12 + 1, QLocale::ShortFormat) + " " + QString::number(w.startYear + n.month / 12);
            static const char *icons[] = {"ℹ", "🏗", "₹", "🌫", "⚠", "★", "§"};
            const QString text = QStringLiteral("%1  %2  %3").arg(date, QString::fromUtf8(icons[std::clamp(int(n.kind), 0, 6)]), n.text);
            auto *it = new QListWidgetItem(text);
            if (n.kind == Notification::Warning) it->setForeground(QColor(201, 120, 0));
            m_all->insertItem(0, it);
            if (n.kind == Notification::Economy || n.kind == Notification::Policy) m_econ->insertItem(0, text);
            if (n.kind == Notification::Environment) m_env->insertItem(0, text);
        }
        m_logSize = w.log.size();
    }
    if (m_tabs->currentWidget() != m_construction && w.month % 3 != 0) return;
    const auto &cat = m_app->resources().catalog;
    const auto &geo = m_app->resources().geo;
    QVector<const PlacedFacility *> rows;
    for (const auto &f : w.facilities) if (f.status == FacilityStatus::UnderConstruction) rows << &f;
    for (const auto &f : w.facilities) if (f.isOperating() && f.completedMonth >= w.month - 12) rows << &f;
    m_construction->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto *f = rows[r];
        const auto *d = cat.facility(f->defId);
        auto *name = new QTableWidgetItem(d ? d->name : f->defId);
        name->setData(Qt::UserRole, f->uid);
        m_construction->setItem(r, 0, name);
        m_construction->setItem(r, 1, new QTableWidgetItem(f->district >= 0 ? geo.districts()[f->district].name + ", " + geo.districts()[f->district].stateName : QString()));
        auto *bar = qobject_cast<QProgressBar *>(m_construction->cellWidget(r, 2));
        if (!bar) { bar = new QProgressBar; bar->setMaximumHeight(16); m_construction->setCellWidget(r, 2, bar); }
        bar->setValue(int(f->progress * 100));
        m_construction->setItem(r, 3, new QTableWidgetItem(fmt::cr(f->capexCr)));
        m_construction->setItem(r, 4, new QTableWidgetItem(f->isOperating() ? tr("Operating (%1 output)").arg(fmt::pct(f->efficiency, 0)) : tr("Under construction")));
    }
}

} // namespace isim
