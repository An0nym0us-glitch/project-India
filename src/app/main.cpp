#include "ai/AppSettings.h"
#include "core/DataPaths.h"
#include "sim/Simulation.h"
#include "ui/AppController.h"
#include "ui/MainWindow.h"
#include "ui/SetupWizard.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPainter>
#include <QSplashScreen>
#include <QStyleFactory>
#include <QTimer>
#include <QComboBox>

using namespace isim;

static QIcon appIcon()
{
    QPixmap px(64, 64);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0x99, 0x33)); p.drawRoundedRect(4, 4, 56, 19, 4, 4);
    p.setBrush(QColor(0xf5, 0xf5, 0xf5)); p.drawRect(4, 23, 56, 18);
    p.setBrush(QColor(0x13, 0x88, 0x08)); p.drawRoundedRect(4, 41, 56, 19, 4, 4);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(0x00, 0x00, 0x80), 2.5)); p.drawEllipse(QPointF(32, 32), 7.5, 7.5);
    return QIcon(px);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("IndiaSim");
    QApplication::setApplicationName("IndiaDevelopmentSimulator");
    QApplication::setApplicationVersion(QStringLiteral(ISIM_VERSION));
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setWindowIcon(appIcon());

    QPixmap splashPx(520, 160);
    splashPx.fill(QColor(0x1d, 0x2b, 0x3a));
    QSplashScreen splash(splashPx);
    splash.show();
    splash.showMessage(QObject::tr("India National Development & Policy Simulator\nLoading geography, catalogue and reference data…"), Qt::AlignCenter, Qt::white);
    app.processEvents();

    static SimulationResources resources;
    QString error;
    if (!resources.load(dataDir(), &error)) {
        splash.close();
        QMessageBox::critical(nullptr, QObject::tr("Data could not be loaded"),
                              QObject::tr("%1\n\nData folder searched: %2\nThe 'data' folder must be next to the executable.").arg(error, dataDir()));
        return 1;
    }
    AppController controller(resources);
    ScenarioSettings first;
    first.name = QObject::tr("My India development plan");
    const int shotIdx = app.arguments().indexOf("--screenshot");
    const QString shotPath = shotIdx > 0 && shotIdx + 1 < app.arguments().size() ? app.arguments().at(shotIdx + 1) : QString();
    if (app.arguments().contains("--wizard-screenshot")) {
        splash.close();
        SetupWizard wiz(&controller);
        wiz.show();
        wiz.next();
        QTimer::singleShot(800, [&]() { wiz.grab().save(app.arguments().last()); app.quit(); });
        return app.exec();
    }
    if (shotPath.isEmpty() && (!AppSettings::firstRunComplete() || app.arguments().contains("--setup"))) {
        splash.close();
        SetupWizard wiz(&controller);
        if (wiz.exec() == QDialog::Accepted) first = wiz.scenarioSettings();
        else AppSettings::setFirstRunComplete(true); // skipped: do not ask again (Settings can rerun it)
    }
    splash.showMessage(QObject::tr("Initialising scenario from World Bank and Census 2011 data…"), Qt::AlignCenter, Qt::white);
    app.processEvents();
    QString openPath;
    for (const auto &a : app.arguments()) if (a.endsWith(".indiasim")) openPath = a;
    if (!openPath.isEmpty() && controller.loadScenario(openPath, &error)) {}
    else controller.newScenario(first);
    MainWindow w(&controller);
    // the local model is started on demand to save memory; start now if it is the default
    if (auto *p = controller.ai()->active(); p && p->isLocal() && p->isConfigured()) p->start();
    w.showMaximized();
    splash.finish(&w);
    if (!shotPath.isEmpty()) {
        // diagnostic: run N months (--months N), render the window to an image and exit
        const int mi = app.arguments().indexOf("--months");
        if (mi > 0) controller.stepMonths(app.arguments().at(mi + 1).toInt());
        w.resize(1700, 1000);
        const int di = app.arguments().indexOf("--dashboard");
        const int dashTab = di > 0 ? app.arguments().at(di + 1).toInt() : -1;
        const int ci = app.arguments().indexOf("--choropleth");
        if (ci > 0) w.setDataLayer(app.arguments().at(ci + 1).toInt());
        QTimer::singleShot(1500, [&, dashTab]() {
            if (dashTab >= 0) {
                QMetaObject::invokeMethod(&w, "showDashboardTab", Q_ARG(int, dashTab));
                QTimer::singleShot(3000, [&]() {
                    for (auto *tl : QApplication::topLevelWidgets()) if (tl->isWindow() && tl != &w && tl->isVisible() && tl->windowTitle().startsWith("Dashboard")) tl->grab().save(shotPath);
                    app.quit();
                });
                return;
            }
            w.grab().save(shotPath);
            app.quit();
        });
    }
    return app.exec();
}
