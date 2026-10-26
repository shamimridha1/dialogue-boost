// Dialogue Boost editor: custom Win32 + GDI+ UI
#include "plugin.h"
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstdio>
#include <cwchar>

using namespace Steinberg;
using namespace Gdiplus;

namespace dlb {

static const int W = 460, H = 300;
static const int TX0 = 24, TX1 = 436; // slider track x range
static const int ROW_Y[3] = {92, 152, 212};
static const Steinberg::Vst::ParamID ROW_ID[3] = {kBoost, kClarity, kCeiling};
static const wchar_t* ROW_LABEL[3] = {L"BOOST", L"CLARITY", L"CEILING"};
static const wchar_t* ROW_HINT[3] = {L"output volume", L"voice EQ amount", L"max peak level"};
static const double ROW_DEFAULT[3] = {0.5, 0.5, 0.75};
static const RECT BYPASS_RC = {356, 22, 436, 48};

static const Color C_BG (255, 23, 24, 28);
static const Color C_PANEL (255, 31, 32, 38);
static const Color C_TRACK (255, 46, 48, 56);
static const Color C_ACCENT (255, 232, 163, 61);
static const Color C_TEXT (255, 236, 236, 238);
static const Color C_DIM (255, 138, 141, 150);
static const Color C_OFF (255, 90, 92, 100);

static ULONG_PTR gdiToken = 0;
static int gdiUsers = 0;
static const wchar_t* kClass = L"DialogueBoostEditor_6D1B4F21";
static bool classRegistered = false;

static LRESULT CALLBACK WndProc (HWND h, UINT msg, WPARAM wp, LPARAM lp);

extern "C" IMAGE_DOS_HEADER __ImageBase;

static void addRoundRect (GraphicsPath& p, REAL x, REAL y, REAL w, REAL h, REAL r)
{
	REAL d = r * 2;
	p.AddArc (x, y, d, d, 180, 90);
	p.AddArc (x + w - d, y, d, d, 270, 90);
	p.AddArc (x + w - d, y + h - d, d, d, 0, 90);
	p.AddArc (x, y + h - d, d, d, 90, 90);
	p.CloseFigure ();
}

static void fillRound (Graphics& g, const Color& c, REAL x, REAL y, REAL w, REAL h, REAL r)
{
	GraphicsPath p;
	addRoundRect (p, x, y, w, h, r);
	SolidBrush b (c);
	g.FillPath (&b, &p);
}

Editor::Editor (PluginEntry* owner) : plug (owner)
{
	ViewRect r (0, 0, W, H);
	setRect (r);
}

Editor::~Editor ()
{
	if (plug && plug->editor == this) plug->editor = nullptr;
}

tresult PLUGIN_API Editor::isPlatformTypeSupported (FIDString type)
{
	return (type && strcmp (type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
}

void Editor::attachedToParent ()
{
	if (gdiUsers++ == 0)
	{
		GdiplusStartupInput in;
		GdiplusStartup (&gdiToken, &in, nullptr);
	}
	HINSTANCE inst = (HINSTANCE)&__ImageBase;
	if (!classRegistered)
	{
		WNDCLASSEXW wc {};
		wc.cbSize = sizeof (wc);
		wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = inst;
		wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
		wc.lpszClassName = kClass;
		RegisterClassExW (&wc);
		classRegistered = true;
	}
	HWND h = CreateWindowExW (0, kClass, L"Dialogue Boost", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
	                          0, 0, W, H, (HWND)systemWindow, nullptr, inst, this);
	hwnd = h;
	plug->editor = this;
	SetTimer (h, 1, 40, nullptr);
}

void Editor::removedFromParent ()
{
	plug->editor = nullptr;
	if (hwnd)
	{
		KillTimer ((HWND)hwnd, 1);
		DestroyWindow ((HWND)hwnd);
		hwnd = nullptr;
	}
	if (--gdiUsers == 0 && gdiToken)
	{
		GdiplusShutdown (gdiToken);
		gdiToken = 0;
	}
}

void Editor::refresh ()
{
	if (hwnd) InvalidateRect ((HWND)hwnd, nullptr, FALSE);
}

static void formatValue (int row, double v, wchar_t* buf, size_t n)
{
	if (row == 0) swprintf (buf, n, L"+%.1f dB", v);
	else if (row == 1) swprintf (buf, n, L"%.0f %%", v);
	else swprintf (buf, n, L"%.1f dB", v);
}

static void paint (Editor* e, HDC hdc)
{
	HDC mem = CreateCompatibleDC (hdc);
	HBITMAP bmp = CreateCompatibleBitmap (hdc, W, H);
	HGDIOBJ old = SelectObject (mem, bmp);
	{
		Graphics g (mem);
		g.SetSmoothingMode (SmoothingModeAntiAlias);
		g.SetTextRenderingHint (TextRenderingHintClearTypeGridFit);
		g.Clear (C_BG);

		FontFamily ff (L"Segoe UI");
		Font fTitle (&ff, 17, FontStyleBold, UnitPixel);
		Font fSub (&ff, 12, FontStyleRegular, UnitPixel);
		Font fLabel (&ff, 11, FontStyleBold, UnitPixel);
		Font fValue (&ff, 14, FontStyleBold, UnitPixel);
		SolidBrush bText (C_TEXT), bDim (C_DIM), bAccent (C_ACCENT), bOff (C_OFF);
		StringFormat right;
		right.SetAlignment (StringAlignmentFar);
		StringFormat center;
		center.SetAlignment (StringAlignmentCenter);
		center.SetLineAlignment (StringAlignmentCenter);

		bool bypassed = e->plug->getParamNormalized (kBypass) > 0.5;

		// header
		g.DrawString (L"DIALOGUE BOOST", -1, &fTitle, PointF (22, 18), &bText);
		g.DrawString (L"Louder, clearer voice — no clipping", -1, &fSub, PointF (24, 42), &bDim);

		// bypass pill
		RECT br = BYPASS_RC;
		fillRound (g, bypassed ? C_TRACK : C_ACCENT, (REAL)br.left, (REAL)br.top,
		           (REAL)(br.right - br.left), (REAL)(br.bottom - br.top), 13);
		SolidBrush pillText (bypassed ? C_DIM : C_BG);
		g.DrawString (bypassed ? L"BYPASSED" : L"ACTIVE", -1, &fLabel,
		              RectF ((REAL)br.left, (REAL)br.top, (REAL)(br.right - br.left),
		                     (REAL)(br.bottom - br.top)),
		              &center, &pillText);

		// divider
		SolidBrush bPanel (C_PANEL);
		g.FillRectangle (&bPanel, 0, 72, W, 1);

		// sliders
		for (int i = 0; i < 3; ++i)
		{
			int y = ROW_Y[i];
			double norm = e->plug->getParamNormalized (ROW_ID[i]);
			double pv = e->plug->plain (ROW_ID[i]);
			wchar_t val[32];
			formatValue (i, pv, val, 32);

			g.DrawString (ROW_LABEL[i], -1, &fLabel, PointF (TX0 - 1, (REAL)y), &bText);
			RectF lb;
			g.MeasureString (ROW_LABEL[i], -1, &fLabel, PointF (0, 0), &lb);
			g.DrawString (ROW_HINT[i], -1, &fSub, PointF (TX0 + lb.Width + 4, (REAL)y - 1), &bDim);
			g.DrawString (val, -1, &fValue, RectF (TX0, (REAL)y - 3, TX1 - TX0, 20), &right,
			              bypassed ? &bOff : &bText);

			REAL ty = (REAL)y + 28, th = 6;
			fillRound (g, C_TRACK, TX0, ty, TX1 - TX0, th, 3);
			REAL fx = (REAL)(TX0 + norm * (TX1 - TX0));
			if (fx - TX0 > 1) fillRound (g, bypassed ? C_OFF : C_ACCENT, TX0, ty, fx - TX0, th, 3);
			SolidBrush thumb (bypassed ? C_DIM : C_TEXT);
			g.FillEllipse (&thumb, fx - 8, ty + th / 2 - 8, 16.f, 16.f);
		}

		// limiter activity
		g.FillRectangle (&bPanel, 0, 258, W, 1);
		float gr = e->shownGr;
		g.DrawString (L"LIMITING", -1, &fLabel, PointF (TX0 - 1, 270), &bDim);
		wchar_t grs[32];
		swprintf (grs, 32, gr < 0.05f ? L"0.0 dB" : L"-%.1f dB", gr);
		REAL mx = 100, mw = 250;
		fillRound (g, C_TRACK, mx, 276, mw, 4, 2);
		REAL gw = (REAL)std::min (1.0f, gr / 12.f) * mw;
		Color meterCol = gr > 6.f ? Color (255, 224, 92, 72) : C_ACCENT;
		if (gw > 1) fillRound (g, meterCol, mx, 276, gw, 4, 2);
		g.DrawString (grs, -1, &fSub, RectF (mx + mw, 269, TX1 - mx - mw, 18), &right, &bDim);
	}
	BitBlt (hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
	SelectObject (mem, old);
	DeleteObject (bmp);
	DeleteDC (mem);
}

static int hitRow (int x, int y)
{
	if (x < TX0 - 14 || x > TX1 + 14) return -1;
	for (int i = 0; i < 3; ++i)
		if (y >= ROW_Y[i] - 4 && y <= ROW_Y[i] + 46) return i;
	return -1;
}

static double xToNorm (int x)
{
	double n = (double)(x - TX0) / (TX1 - TX0);
	return n < 0 ? 0 : (n > 1 ? 1 : n);
}

static LRESULT CALLBACK WndProc (HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
	Editor* e = (Editor*)GetWindowLongPtrW (h, GWLP_USERDATA);
	switch (msg)
	{
		case WM_NCCREATE:
		{
			auto cs = (CREATESTRUCTW*)lp;
			SetWindowLongPtrW (h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
			return DefWindowProcW (h, msg, wp, lp);
		}
		case WM_ERASEBKGND: return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint (h, &ps);
			if (e) paint (e, hdc);
			EndPaint (h, &ps);
			return 0;
		}
		case WM_TIMER:
		{
			if (!e) break;
			float target = e->plug->gainReduction ();
			float s = e->shownGr;
			s = target > s ? target : s + (target - s) * 0.25f;
			if (std::fabs (s - e->shownGr) > 0.01f)
			{
				e->shownGr = s;
				RECT r {0, 260, W, H};
				InvalidateRect (h, &r, FALSE);
			}
			return 0;
		}
		case WM_LBUTTONDOWN:
		{
			if (!e) break;
			int x = GET_X_LPARAM (lp), y = GET_Y_LPARAM (lp);
			POINT pt {x, y};
			if (PtInRect (&BYPASS_RC, pt))
			{
				double b = e->plug->getParamNormalized (kBypass) > 0.5 ? 0.0 : 1.0;
				e->plug->editParam (kBypass, b, 0);
				e->plug->editParam (kBypass, b, 2);
				InvalidateRect (h, nullptr, FALSE);
				return 0;
			}
			int row = hitRow (x, y);
			if (row >= 0)
			{
				e->dragParam = row;
				SetCapture (h);
				e->plug->editParam (ROW_ID[row], xToNorm (x), 0);
				InvalidateRect (h, nullptr, FALSE);
			}
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			if (e && e->dragParam >= 0)
			{
				e->plug->editParam (ROW_ID[e->dragParam], xToNorm (GET_X_LPARAM (lp)), 1);
				InvalidateRect (h, nullptr, FALSE);
			}
			return 0;
		}
		case WM_LBUTTONUP:
		case WM_CAPTURECHANGED:
		{
			if (e && e->dragParam >= 0)
			{
				int row = e->dragParam;
				e->dragParam = -1;
				e->plug->editParam (ROW_ID[row], e->plug->getParamNormalized (ROW_ID[row]), 2);
				if (msg == WM_LBUTTONUP) ReleaseCapture ();
				InvalidateRect (h, nullptr, FALSE);
			}
			return 0;
		}
		case WM_LBUTTONDBLCLK:
		{
			if (!e) break;
			int row = hitRow (GET_X_LPARAM (lp), GET_Y_LPARAM (lp));
			if (row >= 0)
			{
				e->plug->editParam (ROW_ID[row], ROW_DEFAULT[row], 0);
				e->plug->editParam (ROW_ID[row], ROW_DEFAULT[row], 2);
				InvalidateRect (h, nullptr, FALSE);
			}
			return 0;
		}
		case WM_MOUSEWHEEL:
		{
			if (!e) break;
			POINT pt {GET_X_LPARAM (lp), GET_Y_LPARAM (lp)};
			ScreenToClient (h, &pt);
			int row = hitRow (pt.x, pt.y);
			if (row >= 0)
			{
				double step = (GET_WHEEL_DELTA_WPARAM (wp) > 0 ? 1.0 : -1.0) / 48.0;
				double v = e->plug->getParamNormalized (ROW_ID[row]) + step;
				e->plug->editParam (ROW_ID[row], v, 0);
				e->plug->editParam (ROW_ID[row], v, 2);
				InvalidateRect (h, nullptr, FALSE);
			}
			return 0;
		}
	}
	return DefWindowProcW (h, msg, wp, lp);
}

} // namespace dlb
