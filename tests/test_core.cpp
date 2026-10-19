// SPDX-License-Identifier: GPL-2.0-or-later
#include "core.h"
#include "state.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class CoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesStatus();
    void rejectsBadSchema();
    void validatesTailscaleAddresses();
    void filtersAndSortsAndroidPeers();
    void findsPeersCaseInsensitively();
    void reconcilesAndPreservesExternalAddresses();
    void retainsUnavailablePeer();
    void stateRoundTrip();
};

void CoreTest::parsesStatus()
{
    const QByteArray json = R"({
        "BackendState":"Running",
        "Peer":{
            "nodekey:abc":{"HostName":"phone","DNSName":"phone.example.ts.net.","OS":"android","Online":true,
                           "TailscaleIPs":["fd7a:115c:a1e0::1","100.70.1.2"]},
            "nodekey:def":{"HostName":"desktop","OS":"linux","Online":false,"TailscaleIPs":["100.90.1.2"]}
        }
    })";
    QString error;
    const TailStatus status = parseTailStatus(json, &error);
    QCOMPARE(error, QString());
    QCOMPARE(status.backendState, QStringLiteral("Running"));
    QCOMPARE(status.peers.size(), 2);
    const QList<TailPeer> phones = androidPeers(status.peers);
    QCOMPARE(phones.size(), 1);
    QCOMPARE(phones.first().ipv4, QStringLiteral("100.70.1.2"));
    QCOMPARE(phones.first().stableId, QStringLiteral("nodekey:abc"));
}

void CoreTest::rejectsBadSchema()
{
    QString error;
    parseTailStatus(QByteArrayLiteral("{\"Peer\":[]}"), &error);
    QVERIFY(!error.isEmpty());
    parseTailStatus(QByteArrayLiteral("not json"), &error);
    QVERIFY(!error.isEmpty());
}

void CoreTest::validatesTailscaleAddresses()
{
    QVERIFY(isTailscaleIpv4(QStringLiteral("100.64.0.0")));
    QVERIFY(isTailscaleIpv4(QStringLiteral("100.127.255.255")));
    QVERIFY(!isTailscaleIpv4(QStringLiteral("100.128.0.0")));
    QVERIFY(!isTailscaleIpv4(QStringLiteral("192.168.1.2")));
    QVERIFY(!isTailscaleIpv4(QStringLiteral("fd7a:115c:a1e0::1")));
}

void CoreTest::filtersAndSortsAndroidPeers()
{
    const QList<TailPeer> peers{{QStringLiteral("1"), QStringLiteral("zeta"), {}, QStringLiteral("100.64.0.1"), QStringLiteral("android"), false},
                                {QStringLiteral("2"), QStringLiteral("beta"), {}, QStringLiteral("100.64.0.2"), QStringLiteral("Android"), true},
                                {QStringLiteral("3"), QStringLiteral("alpha"), {}, QStringLiteral("100.64.0.3"), QStringLiteral("linux"), true}};
    const QList<TailPeer> result = androidPeers(peers);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result.first().hostName, QStringLiteral("beta"));
}

void CoreTest::findsPeersCaseInsensitively()
{
    const QList<TailPeer> peers{{QStringLiteral("nodekey:1"), QStringLiteral("Phone"), QStringLiteral("phone.tail.ts.net."), QStringLiteral("100.64.1.2"), QStringLiteral("android"), true}};
    QCOMPARE(findPeers(peers, QStringLiteral("phone")).size(), 1);
    QCOMPARE(findPeers(peers, QStringLiteral("PHONE.TAIL.TS.NET.")).size(), 1);
    QCOMPARE(findPeers(peers, QStringLiteral("100.64.1.2")).size(), 1);
}

void CoreTest::reconcilesAndPreservesExternalAddresses()
{
    const QList<ManagedPeer> managed{{QStringLiteral("nodekey:1"), QStringLiteral("phone"), {}, QStringLiteral("100.64.1.1")}};
    const QList<TailPeer> available{{QStringLiteral("nodekey:1"), QStringLiteral("phone"), QStringLiteral("phone.ts.net."), QStringLiteral("100.64.1.2"), QStringLiteral("android"), true}};
    const ReconcileResult result = reconcilePeers(managed, available, {QStringLiteral("192.168.1.5"), QStringLiteral("100.64.1.1")});
    QCOMPARE(result.managedPeers.first().address, QStringLiteral("100.64.1.2"));
    QCOMPARE(result.customDevices, QStringList({QStringLiteral("192.168.1.5"), QStringLiteral("100.64.1.2")}));
    QVERIFY(result.unavailablePeers.isEmpty());
}

void CoreTest::retainsUnavailablePeer()
{
    const QList<ManagedPeer> managed{{QStringLiteral("nodekey:1"), QStringLiteral("phone"), {}, QStringLiteral("100.64.1.1")}};
    const ReconcileResult result = reconcilePeers(managed, {}, {QStringLiteral("100.64.1.1")});
    QCOMPARE(result.customDevices, QStringList({QStringLiteral("100.64.1.1")}));
    QCOMPARE(result.unavailablePeers, QStringList({QStringLiteral("phone")}));
}

void CoreTest::stateRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StateStore store(directory.filePath(QStringLiteral("nested/state.json")));
    const QList<ManagedPeer> peers{{QStringLiteral("nodekey:1"), QStringLiteral("phone"), QStringLiteral("phone.ts.net."), QStringLiteral("100.64.1.2")}};
    QString error;
    QVERIFY(store.save(peers, &error));
    QCOMPARE(error, QString());
    const QList<ManagedPeer> loaded = store.load(&error);
    QCOMPARE(error, QString());
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().stableId, peers.first().stableId);
    QCOMPARE(loaded.first().address, peers.first().address);
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
