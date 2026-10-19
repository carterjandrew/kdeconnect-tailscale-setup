// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core.h"

class TailscaleClient
{
public:
    TailStatus status(QString *error) const;
};
