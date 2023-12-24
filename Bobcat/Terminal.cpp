// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME "Terminal"
#define KEYNAMESPACE TermKeys
#define KEYFILE <Bobcat/Terminal.key>
#include <CtrlLib/key_source.h>

using namespace TermKeys;

Terminal::Terminal(Bobcat& ctx_)
: ctx(ctx_)
, bell(true)
, keep(false)
, filter(false)
{
    titlebar.newterm << [this] { ctx.AddTerminal(ctx.settings.activeprofile); };
    titlebar.close   << [this] { Stop(); };
    titlebar.menu    << [this] { MenuBar::Execute([this](Bar& bar) { ctx.ListMenu(bar); }); };
    InlineImages().Hyperlinks().WindowOps().DynamicColors().SetWantFocus();

    WhenBar     = [this](Bar& bar)             { ContextMenu(bar);                };
    WhenBell    = [this]()                     { if(bell) BeepExclamation();      };
    WhenResize  = [this]()                     { pty.SetSize(GetPageSize());      };
    WhenOutput  = [this](String s)             { pty.Write(s);                    };
    WhenTitle   = [this](const String& s)      { MakeTitle(s);                    };
    WhenSetSize = [this](Size sz)              { ctx.Resize(sz);                  };
    WhenClip    = [this](PasteClip& dnd)       { return filter;                   };
    WhenWindowMinimize       = [this](bool b)  { ctx.Minimize(b);                 };
    WhenWindowMaximize       = [this](bool b)  { ctx.Maximize(b);                 };
    WhenWindowFullScreen     = [this](int i)   { ctx.FullScreen(i);               };
    WhenWindowGeometryChange = [this](Rect r)  { ctx.SetRect(r);                  };
}

bool Terminal::Start(const Profile& p)
{
	SetProfile(p);
	SetPalette(p.palette);
	
	#ifdef PLATFORM_POSIX
	pty.WhenAttrs = [=, &p](termios& t) -> bool
	{
		t.c_iflag |= IXANY;
		#ifdef IUTF8
		if(IsUtf8Mode())
			t.c_iflag |= IUTF8;
		else
			t.c_iflag &= ~IUTF8;
		#endif
		int chr = decode(p.erasechar, "backspace", 0x08, 0x7F);
		t.c_cc[VERASE] = chr;
		Echo(Format("\033[?67%[1:h;l]s", chr == 0x08));
		return true;
	};
	#endif
	
	VectorMap<String, String> vv = clone(Environment());
	MemReadStream ms(p.env, p.env.GetLength());
	while(!ms.IsEof()) {
		String k, v;
		if(SplitTo(ms.GetLine(), '=', k, v)) {
			vv.GetAdd(k, v);
		}
	}
	if(pty.Start(p.command, vv, p.address)) {
		profilename = p.name;
		MakeTitle(profilename);
		ctx.stack.Add(*this);
		return true;
	}
	const char *txt = t_("Command execution failed.&&Profile: %s&Command: %s&Exit code: %d");
	ErrorOK(Format(txt, p.name, p.command, pty.GetExitCode()));
	return false;
}

bool Terminal::Start(const String& profile_name)
{
	Profile p = LoadProfile(profile_name);
	return Start(p);
}

void Terminal::Stop()
{
	if(!pty.IsRunning())
		return;
	pty.Kill();
}

int Terminal::Do()
{
	String s = pty.Get();
	Write(s, IsUtf8Mode());
	return CanExit() ? (pty.IsRunning() ? s.GetLength() : -1) : 0;
}

void Terminal::Reset()
{
	HardReset();
}

bool Terminal::CanExit() const
{
	return !keep;
}

hash_t Terminal::GetHashValue() const
{
	return profilename.GetHashValue();;
}

