#include "NoGemsConfig.h"
#include "Config.h"
#include "Log.h"

NoGemsConfig* NoGemsConfig::instance()
{
    static NoGemsConfig instance;
    return &instance;
}

void NoGemsConfig::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("NoGems.Enable", true);
    _disableGemEffects = sConfigMgr->GetOption<bool>("NoGems.DisableGemEffects", true);
    _disableSocketBonuses = sConfigMgr->GetOption<bool>("NoGems.DisableSocketBonuses", true);
    _blockSocketing = sConfigMgr->GetOption<bool>("NoGems.BlockSocketing", true);
    _replacementEnabled = sConfigMgr->GetOption<bool>("NoGems.Replacement.Enable", true);
    _allowProfessionExclusive = sConfigMgr->GetOption<bool>("NoGems.Replacement.AllowProfessionExclusive", false);
    _minItemQuality = sConfigMgr->GetOption<uint32>("NoGems.Replacement.MinItemQuality", 2);
    _minItemLevel = sConfigMgr->GetOption<uint32>("NoGems.Replacement.MinItemLevel", 1);
    _maxItemLevel = sConfigMgr->GetOption<uint32>("NoGems.Replacement.MaxItemLevel", 0);
    _debugLogging = sConfigMgr->GetOption<bool>("NoGems.DebugLogging", false);

    LOG_INFO("server.loading", "mod-no-gems: Configuration loaded. Enabled={}, DisableGemEffects={}, DisableSocketBonuses={}, BlockSocketing={}, Replacement={}",
        _enabled, _disableGemEffects, _disableSocketBonuses, _blockSocketing, _replacementEnabled);
}
