#include "HudIcon.h"
#include <Logging.h>

#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "Msimg32.lib")

std::vector<uint8_t> HudIcon::GetPNGData(const cmrc::embedded_filesystem& fs, std::string path)
{
	std::vector<uint8_t> data;

	if (!fs.is_file(path))
		Logger::Error("Stealthometer: {} not found in embedded filesystem.", path);
	else
	{
		auto file = fs.open(path);
		size_t size = file.size();
		data = std::vector<uint8_t>(size);

		int j = 0;
		for (auto i = file.begin(); i != file.end(); i++)
		{
			data[j] = *i;
			j++;
		}
	}

	return data;
}

// Creates a 32-bit DIB from the specified WIC bitmap.
HBITMAP HudIcon::CreateHBITMAP(IWICBitmapSource* ipBitmap)
{
	HBITMAP hbmp = NULL;

	// get image attributes and check for valid image
	UINT width = 0;
	UINT height = 0;
	if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0)
		return hbmp;

	// prepare structure giving bitmap information (negative height indicates a top-down DIB)
	BITMAPINFO bminfo;
	ZeroMemory(&bminfo, sizeof(bminfo));
	bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bminfo.bmiHeader.biWidth = width;
	bminfo.bmiHeader.biHeight = -((LONG)height);
	bminfo.bmiHeader.biPlanes = 1;
	bminfo.bmiHeader.biBitCount = 32;
	bminfo.bmiHeader.biCompression = BI_RGB;

	// create a DIB section that can hold the image
	void* pvImageBits = NULL;
	HDC hdcScreen = GetDC(NULL);
	hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
	ReleaseDC(NULL, hdcScreen);
	if (hbmp == NULL)
		return hbmp;

	// extract the image into the HBITMAP
	const UINT cbStride = width * 4;
	const UINT cbImage = cbStride * height;
	if (FAILED(ipBitmap->CopyPixels(NULL, cbStride, cbImage, static_cast<BYTE*>(pvImageBits))))
	{
		// couldn't extract image; delete HBITMAP
		DeleteObject(hbmp);
		hbmp = NULL;
	}

	return hbmp;
}

IWICBitmapSource* HudIcon::GetWICBitmap(uint8_t* data, size_t size)
{
	// WIC interface pointers.
	IWICStream* pIWICStream;
	IWICBitmapDecoder* pIDecoder;
	IWICBitmapFrameDecode* pIDecoderFrame;
	IWICBitmapSource* pBitmap = NULL;

	// Create a WIC stream to map onto the memory.
	HRESULT hr = m_pWICImagingFactory->CreateStream(&pIWICStream);

	if (FAILED(hr))
		return pBitmap;

	// Initialize the stream with the memory pointer and size.
	hr = pIWICStream->InitializeFromMemory(
		reinterpret_cast<BYTE*>(data),
		size);

	if (FAILED(hr))
		return pBitmap;

	// Create a decoder for the stream.
	hr = m_pWICImagingFactory->CreateDecoderFromStream(
		pIWICStream,                   // The stream to use to create the decoder
		NULL,                          // Do not prefer a particular vendor
		WICDecodeMetadataCacheOnLoad,  // Cache metadata when needed
		&pIDecoder);

	if (FAILED(hr))
		return pBitmap;

	hr = pIDecoder->GetFrame(0, &pIDecoderFrame);

	if (FAILED(hr))
		return pBitmap;

	IWICBitmapSource* pSource = pIDecoderFrame;

	// Convert the image to 32bpp BGRA format with pre-multiplied alpha
	WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, pSource, &pBitmap);

	return pBitmap;
}

// Calls UpdateLayeredWindow to set a bitmap (with alpha) as the content of the window
void HudIcon::UpdateIcon(HWND hwnd, HWND hwndParent, HBITMAP hbitmap, int xoffset, int yoffset)
{
	// Get the size of the bitmap
	BITMAP bm;
	GetObject(hbitmap, sizeof(bm), &bm);
	SIZE sizeOriginal = { bm.bmWidth, bm.bmHeight };

	// Determine scaled size and position
	RECT parentClientRect, parentRect;
	GetClientRect(hwndParent, &parentClientRect);
	GetWindowRect(hwndParent, &parentRect);
	SIZE parentSize = { parentClientRect.right - parentClientRect.left, parentClientRect.bottom - parentClientRect.top };

	// TODO: Proper scaling and positioning
	float scale = (parentSize.cx / 1920.f + parentSize.cy / 1080.f) / 2.f;
	scale += (1.f - scale) / 12.f; // ???????
	SIZE sizeScaled = { 32*scale, 32*scale };
	POINT ptOrigin = { parentRect.left + xoffset*scale, parentRect.bottom - yoffset*scale };
	POINT ptZero = { 0 };

	HDC hdcScreen = GetDC(NULL);

	HDC hdcMemSrc = CreateCompatibleDC(hdcScreen);
	HDC hdcMemDest = CreateCompatibleDC(hdcScreen);

	// Create new bitmap for the scaled size
	auto scaledBitmap = CreateCompatibleBitmap(hdcScreen, sizeScaled.cx, sizeScaled.cy);

	HBITMAP hbmpOld1 = (HBITMAP)SelectObject(hdcMemSrc, hbitmap);
	HBITMAP hbmpOld2 = (HBITMAP)SelectObject(hdcMemDest, scaledBitmap);

	// Use the source image's alpha channel for blending
	BLENDFUNCTION blend = { 0 };
	blend.BlendOp = AC_SRC_OVER;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;

	// Draw original bitmap onto new bitmap
	AlphaBlend(hdcMemDest, 0, 0, sizeScaled.cx, sizeScaled.cy, hdcMemSrc, 0, 0, sizeOriginal.cx, sizeOriginal.cy, blend);

	// Paint the window (in the right location) with the scaled bitmap
	UpdateLayeredWindow(hwnd, hdcScreen, &ptOrigin, &sizeScaled,
		hdcMemDest, &ptZero, RGB(0, 0, 0), &blend, ULW_ALPHA);

	// Delete temporary objects
	SelectObject(hdcMemSrc, hbmpOld1);
	DeleteDC(hdcMemSrc);
	SelectObject(hdcMemDest, hbmpOld2);
	DeleteDC(hdcMemDest);

	ReleaseDC(NULL, hdcScreen);
}

