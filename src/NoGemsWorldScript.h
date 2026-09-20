#ifndef _NO_GEMS_WORLD_SCRIPT_H
#define _NO_GEMS_WORLD_SCRIPT_H

#include "ScriptMgr.h"

class NoGemsWorldScript : public WorldScript
{
public:
    NoGemsWorldScript();

    void OnStartup() override;
    void OnAfterConfigLoad(bool reload) override;
};

void AddNoGemsWorldScripts();

#endif // _NO_GEMS_WORLD_SCRIPT_H
