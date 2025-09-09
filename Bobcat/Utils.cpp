// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

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

	FrameLeft<DisplayCtrl> icon;
	icon.SetDisplay(CenteredImageDisplay());
	icon <<= Images::Find();
	dlg.search.NullText(t_("Search font..."));
	dlg.search.AddFrame(icon);

	dlg.font.SetDisplay(FontListDisplay());
	dlg.font.WhenSel = [&]
	{
		dlg.preview.SetFont(Font().FaceName(~dlg.font).Height(~dlg.fontsize));
	};
	dlg.fontsize <<= IsNull(f) ? StdFont().GetHeight() : f.GetHeight();
	dlg.fontsize << [&] { dlg.slider  <<= ~dlg.fontsize;  dlg.font.WhenSel(); };
	dlg.slider.MinMax(6, 128).Step(1) <<= ~dlg.fontsize;
	dlg.slider << [&] { dlg.fontsize  <<= ~dlg.slider; dlg.font.WhenSel(); };
	
	Vector<String> fontfaces;
	
	int h = Draw::GetStdFontSize().cy + 8;
	for(int i = 0; i < Font::GetFaceCount(); i++) {
		dword fi = Font::GetFaceInfo(i);
		if(((type & Font::FIXEDPITCH) & fi) || ((type & Font::SCALEABLE) & fi)) {
			fontfaces.Add(Font::GetFaceName(i));
		}
		h = max(Font().Face(i).GetCy(), h);
	}
	
	dlg.font.ItemHeight(h);

	auto FilterFonts = [&]
	{
		return FilterRange(fontfaces, [&](const String& s) {
			String q = ~dlg.search;
			return q.IsEmpty() || ToLower(s).Find(ToLower(q)) >= 0;
		});
	};
	
	dlg.search.WhenAction = [&]
	{
		dlg.font.Clear();
		for(const String& face : FilterFonts())
			dlg.font.Add(face);
		dlg.search.Error(dlg.font.GetCount() == 0);
		int i = dlg.font.Find(f.GetFaceName());
		if(i >= 0) dlg.font.SetCursor(i);
	};
	
	// Init the list.
	dlg.search.WhenAction();
	
	if(dlg.Sizeable().ExecuteOK() && !IsNull(~dlg.font))
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

int AdjustLineOffset(Terminal& term, const Vector<int>& in, Vector<int>& out)
{
	int i = 0, dx = 0;
	auto range = term.GetPageRange();
	if(out = clone(in), i = out[0]; i == range.a) {
		if(auto span = term.GetPage().GetLineSpan(i); i > span.a && i <= span.b) {
			dx = GetLength(term.GetPage(), span.a, i);
			for(int j = 0; j < out.GetCount(); j++)
				out[j] = span.a + j;
		}
	}
	return dx;
}

static const Vector<Tuple<dword, const char*>> mod_keys = {
	{ K_SHIFT,              "K_SHIFT" },
	{ K_ALT,                "K_ALT"   },
	{ K_CTRL,               "K_CTRL"  },
	{ K_CTRL|K_ALT,         "K_CTRL_ALT" },
	{ K_SHIFT|K_CTRL,       "K_SHIFT_CTRL" },
	{ K_SHIFT|K_ALT,        "K_SHIFT_ALT"  },
	{ K_SHIFT|K_CTRL|K_ALT, "K_SHIFT_CTRL_ALT" },
#ifdef PLATFORM_COCOA
	{ K_OPTION,                      "K_OPTION"      },
	{ K_OPTION|K_SHIFT,              "K_SHIFT_OPTION " },
	{ K_OPTION|K_ALT,                "K_ALT_OPTION " },
	{ K_OPTION|K_CTRL,               "K_CTRL_OPTION" },
	{ K_OPTION|K_CTRL|K_ALT,         "K_CTRL_ALT_OPTION " },
	{ K_OPTION|K_SHIFT|K_ALT,        "K_SHIFT_ALT_OPTION " },
	{ K_OPTION|K_ALT,                "K_SHIFT_CTRL_OPTION " },
	{ K_OPTION|K_SHIFT|K_CTRL|K_ALT, "K_SHIFT_CTRL_ALT_OPTION" },
#endif
};

dword GetModifierKey(String s)
{
	for(const auto& t : mod_keys)
		if(t.b == s)
			return t.a;
	return 0;
}

