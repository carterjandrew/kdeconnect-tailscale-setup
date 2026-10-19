// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

struct TailPeer {
    QString stableId;
    QString hostName;
    QString dnsName;
    QString ipv4;
    QString os;
    bool online = false;
};

struct ManagedPeer {
    QString stableId;
    QString hostName;
    QString dnsName;
    QString address;
};

struct TailStatus {
    QString backendState;
    QList<TailPeer> peers;
};

struct ReconcileResult {
    QList<ManagedPeer> managedPeers;
    QStringList customDevices;
    QStringList unavailablePeers;
};

TailStatus parseTailStatus(const QByteArray &json, QString *error);
bool isTailscaleIpv4(const QString &address);
QList<TailPeer> androidPeers(const QList<TailPeer> &peers);
QList<TailPeer> findPeers(const QList<TailPeer> &peers, const QString &selector);
ReconcileResult reconcilePeers(const QList<ManagedPeer> &managedPeers,
                               const QList<TailPeer> &availablePeers,
                               const QStringList &existingCustomDevices);
