// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME "General"
#define KEYNAMESPACE AppKeys
#define KEYFILE <Bobcat/Application.key>
#include <CtrlLib/key_source.h>

using namespace AppKeys;

FileSel& BobcatFs()
{
	return Single<FileSel>();
}

Bobcat::Bobcat()
: navigator(stack)
{
	SyncTitle();
	window.Sizeable().Zoomable().CenterScreen();
	window.WhenClose = [this]() { Close(); };
	window.Add(view.SizePos());
	window.Add(navigator.SizePos());
	view.AddFrame(menubar);
	view.Add(stack.Animation().Wheel().Horz().SizePos());
	navigator.WhenBar        = [this](Bar& menu) { TermMenu(menu); };
	navigator.WhenClose      = [this]()          { ToggleNavigator(); };
	navigator.WhenGotoItem   = [this](Ctrl& c)   { ToggleNavigator(); stack.Goto(c); };
	navigator.WhenRemoveItem = [this](Ctrl& c)   { RemoveTerminal(AsTerminal(c)); };
	stack.WhenAction         = [this]()          { Sync(); };
	menubar.Set([this](Bar& menu) { MainMenu(menu); });
}

bool Bobcat::AddTerminal(const String& key)
{
	bool ok = terminals.Create(*this).Start(key);
	if(ok) Sync();
	return ok;
}

bool Bobcat::AddTerminal(const Profile& profile)
{
	bool ok = terminals.Create(*this).Start(profile);
	if(ok) Sync();
	return ok;
}

void Bobcat::RemoveTerminal(Terminal& t)
{
	if(t.pty.IsRunning())
		t.Stop();
	stack.Remove(t);
	terminals.RemoveIf([this, &t](int i) { return &t == &terminals[i]; });
	Sync();
}

void Bobcat::ActivateTerminal()
{
	if(Ctrl *c = stack.GetActiveCtrl(); c)
		c->SetFocus();
}

Terminal *Bobcat::GetActiveTerminal()
{
	return dynamic_cast<Terminal*>(stack.GetActiveCtrl());
}

Vector<Terminal *> Bobcat::GetTerminalGroup(hash_t id)
{
	Vector<Terminal*> v;
	for(Terminal& t : terminals) {
		if(t == id)
			v.Add(&t);
	}
	return v;
}

Vector<Terminal *> Bobcat::GetTerminalGroup(const Profile& p)
{
	return GetTerminalGroup(p.GetHashValue());
}

void Bobcat::ProcessEvents()
{
	int l = 10;
	window.ProcessEvents();
	for(int i = 0; i < terminals.GetCount(); i++) {
		Terminal& t = terminals[i];
		if(int n = t.Do(); n < 0) {
			RemoveTerminal(t);
			break;
		}
		else
			l = max(l, n);
	}
	Sleep(l >= 1024 ? 1024 * 10 / l : 10);
}

void Bobcat::Run(const Profile& profile, Size size, bool fullscreen)
{
	if(AddTerminal(profile)) {
		if(fullscreen) {
			FullScreen(1);
			#ifdef PLATFORM_POSIX
			window.OpenMain();
			#endif
		}
		else {
			if(!IsMaximized())
				SetPageSize(size);
			window.OpenMain();
		}
		while(window.IsOpen() && terminals.GetCount()) {
			ProcessEvents();
		};
	}
}

void Bobcat::Close()
{
	if(!window.IsOpen())
		return;

	if(int n = terminals.GetCount(); n > 1)
		if(!PromptYesNo(Format(tt_("%d terminals are open.&Close them all and exit?"), n)))
			return;

	window.Close();
}

