#include "NoGemsWorldScript.h"
#include "NoGemsConfig.h"
#include "NoGemsRegistry.h"
#include "Log.h"

NoGemsWorldScript::NoGemsWorldScript()
    : WorldScript("NoGemsWorldScript", {
        WORLDHOOK_ON_STARTUP,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    })
{
}

void NoGemsWorldScript::OnStartup()
{
    LOG_INFO("server.loading", ">> Initializing mod-no-gems...");

    sNoGemsConfig->Load();

    if (!sNoGemsConfig->Enable)
    {
        LOG_INFO("server.loading", ">> mod-no-gems is disabled in configuration.");
        return;
    }

    sNoGemsRegistry->Initialize();

    LOG_INFO("server.loading", ">> mod-no-gems initialized successfully.");
}

void NoGemsWorldScript::OnAfterConfigLoad(bool reload)
{
    if (reload)
    {
        sNoGemsConfig->Load();
    }
}

void AddNoGemsWorldScripts()
{
    new NoGemsWorldScript();
}
