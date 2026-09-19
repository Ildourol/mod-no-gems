#ifndef MOD_NO_GEMS_CONFIG_H
#define MOD_NO_GEMS_CONFIG_H

#include "Common.h"

class NoGemsConfig
{
public:
    static NoGemsConfig* instance();

    void LoadConfig();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsDisableGemEffects() const { return _enabled && _disableGemEffects; }
    [[nodiscard]] bool IsDisableSocketBonuses() const { return _enabled && _disableSocketBonuses; }
    [[nodiscard]] bool IsBlockSocketing() const { return _enabled && _blockSocketing; }
    [[nodiscard]] bool IsReplacementEnabled() const { return _enabled && _replacementEnabled; }
    [[nodiscard]] bool IsAllowProfessionExclusive() const { return _allowProfessionExclusive; }
    [[nodiscard]] uint32 GetMinItemQuality() const { return _minItemQuality; }
    [[nodiscard]] uint32 GetMinItemLevel() const { return _minItemLevel; }
    [[nodiscard]] uint32 GetMaxItemLevel() const { return _maxItemLevel; }
    [[nodiscard]] bool IsDebugLogging() const { return _debugLogging; }

private:
    NoGemsConfig() = default;

    bool _enabled{true};
    bool _disableGemEffects{true};
    bool _disableSocketBonuses{true};
    bool _blockSocketing{true};
    bool _replacementEnabled{true};
    bool _allowProfessionExclusive{false};
    uint32 _minItemQuality{2};
    uint32 _minItemLevel{1};
    uint32 _maxItemLevel{0};
    bool _debugLogging{false};
};

#define sNoGemsConfig NoGemsConfig::instance()

#endif // MOD_NO_GEMS_CONFIG_H
