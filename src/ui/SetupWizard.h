#pragma once
#include "AppController.h"
#include <QWizard>

class QLineEdit;
class QSpinBox;

namespace isim {
class LocalModelSettings;
class CloudKeySettings;
class DefaultProviderSettings;

// First-launch setup: local model, optional OpenAI / Gemini keys, default AI, first scenario.
class SetupWizard : public QWizard {
    Q_OBJECT
public:
    explicit SetupWizard(AppController *app, QWidget *parent = nullptr);
    ScenarioSettings scenarioSettings() const;
    void accept() override;
private:
    AppController *m_app;
    LocalModelSettings *m_local;
    CloudKeySettings *m_openai, *m_gemini;
    DefaultProviderSettings *m_default;
    QLineEdit *m_name;
    QSpinBox *m_year;
    QSpinBox *m_fund;
};
}
