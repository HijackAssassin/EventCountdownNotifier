#include <QApplication>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSharedMemory>
#include <QDebug>
#include <QDir>
#include <QUuid>
#include <QMenu>
#include <QAction>

struct CountdownEvent {
    QString uuid;
    QString title;
    QDateTime datetime;
    QTimer* timer;
};

QVector<CountdownEvent> activeTimers;

QString extractOrGenerateUUID(const QJsonObject &obj) {
    if (obj.contains("uuid"))
        return obj["uuid"].toString();
    return QUuid::createUuidV5(
               QUuid::fromString("{00000000-0000-0000-0000-000000000000}"),
               obj["title"].toString().toUtf8()).toString();
}

void clearTimers() {
    for (CountdownEvent &ev : activeTimers) {
        if (ev.timer) {
            ev.timer->stop();
            ev.timer->deleteLater();
        }
    }
    activeTimers.clear();
}

void updateCountdowns(QSystemTrayIcon *tray) {
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(basePath);

    if (dir.dirName().compare("EventCountdowns", Qt::CaseInsensitive) != 0) {
        dir.cdUp();
        dir.mkdir("EventCountdowns");
        dir.cd("EventCountdowns");
    }

    QString path = dir.filePath("countdowns.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Notifier: Failed to open countdowns.json";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) return;

    QDateTime now = QDateTime::currentDateTime();
    QJsonArray array = doc.array();

    for (int i = 0; i < activeTimers.size(); ) {
        bool found = false;
        for (const QJsonValue &val : array) {
            QJsonObject obj = val.toObject();
            QString uuid = extractOrGenerateUUID(obj);
            if (uuid == activeTimers[i].uuid) {
                found = true;
                QDateTime newTime = QDateTime::fromString(obj["datetime"].toString(), Qt::ISODate);
                if (newTime != activeTimers[i].datetime) {
                    activeTimers[i].timer->stop();
                    activeTimers[i].timer->deleteLater();
                    activeTimers.remove(i);
                    found = false;
                }
                break;
            }
        }
        if (!found) {
            activeTimers[i].timer->stop();
            activeTimers[i].timer->deleteLater();
            activeTimers.remove(i);
        } else {
            ++i;
        }
    }

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();
        QDateTime dt = QDateTime::fromString(obj["datetime"].toString(), Qt::ISODate);
        QString title = obj["title"].toString();
        QString uuid = extractOrGenerateUUID(obj);

        if (!dt.isValid()) continue;

        bool alreadyScheduled = std::any_of(activeTimers.begin(), activeTimers.end(), [&](const CountdownEvent &e) {
            return e.uuid == uuid;
        });

        if (dt > now && !alreadyScheduled) {
            int msUntil = now.msecsTo(dt);
            if (msUntil > 0) {
                QTimer *t = new QTimer();
                t->setSingleShot(true);
                QObject::connect(t, &QTimer::timeout, [tray, title]() {
                    tray->showMessage("Countdown Finished", QString("'%1' has ended!").arg(title));
                });
                t->start(msUntil);
                activeTimers.append({uuid, title, dt, t});
            }
        } else if (dt <= now && !alreadyScheduled) {
            int secsAgo = dt.secsTo(now);
            if (secsAgo >= 0 && secsAgo < 60) {
                tray->showMessage("Countdown Finished", QString("'%1' has ended!").arg(title));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QSharedMemory singleInstance("NotifierSingletonKey");
    if (!singleInstance.create(1)) return 0;

    QSystemTrayIcon tray;
    tray.setToolTip("Event Countdown Notifier");
    tray.setIcon(QIcon::fromTheme("applications-system"));
    tray.show();

    QMenu trayMenu;
    QAction *quitAction = trayMenu.addAction("Quit");
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
    tray.setContextMenu(&trayMenu);

    QTimer refresher;
    QObject::connect(&refresher, &QTimer::timeout, [&tray]() {
        if (QTime::currentTime().second() == 0)
            updateCountdowns(&tray);
    });
    refresher.start(1000);

    return app.exec();
}
