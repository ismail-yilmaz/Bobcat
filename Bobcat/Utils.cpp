// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#ifndef bmYEAR
#include <build_info.h>
#endif

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

struct FontListDisplayCls : Display
{
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		AttrText txt = q;
		txt.font = Font().FaceName(q).Height(Draw::GetStdFontSize().cy);
		StdDisplay().Paint(w, r, txt, ink, paper, style);
	}
};

struct StdBackgroundDisplayCls : Display
{
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		w.DrawRect(r, SColorPaper);
	}
};

struct NormalImageDisplayCls : Display
{
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		w.DrawImage(0, 0, q);
	}
};

struct TiledImageDisplayCls : Display
{
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		DrawTiles(w, r, q);
	}
};

const Display& StdBackgroundDisplay()   { return Single<StdBackgroundDisplayCls>(); }
const Display& NormalImageDisplay()     { return Single<NormalImageDisplayCls>(); }
const Display& TiledImageDisplay()      { return Single<TiledImageDisplayCls>(); }
const Display& FontListDisplay()        { return Single<FontListDisplayCls>(); }

Font SelectFont(Font f, dword type)
{
	WithFontSelectorLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, tt_("Select Font"));

	dlg.font.SetDisplay(FontListDisplay());
	dlg.font.WhenSel = [&]
	{
		dlg.preview.SetFont(Font().FaceName(~dlg.font).Height(~dlg.fontsize));
	};
	dlg.fontsize <<= IsNull(f) ? StdFont().GetHeight() : f.GetHeight();
	dlg.fontsize << [&] { dlg.slider  <<= ~dlg.fontsize;  dlg.font.WhenSel(); };
	dlg.slider.MinMax(6, 128).Step(1) <<= ~dlg.fontsize;
	dlg.slider << [&] { dlg.fontsize  <<= ~dlg.slider; dlg.font.WhenSel(); };
	
	int h = Draw::GetStdFontSize().cy + 8;
	for(int i = 0; i < Font::GetFaceCount(); i++) {
		dword fi = Font::GetFaceInfo(i);
		if(((type & Font::FIXEDPITCH) & fi) || ((type & Font::SCALEABLE) & fi)) {
			String face = Font::GetFaceName(i);
			if(dlg.font.Find(face) < 0)
				dlg.font.Add(face);
		}
		h = max(Font().Face(i).GetCy(), h);
	}

	dlg.font.ItemHeight(h);

	int i = dlg.font.Find(f.GetFaceName());
	if(i >= 0) dlg.font.SetCursor(i);
	
	if(dlg.ExecuteOK())
		f.FaceName(~dlg.font).Height(~dlg.fontsize);
	return f;
}


struct EditCodePoint : EditString {
	Terminal& term;
	EditCodePoint(Terminal& t) : term(t)
	{
	}

	Rect GetCaret() const override
	{
		return Null;
	}
	
	void PopUp()
	{
		if(!term.GetRect().Contains(term.GetCursorPoint()))
			return;

		Font f = term.GetFont();
		Size sz = term.GetCellSize();
		Point pt = term.GetScreenView().TopLeft() + term.GetCursorPoint();
		Color paper = term.GetColor(TerminalCtrl::COLOR_PAPER);
		Color ink   = term.GetColor(TerminalCtrl::COLOR_INK);
		FrameLeft<DisplayCtrl> label;
		FrameRight<DisplayCtrl> preview;
		preview.SetDisplay(StdCenterDisplay());
		label.SetDisplay(StdRightDisplay());
		label.SetData(AttrText("U+").SetFont(f).Ink(SColorDisabled));
		AddFrame(label.Width(sz.cx * 2));
		AddFrame(preview.Width(sz.cx * 3));
		MaxChars(5).SetFont(f).SetFilter([](int c) { return IsXDigit(c) ? c : 0; });
		WhenEnter = term.ctx.window.Breaker();
		WhenAction = [=, &preview, &f] {
			AttrText txt;
			txt.Ink(ink).Paper(paper).SetFont(f);
			int n = ScanInt(GetText(), nullptr, 16);
			if(n >= 0x00 && n <= 0x1F)
				txt.Text("C0").Ink(SLtRed);
			else
			if(n >= 0x80 && n <= 0x9F)
				txt.Text("C1").Ink(SLtRed);
			else
				txt = WString(n, 1);
			preview.SetData(txt);
		};
		Rect wa = GetWorkArea();
		Rect r  = RectC(pt.x + 2, pt.y - 4, sz.cx * 12, sz.cy + 8);
		SetRect(r.right > wa.right ? r.OffsetedHorz(wa.right - r.right) : r);
		Ctrl::PopUp(&term.ctx.window, true, true, false, false);
		EventLoop(&term.ctx.window);
	}
	
	bool Key(dword key, int count) override
	{
		if(key == K_ESCAPE) {
			SetData(Null);
			term.ctx.window.Break();
			return true;
		}
		return EditString::Key(key, count);
	}
	
	void LostFocus() override
	{
		Key(K_ESCAPE, 1);
	}
};

void InsertUnicodeCodePoint(Terminal& t)
{
	// Pop up the unicode codepoint input widget at cursor position.

	EditCodePoint q(t);
	q.PopUp();
	if(!IsNull(~q)) {
		dword n = ScanInt(q.GetText(), nullptr, 16);
		t.Key(n, 1);
	}
}

