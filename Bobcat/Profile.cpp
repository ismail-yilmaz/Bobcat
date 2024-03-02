// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define IMAGEFILE <Bobcat/Bobcat.iml>
#define IMAGECLASS Images
#include <Draw/iml_source.h>

struct ProfileNameDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		auto ctx = GetContext();
		const Image& img = ctx && ctx->settings.defaultprofile == q ? Images::DefaultTerminal() : Images::Terminal();
		bool current = ctx && ctx->GetActiveProfile() == q;
		StdDisplay().Paint(w, r, AttrText(q).SetImage(img).Bold(current), ink, paper, style);
	}
};

struct FontProfileDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		Font f = q.To<Font>();
		AttrText txt(Format("%s (%d)", f.GetFaceName(), f.GetHeight()).ToWString());
		txt.font = f.Height(StdFont().GetHeight());
		StdDisplay().Paint(w, r, txt, ink, paper, style);
	}
};

const Display& ProfileNameDisplay()       { return Single<ProfileNameDisplayCls>(); }
const Display& FontProfileDisplay()       { return Single<FontProfileDisplayCls>(); }
const Display& GuiFontProfileDisplay()    { return Single<FontProfileDisplayCls>(); }

Profile::Profile()
: name(String::GetVoid())
, bell(false)
, blinktext(true)
, blinkinterval(500)
, lightcolors(false)
, adjustcolors(false)
, intensify(false)
, dynamiccolors(false)
, cursorstyle("block")
, lockcursor(false)
, blinkcursor(true)
, inlineimages(false)
, hyperlinks("disabled")
, windowactions(false)
, windowreports(true)
, clipboardread(false)
, clipboardwrite(false)
, mousewheelstep(GUI_WheelScrollLines())
, alternatescroll(false)
, dontscrolltoend(false)
, autohidemouse(false)
, history(true)
, historysize(1024)
, keynavigation(false)
, altescapeskeys(true)
, altshiftskeys(false)
, delayedrefresh(true)
, lazyresize(false)
, sizehint(true)
, order(0)
, filterctrl(false)
, font(Monospace())
, linespacing(0)
, noenv(false)
, onexit("exit")
, erasechar("delete")
, functionkeystyle("pc")
, overridetracking("K_SHIFT_CTRL")
, wordselmode("plain")
, wordselchars("_-")
, wordselpattern("")
, encoding(CharsetName(CHARSET_UTF8))
, user(GetUserName())
, address(GetHomeDirectory())
{
#ifdef PLATFORM_POSIX
	command = Nvl(GetEnv("SHELL"), "/bin/sh");
	env << "TERM=" << Nvl(GetEnv("TERM"), "xterm-256color") << "\n"
		<< "COLORTERM=" << Nvl(GetEnv("COLORTERM"), "truecolor") << "\n";
#else
	command = Nvl(GetEnv("ComSpec"), "cmd.exe");
#endif
}

hash_t Profile::GetHashValue() const
{
	return name.GetHashValue();
}