Terminal& Terminal::Sync()
{
	bool b = ctx.stack.GetCount() > 1;
	titlebar.menu.Show(!ctx.HasMenuBar() && b);
	titlebar.newterm.Show(!ctx.HasMenuBar());
	titlebar.close.Show(b || ctx.window.IsFullScreen());
	ShowTitleBar(ctx.settings.showtitle);
	titlebar <<= ctx.settings.titlealignment;
	return *this;
}

Terminal& Terminal::SetProfile(const Profile& p)
{
	bell = p.bell;
	filter = p.filterctrl;
	keep = p.onexit == "keep";
	Hyperlinks(p.hyperlinks);
	WindowActions(p.windowactions);
	WindowReports(p.windowreports);
	History(p.history);
	SetHistorySize(p.historysize);
	LightColors(p.lightcolors);
	AdjustColors(p.adjustcolors);
	IntensifyBoldText(p.intensify);
	DynamicColors(p.dynamiccolors);
	BlinkingText(p.blinktext);
	LockCursor(p.lockcursor);
	SetCursorStyle(p.cursorstyle);
	BlinkingCursor(p.blinkcursor);
	PermitClipboardRead(p.clipboardread);
	PermitClipboardWrite(p.clipboardwrite);
	MouseWheelStep(p.mousewheelstep);
	AutoHideMouseCursor(p.autohidemouse);
	InlineImages(p.inlineimages);
	MetaEscapesKeys(p.altescapeskeys);
	MetaShiftsKeys(p.altshiftskeys);
	KeyNavigation(p.keynavigation);
	PCStyleFunctionKeys(p.functionkeystyle == "pc");
	DelayedRefresh(p.delayedrefresh);
	LazyResize(p.lazyresize);
	SetLocale(p.encoding);
	SetPadding(Size(0, p.linespacing));
	SetFont(p.font);
	OverrideTracking(GetModifierKey(p.overridetracking));
	return *this;
}

Terminal& Terminal::SetPalette(const Palette& p)
{
	for(int i = 0; i< TerminalCtrl::MAX_COLOR_COUNT; i++)
		SetColor(i, p.table[i]);
	return *this;
}


Terminal& Terminal::SetLocale(const String& s)
{
	SetCharset(CharsetByName(Nvl(s, "UTF-8")));
	return *this;
}

Terminal& Terminal::SetEraseKey(const String& s)
{
	Echo(Format("\033[?67%[1:h;l]s", s == "backspace"));
#ifdef PLATFORM_POSIX
	termios tio;
	if(pty.GetAttrs(tio)) {
		tio.c_cc[VERASE] = decode(s, "backspace", 0x08, 0x7f);
		pty.SetAttrs(tio);
	}
#endif
	return *this;
}

Terminal& Terminal::SetCursorStyle(const String& s)
{
	if(s == "block")
		BlockCursor(IsCursorBlinking());
	else
	if(s == "beam")
		BeamCursor(IsCursorBlinking());
	else
	if(s == "underline")
		UnderlineCursor(IsCursorBlinking());
	return *this;
}

Terminal& Terminal::SetFontZoom(int n)
{
	Font f = GetFont();
	int  y = decode(n, 0, GetStdFont().GetHeight(), f.GetHeight());
	SetFont(f.Height(clamp(y + n, 6, 128)));
	return *this;
}

Terminal& Terminal::SetLineSpacing(int n)
{
	SetPadding(Size(0,  decode(n, 0, 0, GetPadding().cy + n)));
	return *this;
}

void Terminal::MakeTitle(const String& s)
{
	title.Clear();
	if(!profilename.IsEmpty())
		title << profilename;
	if(!s.IsEmpty() && s != profilename)
		title << " :: " << s;
	titlebar.title.SetText("\1[g= " << DeQtf(title) << " ]");
	ctx.SyncTitle();
}

String Terminal::GetTitle() const
{
	return title;
}

