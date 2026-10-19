// SPDX-License-Identifier: GPL-2.0-or-later
#include "core.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace
{
QString firstTailscaleIpv4(const QJsonArray &addresses)
{
    for (const QJsonValue &value : addresses) {
        const QString address = value.toString();
        if (isTailscaleIpv4(address)) {
            return address;
        }
    }
    return {};
}

QString displayName(const ManagedPeer &peer)
{
    if (!peer.hostName.isEmpty()) {
        return peer.hostName;
    }
    if (!peer.dnsName.isEmpty()) {
        return peer.dnsName;
    }
    return peer.stableId;
}

const TailPeer *matchManagedPeer(const ManagedPeer &managed, const QList<TailPeer> &available)
{
    for (const TailPeer &peer : available) {
        if (!managed.stableId.isEmpty() && peer.stableId == managed.stableId) {
            return &peer;
        }
    }

    QList<const TailPeer *> matches;
    for (const TailPeer &peer : available) {
        if ((!managed.dnsName.isEmpty() && peer.dnsName.compare(managed.dnsName, Qt::CaseInsensitive) == 0)
            || (!managed.hostName.isEmpty() && peer.hostName.compare(managed.hostName, Qt::CaseInsensitive) == 0)) {
            matches.append(&peer);
        }
    }
    return matches.size() == 1 ? matches.first() : nullptr;
}
}

TailStatus parseTailStatus(const QByteArray &json, QString *error)
{
    if (error) {
        error->clear();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON from tailscale status: %1").arg(parseError.errorString());
        }
        return {};
    }

    const QJsonObject root = document.object();
    TailStatus status;
    status.backendState = root.value(QStringLiteral("BackendState")).toString();

    const QJsonValue peerValue = root.value(QStringLiteral("Peer"));
    if (!peerValue.isUndefined() && !peerValue.isObject()) {
        if (error) {
            *error = QStringLiteral("Unsupported tailscale status schema: Peer is not an object");
        }
        return {};
    }

    const QJsonObject peers = peerValue.toObject();
    for (auto iterator = peers.constBegin(); iterator != peers.constEnd(); ++iterator) {
        if (!iterator.value().isObject()) {
            continue;
        }
        const QJsonObject object = iterator.value().toObject();
        TailPeer peer;
        peer.stableId = iterator.key();
        peer.hostName = object.value(QStringLiteral("HostName")).toString();
        peer.dnsName = object.value(QStringLiteral("DNSName")).toString();
        peer.ipv4 = firstTailscaleIpv4(object.value(QStringLiteral("TailscaleIPs")).toArray());
        peer.os = object.value(QStringLiteral("OS")).toString();
        peer.online = object.value(QStringLiteral("Online")).toBool(false);
        status.peers.append(peer);
    }

    return status;
}

bool isTailscaleIpv4(const QString &address)
{
    QHostAddress host(address);
    bool ok = false;
    const quint32 ipv4 = host.toIPv4Address(&ok);
    return ok && (ipv4 & 0xffc00000U) == 0x64400000U;
}

QList<TailPeer> androidPeers(const QList<TailPeer> &peers)
{
    QList<TailPeer> result;
    for (const TailPeer &peer : peers) {
        if (peer.os.compare(QStringLiteral("android"), Qt::CaseInsensitive) == 0) {
            result.append(peer);
        }
    }
    std::sort(result.begin(), result.end(), [](const TailPeer &left, const TailPeer &right) {
        if (left.online != right.online) {
            return left.online;
        }
        return left.hostName.compare(right.hostName, Qt::CaseInsensitive) < 0;
    });
    return result;
}

QList<TailPeer> findPeers(const QList<TailPeer> &peers, const QString &selector)
{
    QList<TailPeer> matches;
    for (const TailPeer &peer : peers) {
        if (peer.stableId == selector || peer.ipv4 == selector || peer.hostName.compare(selector, Qt::CaseInsensitive) == 0
            || peer.dnsName.compare(selector, Qt::CaseInsensitive) == 0) {
            matches.append(peer);
        }
    }
    return matches;
}

ReconcileResult reconcilePeers(const QList<ManagedPeer> &managedPeers,
                               const QList<TailPeer> &availablePeers,
                               const QStringList &existingCustomDevices)
{
    ReconcileResult result;
    QSet<QString> oldManagedAddresses;
    for (const ManagedPeer &managed : managedPeers) {
        if (!managed.address.isEmpty()) {
            oldManagedAddresses.insert(managed.address);
        }
    }

    for (const QString &address : existingCustomDevices) {
        if (!oldManagedAddresses.contains(address) && !result.customDevices.contains(address)) {
            result.customDevices.append(address);
        }
    }

    for (const ManagedPeer &managed : managedPeers) {
        ManagedPeer updated = managed;
        const TailPeer *available = matchManagedPeer(managed, availablePeers);
        if (available && !available->ipv4.isEmpty()) {
            updated.stableId = available->stableId;
            updated.hostName = available->hostName;
            updated.dnsName = available->dnsName;
            updated.address = available->ipv4;
        } else {
            result.unavailablePeers.append(displayName(managed));
        }
        result.managedPeers.append(updated);
        if (!updated.address.isEmpty() && !result.customDevices.contains(updated.address)) {
            result.customDevices.append(updated.address);
        }
    }
    return result;
}