void Bobcat::Settings()
{
	Profiles  profiles(*this);
	WithSettingsLayout<ParentCtrl> settingspane;

	String st = tt_("Top");
	String sb = tt_("Bottom");
	
	settingspane.titlepos.Add("top", st);
	settingspane.titlepos.Add("bottom", sb);
	settingspane.titlepos.GoBegin();

	settingspane.finderpos.Add("top", st);
	settingspane.finderpos.Add("bottom", sb);
	settingspane.finderpos.GoBegin();
	
	settingspane.direction.Add("horizontal", tt_("Horizontal"));
	settingspane.direction.Add("vertical", tt_("Vertical"));
	settingspane.direction.GoEnd();
	
	for(auto& ch : GetAllGuiThemes()) {
		settingspane.chstyle.Add(ch.b, ch.c);
		settingspane.chstyle.GoBegin();
	}

	settingspane.guifont.SetDisplay(FontProfileDisplay());

	settingspane.guifont.WhenPush = [&]
	{
		dword type = Font::FIXEDPITCH|Font::SCALEABLE;
		settingspane.guifont <<= SelectFont(~settingspane.guifont, type);
		settingspane.guifont.Action();
	};
	
	CtrlRetriever cr;
	cr(settingspane.titlepos, settings.titlealignment);
	cr(settingspane.finderpos, settings.finderalignment);
	cr(settingspane.direction, settings.stackdirection);
	cr(settingspane.animation, settings.stackanimation);
	cr(settingspane.wheel, settings.stackwheel);
	cr(settingspane.showmenu, settings.showmenu);
	cr(settingspane.showtitle, settings.showtitle);
	cr(settingspane.savescreenshot, settings.savescreenshot);
	cr(settingspane.custominput, settings.custominputmethod);
	cr(settingspane.pagesizes, settings.custompagesizes);
	cr(settingspane.chstyle, settings.guitheme);
	cr(settingspane.guifont, settings.guifont);
	
	cr.Set();

	profiles.Load();

	TabDlg dlg;
	dlg.Add(profiles, tt_("Profiles"));
	dlg.Add(settingspane, tt_("General"));
	dlg.WhenClose = dlg.Acceptor(IDEXIT);
	dlg.OK().Cancel().Title(tt_("Settings")).OpenMain();

	while(dlg.IsOpen()) {
		if(window.IsOpen())
			ProcessEvents();
		if(int rc = dlg.GetExitCode(); rc == IDOK) {
			cr.Retrieve();
			profiles.Store();
			SaveConfig(*this);
			if(window.IsOpen()) {
				Sync();
				SyncTerminalProfiles();
			}
			break;
		}
		else
		if(rc)
			break;
		dlg.ProcessEvents();
	}
}

Bobcat& Bobcat::Maximize(bool b)
{
	b ?	window.Maximize() : window.Overlap();
	return *this;
}

bool Bobcat::IsMaximized() const
{
	return window.IsMaximized();
}

Bobcat& Bobcat::Minimize(bool b)
{
	b ? window.Minimize() : window.Overlap();
	return *this;
}

bool Bobcat::IsMinimized() const
{
	return window.IsMinimized();
}

Bobcat& Bobcat::Resize(Size sz)
{
	NoFullScreen();
	Point p = window.GetRect().TopLeft();
	sz = view.AddFrameSize(sz);
	window.SetRect(p.x, p.y, sz.cx, sz.cy);
	return *this;
}

Bobcat& Bobcat::SetPageSize(Size sz)
{
	if(Terminal *t = GetActiveTerminal(); t)
		Resize(t->PageSizeToClient(sz));
	return *this;
}

Bobcat& Bobcat::SetRect(Rect r)
{
	NoFullScreen();
	window.SetRect(r);
	return *this;
}

Bobcat& Bobcat::FullScreen(int mode)
{
	switch(mode) {
	case -1: window.FullScreen(false); break;
	case  0: window.FullScreen(!IsFullScreen()); break;
	case  1: window.FullScreen(true); break;
	default: return *this;
	}
	Sync();
	return *this;
}

Bobcat& Bobcat::ToggleFullScreen()
{
	return FullScreen(0);
}

Bobcat& Bobcat::NoFullScreen()
{
	if(IsFullScreen())
		FullScreen(-1);
	return *this;
}

