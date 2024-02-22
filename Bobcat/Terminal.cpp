// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

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
, filter(false)
, finder(*this)
, linkifier(*this)
, titlebar(*this)
, exitmode(ExitMode::Exit)
, highlight {
	Yellow(),
	SColorHighlightText,
	LtRed(),
	SColorHighlight
	}
{
    titlebar.newterm << [this] { ctx.AddTerminal(ctx.GetActiveProfile()); };
    titlebar.close   << [this] { Stop(); };
    titlebar.menu    << [this] { MenuBar::Execute([this](Bar& bar) { ctx.ListMenu(bar); }); };
    InlineImages().Hyperlinks().WindowOps().DynamicColors().WantFocus();

    WhenBar     = [this](Bar& bar)             { ContextMenu(bar);                };
    WhenBell    = [this]()                     { if(bell) BeepExclamation();      };
    WhenResize  = [this]()                     { pty.SetSize(GetPageSize());      };
    WhenScroll  = [this]()                     { Update();                        };
    WhenOutput  = [this](String s)             { pty.Write(s);                    };
    WhenTitle   = [this](const String& s)      { MakeTitle(s);                    };
    WhenLink    = [this](const String& s)      { LaunchWebBrowser(s);             };
    WhenSetSize = [this](Size sz)              { ctx.Resize(sz);                  };
    WhenClip    = [this](PasteClip& dnd)       { return filter;                   };
    WhenWindowMinimize       = [this](bool b)  { ctx.Minimize(b);                 };
    WhenWindowMaximize       = [this](bool b)  { ctx.Maximize(b);                 };
    WhenWindowFullScreen     = [this](int i)   { ctx.FullScreen(i);               };
    WhenWindowGeometryChange = [this](Rect r)  { ctx.SetRect(r);                  };
    WhenHighlight = THISFN(OnHighlight);
}

void Terminal::SetData(const Value& v)
{
	data = v;
}

Value Terminal::GetData() const
{
	return data;
}

void Terminal::PostParse()
{
	TerminalCtrl::PostParse();
	Update();
}

bool Terminal::StartPty(const Profile& p)
{
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

	VectorMap<String, String> vv;
	if(!p.noenv)
		vv = clone(Environment());
	MemReadStream ms(p.env, p.env.GetLength());
	while(!ms.IsEof()) {
		String k, v;
		if(SplitTo(ms.GetLine(), '=', k, v)) {
			vv.GetAdd(k) = v;
		}
	}
	
	if(pty.Start(p.command, vv, p.address)) {
		MakeTitle(profilename);
		if(ctx.stack.Find(*this) < 0)
			ctx.stack.Add(*this);
		return true;
	}
	const char *txt = t_("Command execution failed.&&Profile: %s&Command: %s&Exit code: %d");
	ErrorOK(Format(txt, p.name, p.command, pty.GetExitCode()));
	return false;
}

bool Terminal::Start(const Profile& p)
{
	SetProfile(p);
	SetPalette(LoadPalette(p.palette));
	return StartPty(p);
}

bool Terminal::Start(const String& profile_name)
{
	Profile p = LoadProfile(profile_name);
	return Start(p);
}

void Terminal::Stop()
{
	exitmode = ExitMode::Exit;
	if(!pty.IsRunning())
		return;
	pty.Kill();
}

int Terminal::Do()
{
	String s = pty.Get();
	Write(s, IsUtf8Mode());
	if(pty.IsRunning())
		return s.GetLength();
	if(ShouldExit())
		return  -1;
	if(ShouldRestart())
		Restart();
	return 0;
}

void Terminal::Restart()
{
	StartPty(LoadProfile(profilename));
}

void Terminal::Reset()
{
	HardReset();
}

bool Terminal::ShouldExit() const
{
	return exitmode == ExitMode::Exit;
}

bool Terminal::ShouldKeep() const
{
	return exitmode == ExitMode::Keep;
}

bool Terminal::ShouldRestart() const
{
	return exitmode == ExitMode::Restart;
}

hash_t Terminal::GetHashValue() const
{
	return profilename.GetHashValue();
}

void Terminal::Update()
{
	if(!IsVisible() || (!HasFinder() && !linkifier.IsEnabled()))
		return;

	auto cb = [this]()
	{
		SyncHighlight();
		linkifier.Update();
		finder.Update();
		if(!HasFinder()) // Finder, if visible, refreshes the display.
			Refresh();
	};
	
	timer.KillSet(20, cb); // Safeguard against spamming.
}

