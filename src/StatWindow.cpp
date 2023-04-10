#include <Windows.h>
#include <string>
#include <array>
#include <format>
#include <thread>
#include <tuple>
#include <vector>

#include "StatWindow.h"
#include "Logging.h"
#include "FixMinMax.h"

using namespace std::string_literals;

enum class TextAlign
{
	Left = 0,
	Right = 1,
	Center = 2,
};

enum class Anchor
{
	Left = 0,
	Center = 1,
	Right = 2,
};

enum class VAnchor
{
	Top = 0,
	Middle = 1,
	Bottom = 2,
};

auto createWindowStatic(DWORD dwExStyle, LPCSTR windowName, DWORD dwStyle, int x, int y, int w, int h, HWND parent) -> HWND
{
	return CreateWindowEx(dwExStyle, "Static", windowName, dwStyle, x, y, w, h, parent, NULL, NULL, NULL);
}

class TextLabel
{
public:
	TextLabel(std::string text, int x = 0, int y = 0, int w = 0, int h = 0) :
		text(text), width(w), height(h), x(x), y(y)
	{
	}

	~TextLabel()
	{
		this->destroyFont();
	}

	auto setHAnchor(Anchor anchor)
	{
		this->anchor = anchor;
	}

	auto setVAnchor(VAnchor anchor)
	{
		this->vanchor = anchor;
	}

	auto setAlign(TextAlign align)
	{
		this->align = align;
	}

	auto setSize(int w, int h)
	{
		this->width = w;
		this->height = h;
	}

	auto setWeight(int fontWeight)
	{
		this->weight = fontWeight;
	}

	auto setBold(bool bold)
	{
		if (bold) this->setWeight(FW_BOLD);
		else this->setWeight(FW_DONTCARE);
	}

	auto setColour(DWORD colour)
	{
		this->colour = colour;
	}

	auto setSize(int fontSize)
	{
		this->size = fontSize;
	}

	auto create(HWND parent) -> bool
	{
		if (!parent)
		{
			Logger::Error("Creating text with no parent!");
			return false;
		}
		if (!GetClientRect(parent, &this->clientRect)) return false;

		auto alignFlag = SS_LEFT;

		if (this->align == TextAlign::Center)
			alignFlag = SS_CENTER;
		else if (this->align == TextAlign::Right)
			alignFlag = SS_RIGHT;

		auto w = static_cast<int>((static_cast<double>(this->width) / 100) * this->clientRect.right);

		this->hWnd = createWindowStatic(
			NULL,
			text.c_str(),
			WS_VISIBLE | WS_CHILD | alignFlag,
			this->getLeftPos(), this->getTopPos(),
			w, this->height,
			parent
		);

		if (this->hWnd == nullptr) return false;
		this->applyFont();
		return true;
	}

	auto paint(HWND wnd, HDC hdc, RECT bounds)
	{
		this->clientRect = bounds;
		RECT rect = { this->getLeftPos(), this->getTopPos(), this->getRightpos(), this->getBottomPos() };
		auto font = this->createFont();
		auto oldColour = SetTextColor(hdc, this->colour);
		auto oldFont = SelectObject(hdc, font);
		DrawText(hdc, text.c_str(), -1, &rect, this->getFormat());
		SetTextColor(hdc, oldColour);
		SelectObject(hdc, oldFont);
	}

private:
	auto getAbsWidth() -> int
	{
		return static_cast<int>((static_cast<double>(this->width) / 100) * this->clientRect.right);
	}

	auto getAbsHeight() -> int
	{
		return this->height;
	}

	auto getLeftPos() -> int
	{
		const auto w = this->getAbsWidth();
		if (this->anchor == Anchor::Center)
			return this->x + (this->clientRect.right / 2) - (w / 2);
		else if (this->anchor == Anchor::Right)
			return (this->clientRect.right - w) - this->x;
		return this->x;
	}

	auto getTopPos() -> int
	{
		const auto h = this->getAbsHeight();
		if (this->vanchor == VAnchor::Middle)
			return this->y + (this->clientRect.bottom / 2) - (h / 2);
		else if (this->vanchor == VAnchor::Bottom)
			return (this->clientRect.bottom - h) - this->y;
		return this->y;
	}