void Profile::Jsonize(JsonIO& jio)
{
	jio
	("Name",				 name)
	("User",				 user)
	("Order",				 order)
	("Command",				 command)
	("Address",				 address)
	("Env",					 env)
	("DontInheritEnv",       noenv)
	("Encoding",             encoding)
	("Font",				 font)
	("LineSpacing",          linespacing)
	("Bell",				 bell)
	("BlinkingText",		 blinktext)
	("TextBlinkInterval",	 blinkinterval)
	("BrightColors",		 lightcolors)
	("AdjustColorstoTheme",  adjustcolors)
	("BrightBoldText",       intensify)
	("DynamicColors",        dynamiccolors)
	("CursorStyle",			 cursorstyle)
	("CursorBlink",			 blinkcursor)
	("CursorLocked",		 lockcursor)
	("InlineImages",         inlineimages)
	("Hyperlinks",           hyperlinks)
	("WindowActions",		 windowactions)
	("WindowReports",		 windowreports)
	("ClipboardReadAccess",  clipboardread)
	("ClipboardWriteAccess", clipboardwrite)
	("BackspaceKeySends",    erasechar)
	("FunctionKeyStyle",     functionkeystyle)
	("AltEscapesKeys",       altescapeskeys)
	("AltShiftKeys",         altshiftskeys)
	("KeyNavigation",        keynavigation)
	("TrackingOverrideKey",  overridetracking)
	("MouseWheelStep",       mousewheelstep)
	("DontScrollToEnd",      dontscrolltoend)
	("AlternateScroll",      alternatescroll)
	("AutoHideMouseCursor",  autohidemouse)
	("HistoryBuffer",		 history)
	("HistorySize",			 historysize)
	("BufferedRefresh",      delayedrefresh)
	("LazyPageResize",       lazyresize)
	("ShowSizeHint",         sizehint)
	("WordSelectionMode",    wordselmode)
	("WordSelectionExtras",  wordselchars)
	("WordSelectionPattern", wordselpattern)
	("FilterCtrlBytes",      filterctrl)
	("OnExit",               onexit)
	("Palette",              palette)
	("Finder",               finder)
	("Linkifier",            linkifier);
}

Profiles::Setup::Setup()
{
	ONCELOCK
	{
		CtrlImg::Set(CtrlImg::I_cut, Images::Cut());
		CtrlImg::Set(CtrlImg::I_copy, Images::Copy());
		CtrlImg::Set(CtrlImg::I_paste, Images::Paste());
		CtrlImg::Set(CtrlImg::I_undo, Images::Undo());
		CtrlImg::Set(CtrlImg::I_undo, Images::Redo());
		CtrlImg::Set(CtrlImg::I_remove, Images::Delete());
		CtrlImg::Set(CtrlImg::I_select_all, Images::SelectAll());
	}
	
	for(int i = 0; i < CharsetCount(); i++) {
		String cset = CharsetName(i);
		if(!cset.StartsWith("dec-"))
			emulation.charset.Add(cset);
	}
	
	Add(tabs.SizePos());
	CtrlLayout(general);
	CtrlLayout(visuals);
	CtrlLayout(emulation);
	dirsel.Attach(general.dir);
	filesel.Attach(general.cmd);
	tabs.Add(general.SizePos(), t_("Environment"));
	tabs.Add(visuals.SizePos(), t_("Appearance"));
	tabs.Add(emulation.SizePos(), t_("Emulation"));
	tabs.Add(finder.SizePos(), t_("Finder"));
	tabs.Add(linkifier.SizePos(), t_("Linkifier"));
	general.cmdexit.Add("exit", t_("Close the terminal"));
	general.cmdexit.Add("keep", t_("Don't close the terminal"));
	general.cmdexit.Add("restart", t_("Restart command"));
	general.cmdexit.Add("restart_failed", t_("Restart command on failure"));
	visuals.cursorstyle.Add("block", t_("Block"));
	visuals.cursorstyle.Add("beam", t_("Beam"));
	visuals.cursorstyle.Add("underline", t_("Underline")).SetIndex(0);
	visuals.font.SetDisplay(FontProfileDisplay());
	visuals.font.WhenPush = [this] { visuals.font <<= SelectFont(~visuals.font); visuals.font.Action(); };
	emulation.keystyle.Add("vt", t_("VT style"));
	emulation.keystyle.Add("pc", t_("PC style")).SetIndex(0);
	emulation.charset.SetIndex(0);
	emulation.erasechar.Add("delete", t_("Delete [^?]"));
	emulation.erasechar.Add("backspace", t_("Backspace [^H]"));
	emulation.erasechar.SetIndex(0);
	emulation.overridetracking.Add("K_SHIFT", "Shift");
	emulation.overridetracking.Add("K_ALT",  "Alt");
	emulation.overridetracking.Add("K_CTRL",  "Ctrl");
	emulation.overridetracking.Add("K_CTRL_ALT", "Ctrl+Alt");
	emulation.overridetracking.Add("K_SHIFT_CTRL", "Shift+Ctrl");
	emulation.overridetracking.Add("K_SHIFT_ALT", "Shift+Alt");
	emulation.overridetracking.Add("K_SHIFT_CTRL_ALT", "Shift+Ctrl+Alt");
	emulation.overridetracking.SetIndex(4);
	emulation.wordselmode.Add("plain", t_("Plain"));
	emulation.wordselmode.Add("smart", t_("Smart"));
	emulation.wordselmode.SetIndex(0);

	for(Ctrl& c : general)   c.WhenAction << [this] { Sync(); };
	for(Ctrl& c : visuals)   c.WhenAction << [this] { Sync(); };
	for(Ctrl& c : emulation) c.WhenAction << [this] { Sync(); };
	
	Sync();
}

