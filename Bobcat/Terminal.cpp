// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME TERMINALCTRL_KEYGROUPNAME
#define KEYNAMESPACE TerminalCtrlKeys
#define KEYFILE <Bobcat/Terminal.key>
#include <CtrlLib/key_source.h>

using namespace TerminalCtrlKeys;

Terminal::Terminal(Bobcat& ctx_)
: ctx(ctx_)
, bell(true)
, filter(false)
, smartwordsel(false)
, shellintegration(false)
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
    WhenLink    = [this](const String& s)      { OnLink(s);                       };
    WhenSetSize = [this](Size sz)              { ctx.Resize(sz);                  };
    WhenClip    = [this](PasteClip& dnd)       { return filter;                   };
    WhenWindowMinimize       = [this](bool b)  { ctx.Minimize(b);                 };
    WhenWindowMaximize       = [this](bool b)  { ctx.Maximize(b);                 };
    WhenWindowFullScreen     = [this](int i)   { ctx.FullScreen(i);               };
    WhenWindowGeometryChange = [this](Rect r)  { ctx.SetRect(r);                  };
    WhenDirectoryChange      = THISFN(SetWorkingDirectory);
    WhenHighlight = THISFN(OnHighlight);
    WhenAnnotation = THISFN(OnAnnotation);
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
	pty.WhenAttrs = [this, &p](termios& t) -> bool
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
	
	MakeTitle(profilename);
	if(ctx.stack.Find(*this) < 0)
		ctx.stack.Add(*this);
	if(pty.Start(p.command, vv, p.address)) {
		pty.SetSize(GetPageSize());
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

bool Terminal::Start(Terminal *term)
{
	Profile p;
	if(term) {
		p = LoadProfile(term->profilename);
		if(term->shellintegration && !IsNull(term->workingdir)) {
			p.address = term->workingdir;
		}
	}
	return Start(p);
}

void Terminal::Restart()
{
	SoftReset();
	StartPty(LoadProfile(profilename));
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
	bool failed = pty.GetExitCode() != 0;
	if(ShouldExit(failed))
		return -1;
	if(ShouldRestart(failed))
		Restart();
	return 0;
}

void Terminal::Reset()
{
	HardReset();
}

bool Terminal::ShouldExit(bool failed) const
{
	return exitmode == ExitMode::Exit || (!failed && exitmode == ExitMode::RestartFailed);
}

bool Terminal::ShouldRestart(bool failed) const
{
	return exitmode == ExitMode::Restart || (failed && exitmode == ExitMode::RestartFailed);
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
	workingdir  = p.address;
	shellintegration = p.shellintegration;
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
	BlinkInterval(p.blinkinterval);
	UnlockCursor();
	SetCursorStyle(p.cursorstyle);
	BlinkingCursor(p.blinkcursor);
	LockCursor(p.lockcursor);
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
	Annotations(p.annotations);
	SetWordSelectionFilter(p.wordselchars);
	SetWordSelectionPattern(p.wordselpattern);
	smartwordsel = p.wordselmode == "smart";
	finder.SetConfig(p);
	linkifier.SetConfig(p);
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
        "restart",        ExitMode::Restart,
        "restart_failed", ExitMode::RestartFailed,
        "keep",           ExitMode::Keep,
     /* "exit" */         ExitMode::Exit);
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

Terminal& Terminal::SetWordSelectionFilter(const String& s)
{
	TerminalCtrl::SetWordSelectionFilter([s = clone(s)](const VTCell& cell) {
		return !cell.IsImage() && (IsLeNum(cell) || FindIndex(s, cell.chr) >= 0);
	});
	return *this;
}

Terminal& Terminal::SetWordSelectionPattern(const String& s)
{
	// Acquire the pattern only once.
	if(auto& m = GetWordSelectionPatterns(); !IsNull(profilename) && m.Find(profilename) < 0) {
		m.Add(profilename) = s;
	}
	return *this;
}

void Terminal::SetWorkingDirectory(const String& s)
{
	if(shellintegration && !IsNull(s)) {
		if(UrlInfo url(s); url.scheme == "file" && !IsNull(url.path) && DirectoryExists(url.path))
			workingdir = url.path;
	}
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

bool Terminal::IsEditable()
{
	return TerminalCtrl::IsEditable() && !TerminalCtrl::IsSelectorMode();
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

void Terminal::CopyLink(const String& s)
{
	Copy(s.ToWString());
	linkifier.ClearCursor();
}

void Terminal::CopyLink()
{
	if(String uri = GetLink(); !IsNull(uri))
		CopyLink(uri);
}

void Terminal::OpenLink(const String& s)
{
	OnLink(s);
	linkifier.ClearCursor();
}

void Terminal::OpenLink()
{
	if(String uri = GetLink(); !IsNull(uri))
		OpenLink(uri);
}

void Terminal::OnLink(const String& s)
{
	LaunchWebBrowser(s);
}

void Terminal::CopyAnnotation(const String& s)
{
	Copy(s.ToWString());
}

bool Terminal::OnAnnotation(Point pt, String& s)
{
	return AnnotationEditor(s, t_("Annotation"));
}

const VTPage& Terminal::GetPage() const
{
	return TerminalCtrl::GetPage();
}

int Terminal::GetPosAsIndex(Point pt, bool relative) const
{
	const VTPage& page = TerminalCtrl::GetPage();
	int n = GetLength(page, relative ? GetSbPos() : 0, pt.y);
	return n + pt.x - GetOffset(page.FetchLine(pt.y), 0, pt.x);
}

int Terminal::GetMousePosAsIndex() const
{
	return GetPosAsIndex(GetMousePagePos(), true);
}

void Terminal::OnHighlight(VectorMap<int, VTLine>& hl)
{
	linkifier.OnHighlight(hl);
	finder.OnHighlight(hl);
}

bool Terminal::GetWordSelection(const Point& pt, Point& pl, Point& ph) const
{
	if(smartwordsel && GetWordSelectionByPattern(pt, pl, ph))
		return true;
	return TerminalCtrl::GetWordSelection(pt, pl, ph);
}

bool Terminal::GetWordSelectionByPattern(const Point& pt, Point& pl, Point& ph) const
{
	const String& sp = GetWordSelectionPatterns().Get(profilename);
	if(IsNull(sp))
		return false;

	pl = ph = pt;

	const VTPage& page = TerminalCtrl::GetPage();
	int linespan = 0, maxlinespan = max(1, (2048 / GetPageSize().cx) / 2);

	while(linespan < maxlinespan) {
		VectorMap<int, VTLine> m;
		auto fn = [&](int n, const VTLine& l) {
			m.Add(n, clone(l));
			return false;
		};
		page.FetchLine(pt.y, fn, ++linespan);

		WString q;
		for(const VTLine& l : m)
			q << l.ToWString();

		if(q.IsEmpty())
			return false;

		int pos = GetLength(page, m.GetKey(0), pt.y);
		pos += pt.x - GetOffset(page.FetchLine(pt.y), 0, pt.x);
		
		RegExp r(sp);
		String l = ToUtf8(q);
		while(r.GlobalMatch(l)) {
			int o = r.GetOffset();
			int begin = Utf32Len(~l, o);
			int end   = begin + Utf32Len(~l + o, r.GetLength());
			if(pos >= begin && pos < end) {
				for(int col = 0, row = 0, offset = 0; row < m.GetCount(); row++) {
					const VTLine& l = m[row];
					for(int j = 0; j < l.GetCount(); j++, col++) {
						offset += l[j] == 1;
						if(col == begin + offset) { // anchor
							pl.x = j;
							pl.y = m.GetKey(row);
						}
						else
						if(col == end - 1 + offset) { // selection
							ph.x = 1 + j;
							ph.y = m.GetKey(row);
							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
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
	if(HasHyperlinks() && IsMouseOverLink()) {
		menu.Separator();
		String lnk = GetLink();
		menu.Add(AK_COPYLINK, Images::Copy(),  [this, lnk = pick(lnk)] { CopyLink(lnk); });
		menu.Add(AK_OPENLINK, CtrlImg::open(), [this, lnk = pick(lnk)] { OpenLink(lnk); });
	}
	else
	if(HasAnnotations() && IsMouseOverAnnotation()) {
		menu.Separator();
		String txt = GetAnnotationText();
		menu.Add(AK_COPYANNOTATION, Images::Copy(),     [this, txt = pick(txt)] { CopyAnnotation(txt); });
		menu.Add(AK_EDITANNOTATION, Images::Edit(),     [this]   { EditAnnotation();    });
		menu.Add(AK_DELETEANNOTATION, Images::Delete(), [this]   { DeleteAnnotation();  });
	}
	else {
		menu.Separator();
		menu.Add(IsSelection(), AK_COPY,  Images::Copy(), [this] { Copy();  });
		menu.Add(IsEditable(),  AK_PASTE, Images::Paste(),[this] { Paste(); });
		if(ctx.settings.custominputmethod) {
			menu.Add(IsEditable(),  AK_CODEPOINT, Images::InsertUnicode(),  [this] { InsertUnicodeCodePoint(*this); });
		}
		if(HasAnnotations()) {
			menu.Add(IsSelection() && !IsSelectorMode(), AK_ANNOTATE, [=] { AddAnnotation(); });
		}
		menu.Separator();
		menu.Add(IsEditable(),  AK_SELECTALL, Images::SelectAll(), [this] { SelectAll(); });
	}
	menu.Separator();
	menu.Add(AK_FINDER, Images::Find(), [this] { ShowFinder(true); });
	menu.AddKey(AK_SELECTOR_ENTER,      [this] { BeginSelectorMode(); });
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
	menu.Add(AK_SHELLINTEGRATION, [this] { shellintegration = !shellintegration; }).Check(shellintegration);
	menu.Add(AK_VTFUNCTIONKEYS,   [this] { PCStyleFunctionKeys(!HasPCStyleFunctionKeys()); }).Check(HasPCStyleFunctionKeys());
	menu.Add(AK_KEYNAVIGATION,    [this] { KeyNavigation(!HasKeyNavigation()); }).Check(HasKeyNavigation());
	menu.Add(AK_SCROLLBAR,        [this] { ShowScrollBar(!HasScrollBar()); }).Check(HasScrollBar());
	menu.Add(AK_AUTOSCROLL,       [this] { ScrollToEnd(!IsScrollingToEnd()); }).Check(IsScrollingToEnd());
	menu.Add(AK_ALTERNATESCROLL,  [this] { AlternateScroll(!HasAlternateScroll()); }).Check(HasAlternateScroll());
	menu.Add(AK_HIDEMOUSE,	      [this] { AutoHideMouseCursor(!IsMouseCursorAutoHidden()); }).Check(IsMouseCursorAutoHidden());
	menu.Add(AK_REVERSEWRAP,      [this] { ReverseWrap(!HasReverseWrap()); }).Check(HasReverseWrap());
	menu.Add(AK_DYNAMICCOLORS,    [this] { DynamicColors(!HasDynamicColors()); }).Check(HasDynamicColors());
	menu.Add(AK_BRIGHTCOLORS,     [this] { LightColors(!HasLightColors()); }).Check(HasLightColors());
	menu.Add(AK_ADJUSTCOLORS,     [this] { AdjustColors(!HasAdjustedColors()); }).Check(HasAdjustedColors());
	menu.Add(AK_BLINKINGTEXT,     [this] { BlinkingText(!HasBlinkingText()); }).Check(HasBlinkingText());
	menu.Add(AK_BELL,             [this] { bell = !bell; }).Check(bell);
	menu.Add(AK_INLINEIMAGES,     [this] { InlineImages(!HasInlineImages()); }).Check(HasInlineImages());
	menu.Add(AK_HYPERLINKS,       [this] { Hyperlinks(!HasHyperlinks()); }).Check(HasHyperlinks());
	menu.Add(AK_ANNOTATIONS,      [this] { Annotations(!HasAnnotations()); }).Check(HasAnnotations());
	menu.Add(AK_SIZEHINT,         [this] { ShowSizeHint(!HasSizeHint()); }).Check(HasSizeHint());
	menu.Add(AK_BUFFEREDREFRESH,  [this] { DelayedRefresh(!IsDelayingRefresh()); }).Check(IsDelayingRefresh());
	menu.Add(AK_LAZYRESIZE,       [this] { LazyResize(!IsLazyResizing()); }).Check(IsLazyResizing());
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

VectorMap<String, String>& GetWordSelectionPatterns()
{
	return Single<VectorMap<String, String>>();
}

}
