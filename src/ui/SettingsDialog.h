#pragma once
#include "AppController.h"
#include <QDialog>
namespace isim {
class LocalModelSettings;
class CloudKeySettings;
class DefaultProviderSettings;
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(AppController *app, QWidget *parent = nullptr);
    void accept() override;
private:
    LocalModelSettings *m_local;
    CloudKeySettings *m_openai, *m_gemini;
    DefaultProviderSettings *m_default;
};
}