	auto getRightpos() -> int
	{
		auto w = static_cast<int>((static_cast<double>(this->width) / 100) * this->clientRect.right);
		return this->getLeftPos() + w;
	}

	auto getBottomPos() -> int
	{
		return this->getTopPos() + this->height;
	}

	auto getFormat() -> UINT
	{
		UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_WORDBREAK;
		if (this->align == TextAlign::Left) fmt |= DT_LEFT;
		else if (this->align == TextAlign::Center) fmt |= DT_CENTER;
		return fmt;
	}

	auto createFont() -> HFONT
	{
		this->destroyFont();
		return this->hFont = CreateFont(
			this->size, 0, 0, 0,
			this->weight,
			false, false, false,
			ANSI_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_SWISS,
			"Arial"
		);
	}

	auto applyFont() -> void
	{
		this->hFont = this->createFont();
		if (this->hFont)
			SendMessage(this->hWnd, WM_SETFONT, WPARAM(hFont), true);
		else Logger::Error("Font not created");
	}

	auto destroyFont() -> void
	{
		if (!this->hFont) return;
		DeleteObject(this->hFont);
		this->hFont = nullptr;
	}

private:
	HWND hWnd = nullptr;
	HFONT hFont = nullptr;
	RECT clientRect = {};
	COLORREF colour = RGB(0, 0, 0);
	std::string text;
	int x = 0; // relative to anchor
	int y = 0;
	int width = 0; // percentage
	int height = 0;
	int size = 24;
	int weight = FW_DONTCARE;
	Anchor anchor = Anchor::Left;
	VAnchor vanchor = VAnchor::Top;
	TextAlign align = TextAlign::Left;
};

StatWindow::StatWindow(const DisplayStats& stats) : stats(stats)
{
}

StatWindow::~StatWindow()
{
	this->destroy();
}

auto StatWindow::create(HINSTANCE instance) -> void
{
	if (this->runningWindow) return;

	auto hitmanWindow = FindWindow(NULL, "HITMAN 3");

	if (!hitmanWindow)
	{
		Logger::Warn("Could not locate game window.");
		return;
	}

	Logger::Info("Creating external window");

	this->runningWindow = true;

	windowThread = std::thread([this, instance, hitmanWindow]
		{
			WNDCLASSEXA wcl = {};
			wcl.cbSize = sizeof(WNDCLASSEXA);
			wcl.style = CS_OWNDC;
			wcl.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
			{
				if (msg == WM_CREATE)
					SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));

				auto statWindow = reinterpret_cast<StatWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

				switch (msg)
				{
					case WM_CREATE:
						break;
					case WM_PAINT:
						statWindow->paint(hwnd);
						break;
					case WM_ERASEBKGND:
						statWindow->paintBg(hwnd, reinterpret_cast<HDC>(wParam));
						break;
				}

				return DefWindowProc(hwnd, msg, wParam, lParam);
			};
			wcl.hInstance = instance;
			wcl.hIcon = reinterpret_cast<HICON>(GetClassLongPtr(hitmanWindow, GCLP_HICON));
			wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
			wcl.hbrBackground = NULL;//(HBRUSH)COLOR_WINDOW;
			wcl.lpszMenuName = NULL;
			wcl.lpszClassName = "Stealthometer";
			wcl.hIconSm = reinterpret_cast<HICON>(GetClassLongPtr(hitmanWindow, GCLP_HICONSM));

			wclAtom = RegisterClassEx(&wcl);
			this->hWnd = CreateWindow(
				reinterpret_cast<LPCSTR>(wclAtom),
				static_cast<LPCSTR>("Stealthometer"),
				WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
				CW_USEDEFAULT, CW_USEDEFAULT,
				400, 485,
				NULL, NULL,
				instance,
				this
			);

			if (!this->hWnd)
			{
				Logger::Error("Error: Stealthometer external window could not be created.");
				this->runningWindow = false;
				return;
			}

			ShowWindow(this->hWnd, SW_SHOW);
			UpdateWindow(this->hWnd);

			MSG msg;
			while (this->runningWindow)
			{
				if (GetMessage(&msg, this->hWnd, NULL, NULL) > 0)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);

					if (msg.message == WM_CLOSE || msg.message == WM_DESTROY)
						this->runningWindow = false;
				}
				else break;
			}

			this->runningWindow = false;
		});
}