String GetModifierKeyDesc(dword key)
{
#ifdef PLATFORM_COCOA
	key &= (K_SHIFT|K_CTRL|K_ALT|K_OPTION);
#else
	key &= (K_SHIFT|K_CTRL|K_ALT);
#endif
	for(const auto& t : mod_keys)
		if(t.a == key)
			return t.b;
	return String::GetVoid();
}

dword GetAKModifierKey(const KeyInfo& k, int index)
{
	ASSERT(index >= 0 && index < 4);
#ifdef PLATFORM_COCOA
	return k.key[index] & (K_CTRL|K_ALT|K_SHIFT|K_OPTION);
#else
	return k.key[index] & (K_CTRL|K_ALT|K_SHIFT);
#endif
}

String GetVersion()
{
	String ver = Format("%d.%d.%d", 0, 9, 6);

#ifdef bmGIT_REVCOUNT
	return ver + " (r" + bmGIT_REVCOUNT + ")";
#else
	return ver;
#endif
}

String GetBuildInfo()
{
	String h;
	
	h << t_("Version") << ": " << GetVersion();
	h << '\n';

	h << t_("Build flags") << ": ";
	
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

#if __cplusplus >= 202300
	h << " (C++23)";
#elif __cplusplus >= 202000
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
	int i = 0;
	if(auto ctx = GetContext(); ctx)
		i = ctx->window.IsWayland() ? 1 : 0;
	h << Format(" (Gtk - %[1:Wayland;XWayland]s)", i);
#elif  flagWEBGUI
	h << " (Turtle)";
#elif  flagSDLGUI
	h << " (SDL2-GL)";;
#endif
	h << '\n';
#ifdef bmTIME
	h << t_("Compiled") << ": " << bmTIME;
#endif

	h << '\n';
	h << t_("Path") << ": " << GetExeFilePath();

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
	for(const auto& q : GetAllGuiThemes()) {
		if(ctx.settings.guitheme == q.b) {
			Ctrl::SetSkin(q.a);
			return;
		}
	}
}

void LoadGuiFont(Bobcat& ctx)
{
	SetStdFont(Nvl(ctx.settings.guifont, GetStdFont()));
}

bool IsWaylandEnabled()
{
#ifdef GUI_GTK
	auto ctx = GetContext();
	return ctx
		&& (ctx->window.IsWayland() || ctx->window.IsXWayland())
		&& FileExists(ConfigFile("USE_WAYLAND"));
#else
	return false;
#endif
}

void EnableWayland(bool enabled)
{
#ifdef GUI_GTK
	if(auto ctx = GetContext(); ctx && IsWaylandEnabled() != enabled) {
		String wlpath = ConfigFile("USE_WAYLAND");
		enabled ? SaveFile(wlpath, Null) : DeleteFile(wlpath);
	}
#endif
}

MessageCtrl& GetNotificationDaemon()
{
	return Single<MessageCtrl>();
}

Ptr<MessageBox> AskYesNo(Ctrl& ctrl, const String& text, const String& yes, const String& no, MessageBox::Type type, const Event<int>& action)
{
	auto& m = GetNotificationDaemon();
	auto& c = m.Create();
	c.MessageType(type);
	c.Placement(m.GetPlacement());
	c.ButtonL(IDYES, yes);
	c.ButtonR(IDNO, no);
	c.UseIcon(false);
	c.Set(ctrl, text, m.IsAnimated(), m.IsAppending(), 0);
	c.WhenAction = action;
	return &c;
}

Ptr<MessageBox> AskRestartExitError(Ptr<Terminal> t, const Profile& p)
{
	t->KeepAsking();
	const char *txt = t_("Command execution failed.&Profile: %s&Command: %s&Exit code: %d");
	String text = Format(txt, p.name, p.command, t->pty->GetExitCode());
	return AskYesNo(*t, text, t_("Restart"), t_("Exit"), MessageBox::Type::FAILURE, [t](int id) {
		if(t) id == IDYES ? t->ScheduleRestart() : t->ScheduleExit();
	});
}

Ptr<MessageBox> AskRestartExitError(Ptr<Terminal> t)
{
	return AskRestartExitError(t, LoadProfile(t->profilename));
}

Ptr<MessageBox> AskRestartExitOK(Ptr<Terminal> t, const Profile& p)
{
	t->KeepAsking();
	const char *txt = t_("Command exited.&Profile: %s&Command: %s&Exit code: %d");
	String text = Format(txt, p.name, p.command, t->pty->GetExitCode());
	return AskYesNo(*t, text, t_("Restart"), t_("Close"), MessageBox::Type::INFORMATION, [t](int id) {
		if(!t) return;
		if(id == IDYES) {
			t->ScheduleRestart();
			t->Reset();
		}
		else
			t->ScheduleExit();
	});
}

