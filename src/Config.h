#pragma once
#include <string>
#include <IPluginInterface.h>

enum class DockMode
{
	None,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
};

struct ConfigData
{
	bool externalWindow = true;
	bool externalWindowDark = true;
	bool externalWindowOnTop = false;
	bool inGameOverlay = false;
	bool inGameOverlayDetailed = false;
	bool hudIcon = false;
	DockMode overlayDockMode = DockMode::None;
	bool liveSplitEnabled = false;
	std::string liveSplitIP = "127.0.0.1";
	uint16_t liveSplitPort = 16834;
	int freelancerSA = 0;
};

class Config
{
public:
	Config(IPluginInterface& plugin) : plugin(plugin)
	{ }

	void Load() {
		data.externalWindow = plugin.GetSettingBool("general", "external_window", data.externalWindow);
		data.externalWindowDark = plugin.GetSettingBool("general", "external_window_dark", data.externalWindowDark);
		data.externalWindowOnTop = plugin.GetSettingBool("general", "external_window_on_top", data.externalWindowOnTop);
		data.inGameOverlay = plugin.GetSettingBool("general", "overlay", data.inGameOverlay);
		data.inGameOverlayDetailed = plugin.GetSettingBool("general", "overlay_detailed", data.inGameOverlayDetailed);
		data.hudIcon = plugin.GetSettingBool("general", "hud_icon", data.hudIcon);
		data.liveSplitEnabled = plugin.GetSettingBool("livesplit", "enabled", data.liveSplitEnabled);
		data.liveSplitIP = plugin.GetSetting("livesplit", "ip", data.liveSplitIP);
		data.liveSplitPort = plugin.GetSettingInt("livesplit", "port", data.liveSplitPort);

		auto overlayDock = plugin.GetSetting("general", "overlay_dock", "");

		if (overlayDock == "topleft") data.overlayDockMode = DockMode::TopLeft;
		else if (overlayDock == "topright") data.overlayDockMode = DockMode::TopRight;
		else if (overlayDock == "bottomleft") data.overlayDockMode = DockMode::BottomLeft;
		else if (overlayDock == "bottomright") data.overlayDockMode = DockMode::BottomRight;
		else data.overlayDockMode = DockMode::None;

		data.freelancerSA = plugin.GetSettingInt("freelancer", "sa_status", data.freelancerSA);
	}

	void Save() {
		if (data.liveSplitIP.empty())
			data.liveSplitIP = "127.0.0.1";
		if (!data.liveSplitPort)
			data.liveSplitPort = 16834;

		plugin.SetSettingBool("general", "external_window", data.externalWindow);
		plugin.SetSettingBool("general", "external_window_dark", data.externalWindowDark);
		plugin.SetSettingBool("general", "external_window_on_top", data.externalWindowOnTop);
		plugin.SetSettingBool("general", "overlay", data.inGameOverlay);
		plugin.SetSettingBool("general", "overlay_detailed", data.inGameOverlayDetailed);
		plugin.SetSettingBool("general", "hud_icon", data.hudIcon);
		plugin.SetSettingBool("livesplit", "enabled", data.liveSplitEnabled);
		plugin.SetSetting("livesplit", "ip", data.liveSplitIP);
		plugin.SetSettingInt("livesplit", "port", data.liveSplitPort);
		
		auto spinOverlayDock = "none";
		switch (data.overlayDockMode) {
		case DockMode::TopLeft:
			spinOverlayDock = "topleft";
			break;
		case DockMode::TopRight:
			spinOverlayDock = "topright";
			break;
		case DockMode::BottomLeft:
			spinOverlayDock = "bottomleft";
			break;
		case DockMode::BottomRight:
			spinOverlayDock = "bottomright";
			break;
		}
		plugin.SetSetting("general", "overlay_dock", spinOverlayDock);

		plugin.SetSettingInt("freelancer", "sa_status", data.freelancerSA);
	}

	inline ConfigData& Get() { return data; }
	inline const ConfigData& Get() const { return data; }

private:
	IPluginInterface& plugin;
	ConfigData data;
};
