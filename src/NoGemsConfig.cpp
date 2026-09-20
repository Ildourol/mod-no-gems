#include "NoGemsConfig.h"
#include "Config.h"

NoGemsConfig* NoGemsConfig::instance()
{
    static NoGemsConfig instance;
    return &instance;
}

void NoGemsConfig::Load()
{
    Enable                   = sConfigMgr->GetOption<bool>("ModNoGems.Enable", true);
    ConvertLegacyGearOnLogin = sConfigMgr->GetOption<bool>("ModNoGems.ConvertLegacyGearOnLogin", true);
    BlockGemUse              = sConfigMgr->GetOption<bool>("ModNoGems.BlockGemUse", true);
    StatMultiplier           = sConfigMgr->GetOption<float>("ModNoGems.StatMultiplier", 0.5f);
    MetaMultiplier           = sConfigMgr->GetOption<float>("ModNoGems.MetaMultiplier", 1.0f);
    SocketBonusMultiplier    = sConfigMgr->GetOption<float>("ModNoGems.SocketBonusMultiplier", 0.5f);
    IncludeSocketBonus       = sConfigMgr->GetOption<bool>("ModNoGems.IncludeSocketBonus", true);
    Debug                    = sConfigMgr->GetOption<bool>("ModNoGems.Debug", false);
}