Ptr<MessageBox> AskRestartExitOK(Ptr<Terminal> t)
{
	return AskRestartExitOK(t, LoadProfile(t->profilename));
}

Ptr<MessageBox> Warning(Ctrl& ctrl, const String& text, int timeout)
{
	auto& m = GetNotificationDaemon();
	auto& c = m.Create();
	c.MessageType(MessageBox::Type::WARNING);
	c.Placement(m.GetPlacement());
	c.UseIcon(false);
	c.UseCross();
	c.Set(ctrl, text, m.IsAnimated(), m.IsAppending(), timeout);
	return &c;
}

Ptr<MessageBox> Error(Ctrl& ctrl, const Upp::String& text, int timeout)
{
	auto& m = GetNotificationDaemon();
	auto& c = m.Create();
	c.MessageType(MessageBox::Type::FAILURE);
	c.Placement(m.GetPlacement());
	c.UseIcon(false);
	c.UseCross();
	c.Set(ctrl, text, m.IsAnimated(), m.IsAppending(), timeout);
	return &c;
}

String GetUpTime(Time t)
{
	return Format("%d:%0d:%02d:%02d", t - Date(1, 1, 1), t.hour, t.minute, t.second);
}

String GetDefaultShell()
{
#if defined(PLATFORM_POSIX)
	return Nvl(GetEnv("SHELL"), "/bin/sh");
#elif defined(PLATFORM_WIN32)
	return Nvl(GetEnv("ComSpec"), "cmd.exe");
#endif
}

