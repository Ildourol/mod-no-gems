#ifndef _NO_GEMS_FORMULA_H
#define _NO_GEMS_FORMULA_H

#include "NoGemsCommon.h"
#include "ItemTemplate.h"
#include <string>
#include <vector>

namespace NoGemsFormula
{
    bool HasSockets(ItemTemplate const* proto);
    std::string GetSocketSignature(ItemTemplate const* proto);
    char GetColorLetter(uint32 colorMask);
    std::string GetItemModTypeName(uint32 modType);

    bool ItemHasStat(ItemTemplate const* proto, uint32 modType);
    uint32 SelectSynergisticModType(ItemTemplate const* proto, uint32 socketColor);
    int32 CalculateStatAmount(uint32 modType, uint32 itemLevel, bool isMeta);

    bool ApplyNoGems(ItemTemplate* proto);
}

#endif // _NO_GEMS_FORMULA_H
