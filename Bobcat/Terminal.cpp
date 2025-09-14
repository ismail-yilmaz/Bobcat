// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

using namespace TerminalCtrlKeys;

Terminal::Terminal(Bobcat& ctx_)
: ctx(ctx_)
, bell(true)
, filter(false)
, canresize(true)
, smartwordsel(false)
, shellintegration(false)
, warnonrootaccess(false)
, starttime(Null)
, finder(*this)
, linkifier(*this)
, quicktext(*this)
, websearch(*this)
, titlebar(*this)
, progressbar(*this)
, exitmode(ExitMode::Exit)
, pathmode(PathMode::Native)
, highlight {
	Yellow(),
	SColorHighlightText,
	LtRed(),
	SColorHighlight
	}
{
	SetDeviceId("Bobcat " + GetVersion());
	InlineImages().Hyperlinks().WindowOps().DynamicColors().WantFocus();
	
	WhenBar     = [this](Bar& bar)             { ContextMenu(bar);                 };
	WhenBell    = [this]()                     { if(bell) BeepExclamation();       };
	WhenResize  = [this]()                     { pty->SetSize(GetPageSize());      };
	WhenScroll  = [this]()                     { Update();                         };
	WhenOutput  = [this](String s)             { pty->Write(s);                    };
	WhenTitle   = [this](const String& s)      { MakeTitle(s);                     };
	WhenLink    = [this](const String& s)      { OnLink(s);                        };
	WhenSetSize = [this](Size sz)              { if(CanResize()) ctx.Resize(sz);   };
	WhenWindowMinimize       = [this](bool b)  { if(CanResize()) ctx.Minimize(b);  };
	WhenWindowMaximize       = [this](bool b)  { if(CanResize()) ctx.Maximize(b);  };
	WhenWindowFullScreen     = [this](int i)   { if(CanResize()) ctx.FullScreen(i);};
	WhenWindowGeometryChange = [this](Rect r)  { if(CanResize()) ctx.SetRect(r);   };
	WhenDirectoryChange      = THISFN(SetWorkingDirectory);
	WhenHighlight            = THISFN(OnHighlight);
	WhenAnnotation           = THISFN(OnAnnotation);
	WhenMessage              = THISFN(OnNotification);
	WhenProgress             = THISFN(OnProgress);
	WhenSelectorScan         = THISFN(OnSelectorScan);
}