auto getCellPosY(int row)
{
	constexpr int marginTop = 5;
	return marginTop + (60 * row);
}

auto StatWindow::paintCell(HDC hdc, std::string header, Stat stat, int col, int row) -> void
{
	auto textColour = this->darkMode ? RGB(255, 255, 255) : RGB(0, 0, 0);
	auto anchor = col == 0 ? Anchor::Left : Anchor::Right;
	auto y = getCellPosY(row);
	RECT rect;
	GetClientRect(this->hWnd, &rect);

	if (row == 6) y += 12;
	else
	{
		TextLabel label(header, 0, y, 50, 30);
		label.setHAnchor(anchor);
		label.setAlign(TextAlign::Center);
		label.setColour(textColour);
		label.paint(this->hWnd, hdc, rect);
	}

	if (row != 6) y += 25;
	TextLabel valueText(this->formatStat(stat), 0, y, 50, 30);
	valueText.setBold(true);
	valueText.setHAnchor(anchor);
	valueText.setAlign(TextAlign::Center);
	valueText.setColour(textColour);
	valueText.paint(this->hWnd, hdc, rect);
}

auto StatWindow::paintBg(HWND wnd, HDC hdc) -> void
{
	RECT rc;
	HBRUSH brush = this->darkMode ? static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)) : reinterpret_cast<HBRUSH>(COLOR_WINDOW);
	GetClientRect(wnd, &rc);
	//SetMapMode(hdc, MM_ANISOTROPIC);
	//SIZE oldWinExt;
	//SetWindowExtEx(hdc, 100, 100, &oldWinExt);
	//auto oldViewExt = SetViewportExtEx(hdc, rc.right, rc.bottom, NULL);
	FillRect(hdc, &rc, brush);
	//SetWindowExtEx(hdc, oldWinExt.cx, oldWinExt.cy, NULL);
}

auto StatWindow::paint(HWND wnd) -> void
{
	PAINTSTRUCT ps;
	RECT rect;
	GetClientRect(wnd, &rect);
	auto hdc = BeginPaint(wnd, &ps);
	auto oldBkMode = SetBkMode(hdc, TRANSPARENT);

	this->paintCell(hdc, "Tension", Stat::Tension, 0, 0);
	this->paintCell(hdc, "Pacifications", Stat::Pacifications, 0, 1);
	this->paintCell(hdc, "Spotted", Stat::Spotted, 0, 2);
	this->paintCell(hdc, "Bodies Found", Stat::BodiesFound, 0, 3);
	this->paintCell(hdc, "Disguises Blown", Stat::DisguisesBlown, 0, 4);
	this->paintCell(hdc, "Recorded", Stat::Recorded, 0, 5);
	this->paintCell(hdc, "Guard Kills (NTK)", Stat::GuardKills, 1, 0);
	this->paintCell(hdc, "Civilian Kills (NTK)", Stat::CivKills, 1, 1);
	this->paintCell(hdc, "Witnesses", Stat::Witnesses, 1, 2);
	this->paintCell(hdc, "Bodies Hidden", Stat::BodiesHidden, 1, 3);
	this->paintCell(hdc, "Disguises Taken", Stat::DisguisesTaken, 1, 4);
	this->paintCell(hdc, "Targets Found", Stat::TargetsFound, 1, 5);

	this->paintCell(hdc, "Traceless", Stat::PlayStyle, 0, 6);
	this->paintCell(hdc, "Stealth", Stat::StealthRating, 1, 6);

	// paint rating area
	/*auto anchor = col == 0 ? Anchor::Left : Anchor::Right;
	auto y = getCellPosY(row);
	RECT rect;
	GetClientRect(this->hWnd, &rect);
	TextLabel label(header, 0, y, 50, 30);
	label.setHAnchor(anchor);
	label.setAlign(TextAlign::Center);
	label.paint(this->hWnd, hdc, rect);*/

	TextLabel saText(this->formatStat(Stat::SilentAssassin), 0, 2, 100, 30);
	saText.setSize(28);
	saText.setBold(true);
	saText.setHAnchor(Anchor::Center);
	saText.setVAnchor(VAnchor::Bottom);
	saText.setAlign(TextAlign::Center);

	if (this->stats.silentAssassin == SilentAssassinStatus::OK)
		saText.setColour(RGB(0, 150, 0));
	else if (this->stats.silentAssassin == SilentAssassinStatus::Fail)
		saText.setColour(RGB(230, 0, 0));
	else
		saText.setColour(RGB(217, 109, 0));

	saText.paint(this->hWnd, hdc, rect);

	SetBkMode(hdc, oldBkMode);
	EndPaint(wnd, &ps);
}