void Terminal::SyncHighlight()
{
	EnableHighlight(HasFinder() || HasLinkifier());
	Refresh();
}

Terminal& Terminal::Sync()
{
	bool b = ctx.stack.GetCount() > 1;
	titlebar.menu.Show(!ctx.HasMenuBar() && b);
	titlebar.newterm.Show(!ctx.HasMenuBar());
	titlebar.close.Show(b || ctx.window.IsFullScreen());
	ShowTitleBar(ctx.settings.showtitle);
	titlebar <<= ctx.settings.titlealignment;
	finder   <<= ctx.settings.finderalignment;
	Update();
	return *this;
}

void Terminal::Layout()
{
	TerminalCtrl::Layout();
	Update();
}

Terminal& Terminal::SetProfile(const Profile& p)
{
	profilename = p.name;
	bell = p.bell;
	filter = p.filterctrl;
	WindowActions(p.windowactions);
	WindowReports(p.windowreports);
	History(p.history);
	SetExitMode(p.onexit);
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
	ShowSizeHint(p.sizehint);
	SetLocale(p.encoding);
	SetPadding(Size(0, p.linespacing));
	SetFont(p.font);
	AlternateScroll(p.alternatescroll);
	ScrollToEnd(!p.dontscrolltoend);
	OverrideTracking(GetModifierKey(p.overridetracking));
	Hyperlinks(p.hyperlinks);
	finder.SetConfig(p.finder);
	return *this;
}

void Terminal::MouseEnter(Point pt, dword keyflags)
{
	linkifier.UpdatePos();
	Refresh();
}

void Terminal::MouseLeave()
{
	linkifier.ClearPos();
	Refresh();
}

void Terminal::MouseMove(Point pt, dword keyflags)
{
	if(linkifier.Sync())
		Refresh();
	TerminalCtrl::MouseMove(pt, keyflags);
}

void Terminal::LeftDouble(Point pt, dword keyflags)
{
	if((keyflags & K_CTRL) == K_CTRL
	&& !IsMouseOverExplicitHyperlink()
	&&  IsMouseOverImplicitHyperlink()) {
		OpenLink();
	}
	else
		TerminalCtrl::LeftDouble(pt, keyflags);
}

Image Terminal::CursorImage(Point pt, dword keyflags)
{
	if(IsMouseOverImplicitHyperlink())
		return Image::Hand();
	return TerminalCtrl::CursorImage(pt, keyflags);
}

Terminal& Terminal::SetPalette(const Palette& p)
{
	int i = 0;
	for(int i = 0, j = 0; i < Palette::MAX_COLOR_COUNT; i++) {
		if(i < TerminalCtrl::MAX_COLOR_COUNT)
			SetColor(i, p.table[i]);
		else
			highlight[j++] = p.table[i];
	}
	return *this;
}

Terminal& Terminal::SetExitMode(const String& s)
{
	exitmode = decode(s,
        "restart", ExitMode::Restart,
        "keep",    ExitMode::Keep,
     /* "exit" */  ExitMode::Exit);
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
	String title;
	if(!profilename.IsEmpty())
		title << profilename;
	if(!s.IsEmpty() && s != profilename)
		title << " :: " << s;
	titlebar.title.SetText("\1[g= " << DeQtf(title) << " ]");
	SetData(title);
	ctx.SyncTitle();
}

String Terminal::GetTitle() const
{
	return GetData();
}

void Terminal::ShowTitleBar(bool b)
{
	if(!titlebar.IsChild() && b) {
		titlebar.Show();
	}
	else
	if(titlebar.IsChild() && !b)
		titlebar.Hide();
}

void Terminal::HideTitleBar()
{
	ShowTitleBar(false);
}

bool Terminal::HasTitleBar() const
{
	return titlebar.IsChild();
}

void Terminal::ShowFinder(bool b)
{
	b ? finder.Show() : finder.Hide();
}

void Terminal::HideFinder()
{
	ShowFinder(false);
}