const Array<CmdArg>& GetCmdArgs()
{
	// TODO: Add more options.
	
	const static Array<CmdArg> sArgs = {
		// General options
		{ CmdArgType::General,     "h", "help",                   "",         t_("Show help.") },
		{ CmdArgType::General,     "v", "version",                "",         t_("Show version information.") },
		{ CmdArgType::General,     "l", "list",                   "",         t_("List available terminal profiles.") },
		{ CmdArgType::General,     "p", "profile",                "PROFILE",  t_("Run with the given terminal PROFILE. (Names are case-sensitive)") },
		{ CmdArgType::General,     "s", "settings",               "",         t_("Open settings window.") },
		{ CmdArgType::General,     "" , "list-palettes",          "",         t_("List available color palettes.") },
		{ CmdArgType::General,     "" , "list-fonts",             "",         t_("List available monospaced fonts.") },
		{ CmdArgType::General,     "" , "list-gui-themes",        "",         t_("List available GUI themes.") },
		{ CmdArgType::General,     "",  "gui-theme",              "THEME",    t_("Set the GUI theme to THEME.") },
		{ CmdArgType::General,     "b", "show-bars",              "",         t_("Show the menu and title bar.") },
		{ CmdArgType::General,     "B", "hide-bars",              "",         t_("Hide the menu and title bar.") },
		{ CmdArgType::General,     "",  "show-menubar",           "",         t_("Show the menu bar.") },
		{ CmdArgType::General,     "",  "hide-menubar",           "",         t_("Hide the menu bar.") },
		{ CmdArgType::General,     "",  "show-titlebar",          "",         t_("Show the title bar.") },
		{ CmdArgType::General,     "",  "hide-titlebar",          "",         t_("Hide the title bar.") },
		{ CmdArgType::General,     "",  "show-borders",           "",         t_("Show window borders.") },
		{ CmdArgType::General,     "",  "hide-borders",           "",         t_("Hide window borders. (Frameless window mode)") },
		{ CmdArgType::General,     "f", "fullscreen",             "",         t_("Full screen mode.") },
		{ CmdArgType::General,     "m", "maximize",               "",         t_("Maximize the window.") },
		{ CmdArgType::General,     "g", "geometry",               "GEOMETRY", t_("Set the initial window geometry. (E.g. 80x24, 132x24)") },
		
		// Environment options
#ifdef PLATFORM_WIN32
		{ CmdArgType::Environment, "z", "pty-backend",            "PTY",      t_("Set the pseudoconsole backend to be used.") },
#endif
		{ CmdArgType::Environment, "r", "restart",                "",         t_("Restart the command on exit.") },
		{ CmdArgType::Environment, "R", "restart-failed",         "",         t_("Restart the command when it fails.") },
		{ CmdArgType::Environment, "k", "keep",                   "",         t_("Don't close the terminal on exit.") },
		{ CmdArgType::Environment, "K", "dont-keep",              "",         t_("Close the terminal on exit.") },
		{ CmdArgType::Environment, "y", "ask",                    "",         t_("Ask what to do on exit.") },
		{ CmdArgType::Environment, "n", "environment",            "",         t_("Inherit the environment.") },
		{ CmdArgType::Environment, "N", "no-environment",         "",         t_("Don't inherit the environment.") },
		{ CmdArgType::Environment, "d", "working-dir",            "PATH",     t_("Set the working directory to PATH.") },

		// Emulation options
		{ CmdArgType::Emulation,   "q", "vt-style-fkeys",         "",         t_("Use VT-style function keys.") },
		{ CmdArgType::Emulation,   "Q", "pc-style-fkeys",         "",         t_("Use PC-style function keys.") },
		{ CmdArgType::Emulation,   "w", "window-reports",         "",         t_("Enable window reports.") },
		{ CmdArgType::Emulation,   "W", "no-window-reports",      "",         t_("Disable window reports.") },
		{ CmdArgType::Emulation,   "a", "window-actions",         "",         t_("Enable window actions.") },
		{ CmdArgType::Emulation,   "A", "no-window-actions",      "",         t_("Disable window actions.") },
		{ CmdArgType::Emulation,   "c", "clipboard-access",       "",         t_("Enable application clipboard access.") },
		{ CmdArgType::Emulation,   "C", "no-clipboard-access",    "",         t_("Disable application clipboard access.") },
		{ CmdArgType::Emulation,   "",  "hyperlinks",             "",         t_("Enable hyperlink detection. (OSC 52 + Linkifier)") },
		{ CmdArgType::Emulation,   "",  "no-hyperlinks",          "",         t_("Disable hyperlink detection.") },
		{ CmdArgType::Emulation,   "",  "inline-images",          "",         t_("Enable inline images support. (Sixel, iTerm2, Jexer)") },
		{ CmdArgType::Emulation,   "",  "no-inline-images",       "",         t_("Disable inline images support.") },
		{ CmdArgType::Emulation,   "",  "annotations",            "",         t_("Enable rich-text annotations.") },
		{ CmdArgType::Emulation,   "",  "no-annotations",         "",         t_("Disable rich-text annotations.") },

		// Appearance options
		{ CmdArgType::Appearance,  "",  "palette",                "PALETTE",  t_("Set color palette to PALETTE.") },
		{ CmdArgType::Appearance,  "",  "font-family",            "FONT",     t_("Set the font family to be used to FONT.") },
		{ CmdArgType::Appearance,  "",  "font-size",              "SIZE",     t_("Set the font size to SIZE") },
		{ CmdArgType::Appearance,  "",  "bell",                   "",         t_("Enable notification bell.") },
		{ CmdArgType::Appearance,  "",  "no-bell",                "",         t_("Disable notification bell.") },
	};

	return sArgs;
}

Vector<const CmdArg*> FindCmdArgs(CmdArgType t)
{
	Vector<const CmdArg*> v;
	for(const CmdArg& a : GetCmdArgs())
		if(a.type == t)
			v.Add(&a);
	return v;
}

const char* GetCmdArgTypeName(CmdArgType cat)
{
	switch(cat) {
	case CmdArgType::General:     return t_("General");
	case CmdArgType::Environment: return t_("Environment");
	case CmdArgType::Emulation:   return t_("Emulation");
	case CmdArgType::Appearance:  return t_("Appearance");
	default:  break;
	}
	return t_("Other");
}

const String& CmdArgList::Get(const char *id, const String& defval)
{
	return options.Get(id, defval);
}

bool CmdArgList::HasOption(const char *id) const
{
	return options.Find(id) >= 0;
}

CmdArgParser::CmdArgParser(const Array<CmdArg>& args)
: args(args)
{
}

