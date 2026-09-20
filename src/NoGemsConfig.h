#ifndef _NO_GEMS_CONFIG_H
#define _NO_GEMS_CONFIG_H

#include "Common.h"

class NoGemsConfig
{
public:
    static NoGemsConfig* instance();

    void Load();

    bool Enable{true};
    bool ConvertLegacyGearOnLogin{true};
    bool BlockGemUse{true};
    float StatMultiplier{0.5f};
    float MetaMultiplier{1.0f};
    float SocketBonusMultiplier{0.5f};
    bool IncludeSocketBonus{true};
    bool Debug{false};

private:
    NoGemsConfig() = default;
};

#define sNoGemsConfig NoGemsConfig::instance()

#endif // _NO_GEMS_CONFIG_H