bool Terminal::HasFinder() const
{
	return finder.IsChild();
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

bool Terminal::HasLinkifier() const
{
	if(!linkifier.IsEnabled())
		return false;
	const auto& m = GetHyperlinkPatterns();
	int i = m.Find(profilename);
	return i >= 0 && m[i].GetCount();
}

void Terminal::Hyperlinks(bool b)
{
	TerminalCtrl::Hyperlinks(b);
	linkifier.Enable(b);
	Update();
}

bool Terminal::HasHyperlinks() const
{
	return TerminalCtrl::HasHyperlinks() || HasLinkifier();
}

bool Terminal::IsMouseOverExplicitHyperlink()
{
	return IsMouseOverHyperlink();
}

bool Terminal::IsMouseOverImplicitHyperlink()
{
	return linkifier.IsCursor();
}

bool Terminal::IsMouseOverLink()
{
	return IsMouseOverExplicitHyperlink() || IsMouseOverImplicitHyperlink();
}

String Terminal::GetLink()
{
	if(IsMouseOverExplicitHyperlink())
		return GetHyperlinkUri();
	if(IsMouseOverImplicitHyperlink())
		return linkifier.GetCurrentLinkInfo().url;
	return Null;
}

void Terminal::CopyLink()
{
	if(String uri = GetLink(); !IsNull(uri))
		Copy(uri.ToWString());
	linkifier.ClearCursor();
}

void Terminal::OpenLink()
{
	if(String uri = GetLink(); !IsNull(uri))
		WhenLink(uri);
	linkifier.ClearCursor();
}

int Terminal::GetPosAsIndex(Point pt)
{
	if(const VTPage& p = GetPage(); pt.y >= 0 && pt.y < p.GetLineCount()) {
		int i = 0, n = 0;
		while(i < pt.y)
			n += p.FetchLine(i++).GetCount();
		return n + pt.x;
	}
	return -1;
}

int Terminal::GetMousePosAsIndex()
{
	return GetPosAsIndex(GetMousePagePos());
}

void Terminal::OnHighlight(VectorMap<int, VTLine>& hl)
{
	linkifier.OnHighlight(hl);
	finder.OnHighlight(hl);
}

void Terminal::FileMenu(Bar& menu)
{
}

void Terminal::EditMenu(Bar& menu)
{
	menu.Add(AK_READONLY, [this] { SetEditable(IsReadOnly()); }).Check(IsReadOnly());

	if(IsMouseOverImage()) {
		menu.Separator();
		menu.Add(AK_COPYIMAGE, Images::Copy(),  [this] { CopyImage(); });
		menu.Add(AK_OPENIMAGE, CtrlImg::open(), [this] { OpenImage(); });
	}
	else
	if(IsMouseOverLink()) {
		menu.Separator();
		menu.Add(AK_COPYLINK, Images::Copy(),  [this] { CopyLink(); });
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
	menu.Separator();
	menu.Add(AK_FINDER, Images::Find(), [this] { ShowFinder(true); });
}

void Terminal::ViewMenu(Bar& menu)
{
	ctx.SizeMenu(menu);
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
	menu.Add(AK_NOSCROLL,       [this] { ScrollToEnd(!IsScrollingToEnd()); }).Check(IsScrollingToEnd());
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
	menu.AddKey(AK_CLOSE, [this] { Stop(); });
	menu.AddKey(AppKeys::AK_EXIT, [this] { ctx.Close(); });
}

Terminal::TitleBar::TitleBar(Terminal& ctx_)
: ctx(ctx_)
{
	SetData("top");
	Add(title.VSizePosZ(0, 0).HSizePosZ(24, 24));
	Add(newterm.LeftPosZ(4, 12).VCenterPosZ(12, 0));
	Add(menu.LeftPosZ(18, 12).VCenterPosZ(12, 0));
	Add(close.RightPosZ(4, 12).VCenterPosZ(12, 0));
	newterm.Image(Images::Add()).Tip(t_("Open new terminal"));
	close.Image(Images::Delete()).Tip(t_("Close terminal"));
	menu.Image(CtrlImg::down_arrow()).Tip(t_("Terminal list"));
}

void Terminal::TitleBar::SetData(const Value& v)
{
	data = v;
	ctx.RefreshLayout();
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

void Terminal::TitleBar::Show()
{
	bool b = ctx.HasSizeHint();
	ctx.HideSizeHint();
	ctx.InsertFrame(0, Height(StdFont().GetCy() + Zy(4)));
	ctx.ShowSizeHint(b);
}

void Terminal::TitleBar::Hide()
{
	bool b = ctx.HasSizeHint();
	ctx.HideSizeHint();
	ctx.RemoveFrame(*this);
	ctx.ShowSizeHint(b);
	ctx.SetFocus();
}

Terminal& AsTerminal(Ctrl& c)
{
	return static_cast<Terminal&>(c);
}

}