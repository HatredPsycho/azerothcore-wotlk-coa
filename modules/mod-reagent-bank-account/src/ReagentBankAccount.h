// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef AZEROTHCORE_REAGENTBANKACCOUNT_H
#define AZEROTHCORE_REAGENTBANKACCOUNT_H

#include "Define.h"

#include <cstdint>

static constexpr uint32 DEFAULT_REAGENT_BANK_ITEMS_PER_PAGE = 12;
static constexpr uint32 MAX_REAGENT_BANK_PAGE_NUMBER = 700;

extern uint32 g_reagentBankMaxItemsPerPage;
extern bool g_accountWideReagentBank;
extern bool g_reagentBankEnabled;

#endif // AZEROTHCORE_REAGENTBANKACCOUNT_H