void Terminal::ShowTitleBar(bool b)
{
	if(!titlebar.IsChild() && b) {
		AddFrame(titlebar.Height(StdFont().GetCy() + Zy(4)));
	}
	else
	if(titlebar.IsChild() && !b)
		RemoveFrame(titlebar);
}

void Terminal::HideTitleBar()
{
	ShowTitleBar(false);
}

bool Terminal::HasTitleBar() const
{
	return titlebar.IsChild();
}

void Terminal::CopyImage()
{
	if(Image img = GetInlineImage(); !IsNull(img))
		AppendClipboardImage(img);
}

void Terminal::OpenImage()
{
	if(Image img = GetInlineImage(); !IsNull(img))
		WhenImage(PNGEncoder().SaveString(img));
}

void Terminal::CopyLink()
{
	if(String uri = GetHyperlinkUri(); !IsNull(uri))
		Copy(uri.ToWString());
}

void Terminal::OpenLink()
{
	if(String uri = GetHyperlinkUri(); !IsNull(uri))
		WhenLink(uri);
}

void Terminal::FileMenu(Bar& menu)
{
}

void Terminal::EditMenu(Bar& menu)
{
	menu.Add(AK_READONLY, [this] { SetEditable(IsReadOnly()); }).Check(IsReadOnly());

	if(IsMouseOverImage()) {
		menu.Separator();
		menu.Add(AK_COPYIMAGE, Images::Copy(), [this] { CopyImage(); });
		menu.Add(AK_OPENIMAGE, CtrlImg::open(), [this] { OpenImage(); });
	}
	else
	if(IsMouseOverHyperlink()) {
		menu.Separator();
		menu.Add(AK_COPYLINK, Images::Copy(), [this] { CopyLink(); });
		menu.Add(AK_OPENLINK, CtrlImg::open(), [this] { OpenLink(); });
	}
	else {
		menu.Separator();
		menu.Add(IsSelection(), AK_COPYTEXT,  Images::Copy(), [this] { Copy();  });
		menu.Add(IsEditable(),  AK_PASTETEXT, Images::Paste(),[this] { Paste(); });
		if(ctx.settings.custominputmethod)
			menu.Add(IsEditable(),  AK_CODEPOINT, Images::InsertUnicode(),  [this] { InsertUnicodeCodePoint(*this); });
		menu.Separator();
		menu.Add(IsEditable(),  AK_SELECTALL, Images::SelectAll(), [this] { SelectAll(); });

	}
}

void Terminal::ViewMenu(Bar& menu)
{
	menu.Separator();
	menu.Add(AK_80X24,         [this] { ctx.Resize(PageSizeToClient(80, 24));  });
	menu.Add(AK_80X48,         [this] { ctx.Resize(PageSizeToClient(80, 48));  });
	menu.Add(AK_132X24,        [this] { ctx.Resize(PageSizeToClient(132, 24)); });
	menu.Add(AK_132X48,        [this] { ctx.Resize(PageSizeToClient(132, 48)); });
	menu.AddKey(AK_ZOOMIN,     [this] { SetFontZoom(1);     });
	menu.AddKey(AK_ZOOMOUT,    [this] { SetFontZoom(-1);    });
	menu.AddKey(AK_NOZOOM,     [this] { SetFontZoom(0);     });
	menu.AddKey(AK_INCPADDING, [this] { SetLineSpacing(1);  });
	menu.AddKey(AK_DECPADDING, [this] { SetLineSpacing(-1); });
	menu.AddKey(AK_NOPADDING,  [this] { SetLineSpacing(0);  });
}