bool Bobcat::IsFullScreen() const
{
	return window.IsFullScreen();
}

Bobcat& Bobcat::ShowMenuBar(bool b)
{
	if(HasMenuBar() && !b)  {
		view.RemoveFrame(menubar);
		settings.showmenu = b;
	}
	else
	if(!HasMenuBar() && b) {
		view.AddFrame(menubar);
		settings.showmenu = b;
	}
	return *this;
}

Bobcat& Bobcat::HideMenuBar()
{
	return ShowMenuBar(false);
}

bool Bobcat::HasMenuBar() const
{
	return menubar.IsChild();
}

Bobcat& Bobcat::ToggleNavigator()
{
	if(view.IsShown()) {
		view.Hide();
		navigator.Show().SetFocus();
	}
	else {
		navigator.Hide();
		view.Show();
		if(Terminal * t = GetActiveTerminal(); t)
			t->SetFocus();
	}
	return *this;
}

void Bobcat::Sync()
{
	ShowMenuBar(settings.showmenu);
	settings.stackdirection == "horizontal"	? stack.Horz() : stack.Vert();
	stack.Wheel(settings.stackwheel);
	stack.Animation(settings.stackanimation);
	for(int i = 0; i < stack.GetCount(); i++) {
		Terminal& t = AsTerminal(stack[i]);
		t.Sync();
	}
	SyncTitle();
	navigator.Sync();
}

void Bobcat::SyncTitle()
{
	String s = t_("Bobcat");
	if(const Terminal *t = GetActiveTerminal(); !settings.showtitle && t)
		s << " [" << t->GetData() << "]";
	window.Title(s);
}

void Bobcat::SyncTerminalProfiles()
{
	if(VectorMap<String, Profile> v; LoadProfiles(v) >= 0) {
		for(const Profile& p : v.GetValues()) {
			Palette q = LoadPalette(p.palette);
			for(Terminal* t : GetTerminalGroup(p)) {
				t->SetProfile(p);
				t->SetPalette(q);
			}
		}
	}
}

void Bobcat::MainMenu(Bar& menu)
{
	menu.Sub(t_("File"),  [this](Bar& menu) { FileMenu(menu);  });
	menu.Sub(t_("Edit"),  [this](Bar& menu) { EditMenu(menu);  });
	menu.Sub(t_("View"),  [this](Bar& menu) { ViewMenu(menu);  });
	menu.Sub(t_("List"),  [this](Bar& menu) { ListMenu(menu);  });
	menu.Sub(t_("Setup"), [this](Bar& menu) { SetupMenu(menu); });
	menu.Sub(t_("Help"),  [this](Bar& menu) { HelpMenu(menu);  });
}

void Bobcat::FileMenu(Bar& menu)
{
	TermMenu(menu);
	if(Terminal *t = GetActiveTerminal(); t)
		t->FileMenu(menu);
	menu.Separator();
	menu.Add(t_("Reset"), [this] { if(Terminal *t = GetActiveTerminal(); t) t->Reset(); });
	menu.Separator();
	menu.Add(AK_SCREENSHOT, [this] { ScreenShot(); });
	menu.Separator();
	menu.Add(t_("Exit"),  [this] { Close(); });
}

void Bobcat::EditMenu(Bar& menu)
{
	if(Terminal *t = GetActiveTerminal(); t)
		t->EditMenu(menu);
}