auto StatWindow::formatStat(Stat stat) -> std::string
{
	switch (stat)
	{
		case Stat::Tension:
			return std::format("{:d}%", this->stats.tension);
		case Stat::Pacifications:
			return std::format("{:d}", this->stats.pacifications);
		case Stat::Spotted:
			return std::format("{:d}", this->stats.spotted);
		case Stat::BodiesFound:
			return std::format("{:d}", this->stats.bodiesFound);
		case Stat::BodiesHidden:
			return std::format("{:d}", this->stats.bodiesHidden);
		case Stat::Witnesses:
			return std::format("{:d}", this->stats.witnesses);
		case Stat::GuardKills:
			return std::format("{:d}", this->stats.guardKills);
		case Stat::CivKills:
			return std::format("{:d}", this->stats.civilianKills);
		case Stat::DisguisesBlown:
			return std::format("{:d}", this->stats.disguisesBlown);
		case Stat::DisguisesTaken:
			return std::format("{:d}", this->stats.disguisesTaken);
		case Stat::TargetsFound:
			return this->stats.targetsFound ? "Yes"s : "No"s;
		case Stat::Recorded:
			return this->stats.recorded ? "Yes"s : "No"s;
		case Stat::StealthRating: {
			return std::format("{:g}%", static_cast<double>(static_cast<int>(this->stats.stealthRating * 10)) / 10);
		}
		case Stat::PlayStyle: {
			return this->stats.playstyle;
		}
		case Stat::SilentAssassin:
			switch (this->stats.silentAssassin)
			{
				case SilentAssassinStatus::OK:
					return "Silent Assassin";
				case SilentAssassinStatus::Fail:
					return "X Silent Assassin";
				case SilentAssassinStatus::RedeemableCamera:
					return "Silent Assassin (Cams)";
				case SilentAssassinStatus::RedeemableTarget:
					return "Silent Assassin (Target)";
				case SilentAssassinStatus::RedeemableCameraAndTarget:
					return "Silent Assassin (Cams, Target)";
			}
			break;
	}
	return "Allan add details"s;
}

auto createWindowBottomRow(HWND parent) -> HWND
{
	RECT rect;
	if (!GetClientRect(parent, &rect)) return nullptr;
	auto hWnd = createWindowStatic(
		NULL, "",
		WS_VISIBLE | WS_CHILD,
		0, rect.bottom - 65,
		rect.right, 65,
		parent
	);
	return hWnd;
}

auto StatWindow::update() -> void
{
	if (this->hWnd)
	{
		InvalidateRect(this->hWnd, NULL, true);
		UpdateWindow(this->hWnd);
	}
}

auto StatWindow::destroy() -> void
{
	this->runningWindow = false;
	this->windowThread.detach();

	if (this->hWnd) DestroyWindow(this->hWnd);
	if (this->wclAtom) UnregisterClass((LPCSTR)this->wclAtom, NULL);

	this->hWnd = nullptr;
	this->wclAtom = NULL;
}

auto StatWindow::setDarkMode(bool enable) -> void
{
	this->darkMode = enable;
}