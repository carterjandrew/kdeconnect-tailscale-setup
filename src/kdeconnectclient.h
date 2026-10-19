// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QStringList>

class KdeConnectClient
{
public:
    bool ensureAvailable(QString *error) const;
    QStringList customDevices(QString *error) const;
    bool setCustomDevices(const QStringList &addresses, QString *error) const;
    QStringList linkProviders(QString *error) const;
    bool triggerDiscovery(QString *error) const;
};
