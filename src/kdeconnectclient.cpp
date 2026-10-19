// SPDX-License-Identifier: GPL-2.0-or-later
#include "kdeconnectclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

namespace
{
constexpr auto service = "org.kde.kdeconnect";
constexpr auto path = "/modules/kdeconnect";
constexpr auto interface = "org.kde.kdeconnect.daemon";

QDBusInterface daemonInterface()
{
    return QDBusInterface(QString::fromLatin1(service), QString::fromLatin1(path), QString::fromLatin1(interface), QDBusConnection::sessionBus());
}
}

bool KdeConnectClient::ensureAvailable(QString *error) const
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        if (error) {
            *error = QStringLiteral("Could not connect to the user session D-Bus");
        }
        return false;
    }
    auto *bus = QDBusConnection::sessionBus().interface();
    if (!bus->isServiceRegistered(QString::fromLatin1(service))) {
        const QDBusReply<void> reply = bus->startService(QString::fromLatin1(service));
        if (!reply.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not start KDE Connect: %1").arg(reply.error().message());
            }
            return false;
        }
    }
    QDBusInterface daemon = daemonInterface();
    if (!daemon.isValid()) {
        if (error) {
            *error = QStringLiteral("KDE Connect's daemon D-Bus interface is unavailable: %1").arg(daemon.lastError().message());
        }
        return false;
    }
    return true;
}

QStringList KdeConnectClient::customDevices(QString *error) const
{
    QDBusInterface daemon = daemonInterface();
    const QVariant value = daemon.property("customDevices");
    if (!value.isValid()) {
        if (error) {
            *error = QStringLiteral("Could not read KDE Connect customDevices: %1").arg(daemon.lastError().message());
        }
        return {};
    }
    return value.toStringList();
}

bool KdeConnectClient::setCustomDevices(const QStringList &addresses, QString *error) const
{
    QDBusInterface daemon = daemonInterface();
    if (!daemon.setProperty("customDevices", addresses)) {
        if (error) {
            *error = QStringLiteral("Could not update KDE Connect customDevices: %1").arg(daemon.lastError().message());
        }
        return false;
    }
    return true;
}

QStringList KdeConnectClient::linkProviders(QString *error) const
{
    QDBusInterface daemon = daemonInterface();
    const QDBusReply<QStringList> reply = daemon.call(QStringLiteral("linkProviders"));
    if (!reply.isValid()) {
        if (error) {
            *error = QStringLiteral("Could not read KDE Connect link providers: %1").arg(reply.error().message());
        }
        return {};
    }
    return reply.value();
}

bool KdeConnectClient::triggerDiscovery(QString *error) const
{
    QDBusInterface daemon = daemonInterface();
    const QDBusReply<void> reply = daemon.call(QStringLiteral("forceOnNetworkChange"));
    if (!reply.isValid()) {
        if (error) {
            *error = QStringLiteral("Could not trigger KDE Connect discovery: %1").arg(reply.error().message());
        }
        return false;
    }
    return true;
}