void Bobcat::ViewMenu(Bar& menu)
{
	bool enable = stack.GetCount() > 1;
	
	menu.Add(AK_MENUBAR,       [this] { settings.showmenu  ^= 1; Sync(); }).Check(settings.showmenu);
	menu.Add(AK_TITLEBAR,      [this] { settings.showtitle ^= 1; Sync(); }).Check(settings.showtitle);
	menu.Add(AK_FULLSCREEN,    [this] { FullScreen(0); }).Check(window.IsFullScreen());
	menu.AddKey(AK_MAXIMIZE,   [this] { Maximize(!window.IsMaximized()); });
	menu.AddKey(AK_MINIMIZE,   [this] { Minimize(!window.IsMinimized()); });
	menu.Separator();
	menu.Add(enable, AK_PREV,  [this] { stack.Prev();    SyncTitle(); });
	menu.Add(enable, AK_NEXT,  [this] { stack.Next();    SyncTitle(); });
	menu.Add(enable, AK_BEGIN, [this] { stack.GoBegin(); SyncTitle(); });
	menu.Add(enable, AK_END,   [this] { stack.GoEnd();   SyncTitle(); });

	if(Terminal *t = GetActiveTerminal(); t)
		t->ViewMenu(menu);
}

void Bobcat::SetupMenu(Bar& menu)
{
	menu.Add(AK_SETTINGS,  [this] { Settings(); });
	menu.Add(AK_KEYCONFIG, [this] { EditKeys(); SaveShortcutKeys(); });
}

void Bobcat::HelpMenu(Bar& menu)
{
	menu.Add(t_("Help"), [this] { Help(); });
	menu.Add(t_("About"), [this] { About(); });
}

void Bobcat::TermMenu(Bar& menu)
{
	Vector<String> pnames = GetProfileNames();
	menu.Add(AK_NEWTAB, Images::Terminal(), [this] {  AddTerminal(settings.activeprofile); });
	if(!pnames.GetCount())
		return;
	
	if(int n = FindIndex(pnames, settings.activeprofile); n > 0) {
		String s = pnames[n];
		pnames.Remove(n);
		pnames.Insert(0, s); // Move the active (default) profile to the top of the menu.
	}
	
	menu.Sub(t_("New terminal from"), [this, pnames = pick(pnames)](Bar& menu) {
		for(int i = 0; i < pnames.GetCount(); i++) {
			const String& name = pnames[i];
			auto& item = menu.Add(name, Images::Terminal(), [this, name] { AddTerminal(name); });
			if(i < 10) { // Set up the accelerator keys for the first ten profiles.
				item.Key(decode(i,
					0, (KeyInfo& (*)()) AK_PROFILE1,
					1, (KeyInfo& (*)()) AK_PROFILE2,
					2, (KeyInfo& (*)()) AK_PROFILE3,
					3, (KeyInfo& (*)()) AK_PROFILE4,
					4, (KeyInfo& (*)()) AK_PROFILE5,
					5, (KeyInfo& (*)()) AK_PROFILE6,
					6, (KeyInfo& (*)()) AK_PROFILE7,
					7, (KeyInfo& (*)()) AK_PROFILE8,
					8, (KeyInfo& (*)()) AK_PROFILE9,
					(KeyInfo& (*)()) AK_PROFILE10));
			}
		}
	});
	menu.AddKey(AK_NAVIGATOR, [this, &menu] { ToggleNavigator(); });
}

void Bobcat::ListMenu(Bar& menu)
{
	menu.Add(AK_NAVIGATOR, [this, &menu] { ToggleNavigator(); });
	menu.Separator();
	for(int i = 0; i < stack.GetCount(); i++) {
		Terminal& t = (Terminal &) stack[i];
		menu.Add(t.GetTitle(), [this, i] { stack.Goto(i); SyncTitle(); }).Radio(stack.GetCursor() == i);
	}
}

void Bobcat::SizeMenu(Bar& menu)
{
	menu.Separator();
	menu.Add(AK_80X24,  [this] { SetPageSize(Size(80, 24));  });
	menu.Add(AK_80X48,  [this] { SetPageSize(Size(80, 48));  });
	menu.Add(AK_132X24, [this] { SetPageSize(Size(132, 24)); });
	menu.Add(AK_132X48, [this] { SetPageSize(Size(132, 48)); });
	StringStream ss(settings.custompagesizes);
	while(!ss.IsEof()) {
		String row, col;
		if(SplitTo(ss.GetLine(), 'x', col, row)) {
			Size sz(StrInt(col), StrInt(row));
			if(10 <= sz.cx && sz.cx <= 300
			&& 10 <= sz.cy && sz.cy <= 300) {
				menu.Add(col + "x" + row, [this, sz] { SetPageSize(sz); });
			}
		}
	}
}

