#include <QCoreApplication.h>
#include <QEvent>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include "Args.h"
#include "HaloManager.h"
#include "Options.h"

class QuitEvent : public QEvent
{
public:
    QuitEvent()
        : QEvent(static_cast<QEvent::Type>(QEvent::User + 1))
    {
    }
};

class QuitEventFilter : public QObject
{
public:
    QuitEventFilter(HaloManager* manager, QObject* parent)
        : QObject(parent), mManager(manager)
    {
    }

protected:
    virtual bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (ev->type() == QEvent::User + 1) {
            qDebug() << "would like to quit";
            mManager->quit();
            return true;
        }
        return false;
    }

private:
    HaloManager* mManager;
};

static void sigHandler(int sig)
{
    QCoreApplication::postEvent(QCoreApplication::instance(), new QuitEvent());
}

int main(int argc, char** argv, char** envp)
{
    signal(SIGINT, sigHandler);

    auto args = args::Parser::parse(argc, argv, envp, "HALO_", [](const char* msg, size_t offset, char* arg) {
        fprintf(stderr, "%s: %zu (%s)", msg, offset, arg);
        exit(1);
    });

    QCoreApplication app(argc, argv);

    Options options;
    options.locations = args.value<QString>("locations");
    options.devices = args.value<QString>("devices");
    options.mqttUser = args.value<QString>("mqtt-user");
    options.mqttPassword = args.value<QString>("mqtt-password");
    options.mqttHost = args.value<QString>("mqtt-host");

    if (options.locations.isEmpty()) {
        fprintf(stderr, "No --locations\n");
        exit(1);
    } else if (options.devices.isEmpty()) {
        fprintf(stderr, "No --devices\n");
        exit(1);
    } else if (options.mqttUser.isEmpty()) {
        fprintf(stderr, "No --mqtt-user\n");
        exit(1);
    } else if (options.mqttPassword.isEmpty()) {
        fprintf(stderr, "No --mqtt-password\n");
        exit(1);
    } else if (options.mqttHost.isEmpty()) {
        fprintf(stderr, "No --mqtt-host\n");
        exit(1);
    }

    HaloManager haloMqtt(std::move(options));
    app.installEventFilter(new QuitEventFilter(&haloMqtt, &app));

    return app.exec();
}