bool CmdArgParser::Parse(const Vector<String>& cmdline, CmdArgList& list, String& error)
{
	list.options.Clear();
	list.command.Clear();
	bool cmdsep = false;
	
	for(int i = 0; i < cmdline.GetCount(); i++) {
		const String& arg = cmdline[i];
		
		// Handle command after "--"
		if(cmdsep) {
			list.command << arg << ' ';
			continue;
		}
		
		if(arg == "--") {
			cmdsep = true;
			continue;
		}
		
		// Parse options
		if(arg[0] == '-') {
			const CmdArg* opt = Find(arg);
			if(!opt) {
				error = Format(t_("Unknown option: %s"), arg);
				return false;
			}
			
			String value = String(opt->arg);
			if(!value.IsEmpty()) {
				// Option requires a value
				if(i + 1 >= cmdline.GetCount()) {
					error = Format(t_("Option '%s' requires a value"), arg);
					return false;
				}
				value = cmdline[++i];
			}
			
			// Store using the long option name for consistency
			list.options.Add(opt->lopt, value);
		}
		else {
			list.command = arg;
		}
	}
	
	if(!IsNull(list.command))
		list.command = TrimBoth(list.command);

	return true;
}

const CmdArg *CmdArgParser::Find(const String& arg) const
{
	if(arg.GetCount() < 2 || arg[0] != '-')
		return nullptr;
	
	bool islopt = arg[1] == '-';
	
	String s = arg.Mid(islopt ? 2 : 1);
		
	for(const auto& q : args) {
		if(islopt){
			if(q.lopt == s) return &q;
		}
		else {
			if(q.sopt == s) return &q;
		}
	}
	return nullptr;
}

Size ParsePageSize(const String& s)
{
	// Accepted formats: COLxROW or COL:ROW
	auto sDelimFn = [](int c) { return decode(c, 'x', 1, ':', 1, 0); };
	if(String c, r; SplitTo(ToLower(s), sDelimFn, c, r)) {
		return { clamp(StrInt(c), 10, 300), clamp(StrInt(r), 10, 300) };
	}
	return  Null;
}

#ifdef PLATFORM_LINUX

pid_t GetProcessGroupId(APtyProcess& pty)
{
	return tcgetpgrp(static_cast<LinuxPtyProcess&>(pty).GetSocket());
}

void ReadProcessStatus(pid_t pid, const Gate<String&>& fn)
{
	if(FILE *f = fopen(~Format("/proc/%d/status", pid), "r"); f) {
		char line[256] = { 0 };
		while(fgets(line, sizeof(line), f)) {
			String ln(line, min(sizeof(line), strlen(line)));
			if(fn(ln))
				break;
		}
		fclose(f);
	}
}

Tuple<uid_t, uid_t> GetUserIdsFromProcess(pid_t pid)
{
	uid_t ruid = -1, euid = -1;

	ReadProcessStatus(pid, [&](String& line) {
		if(line.TrimStart("Uid:")) {
			if(auto v = Split(line, '\t'); v.GetCount() > 3) {
				ruid = StrInt(v[0]);
				euid = StrInt(v[1]);
				return true;
			}
		}
		return false;
	});
	
	return { ruid, euid };
}

String GetRunningProcessName(APtyProcess& pty)
{
	pid_t pgid = GetProcessGroupId(pty);
	if(pgid <= 0)
		return Null;

	String name;
	ReadProcessStatus(pgid, [&](String& line) {
		if(line.TrimStart("Name:")) {
			name = line;
			return true;
		}
		return false;
	});

	return name;
}

String GetUsernameFromUserId(uid_t uid)
{
	if(passwd *p = getpwuid(uid); p)
		return String(p->pw_name);
	return AsString(uid);
}

bool LinuxPtyProcess::IsRoot()
{
	if(pid_t pgid = GetProcessGroupId(*this); pgid > 0)
		if(auto [r, e] = GetUserIdsFromProcess(pgid); e == 0) {
			ruid = r;
			euid = e;
			return true;
		}
	return false;
}

bool LinuxPtyProcess::CheckPrivileges(Terminal& t)
{
	
	if(bool b = IsRoot(); b && !isroot) {
		isroot = true;
		String txt = t_("Warning: Privilege escalation!");
		txt << "&"
			<< t_("Process") << ": " << Nvl(GetRunningProcessName(*this), t_("Unknown"))
			<< ", ruid: "  << (int) ruid << " (" << GetUsernameFromUserId(ruid) << ")"
			<< ", euid: " << (int) euid << " (" << GetUsernameFromUserId(euid) << ")";
		notification = Warning(t, txt);
		if(t.bell)
			BeepExclamation();
	}
	else
	if(isroot && !b) {
		isroot = false;
		GetNotificationDaemon().Remove(notification);
	}
	return isroot;
}

#endif

}
