#ifndef _NO_GEMS_COMMON_H
#define _NO_GEMS_COMMON_H

#include "Common.h"
#include "SharedDefines.h"
#include "ItemTemplate.h"
#include <string>
#include <vector>

enum NoGemsSocketColor : uint32
{
    NOGEMS_SOCKET_COLOR_META   = 1,
    NOGEMS_SOCKET_COLOR_RED    = 2,
    NOGEMS_SOCKET_COLOR_YELLOW = 4,
    NOGEMS_SOCKET_COLOR_BLUE   = 8
};

struct StatTierBudget
{
    int32 primary;
    int32 stamina;
    int32 ap;
    int32 sp;
    int32 rating;
    int32 meta_primary;
    int32 meta_stamina;
    int32 meta_ap;
    int32 meta_sp;
    int32 meta_rating;
};

struct SocketAffixEntry
{
    uint8 socketIndex;
    uint32 socketColor;
    uint32 itemModType;
    int32 amount;
    std::string statName;
};

#endif // _NO_GEMS_COMMON_H