bool AnnotationEditor(String& s, const char *title)
{
	WithAnnotationEditorLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, title);
	dlg.text <<= s.ToWString();
	dlg.text.SetFilter(CharFilterUnicode);
	if(dlg.Sizeable().ExecuteOK()) {
		s = ~dlg.text;
		return true;
	}
	return false;
}

static const Tuple<dword, const char*> mod_keys[] = {
    { K_SHIFT,              "K_SHIFT" },
    { K_ALT,                "K_ALT"   },
    { K_CTRL,               "K_CTRL"  },
    { K_CTRL|K_ALT,         "K_CTRL_ALT" },
    { K_SHIFT|K_CTRL,       "K_SHIFT_CTRL" },
    { K_SHIFT|K_ALT,        "K_SHIFT_ALT"  },
    { K_SHIFT|K_CTRL|K_ALT, "K_SHIFT_CTRL_ALT" }
#ifdef PLATFORM_COCOA
    { K_OPTION,                      "K_OPTION"      },
    { K_OPTION|K_SHIFT,              "K_SHIFT_OPTION " },
    { K_OPTION|K_ALT,                "K_ALT_OPTION " },
    { K_OPTION|K_CTRL,               "K_CTRL_OPTION" },
    { K_OPTION|K_CTRL|K_ALT          "K_CTRL_ALT_OPTION " },
    { K_OPTION|K_SHIFT|K_ALT         "K_SHIFT_ALT_OPTION " },
    { K_OPTION|K_ALT,                "K_SHIFT_CTRL_OPTION " },
    { K_OPTION|K_SHIFT|K_CTRL|K_ALT, "K_SHIFT_CTRL_ALT_OPTION" },
#endif
};

dword GetModifierKey(String s)
{
	for(int i = 0; i < __countof(mod_keys); i++)
		if(mod_keys[i].b == s)
			return mod_keys[i].a;
	return 0;
}

String GetModifierKeyDesc(dword key)
{
	key &= (K_SHIFT|K_CTRL|K_ALT);
	for(int i = 0; i < __countof(mod_keys); i++)
		if(mod_keys[i].a == key)
			return mod_keys[i].b;
	return String::GetVoid();
}

String GetVersion()
{
#ifdef bmGIT_REVCOUNT
	return AsString(atoi(bmGIT_REVCOUNT) + Date(23, 12, 19).Get());
#endif
	return "(pre-alpha)";
}

String GetBuildInfo()
{
	String h;
	
	h << "Version: " << GetVersion();
	h << '\n';
	if(sizeof(void *) == 8)
		h << "(64 bit)";
	else
		h << "(32 bit)";
#ifdef _MSC_VER
	h << " (MSC)";
#endif
#if __GNUC__
#if __clang__
	h << " (CLANG)";
#else
	h << " (GCC)";
#endif
#endif

#if __cplusplus >= 202000
	h << " (C++20)";
#elif __cplusplus >= 201700
	h << " (C++17)";
#endif

#if CPU_ARM
	h << " (ARM)";
#endif

#if CPU_SIMD
	h << " (SIMD)";
#endif

#ifdef GUI_GTK
	h << " (Gtk)";
#elif  flagWEBGUI
	h << " (Turtle)";
#elif  flagSDLGUI
	h << " (SDL2-GL)";
#endif
	h << '\n';
#ifdef bmTIME
	h << "Compiled: " << bmTIME;
#endif

	h << '\n';;
	h << GetExeFilePath();

	return h;
}

Vector<Tuple<void (*)(), String, String>> GetAllGuiThemes()
{
	return Vector<Tuple<void (*)(), String, String>> {
		{ ChHostSkin, "host", tt_("Host platform") },
	    { ChClassicSkin, "classic", tt_("Classic") },
		{ ChStdSkin, "standard", tt_("Standard") },
		{ ChGraySkin, "gray", tt_("Gray") },
		{ ChDarkSkin, "dark", tt_("Dark") },
		{ ChFlatSkin, "flat", tt_("Flat") },
		{ ChFlatGraySkin, "flatgray", tt_("Flat Gray") },
		{ ChFlatDarkSkin, "flatdark", tt_("Flat Dark") }
	};
}

void LoadGuiTheme(Bobcat& ctx)
{
	for(const auto& ch : GetAllGuiThemes()) {
		if(ctx.settings.guitheme == ch.b) {
			Ctrl::SetSkin(ch.a);
			return;
		}
	}
}

void LoadGuiFont(Bobcat& ctx)
{
	SetStdFont(Nvl(ctx.settings.guifont, GetStdFont()));
}

void NotifyDesktop(const String& title, const String& text, int timeout)
{
	// TODO: This should be implemented across the supported platforms (GTK, WINDOWS, X11)
	
#ifdef GUI_GTK
	if(!notify_is_initted() && !notify_init(title))
		return;
	GError *error = nullptr;
	NotifyNotification *notification = notify_notification_new (
					~title,
					~text,
					"gtk-dialog-info" // TODO: Support all types (warning, error, etc.)
					#ifndef NOTIFY_VERSION_GT_0_7_0
					, nullptr
					#endif
					);
	notify_notification_set_timeout(notification, timeout * 1000);
	notify_notification_show(notification, &error);
	notify_uninit();
#endif
}
}