Terminal::~Terminal()
{
	GetNotificationDaemon().Clear(this);
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

bool Terminal::StartPty(const Profile& p, bool pane)
{
	#ifdef PLATFORM_POSIX
	#ifdef PLATFORM_LINUX
	pty.Create<LinuxPtyProcess>().WhenAttrs = [this, &p](termios& t) -> bool
	#else
	pty.Create<PosixPtyProcess>().WhenAttrs = [this, &p](termios& t) -> bool
	#endif
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
	#elif PLATFORM_WIN32
		#if defined(flagWIN10)
			if(p.ptybackend == "conpty")
				pty.Create<ConPtyProcess>();
			else
				pty.Create<WinPtyProcess>();
		#else
			pty.Create<WinPtyProcess>();
		#endif
	#endif

	VectorMap<String, String> m;
	if(!p.noenv)
		m = clone(Environment());
	if(p.addtopath) {
		String& s = m.GetAdd("PATH");
		if(String  q = GetExeFolder(); s.Find(q) < 0) {
			if(!IsNull(s)) {
			#ifdef PLATFORM_WIN32
				s += ";";
			#else
				s += ":";
			#endif
			}
			s += q;
		}
	}
	MemReadStream ms(p.env, p.env.GetLength());
	while(!ms.IsEof()) {
		String k, v;
		if(SplitTo(ms.GetLine(), '=', k, v)) {
			m.GetAdd(k) = v;
		}
	}
	
	MakeTitle(profilename);
	if(ctx.stack.Find(*this) < 0) {
		if(pane)
			ctx.stack.Split(*this);
		else
			ctx.stack.Add(*this);
	}
	if(pty->Start(p.command, m, p.address)) {
		starttime = GetSysTime();
		pty->SetSize(GetPageSize());
		return true;
	}
	AskRestartExitError(this, p);
	return false;
}

bool Terminal::Start(const Profile& p, bool pane)
{
	SetProfile(p);
	SetPalette(LoadPalette(p.palette));
	return StartPty(p, pane);
}

bool Terminal::Start(const String& profile_name, bool pane)
{
	Profile p = LoadProfile(profile_name);
	return Start(p, pane);
}

bool Terminal::Start(Terminal *term, bool pane)
{
	Profile p;
	if(term) {
		p = LoadProfile(term->profilename);
		if(term->shellintegration && !IsNull(term->workingdir)) {
			p.address = term->workingdir;
		}
	}
	return Start(p, pane);
}

void Terminal::Restart()
{
	SoftReset();
	Profile p = LoadProfile(profilename);
	SetExitMode(p.onexit);
	StartPty(p);
}

void Terminal::Stop()
{
	exitmode = ExitMode::Exit;
	if(!pty->IsRunning())
		return;
	pty->Kill();
}

bool Terminal::Do()
{
	if(pty->IsRunning()) {
		Write(pty->Get(), IsUtf8Mode());
		#ifdef PLATFORM_LINUX
		// Warn the user about root access.
		if(warnonrootaccess)
			static_cast<LinuxPtyProcess&>(*pty).CheckPrivileges(*this);
		#endif
		return true;
	}
	if(ShouldExit())
		return false;
	else
	if(ShouldRestart())
		Restart();
	else
	if(ShouldAsk())
		AskRestartExit();
	return true;
}

void Terminal::Reset()
{
	HardReset();
	progressbar.Hide();
}

bool Terminal::IsRunning()
{
	return pty && pty->IsRunning();
}

bool Terminal::IsFailure()
{
	return !IsRunning() && pty->GetExitCode() != 0;
}

bool Terminal::IsSuccess()
{
	return !IsRunning() && pty->GetExitCode() == 0;
}

bool Terminal::IsAsking()
{
	return !IsRunning() && exitmode == ExitMode::KeepAsking;
}

bool Terminal::IsRoot()
{
#ifdef PLATFORM_LINUX
	return warnonrootaccess && static_cast<LinuxPtyProcess&>(*pty).IsRoot();
#else
	return false;
#endif
}

void Terminal::DontExit()
{
	exitmode = ExitMode::Keep;
}

void Terminal::KeepAsking()
{
	exitmode = ExitMode::KeepAsking;
}

void Terminal::ScheduleRestart()
{
	exitmode = ExitMode::Restart;
}

void Terminal::ScheduleExit()
{
	exitmode = ExitMode::Exit;
}

bool Terminal::ShouldAsk()
{
	return exitmode == ExitMode::Ask;
}

bool Terminal::ShouldExit()
{
	return exitmode == ExitMode::Exit || (IsSuccess() && exitmode == ExitMode::RestartFailed);
}

bool Terminal::ShouldRestart()
{
	return exitmode == ExitMode::Restart || (IsFailure() && exitmode == ExitMode::RestartFailed);
}

void Terminal::AskRestartExit()
{
	AskRestartExitOK(this);
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
	titlebar.menu.Show(!ctx.HasMenuBar());
	titlebar.newterm.Show(!ctx.HasMenuBar());
	titlebar.navlist.Show(!ctx.HasMenuBar() && b);
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
	findselectedtext = p.findselectedtext;
	warnonrootaccess = p.warnonrootaccess;
	bell = p.bell;
	filter = p.filterctrl;
	NotifyProgress(p.progress);
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
	TreatAmbiguousCharsAsWideChars(IsUtf8Mode() && p.ambiguoustowide);
	SetPadding(Size(0, p.linespacing));
	SetFont(p.font);
	AlternateScroll(p.alternatescroll);
	ScrollToEnd(!p.dontscrolltoend);
	OverrideTracking(GetModifierKey(p.overridetracking));
	Hyperlinks(p.hyperlinks);
	Annotations(p.annotations);
	SetWordSelectionFilter(p.wordselchars);
	SetWordSelectionPattern(p.wordselpattern);
	SetPathTranslationMode(p.pathtranslation);
	SetPathDelimiter(p.pathdelimiter);
	AnswerBackMessage(p.answerbackmsg);
	smartwordsel = p.wordselmode == "smart";
	finder.SetConfig(p);
	linkifier.SetConfig(p);
	websearch.SetConfig(p);
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

void Terminal::MouseWheel(Point pt, int zdelta, dword keyflags)
{
	// Since we are using wheel, combine the keyflags for zoom-in and zoom-out.
	if(dword k = GetAKModifierKey(AK_ZOOMIN())
	           | GetAKModifierKey(AK_ZOOMOUT()); k > 0 && k == keyflags) {
		SetFontZoom(zdelta >= 0 ? 1 : -1);
	}
	else
		TerminalCtrl::MouseWheel(pt, zdelta, keyflags);
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

void Terminal::GotFocus()
{
	TerminalCtrl::GotFocus();
	if(IsSplitterPane())
		ctx.stack.Goto(*this);
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
		"ask",            ExitMode::Ask,
	/* "exit" */          ExitMode::Exit);
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
	if(auto& q = pty->To<PosixPtyProcess>(); q.GetAttrs(tio)) {
		tio.c_cc[VERASE] = decode(s, "backspace", 0x08, 0x7f);
		q.SetAttrs(tio);
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

Terminal& Terminal::SetPathTranslationMode(const String& s)
{
	pathmode = decode(s, "unix", PathMode::Unix, "windows", PathMode::Windows, PathMode::Native);
	return *this;
}

Terminal& Terminal::SetPathDelimiter(const String& s)
{
	pathdelimiter = s;
	return *this;
}

void Terminal::SetWorkingDirectory(const String& s)
{
	if(shellintegration && !IsNull(s)) {
		if(UrlInfo url(s); url.scheme == "file" && !IsNull(url.path) && DirectoryExists(url.path))
			workingdir = url.path;
	}
}

String Terminal::GetWorkingDirectory() const
{
	return DirectoryExists(workingdir) ? workingdir : GetHomeDirectory();
}

void Terminal::MakeTitle(const String& s)
{
	String title;
	if(!profilename.IsEmpty())
		title << profilename;
	if(!s.IsEmpty() && s != profilename)
		title << " :: " << s;
	SetData(title);
	titlebar.title.SetText("\1[g= " << DeQtf(title) << " ]");
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

void Terminal::EnableResize(bool b)
{
	canresize = b;
}

void Terminal::DisableResize()
{
	EnableResize(false);
}

bool Terminal::CanResize() const
{
	// Only the user or the active terminal is allowed to change the geometry.
	return canresize && ctx.GetActiveTerminal() == this;
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
		return linkifier.GetCurrentItemInfo().data;
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

void Terminal::OnNotification(const String& text)
{
	GetNotificationDaemon().NoIcon().Information(*this, text);
}

bool Terminal::OnSelectorScan(dword key)
{
	return MenuBar::Scan([&](Bar& menu) { websearch.ContextMenu(menu); }, key);
}

void Terminal::OnProgress(int type, int data)
{
	switch(type) {
	case TerminalCtrl::PROGRESS_NORMAL:
		progressbar.Show(data);
		break;
	case TerminalCtrl::PROGRESS_BUSY:
		progressbar.Show(Null);
		break;
	case TerminalCtrl::PROGRESS_ERROR:
		progressbar.Hide();
		Error(*this, Format(t_("Progress failed.&Error code: %d"), data));
		break;
	case TerminalCtrl::PROGRESS_OFF:
	default:
		progressbar.Hide();
		break;
	}
}

bool Terminal::InProgress() const
{
	return progressbar.IsChild();
}

void Terminal::DragAndDrop(Point pt, PasteClip& d)
{
	if(IsReadOnly() || IsDragAndDropSource())
		return;
	
	WString s;

	if(AcceptFiles(d)) {
		WString q = pathdelimiter.ToWString();
		for(const auto& f : GetFiles(d)) {
			s << q;
			switch(pathmode) {
			case PathMode::Unix:
				s << UnixPath(f).ToWString();
				break;
			case PathMode::Windows:
				s << WinPath(f).ToWString();
				break;
			case PathMode::Native:
			default:
				s << f.ToWString();
				break;
			}
			s << q;
		}
		s = TrimRight(s);
	}
	
	else
	if(AcceptText(d))
		s = GetWString(d);
	else
		return;

	d.SetAction(DND_COPY);
	
	if(d.IsAccepted())
		Paste(s, filter);
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

void Terminal::OnHighlight(VectorMap<int, VTLine>& line)
{
	HighlightInfo hl;
	hl.line = &line;
	linkifier.OnHighlight(hl);
	finder.OnHighlight(hl);
}

void Terminal::DoHighlight(const SortedIndex<ItemInfo>& items, Upp::HighlightInfo& hl, const Event<HighlightInfo&>& cb)
{
	// Unified highlighting.
	
	LTIMING("Terminal::DoHighlight");
	
	auto ScanCells = [&](const ItemInfo& q) {
		int col = 0;
		hl.iteminfo = &q;
		for(VTLine& l : *hl.line) {
			for(VTCell& c : l) {
				if(c.IsSpecial()) // offset special values (e.g. double width char trail)
					continue;
				if(q.pos.x + hl.offset <= col && col < q.pos.x + hl.offset + q.length) {
					hl.highlighted.Add(&c);
				}
				else
				if(!hl.highlighted.IsEmpty()) {
					cb(hl);
					hl.highlighted.Clear();
				}
				col++;
			}
		}
		if(!hl.highlighted.IsEmpty())
			cb(hl);
	};

	ItemInfo dummy;
	
	if(hl.adjusted) {
		Vector<int> rows;
		hl.offset  = -AdjustLineOffset(*this, hl.line->GetKeys(), rows);
		for(int row : rows) {
			dummy.pos.y = row;
			if(int i = items.Find(dummy); i >= 0) {
				do {
					ScanCells(items[i]);
					i = items.FindNext(i);
				}
				while(i >= 0);
			}
		}
	}
	else {
		dummy.pos.y = hl.line->GetKey(0);
		if(int i = items.Find(dummy); i >= 0) {
			do {
				hl.posindex = GetPosAsIndex(items[i].pos, true);
				ScanCells(items[i]);
				i = items.FindNext(i);
			}
			while(i >= 0);
		}
	}
}

WString Terminal::GetSelectedText() const
{
	return TerminalCtrl::GetSelectedText();
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
		String s = ToUtf8(q);
	
		while(r.GlobalMatch(s)) {
			int o = r.GetOffset();
			int begin = Utf32Len(~s, o);
			int end   = begin + Utf32Len(~s + o, r.GetLength());
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

Terminal& Terminal::PutText(const WString& txt)
{
	TerminalCtrl::Put(txt);
	return *this;
}

void Terminal::FindText(const WString& text)
{
	finder.SearchText(text);
	ShowFinder(true);
}

void Terminal::OpenFinder()
{
	if(findselectedtext && IsSelection())
		finder.SearchText(GetSelectedText());
	ShowFinder(true);
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
		menu.Add(AK_COPYLINK, Images::Copy(),  [this, lnk] { CopyLink(lnk); });
		menu.Add(AK_OPENLINK, CtrlImg::open(), [this, lnk] { OpenLink(lnk); });
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
			menu.Add(IsSelection() && !IsSelectorMode(), AK_ANNOTATE, Images::Annotate(), [=] { AddAnnotation(); });
		}
		menu.Separator();
		menu.Add(IsEditable(),  AK_SELECTALL, Images::SelectAll(), [this] { SelectAll(); });
	}
	menu.Separator();
	menu.Add(AK_FINDER, Images::Find(), [this] { OpenFinder(); });
	websearch.ContextMenu(menu);
	menu.AddKey(AK_SELECTOR_ENTER,      [this] { BeginSelectorMode(); });
	menu.AddKey(AK_QUICKTEXT,           [this] { quicktext.Popup(); });
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
	menu.Add(AK_PROGRESS,         [this] { NotifyProgress(!IsNotifyingProgress()); }).Check(IsNotifyingProgress());
	menu.Add(AK_SIZEHINT,         [this] { ShowSizeHint(!HasSizeHint()); }).Check(HasSizeHint());
	menu.Add(AK_BUFFEREDREFRESH,  [this] { DelayedRefresh(!IsDelayingRefresh()); }).Check(IsDelayingRefresh());
	menu.Add(AK_LAZYRESIZE,       [this] { LazyResize(!IsLazyResizing()); }).Check(IsLazyResizing());
	menu.Add(AK_WIDECHARS,        [this] { TreatAmbiguousCharsAsWideChars(IsUtf8Mode() && !IsAmbiguousCharsWide()); }).Check(IsAmbiguousCharsWide()).Enable(IsUtf8Mode());
#ifdef PLATFORM_LINUX
	menu.Add(AK_WARNROOT,         [this] { warnonrootaccess = !warnonrootaccess; }).Check(warnonrootaccess);
#endif
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
	if(shellintegration) {
		menu.Add(AK_OPENDIR, Images::Directory(), [this] { LaunchWebBrowser(GetWorkingDirectory()); });
		menu.Separator();
	}
	ctx.SetupMenu(menu);
	ctx.HelpMenu(menu);
	menu.AddKey(AK_CLOSE, [this] { Stop(); });
	menu.AddKey(AppKeys::AK_EXIT, [this] { ctx.Close(); });
}

Splitter* Terminal::GetParentSplitter() const
{
	return dynamic_cast<Splitter*>(GetParent());
}

bool Terminal::IsSplitterPane() const
{
	return GetParentSplitter() != nullptr;
}

Time Terminal::GetUpTime() const
{
	return Time(1, 1, 1, 0, 0, 0) + (GetSysTime() - starttime);
}

Terminal::TitleBar::TitleBar(Terminal& ctx)
: term(ctx)
{
	SetData("top");
	Add(title.VSizePosZ(0, 0).HSizePosZ(24, 24));
	Add(newterm.LeftPosZ(4, 12).VCenterPosZ(12, 0));
	Add(menu.LeftPosZ(18, 12).VCenterPosZ(12, 0));
	Add(navlist.RightPos(24, 12).VCenterPosZ(12, 0));
	Add(close.RightPosZ(4, 12).VCenterPosZ(12, 0));
	newterm.Image(Images::Add()).Tip(t_("Open new terminal"));
	navlist.Image(CtrlImg::down_arrow()).Tip(t_("Terminal list"));
	close.Image(Images::Delete()).Tip(t_("Close terminal"));
	menu.Image(CtrlImg::down_arrow()).Tip(t_("Open new terminal from..."));
	
	newterm << [this] { term.ctx.AddTerminal(term.ctx.GetActiveProfile()); };
	navlist << [this] { MenuBar::Execute([this](Bar& bar) { term.ctx.ListMenu(bar); }); };
	close   << [this] { term.Stop(); };
	menu    << [this] { Menu(); };
}

void Terminal::TitleBar::SetData(const Value& v)
{
	data = v;
	term.RefreshLayout();
}

Value Terminal::TitleBar::GetData() const
{
	return data;
}

void Terminal::TitleBar::FrameLayout(Rect& r)
{
	data == "bottom"
		? LayoutFrameBottom(r, this, cy ? cy : r.Height())
		: LayoutFrameTop(r, this, cy ? cy : r.Height()); // default
}

void Terminal::TitleBar::Show()
{
	bool b = term.HasSizeHint();
	term.HideSizeHint();
	term.InsertFrame(0, Height(StdFont().GetCy() + Zy(4)));
	term.ShowSizeHint(b);
}

void Terminal::TitleBar::Hide()
{
	bool b = term.HasSizeHint();
	term.HideSizeHint();
	term.RemoveFrame(*this);
	term.ShowSizeHint(b);
	term.SetFocus();
}

void Terminal::TitleBar::Menu()
{
	if(Vector<String> pnames = GetProfileNames(); pnames.GetCount()) {
		MenuBar::Execute([this, &pnames](Bar& menu) { term.ctx.TermSubmenu(menu, pnames);});
	}
}

Terminal::ProgressBar::ProgressBar(Terminal& t)
: term(t)
{
}

void Terminal::ProgressBar::SetData(const Value& v)
{
	data = v;
	term.RefreshLayout();
}

Value Terminal::ProgressBar::GetData() const
{
	return data;
}

void Terminal::ProgressBar::FrameLayout(Rect& r)
{
	// Snap to title bar.
	term.titlebar.data == "bottom"
		? LayoutFrameBottom(r, this, cy ? cy : r.Height())
		: LayoutFrameTop(r, this, cy ? cy : r.Height()); // default
}

void Terminal::ProgressBar::Show(int percent)
{
	if(!IsChild()) {
		bool b = term.HasSizeHint();
		term.HideSizeHint();
		int i = term.FindFrame(term.titlebar);
		term.InsertFrame(decode(i, 0, 1, 0), Height(Zy(4)));
		term.ShowSizeHint(b);
	}
	timer.Kill();
	if(IsNull(percent)) { // "Busy" mode...
		Set(0, 0);
		timer.Set(-12, [this] { if(IsChild()) static_cast<ProgressIndicator&>(*this)++; });
	}
	else
		Set(percent, 100);
}

void Terminal::ProgressBar::Hide()
{
	if(IsChild()) {
		bool b = term.HasSizeHint();
		term.HideSizeHint();
		term.RemoveFrame(*this);
		term.ShowSizeHint(b);
	}
	timer.Kill();
	Set(0, 0);
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
