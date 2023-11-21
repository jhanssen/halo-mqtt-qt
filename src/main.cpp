#include <QCoreApplication.h>
#include <cstdio>
#include <cstdlib>
#include "Args.h"
#include "HaloMqtt.h"
#include "Options.h"

int main(int argc, char** argv, char** envp)
{
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

    HaloMqtt haloMqtt(std::move(options));

    return app.exec();
}