void Profiles::Setup::MapData(CtrlMapper& m, Profile& p) const
{
    m(general.cmd,              p.command)
     (general.dir,              p.address)
     (general.env,              p.env)
     (general.noenv,            p.noenv)
     (general.cmdexit,          p.onexit)
     (visuals.font,             p.font)
     (visuals.linespacing,      p.linespacing)
     (visuals.cursorstyle,      p.cursorstyle)
     (visuals.blinkcursor,      p.blinkcursor)
     (visuals.lockcursor,       p.lockcursor)
     (visuals.colorprofiles,    p.palette)
     (visuals.adjustcolors,     p.adjustcolors)
     (visuals.lightcolors,      p.lightcolors)
     (visuals.intensify,        p.intensify)
     (visuals.dynamiccolors,    p.dynamiccolors)
     (visuals.blinktext,        p.blinktext)
     (visuals.blinkinterval,    p.blinkinterval)
     (visuals.bell,             p.bell)
     (visuals.hidemouse,        p.autohidemouse)
     (visuals.sizehint,         p.sizehint)
     (emulation.charset,        p.encoding)
     (emulation.images,         p.inlineimages)
     (emulation.hyperlinks,     p.hyperlinks)
     (emulation.history,        p.history)
     (emulation.historysize,    p.historysize)
     (emulation.windowactions,  p.windowactions)
     (emulation.windowreports,  p.windowreports)
     (emulation.readclipboard,  p.clipboardread)
     (emulation.writeclipboard, p.clipboardwrite)
     (emulation.wheelstep,      p.mousewheelstep)
     (emulation.erasechar,      p.erasechar)
     (emulation.keystyle,       p.functionkeystyle)
     (emulation.altescapes,     p.altescapeskeys)
     (emulation.altshifts,      p.altshiftskeys)
     (emulation.keynavigation,  p.keynavigation)
     (emulation.noscroll,       p.dontscrolltoend)
     (emulation.alternatescroll,p.alternatescroll)
     (emulation.delayedrefresh, p.delayedrefresh)
     (emulation.lazyresize,     p.lazyresize)
     (emulation.wordselmode,    p.wordselmode)
     (emulation.wordselchars,   p.wordselchars)
     (emulation.wordselpattern, p.wordselpattern)
     (emulation.filter,         p.filterctrl)
     (emulation.overridetracking, p.overridetracking);
}

void Profiles::Setup::SetData(const Value& data)
{
	if(!data.Is<Profile>())
		return;
	Profile p = data.To<Profile>();
	name = p.name;
	MapData(CtrlMapper().ToCtrls(), p);
	finder.Load(p);
	linkifier.Load(p);
}

Value Profiles::Setup::GetData() const
{
	Profile p(name);
	MapData(CtrlMapper().ToValues(), p);
	finder.Store(p);
	linkifier.Store(p);
	return RawToValue(p);
}

void Profiles::Setup::Sync()
{
	visuals.blinkinterval.Enable(~visuals.blinktext);
	visuals.cursorstyle.Enable(!~visuals.lockcursor);
	visuals.blinkcursor.Enable(!~visuals.lockcursor);
	emulation.historysize.Enable(~emulation.history);
	emulation.wordselpattern.Enable(~emulation.wordselmode == "smart");
	Update();
}

