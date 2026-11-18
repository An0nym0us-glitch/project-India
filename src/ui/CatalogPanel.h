#pragma once
#include "AppController.h"
#include <QWidget>

class QLineEdit;
class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

namespace isim {

// Searchable infrastructure catalogue with filters, favourites, recent items and recommendations.
class CatalogPanel : public QWidget {
    Q_OBJECT
public:
    explicit CatalogPanel(AppController *app, QWidget *parent = nullptr);
    void focusSearch();
    void refresh();
private:
    QTreeWidgetItem *addItem(QTreeWidgetItem *parent, const FacilityDefinition *d, const QString &extra = {});
    AppController *m_app;
    QLineEdit *m_search;
    QComboBox *m_field, *m_category;
    QTreeWidget *m_tree;
    QLabel *m_count;
};

} // namespace isim