HudIcon::HudIcon()
{
}

HudIcon::~HudIcon()
{
	this->destroy();
	if (this->m_wclAtom) UnregisterClass((LPCSTR)this->m_wclAtom, NULL);
}

ATOM HudIcon::registerWindowClass(HINSTANCE instance)
{
	auto wcl = WNDCLASSEXA{};
	wcl.cbSize = sizeof(WNDCLASSEXA);
	wcl.style = CS_OWNDC;
	wcl.lpfnWndProc = DefWindowProc;
	wcl.hInstance = instance;
	wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcl.hbrBackground = NULL;
	wcl.lpszMenuName = NULL;
	wcl.lpszClassName = "SA_HudIcon";

	return RegisterClassEx(&wcl);
}

void HudIcon::create(HINSTANCE instance, const cmrc::embedded_filesystem & fs, int show, SilentAssassinStatus sa)
{
	if (this->m_runningWindow) return;

	m_hwndParent = FindWindow(NULL, "HITMAN 3");

	if (!m_hwndParent)
	{
		Logger::Warn("Stealthometer: could not locate game window.");
		return;
	}

	if (!m_pWICImagingFactory)
	{
		// Create the COM imaging factory
		HRESULT hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_pWICImagingFactory)
		);

		if (FAILED(hr))
		{
			m_pWICImagingFactory = nullptr;
			return;
		}

		auto pngdata = GetPNGData(fs, "data/SA-OK.png");
		m_icons[0] = CreateHBITMAP(GetWICBitmap(pngdata.data(), pngdata.size()));
		pngdata = GetPNGData(fs, "data/SA-Fail.png");
		m_icons[1] = CreateHBITMAP(GetWICBitmap(pngdata.data(), pngdata.size()));
		pngdata = GetPNGData(fs, "data/SA-Redeemable.png");
		m_icons[2] = CreateHBITMAP(GetWICBitmap(pngdata.data(), pngdata.size()));
	}

	m_windowThread = std::thread([this, instance, show, sa] {
		if (!this->m_wclAtom) this->m_wclAtom = this->registerWindowClass(instance);

		this->m_hwnd = CreateWindowEx(
			WS_EX_LAYERED,
			reinterpret_cast<LPCSTR>(this->m_wclAtom),
			"SA_HudIcon",
			WS_POPUP,
			0, 0,
			0, 0,
			NULL, NULL,
			instance,
			this
		);

		if (!this->m_hwnd)
			return;

		this->m_runningWindow = true;

		update(show, sa);

		auto msg = MSG {};

		while (GetMessage(&msg, NULL, NULL, NULL) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == STEALTHOMETER_CLOSE_WINDOW)
			{
				if (this->m_hwnd)
				{
					DestroyWindow(this->m_hwnd);
					this->m_hwnd = nullptr;
				}
				break;
			}
		}

		this->m_runningWindow = false;
	});
}

void HudIcon::destroy()
{
	if (this->m_hwnd)
	{
		PostThreadMessage(GetThreadId(this->m_windowThread.native_handle()), STEALTHOMETER_CLOSE_WINDOW, 0, 0);
		this->m_windowThread.detach();
	}

	m_visible = false;
}

void HudIcon::update(int show, SilentAssassinStatus sa)
{
	if (!m_runningWindow)
		return;

	if (!show)
	{
		if (m_visible)
		{
			m_visible = false;
			ShowWindow(m_hwnd, SW_HIDE);
		}

		return;
	}

	if (!m_visible)
	{
		m_visible = true;
		ShowWindow(m_hwnd, SW_SHOW);
		SetWindowPos(this->m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

	int icon, xoffset, yoffset;

	if (sa == SilentAssassinStatus::OK) icon = 0;
	else if (sa == SilentAssassinStatus::Fail) icon = 1;
	else icon = 2;

	if (show == 1)
	{
		xoffset = 261;
		yoffset = 111;
	}
	else
	{
		xoffset = 32;
		yoffset = 64;
	}

	UpdateIcon(m_hwnd, m_hwndParent, m_icons[icon], xoffset, yoffset);
}
