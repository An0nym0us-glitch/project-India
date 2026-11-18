#pragma once
#include "ai/AIManager.h"
#include <QWidget>

class QLineEdit;
class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;
class QRadioButton;

namespace isim {

// Reusable settings widgets for AI providers (used by the setup wizard and the settings dialog).
class LocalModelSettings : public QWidget {
    Q_OBJECT
public:
    explicit LocalModelSettings(AIManager *ai, QWidget *parent = nullptr);
    void apply();
    bool hasModel() const;
private:
    void inspect();
    AIManager *m_ai;
    QLineEdit *m_model, *m_server;
    QLabel *m_info, *m_testResult;
    QSpinBox *m_ctx, *m_gpu, *m_threads;
    QPushButton *m_test;
};

class CloudKeySettings : public QWidget {
    Q_OBJECT
public:
    CloudKeySettings(AIManager *ai, const QString &providerId, QWidget *parent = nullptr);
    void apply();
private:
    AIManager *m_ai;
    QString m_id;
    QLineEdit *m_key, *m_model, *m_endpoint = nullptr;
    QLabel *m_state, *m_testResult;
};

class DefaultProviderSettings : public QWidget {
    Q_OBJECT
public:
    explicit DefaultProviderSettings(AIManager *ai, QWidget *parent = nullptr);
    void apply();
private:
    AIManager *m_ai;
    QList<QPair<QString, QRadioButton *>> m_buttons;
};

} // namespace isim