void Bobcat::ScreenShot()
{
	if(Terminal *t = GetActiveTerminal(); t) {
		ImagePainter w(t->GetSize());
		t->PaintPage(w);
		if(!settings.savescreenshot) {
			WriteClipboardImage(w);
			PromptOK(t_("Screenshot is copied to the clipboard."));
			return;
		}
		FileSel& fs = BobcatFs();
		if(fs.Type(t_("PNG images (*.png)"), "*.png").ExecuteSaveAs(t_("Save Screenshot")))
			PNGEncoder().SaveFile(~fs, w);
	}
}

void Bobcat::About()
{
	TabDlg dlg;
	WithAboutLayout<ParentCtrl> about, licenses;
	CtrlLayout(about);
	CtrlLayout(licenses);
	dlg.Add(about, tt_("About"));
	dlg.Add(licenses, tt_("Licenses"));
	String atxt =  GetTopic("topic://Bobcat/docs/about_en-us");
	atxt.Replace("$(BUILD)", DeQtf(GetBuildInfo()));
	about.txt.SetQTF(atxt);
	licenses.txt.SetQTF(GetTopic("topic://Bobcat/docs/licenses_en-us"));
	licenses.txt.SetZoom(Zoom(1200, 1800));
	dlg.OK().Open(&window);
	while(window.IsOpen() && dlg.IsOpen() && !dlg.GetExitCode()) {
		ProcessEvents();
		dlg.ProcessEvents();
	}
}

void Bobcat::Help()
{
	static const Tuple<const char*, const char*> topics[] {
		{ "overview",  t_("Overview")             },
		{ "usage",     t_("Command line options") },
		{ "shortcuts", t_("Keyboard shortcuts")   }
	};
	
	HelpWindow dlg;
	dlg.MaximizeBox().Title(t_("Bobcat Help"));
	for(int i = 0; i < __countof(topics); i++) {
		String s = "topic://Bobcat/docs/";
		dlg.AddTree(0, Null, s << topics[i].a << "_en-us", topics[i].b);
	}
	dlg.CurrentOrHome();
	dlg.SetRect(0, 0, 800, 600);
	dlg.Open(&window);
	while(window.IsOpen() && dlg.IsOpen() && !dlg.GetExitCode()) {
		ProcessEvents();
		dlg.ProcessEvents();
	}
}

Bobcat::Config::Config()
: guitheme("host")
, guifont(GetStdFont())
, titlealignment("top")
, finderalignment("bottom")
, stackdirection("horizontal")
, stackanimation(150)
, stackwheel(true)
, showmenu(true)
, showtitle(false)
, savescreenshot(false)
, custominputmethod(false)
, serializeplacement(false)
{
}

void Bobcat::Config::Jsonize(JsonIO& jio)
{
	jio("ActiveProfile", activeprofile)
	   ("TitleBarAlignment", titlealignment)
	   ("FinderBarAlignment", finderalignment)
	   ("StackAnimationDirection", stackdirection)
	   ("StackAnimationDuration", stackanimation)
	   ("StackWheelMode", stackwheel)
	   ("ShowMenuBar", showmenu)
	   ("ShowTitleBar", showtitle)
	   ("SaveScreenshot", savescreenshot)
	   ("CustomInputMethod", custominputmethod)
	   ("CustomPageSizes", custompagesizes)
	   ("GuiTheme", guitheme)
	   ("GuiFont", guifont);
}

String GetConfigFile()
{
	return ConfigFile("Bobcat.cfg");
}

void LoadConfig(Bobcat& ctx)
{
	LoadFromJsonFile(ctx.settings, GetConfigFile());
}

void SaveConfig(Bobcat& ctx)
{
	StoreAsJsonFile(ctx.settings, GetConfigFile(), true);
}

}
