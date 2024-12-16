#pragma once

#include <wincodec.h>
#include <cstdint>
#include <thread>
#include "Stats.h"
#include <cmrc/cmrc.hpp>

#define STEALTHOMETER_CLOSE_WINDOW (WM_USER + 0x8008)

class HudIcon
{
public:
	HudIcon();
	~HudIcon();

	void create(HINSTANCE instance, const cmrc::embedded_filesystem& fs, int show, SilentAssassinStatus sa);
	void destroy();

	void update(int show, SilentAssassinStatus sa);

private:
	static ATOM registerWindowClass(HINSTANCE instance);

	std::vector<uint8_t> GetPNGData(const cmrc::embedded_filesystem& fs, std::string path);
	HBITMAP CreateHBITMAP(IWICBitmapSource* ipBitmap);
	IWICBitmapSource* GetWICBitmap(uint8_t* data, size_t size);
	void UpdateIcon(HWND hwnd, HWND hwndParent, HBITMAP hbitmap, int xoffset, int yoffset);
	
private:
	HWND m_hwnd = nullptr;
	HWND m_hwndParent = nullptr;
	ATOM m_wclAtom = NULL;

	std::thread m_windowThread;

	volatile bool m_runningWindow = false;

	IWICImagingFactory* m_pWICImagingFactory = nullptr;
	HBITMAP m_icons[3];
	bool m_visible = false;
};