Profiles::Profiles(Bobcat& ctx)
: ctx(ctx)
{
	CtrlLayout(*this);
	Ctrl::Add(setup.HSizePosZ(141, 2).VSizePosZ(3, 3));
	list.AddColumn(t_("Name"));
	list.AddCtrl(setup);
	list.WhenSel = [this] { Sync(); };
	list.WhenBar = [this](Bar& bar) { ContextMenu(bar); };
	list.SetFrame(0, toolbar);
	Sync();
}

void Profiles::ContextMenu(Bar& bar)
{
	bool b = list.IsCursor();
	bool q = b && list.GetCursor() < list.GetCount() - 1;
	bool m = bar.IsMenuBar();
	bar.Add(tt_("Add profile"), Images::Add(), [this]() { Add(); }).Key(K_INSERT);
	bar.Add(b, tt_("Clone profile"), Images::Copy(), [this]() { Clone(); }).Key(K_CTRL|K_C);
	bar.Add(b, tt_("Rename profile"), Images::Rename(), [this]() { Rename(); }).Key(K_F2);
	bar.Add(b, tt_("Remove profile"), Images::Delete(), [this]() { Remove(); }).Key(K_DELETE);
	bar.Add(list.GetCursor() > 0, tt_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, tt_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	if(bar.IsMenuBar()) {
		bar.Separator();
		bar.Add(b, tt_("Set as default profile"), [this]() { SetDefault(); }).Key(K_CTRL|K_D);
		bar.Separator();
		bar.Add(tt_("Open profiles directory"), Images::Directory(), []{ LaunchWebBrowser(ProfilesDir()); }).Key(K_CTRL_HOME);
	}
}

void Profiles::Add()
{
	if(String s; EditTextNotNull(s, t_("New Profile"), t_("Name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Profile named \"" + s + "\" already exists.");
		} else {
			list.Add(s, RawToValue(Profile(s)));
			list.GoEnd();
		}
	}
	Sync();
}

void Profiles::Clone()
{
	if(String s; EditTextNotNull(s, t_("Clone Profile"), t_("Name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Profile named \"" + s + "\" already exists.");
		} else {
			Profile source = list.Get(1).To<Profile>();
			Profile p(source);
			p.name = s;
			auto& m = GetHyperlinkPatterns();
			int i = m.FindAdd(p.name);
			m[i] <<= m.Get(source.name);
			list.Add(s, RawToValue(p));
			list.GoEnd();
		}
	}
	Sync();
}

void Profiles::Rename()
{
	if(String s(list.Get(0)); EditTextNotNull(s, t_("Rename Profile"), t_("New name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Profile named \"" + s + "\" already exists.");
		} else {
			Profile p = list.Get(1).To<Profile>();
			auto& m = GetHyperlinkPatterns();
			String f = ProfileFile(p.name);
			if(FileExists(f))
				DeleteFile(f);
			if (p.name == ctx.settings.defaultprofile) {
				ctx.settings.defaultprofile = s;
				SaveConfig(ctx);
			}
			int i = m.Find(p.name);
			p.name = s;
			m.Add(p.name, pick(m[i]));
			m.Remove(i);
			list.Set(0, p.name);
			list.Set(1, RawToValue(p));
		}
	}
	Sync();
}

void Profiles::Remove()
{
	const char* msg = t_("Do you really want to delete profile '%s'?");
	if(!list.IsCursor())
		return;
	String s = list.GetKey();
	if(PromptYesNo(DeQtf(Format(msg, s)))) {
		String f = ProfileFile(s);
		if(FileExists(f))
			DeleteFile(f);
		list.Remove(list.GetCursor());
		Sync();
	}
}

void Profiles::SetDefault() {
	ctx.settings.defaultprofile = list.Get(0);
	SaveConfig(ctx);
	Sync();
}

void Profiles::Sync()
{
	setup.Sync();
	toolbar.Set([this](Bar& bar) { ContextMenu(bar); });
	for(int i = 0; i < list.GetCount(); i++) {
		list.SetDisplay(i, 0, ProfileNameDisplay());
	}
}

int Profiles::Load()
{
	VectorMap<String, Profile> profiles;
	int rc = LoadProfiles(profiles);
	if(profiles.GetCount()) {
		list.Clear();
		for(const auto& p : ~profiles)
			list.Add(p.key, RawToValue(clone(p.value)));
		list.Sort(1, [](const Value& a, const Value& b) -> int { return a.To<Profile>().order - b.To<Profile>().order; });
		int current = list.Find(ctx.GetActiveProfile(), 0);
		if (current >= 0)
			list.SetCursor(current);
	}
	return rc;
}

void Profiles::Store()
{
	bool hasDefault = false;
	for(int i = 0; i < list.GetCount(); i++) {
		String  s = list.Get(i, 0).To<String>();
		Profile p = list.Get(i, 1).To<Profile>();
		p.order = i;
		hasDefault |= s == ctx.settings.defaultprofile;
		try
		{
			JsonIO jio;
			p.Jsonize(jio);
			String path = ProfileFile(s);
			if(!FileExists(path))
				RealizePath(path);
			SaveFile(path, (String) AsJSON(jio.GetResult(), true));
		}
		catch(const JsonizeError& e)
		{
			LLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			LLOG("Value type error: " << e);
		}
	}
	if(!hasDefault && list.GetCount()) {
		ctx.settings.defaultprofile = list.Get(0, 0).To<String>();
		SaveConfig(ctx);
	}
}

Profile LoadProfile(const String& name)
{
	Profile p;
	for(const FindFile& f : FindFile(ProfileFile(name))) {
		try
		{
			Value q = ParseJSON(LoadFile(f.GetPath()));
			JsonIO jio(q);
			p.Jsonize(jio);
		}
		catch(const JsonizeError& e)
		{
			LLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			LLOG("Value type error: " << e);
		}
		catch(const CParser::Error& e)
		{
			LLOG("Parser error: " << e);
		}
		catch(...)
		{
			LLOG("Unknown exception");
		}
	}
	return pick(p);
}

int LoadProfiles(VectorMap<String, Profile>& v)
{
	int failures = 0;
	for(const String& s : GetProfileFilePaths()) {
		try
		{
			Value q = ParseJSON(LoadFile(s));
			JsonIO jio(q);
			Profile p;
			p.Jsonize(jio);
			String name = p.name;
			v.Add(name, pick(p));
		}
		catch(const JsonizeError& e)
		{
			failures++;
			LLOG(e);
		}
		catch(const ValueTypeError& e)
		{
			failures++;
			LLOG(e);
		}
		catch(const CParser::Error& e)
		{
			failures++;
			LLOG(e);
		}
		catch(...)
		{
			LLOG("Unknown exception");
		}
	}
	return v.GetCount() ? failures : -1;
}

String ProfilesDir()
{
	return ConfigFile("profiles");
}

String ProfileFile(const String& name)
{
	return AppendFileName(ProfilesDir(), name + ".profile");
}

Vector<String> GetProfileFilePaths()
{
	return FindAllPaths(ProfilesDir(), "*.profile");
}

Vector<String> GetProfileNames()
{
	VectorMap<String, Profile> v;
	LoadProfiles(v);
	Vector<String> names(v.GetKeys(), 0);
	Sort(names, [&v](const String& a, const String& b) -> int { return v.Get(a).order < v.Get(b).order; });
	return names;
}

String ShortcutKeysFile()
{
	return ConfigFile("bobcat.keys");
}

bool LoadShortcutKeys()
{
	String s = LoadFile(ShortcutKeysFile());
	if(!s.IsVoid())
		RestoreKeys(s);
	return !s.IsVoid();
}

void SaveShortcutKeys()
{
	String s = StoreKeys();
	SaveFile(ShortcutKeysFile(), s);
}

}