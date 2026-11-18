#include "CatalogPanel.h"
#include "Format.h"
#include "ai/AppSettings.h"
#include "catalog/Catalog.h"
#include "engines/EconomyEngine.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace isim {

static QIcon swatch(const QColor &c)
{
    QPixmap px(12, 12);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c);
    p.setPen(c.darker(150));
    p.drawRoundedRect(1, 1, 10, 10, 2, 2);
    return QIcon(px);
}

CatalogPanel::CatalogPanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search facilities, industries, technologies, resources…  (Ctrl+F)"));
    m_search->setClearButtonEnabled(true);
    QFont f = m_search->font(); f.setPointSizeF(f.pointSizeF() + 1); m_search->setFont(f);
    m_search->setMinimumHeight(30);
    lay->addWidget(m_search);
    auto *row = new QHBoxLayout;
    m_field = new QComboBox;
    m_field->addItem(tr("All fields"), int(SearchField::All));
    m_field->addItem(tr("Name"), int(SearchField::Name));
    m_field->addItem(tr("Industry"), int(SearchField::Industry));
    m_field->addItem(tr("Technology"), int(SearchField::Technology));
    m_field->addItem(tr("Category"), int(SearchField::Category));
    m_field->addItem(tr("Function"), int(SearchField::Function));
    m_field->addItem(tr("Required resource"), int(SearchField::Resource));
    m_category = new QComboBox;
    m_category->addItem(tr("All categories"), QString());
    for (const auto &c : app->resources().catalog.categories()) m_category->addItem(swatch(fmt::categoryColor(c)), c, c);
    row->addWidget(m_field, 1);
    row->addWidget(m_category, 2);
    lay->addLayout(row);
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Facility"), tr("Capex")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    lay->addWidget(m_tree, 1);
    auto *btns = new QHBoxLayout;
    auto *place = new QPushButton(tr("Place on map"));
    place->setDefault(true);
    auto *fav = new QPushButton(tr("☆ Favourite"));
    btns->addWidget(place);
    btns->addWidget(fav);
    lay->addLayout(btns);
    m_count = new QLabel;
    m_count->setStyleSheet("color: gray");
    lay->addWidget(m_count);

    connect(m_search, &QLineEdit::textChanged, this, &CatalogPanel::refresh);
    connect(m_field, &QComboBox::currentIndexChanged, this, &CatalogPanel::refresh);
    connect(m_category, &QComboBox::currentIndexChanged, this, &CatalogPanel::refresh);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
        if (!it) return;
        const QString id = it->data(0, Qt::UserRole).toString();
        if (!id.isEmpty()) m_app->select({Selection::Definition, -1, id});
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) {
        const QString id = it->data(0, Qt::UserRole).toString();
        if (!id.isEmpty()) m_app->beginPlacement(id);
    });
    connect(place, &QPushButton::clicked, this, [this]() {
        auto *it = m_tree->currentItem();
        if (it && !it->data(0, Qt::UserRole).toString().isEmpty()) m_app->beginPlacement(it->data(0, Qt::UserRole).toString());
    });
    auto toggleFav = [this]() {
        auto *it = m_tree->currentItem();
        if (!it) return;
        const QString id = it->data(0, Qt::UserRole).toString();
        if (id.isEmpty()) return;
        auto favs = AppSettings::favourites();
        if (favs.contains(id)) favs.removeAll(id); else favs << id;
        AppSettings::setFavourites(favs);
        refresh();
    };
    connect(fav, &QPushButton::clicked, this, toggleFav);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this, toggleFav](const QPoint &pos) {
        auto *it = m_tree->itemAt(pos);
        if (!it || it->data(0, Qt::UserRole).toString().isEmpty()) return;
        m_tree->setCurrentItem(it);
        QMenu m;
        m.addAction(tr("Place on map"), [this, it]() { m_app->beginPlacement(it->data(0, Qt::UserRole).toString()); });
        m.addAction(AppSettings::favourites().contains(it->data(0, Qt::UserRole).toString()) ? tr("Remove from favourites") : tr("Add to favourites"), toggleFav);
        m.exec(m_tree->viewport()->mapToGlobal(pos));
    });
    connect(app, &AppController::worldReset, this, &CatalogPanel::refresh);
    refresh();
}

