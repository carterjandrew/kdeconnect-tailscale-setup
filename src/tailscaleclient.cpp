// SPDX-License-Identifier: GPL-2.0-or-later
#include "tailscaleclient.h"

#include <QProcess>
#include <QStandardPaths>

TailStatus TailscaleClient::status(QString *error) const
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("tailscale"));
    if (executable.isEmpty()) {
        if (error) {
            *error = QStringLiteral("The tailscale CLI was not found on PATH");
        }
        return {};
    }

    QProcess process;
    process.start(executable, {QStringLiteral("status"), QStringLiteral("--json")});
    if (!process.waitForStarted(3000) || !process.waitForFinished(10000)) {
        process.kill();
        if (error) {
            *error = QStringLiteral("Timed out while running tailscale status --json");
        }
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString details = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = details.isEmpty() ? QStringLiteral("tailscale status failed") : details;
        }
        return {};
    }
    return parseTailStatus(process.readAllStandardOutput(), error);
}
