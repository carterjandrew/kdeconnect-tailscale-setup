// SPDX-License-Identifier: GPL-2.0-or-later
#include "core.h"
#include "kdeconnectclient.h"
#include "state.h"
#include "tailscaleclient.h"

#include <QCoreApplication>
#include <QTextStream>

namespace
{
QTextStream out(stdout);
QTextStream err(stderr);

void printUsage()
{
    out << "Usage: kdeconnect-tailscale <command> [peer]\n\n"
           "Commands:\n"
           "  peers          List Android peers visible to the local Tailscale client\n"
           "  add [peer]     Add a peer, prompting when no selector is supplied\n"
           "  repair         Refresh managed addresses and trigger KDE Connect discovery\n"
           "  remove <peer>  Stop managing a peer and remove its address from KDE Connect\n"
           "  doctor         Check the local Tailscale and KDE Connect integration\n";
}

QString peerLabel(const TailPeer &peer)
{
    QString label = peer.hostName;
    if (label.isEmpty()) {
        label = peer.dnsName;
    }
    if (label.isEmpty()) {
        label = peer.stableId;
    }
    return label;
}

void printPeers(const QList<TailPeer> &peers)
{
    if (peers.isEmpty()) {
        out << "No Android peers are visible in this tailnet.\n";
        return;
    }
    for (qsizetype index = 0; index < peers.size(); ++index) {
        const TailPeer &peer = peers.at(index);
        out << (index + 1) << ". " << peerLabel(peer) << "  "
            << (peer.ipv4.isEmpty() ? QStringLiteral("no supported IPv4 address") : peer.ipv4) << "  "
            << (peer.online ? QStringLiteral("online") : QStringLiteral("offline")) << '\n';
    }
}

bool readStatus(TailStatus *status)
{
    QString error;
    *status = TailscaleClient().status(&error);
    if (!error.isEmpty()) {
        err << "Error: " << error << '\n';
        return false;
    }
    if (status->backendState.compare(QStringLiteral("Running"), Qt::CaseInsensitive) != 0) {
        err << "Error: Tailscale is not running (backend state: "
            << (status->backendState.isEmpty() ? QStringLiteral("unknown") : status->backendState) << ").\n";
        return false;
    }
    return true;
}

bool readState(const StateStore &store, QList<ManagedPeer> *peers)
{
    QString error;
    *peers = store.load(&error);
    if (!error.isEmpty()) {
        err << "Error: " << error << '\n';
        return false;
    }
    return true;
}

bool connectKde(KdeConnectClient *client)
{
    QString error;
    if (!client->ensureAvailable(&error)) {
        err << "Error: " << error << '\n';
        return false;
    }
    return true;
}

bool applyState(const QList<ManagedPeer> &managed,
                const QStringList &addresses,
                const QStringList &previousAddresses,
                const StateStore &store,
                KdeConnectClient *client)
{
    QString error;
    if (!client->setCustomDevices(addresses, &error)) {
        err << "Error: " << error << '\n';
        return false;
    }
    if (!store.save(managed, &error)) {
        QString ignored;
        client->setCustomDevices(previousAddresses, &ignored);
        err << "Error: " << error << " (the KDE Connect change was rolled back)\n";
        return false;
    }
    if (!client->triggerDiscovery(&error)) {
        err << "Warning: settings were saved, but discovery could not be triggered: " << error << '\n';
        return false;
    }
    return true;
}

int repair(const TailStatus &status, const StateStore &store, KdeConnectClient *client)
{
    QList<ManagedPeer> managed;
    if (!readState(store, &managed)) {
        return 1;
    }
    if (managed.isEmpty()) {
        err << "No managed peers. Use 'kdeconnect-tailscale add' first.\n";
        return 1;
    }
    if (!connectKde(client)) {
        return 1;
    }
    QString error;
    const QStringList existing = client->customDevices(&error);
    if (!error.isEmpty()) {
        err << "Error: " << error << '\n';
        return 1;
    }
    const ReconcileResult result = reconcilePeers(managed, status.peers, existing);
    if (!applyState(result.managedPeers, result.customDevices, existing, store, client)) {
        return 1;
    }
    out << "KDE Connect discovery was triggered for " << result.managedPeers.size() << " managed peer(s).\n";
    for (const QString &name : result.unavailablePeers) {
        out << "Warning: " << name << " was not in the current peer map; its last address was retained.\n";
    }
    out << "Open KDE Connect on both devices and approve the normal pairing request if needed.\n";
    return 0;
}

int addPeer(const QString &selector, const TailStatus &status, const StateStore &store, KdeConnectClient *client)
{
    const QList<TailPeer> candidates = androidPeers(status.peers);
    TailPeer selected;
    if (selector.isEmpty()) {
        printPeers(candidates);
        if (candidates.isEmpty()) {
            return 1;
        }
        out << "Select a peer number: " << Qt::flush;
        QTextStream input(stdin);
        bool ok = false;
        const int choice = input.readLine().trimmed().toInt(&ok);
        if (!ok || choice < 1 || choice > candidates.size()) {
            err << "Error: invalid selection.\n";
            return 2;
        }
        selected = candidates.at(choice - 1);
    } else {
        const QList<TailPeer> matches = findPeers(candidates, selector);
        if (matches.isEmpty()) {
            err << "Error: no Android peer matches '" << selector << "'.\n";
            return 1;
        }
        if (matches.size() != 1) {
            err << "Error: the selector is ambiguous; use the peer's Tailscale IPv4 address.\n";
            return 2;
        }
        selected = matches.first();
    }
    if (selected.ipv4.isEmpty()) {
        err << "Error: " << peerLabel(selected) << " has no IPv4 address in Tailscale's 100.64.0.0/10 range.\n";
        return 1;
    }

    QList<ManagedPeer> managed;
    if (!readState(store, &managed)) {
        return 1;
    }
    ManagedPeer added{selected.stableId, selected.hostName, selected.dnsName, selected.ipv4};
    bool replaced = false;
    for (ManagedPeer &peer : managed) {
        if (peer.stableId == added.stableId) {
            peer = added;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        managed.append(added);
    }
    if (!connectKde(client)) {
        return 1;
    }
    QString error;
    const QStringList existing = client->customDevices(&error);
    if (!error.isEmpty()) {
        err << "Error: " << error << '\n';
        return 1;
    }
    const ReconcileResult result = reconcilePeers(managed, status.peers, existing);
    if (!applyState(result.managedPeers, result.customDevices, existing, store, client)) {
        return 1;
    }
    out << (replaced ? "Updated " : "Added ") << peerLabel(selected) << " at " << selected.ipv4 << ".\n"
        << "KDE Connect discovery was triggered. Approve the normal pairing request on both devices.\n";
    return 0;
}

int removePeer(const QString &selector, const StateStore &store, KdeConnectClient *client)
{
    if (selector.isEmpty()) {
        err << "Error: remove requires a peer hostname, DNS name, ID, or address.\n";
        return 2;
    }
    QList<ManagedPeer> managed;
    if (!readState(store, &managed)) {
        return 1;
    }
    QList<ManagedPeer> remaining;
    QStringList removedAddresses;
    for (const ManagedPeer &peer : managed) {
        if (peer.stableId == selector || peer.address == selector || peer.hostName.compare(selector, Qt::CaseInsensitive) == 0
            || peer.dnsName.compare(selector, Qt::CaseInsensitive) == 0) {
            removedAddresses.append(peer.address);
        } else {
            remaining.append(peer);
        }
    }
    if (removedAddresses.isEmpty()) {
        err << "Error: no managed peer matches '" << selector << "'.\n";
        return 1;
    }
    if (!connectKde(client)) {
        return 1;
    }
    QString error;
    const QStringList existing = client->customDevices(&error);
    if (!error.isEmpty()) {
        err << "Error: " << error << '\n';
        return 1;
    }
    QStringList updated = existing;
    for (const QString &address : removedAddresses) {
        updated.removeAll(address);
    }
    if (!applyState(remaining, updated, existing, store, client)) {
        return 1;
    }
    out << "Removed " << removedAddresses.size() << " managed peer(s) and triggered discovery.\n";
    return 0;
}

int doctor(const StateStore &store, KdeConnectClient *client)
{
    bool healthy = true;
    TailStatus status;
    QString error;
    status = TailscaleClient().status(&error);
    if (!error.isEmpty()) {
        out << "[FAIL] Tailscale: " << error << '\n';
        healthy = false;
    } else if (status.backendState.compare(QStringLiteral("Running"), Qt::CaseInsensitive) != 0) {
        out << "[FAIL] Tailscale backend state: " << status.backendState << '\n';
        healthy = false;
    } else {
        out << "[ OK ] Tailscale is running; " << androidPeers(status.peers).size() << " Android peer(s) visible.\n";
    }

    if (!client->ensureAvailable(&error)) {
        out << "[FAIL] KDE Connect: " << error << '\n';
        healthy = false;
    } else {
        out << "[ OK ] KDE Connect D-Bus interface is available.\n";
        const QStringList providers = client->linkProviders(&error);
        if (!error.isEmpty()) {
            out << "[FAIL] LAN provider check: " << error << '\n';
            healthy = false;
        } else {
            bool lanEnabled = false;
            for (const QString &provider : providers) {
                lanEnabled = lanEnabled || provider.compare(QStringLiteral("LAN|lan|enabled"), Qt::CaseInsensitive) == 0;
            }
            out << (lanEnabled ? "[ OK ]" : "[FAIL]") << " KDE Connect LAN provider is "
                << (lanEnabled ? "enabled" : "disabled or missing") << ".\n";
            healthy = healthy && lanEnabled;
        }
    }

    const QList<ManagedPeer> managed = store.load(&error);
    if (!error.isEmpty()) {
        out << "[FAIL] State: " << error << '\n';
        healthy = false;
    } else {
        out << "[ OK ] " << managed.size() << " peer(s) managed in " << store.path() << ".\n";
    }
    out << "[INFO] Tailnet policy and the local firewall must permit TCP and UDP 1714-1764 in both directions.\n";
    return healthy ? 0 : 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kdeconnect-tailscale"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    const QStringList arguments = application.arguments().mid(1);
    if (arguments.isEmpty() || arguments.first() == QStringLiteral("--help") || arguments.first() == QStringLiteral("-h")) {
        printUsage();
        return arguments.isEmpty() ? 2 : 0;
    }
    if (arguments.first() == QStringLiteral("--version")) {
        out << QCoreApplication::applicationName() << ' ' << QCoreApplication::applicationVersion() << '\n';
        return 0;
    }

    const QString command = arguments.first();
    const QString selector = arguments.value(1);
    StateStore store;
    KdeConnectClient kdeConnect;

    if (command == QStringLiteral("peers")) {
        TailStatus status;
        if (!readStatus(&status)) {
            return 1;
        }
        printPeers(androidPeers(status.peers));
        return 0;
    }
    if (command == QStringLiteral("add")) {
        TailStatus status;
        if (!readStatus(&status)) {
            return 1;
        }
        return addPeer(selector, status, store, &kdeConnect);
    }
    if (command == QStringLiteral("repair")) {
        TailStatus status;
        if (!readStatus(&status)) {
            return 1;
        }
        return repair(status, store, &kdeConnect);
    }
    if (command == QStringLiteral("remove")) {
        return removePeer(selector, store, &kdeConnect);
    }
    if (command == QStringLiteral("doctor")) {
        return doctor(store, &kdeConnect);
    }

    err << "Error: unknown command '" << command << "'.\n";
    printUsage();
    return 2;
}