void Terminal::EmulationMenu(Bar& menu)
{
	menu.Add(AK_PCFUNCTIONKEYS, [this] { PCStyleFunctionKeys(!HasPCStyleFunctionKeys()); }).Check(HasPCStyleFunctionKeys());
	menu.Add(AK_KEYNAVIGATION,  [this] { KeyNavigation(!HasKeyNavigation()); }).Check(HasKeyNavigation());
	menu.Add(AK_SCROLLBAR,      [this] { ShowScrollBar(!HasScrollBar()); }).Check(HasScrollBar());
	menu.Add(AK_ALTERNATESCROLL,[this] { AlternateScroll(!HasAlternateScroll()); }).Check(HasAlternateScroll());
	menu.Add(AK_HIDEMOUSE,	    [this] { AutoHideMouseCursor(!IsMouseCursorAutoHidden()); }).Check(IsMouseCursorAutoHidden());
	menu.Add(AK_REVERSEWRAP,    [this] { ReverseWrap(!HasReverseWrap()); }).Check(HasReverseWrap());
	menu.Add(AK_DYNAMICCOLORS,  [this] { DynamicColors(!HasDynamicColors()); }).Check(HasDynamicColors());
	menu.Add(AK_BRIGHTCOLORS,   [this] { LightColors(!HasLightColors()); }).Check(HasLightColors());
	menu.Add(AK_ADJUSTTODARK,   [this] { AdjustColors(!HasAdjustedColors()); }).Check(HasAdjustedColors());
	menu.Add(AK_BLINKINGTEXT,   [this] { BlinkingText(!HasBlinkingText()); }).Check(HasBlinkingText());
	menu.Add(AK_BELL,           [this] { bell = !bell; }).Check(bell);
	menu.Add(AK_INLINEIMAGES,   [this] { InlineImages(!HasInlineImages()); }).Check(HasInlineImages());
	menu.Add(AK_HYPERLINKS,     [this] { Hyperlinks(!HasHyperlinks()); }).Check(HasHyperlinks());
	menu.Add(AK_SIZEHINT,       [this] { ShowSizeHint(!HasSizeHint()); }).Check(HasSizeHint());
	menu.Add(AK_DELAYEDREFRESH, [this] { DelayedRefresh(!IsDelayingRefresh()); }).Check(IsDelayingRefresh());
	menu.Add(AK_LAZYRESIZE,     [this] { LazyResize(!IsLazyResizing()); }).Check(IsLazyResizing());
}

void Terminal::ContextMenu(Bar& menu)
{
	EditMenu(menu);
	menu.Separator();
	ctx.TermMenu(menu);
	menu.Separator();
	menu.Sub(t_("Terminal list"), [this](Bar& menu) { ctx.ListMenu(menu); });
	menu.Separator();
	menu.Sub(t_("Emulation"), [this](Bar& menu) { EmulationMenu(menu); });
	menu.Separator();
	menu.Sub(t_("View"), [this](Bar& menu) { ctx.ViewMenu(menu); });
	menu.Separator();
	ctx.SetupMenu(menu);
	ctx.HelpMenu(menu);
	menu.AddKey(AppKeys::AK_EXIT, [this] { ctx.Close(); });
}

Terminal::TitleBar::TitleBar()
{
	SetData("top");
	Add(title.VSizePosZ(0, 0).HSizePosZ(24, 24));
	Add(newterm.LeftPosZ(4, 12).VCenterPosZ(12, 0));
	Add(menu.LeftPosZ(18, 12).VCenterPosZ(12, 0));
	Add(close.RightPosZ(4, 12).VCenterPosZ(12, 0));
	newterm.Image(Images::New()).Tip(t_("Open new terminal"));
	close.Image(Images::Close()).Tip(t_("Close terminal"));
	menu.Image(CtrlImg::down_arrow()).Tip(t_("Terminal list"));
}

void Terminal::TitleBar::SetData(const Value& v)
{
	data = v;
	if(auto *p = GetParent(); p)
		p->RefreshLayout();
}

Value Terminal::TitleBar::GetData() const
{
	return data;
}

void Terminal::TitleBar::FrameLayout(Rect& r)
{
	data == "bottom"
		? LayoutFrameBottom(r, this, cy ? cy : r.Width())
		: LayoutFrameTop(r, this, cy ? cy : r.Width()); // default
}

}