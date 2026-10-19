// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core.h"

class StateStore
{
public:
    explicit StateStore(QString path = {});

    QString path() const;
    QList<ManagedPeer> load(QString *error) const;
    bool save(const QList<ManagedPeer> &peers, QString *error) const;

private:
    QString m_path;
};