void CatalogPanel::focusSearch() { m_search->setFocus(); m_search->selectAll(); }

QTreeWidgetItem *CatalogPanel::addItem(QTreeWidgetItem *parent, const FacilityDefinition *d, const QString &extra)
{
    auto *it = new QTreeWidgetItem(parent);
    const bool fav = AppSettings::favourites().contains(d->id);
    it->setText(0, (fav ? QStringLiteral("★ ") : QString()) + d->name + (extra.isEmpty() ? QString() : "  — " + extra));
    it->setText(1, fmt::cr(d->capexCr) + (d->isLine() ? "/km" : ""));
    it->setIcon(0, swatch(fmt::categoryColor(d->category)));
    it->setData(0, Qt::UserRole, d->id);
    // flag facilities the technology ecosystem cannot yet support
    auto ctx = m_app->sim().context();
    QStringList why;
    if (EconomyEngine::techFactor(ctx, *d, &why) < 0.5) {
        it->setForeground(0, QColor(160, 90, 40));
        it->setToolTip(0, tr("Technology prerequisites missing: %1").arg(why.join("; ")));
    } else it->setToolTip(0, d->description);
    return it;
}

void CatalogPanel::refresh()
{
    const auto &cat = m_app->resources().catalog;
    m_tree->setUpdatesEnabled(false);
    m_tree->clear();
    const QString text = m_search->text();
    const auto field = SearchField(m_field->currentData().toInt());
    const QString category = m_category->currentData().toString();
    const bool searching = !text.trimmed().isEmpty();
    auto mkGroup = [&](const QString &title) {
        auto *g = new QTreeWidgetItem(m_tree, {title});
        QFont f = g->font(0); f.setBold(true); g->setFont(0, f);
        g->setFlags(Qt::ItemIsEnabled);
        return g;
    };
    if (!searching && category.isEmpty()) {
        // recommended (from the National Development Advisor)
        auto *rec = mkGroup(tr("Recommended by the Advisor"));
        QStringList seen;
        for (const auto &r : m_app->recommendations()) {
            if (r.facilityId.isEmpty() || seen.contains(r.facilityId) || r.decision == "rejected") continue;
            seen << r.facilityId;
            if (const auto *d = cat.facility(r.facilityId)) addItem(rec, d, r.category);
            if (seen.size() >= 8) break;
        }
        rec->setExpanded(true);
        auto *fav = mkGroup(tr("Favourites"));
        for (const auto &id : AppSettings::favourites()) if (const auto *d = cat.facility(id)) addItem(fav, d);
        fav->setExpanded(true);
        auto *rec2 = mkGroup(tr("Recently used"));
        for (const auto &id : AppSettings::recent()) if (const auto *d = cat.facility(id)) addItem(rec2, d);
        rec2->setExpanded(true);
    }
    const auto results = cat.search(text, field, category);
    QHash<QString, QTreeWidgetItem *> groups;
    for (const auto *d : results) {
        const QString key = d->category + " › " + d->subcategory;
        if (!groups.contains(key)) {
            auto *g = mkGroup(key);
            g->setIcon(0, swatch(fmt::categoryColor(d->category)));
            g->setExpanded(searching || !category.isEmpty());
            groups.insert(key, g);
        }
        addItem(groups.value(key), d);
    }
    m_count->setText(searching ? tr("%1 of %2 facility types match").arg(results.size()).arg(cat.facilities().size())
                               : tr("%1 facility types in %2 categories").arg(cat.facilities().size()).arg(cat.categories().size()));
    m_tree->setUpdatesEnabled(true);
}

} // namespace isim
