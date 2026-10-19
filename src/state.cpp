// SPDX-License-Identifier: GPL-2.0-or-later
#include "state.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

StateStore::StateStore(QString path)
    : m_path(std::move(path))
{
    if (m_path.isEmpty()) {
        m_path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/kdeconnect-tailscale/state.json");
    }
}

QString StateStore::path() const
{
    return m_path;
}

QList<ManagedPeer> StateStore::load(QString *error) const
{
    if (error) {
        error->clear();
    }
    QFile file(m_path);
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read %1: %2").arg(m_path, file.errorString());
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid state file %1: %2").arg(m_path, parseError.errorString());
        }
        return {};
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("managedPeers")).isArray()) {
        if (error) {
            *error = QStringLiteral("Unsupported state file format in %1").arg(m_path);
        }
        return {};
    }

    QList<ManagedPeer> peers;
    for (const QJsonValue &value : root.value(QStringLiteral("managedPeers")).toArray()) {
        const QJsonObject object = value.toObject();
        ManagedPeer peer;
        peer.stableId = object.value(QStringLiteral("stableId")).toString();
        peer.hostName = object.value(QStringLiteral("hostName")).toString();
        peer.dnsName = object.value(QStringLiteral("dnsName")).toString();
        peer.address = object.value(QStringLiteral("address")).toString();
        if (!peer.stableId.isEmpty() && isTailscaleIpv4(peer.address)) {
            peers.append(peer);
        }
    }
    return peers;
}

bool StateStore::save(const QList<ManagedPeer> &peers, QString *error) const
{
    if (error) {
        error->clear();
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create the state directory for %1").arg(m_path);
        }
        return false;
    }

    QJsonArray array;
    for (const ManagedPeer &peer : peers) {
        array.append(QJsonObject{{QStringLiteral("stableId"), peer.stableId},
                                 {QStringLiteral("hostName"), peer.hostName},
                                 {QStringLiteral("dnsName"), peer.dnsName},
                                 {QStringLiteral("address"), peer.address}});
    }
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("managedPeers"), array}};

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0 || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not write %1: %2").arg(m_path, file.errorString());
        }
        return false;
    }
    return true;
}
