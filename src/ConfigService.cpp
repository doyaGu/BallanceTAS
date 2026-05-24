#include "ConfigService.h"
#include "EventBus.h"
#include "GameEvents.h"

void ConfigService::RegisterProperties(IConfig *config) {
    // --- TAS ---
    m_Enabled = config->GetProperty("TAS", "Enable");
    m_Enabled->SetComment("Enables TAS features.");
    m_Enabled->SetDefaultBoolean(true);

    m_Validation = config->GetProperty("TAS", "Validation");
    m_Validation->SetComment(
        "Enables validation recording for script playback. "
        "Records actual input during playback for verification.");
    m_Validation->SetDefaultBoolean(false);

    m_AutoRestart = config->GetProperty("TAS", "AutoRestart");
    m_AutoRestart->SetComment("Automatically restarts the current project when the same level is loaded again.");
    m_AutoRestart->SetDefaultBoolean(false);

    m_StopOnFinish = config->GetProperty("TAS", "StopOnFinish");
    m_StopOnFinish->SetComment("Automatically stops TAS playback/recording when the level ends.");
    m_StopOnFinish->SetDefaultBoolean(true);

    // --- Hotkeys ---
    m_StopKey = config->GetProperty("Hotkeys", "StopKey");
    m_StopKey->SetComment("Key for stopping TAS playback or recording");
    m_StopKey->SetDefaultKey(CKKEY_F3);

    // --- Recording ---
    m_RecordingMaxFrames = config->GetProperty("Recording", "MaxFrames");
    m_RecordingMaxFrames->SetComment("Maximum frames to record (prevents memory issues)");
    m_RecordingMaxFrames->SetDefaultInteger(1000000);

    // --- Startup Script ---
    m_StartupScriptEnabled = config->GetProperty("Startup", "Enabled");
    m_StartupScriptEnabled->SetComment("Enable global TAS projects with trigger = startup/menu/level");
    m_StartupScriptEnabled->SetDefaultBoolean(true);

    m_StartupScriptProject = config->GetProperty("Startup", "Project");
    m_StartupScriptProject->SetComment("Optional TAS project name to run for startup triggers; leave empty to auto-select manifest trigger projects");
    m_StartupScriptProject->SetDefaultString("");

    m_AutoLoadStartupScript = config->GetProperty("Startup", "AutoLoad");
    m_AutoLoadStartupScript->SetComment("Automatically run global TAS projects with trigger = startup when the game launches");
    m_AutoLoadStartupScript->SetDefaultBoolean(true);

    // --- OSD ---
    m_ShowOSD = config->GetProperty("OSD", "ShowOSD");
    m_ShowOSD->SetComment("Controls the visibility of the in-game On-Screen Display.");
    m_ShowOSD->SetDefaultBoolean(true);

    m_ShowOSDStatus = config->GetProperty("OSD", "ShowStatusPanel");
    m_ShowOSDStatus->SetComment("Show the status panel (TAS mode, frame count, ground contact)");
    m_ShowOSDStatus->SetDefaultBoolean(true);

    m_ShowOSDVelocity = config->GetProperty("OSD", "ShowVelocityPanel");
    m_ShowOSDVelocity->SetComment("Show the velocity panel (speed, velocity components, graphs)");
    m_ShowOSDVelocity->SetDefaultBoolean(true);

    m_ShowOSDPosition = config->GetProperty("OSD", "ShowPositionPanel");
    m_ShowOSDPosition->SetComment("Show the position panel (coordinates, trajectory graph)");
    m_ShowOSDPosition->SetDefaultBoolean(true);

    m_ShowOSDPhysics = config->GetProperty("OSD", "ShowPhysicsPanel");
    m_ShowOSDPhysics->SetComment("Show the physics panel (angular velocity, mass, physics state)");
    m_ShowOSDPhysics->SetDefaultBoolean(false);

    m_ShowOSDKeys = config->GetProperty("OSD", "ShowKeysPanel");
    m_ShowOSDKeys->SetComment("Show the input keys panel (real-time key state display)");
    m_ShowOSDKeys->SetDefaultBoolean(true);

    m_OSDPositionX = config->GetProperty("OSD", "PositionX");
    m_OSDPositionX->SetComment("OSD horizontal position (0.0 = left, 1.0 = right)");
    m_OSDPositionX->SetDefaultFloat(0.02f);

    m_OSDPositionY = config->GetProperty("OSD", "PositionY");
    m_OSDPositionY->SetComment("OSD vertical position (0.0 = top, 1.0 = bottom)");
    m_OSDPositionY->SetDefaultFloat(0.02f);

    m_OSDOpacity = config->GetProperty("OSD", "Opacity");
    m_OSDOpacity->SetComment("OSD transparency (0.0 = transparent, 1.0 = opaque)");
    m_OSDOpacity->SetDefaultFloat(0.9f);

    m_OSDScale = config->GetProperty("OSD", "Scale");
    m_OSDScale->SetComment("OSD scale factor (1.0 = normal size)");
    m_OSDScale->SetDefaultFloat(1.0f);
}

void ConfigService::OnPropertyChanged(const char *category, const char *key, IProperty *prop) {
    if (!m_EventBus) return;

    if (prop == m_Enabled || prop == m_Validation || prop == m_AutoRestart ||
        prop == m_StopOnFinish || prop == m_StopKey || prop == m_RecordingMaxFrames ||
        prop == m_StartupScriptEnabled || prop == m_StartupScriptProject ||
        prop == m_AutoLoadStartupScript || prop == m_ShowOSD ||
        prop == m_ShowOSDStatus || prop == m_ShowOSDVelocity ||
        prop == m_ShowOSDPosition || prop == m_ShowOSDPhysics ||
        prop == m_ShowOSDKeys || prop == m_OSDPositionX || prop == m_OSDPositionY ||
        prop == m_OSDOpacity || prop == m_OSDScale) {
        m_EventBus->Publish(ConfigChangedEvent{category, key});
    }
}

StartupConfig ConfigService::GetStartupConfig() const {
    StartupConfig cfg;
    if (m_StartupScriptEnabled) cfg.enabled = m_StartupScriptEnabled->GetBoolean();
    if (m_StartupScriptProject) cfg.project = m_StartupScriptProject->GetString();
    if (m_AutoLoadStartupScript) cfg.autoLoad = m_AutoLoadStartupScript->GetBoolean();
    return cfg;
}

OSDConfig ConfigService::GetOSDConfig() const {
    OSDConfig cfg;
    if (m_ShowOSDStatus)   cfg.showStatus   = m_ShowOSDStatus->GetBoolean();
    if (m_ShowOSDVelocity) cfg.showVelocity = m_ShowOSDVelocity->GetBoolean();
    if (m_ShowOSDPosition) cfg.showPosition = m_ShowOSDPosition->GetBoolean();
    if (m_ShowOSDPhysics)  cfg.showPhysics  = m_ShowOSDPhysics->GetBoolean();
    if (m_ShowOSDKeys)     cfg.showKeys     = m_ShowOSDKeys->GetBoolean();
    if (m_OSDPositionX)    cfg.positionX    = m_OSDPositionX->GetFloat();
    if (m_OSDPositionY)    cfg.positionY    = m_OSDPositionY->GetFloat();
    if (m_OSDOpacity)      cfg.opacity      = m_OSDOpacity->GetFloat();
    if (m_OSDScale)        cfg.scale        = m_OSDScale->GetFloat();
    return cfg;
}
