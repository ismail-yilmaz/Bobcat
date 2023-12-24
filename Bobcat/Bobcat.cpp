// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

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

static Terminal& AsTerminal(Ctrl& c)
{
	return static_cast<Terminal&>(c);
}

static void sEventLoop(Bobcat& app)
{
	app.Resize(app.GetActiveTerminal()->GetStdSize());
	app.window.OpenMain();
	while(app.window.IsOpen() && app.terminals.GetCount()) {
		app.window.ProcessEvents();
		int l = 10;
		for(int i = 0; i < app.terminals.GetCount(); i++) {
			Terminal& t = app.terminals[i];
			if(int n = t.Do(); n < 0) {
				app.stack.Remove(t);
				app.terminals.Remove(i);
				app.Sync();
				break;
			}
			else
				l = max(l, n);
		}
		Sleep(l >= 1024 ? 1024 * 10 / l : 10);
	}
}

Bobcat::Bobcat()
: navigator(*this)
{
	SyncTitle();
	window.Sizeable().Zoomable().CenterScreen();
	window.WhenClose = [this]() { Close(); };
	window.Add(view.SizePos());
	view.AddFrame(menubar);
	view.Add(stack.Animation().Wheel().Horz().SizePos());
	menubar.Set([this](Bar& menu) { MainMenu(menu); });
}

bool Bobcat::AddTerminal(const Value& key)
{
	bool ok = terminals.Create(*this).Start(key);
	if(ok) Sync();
	return ok;
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

void Bobcat::RunCommand(const String& cmd)
{
	Profile p("external");
	p.command = cmd;
	if(HideMenuBar().terminals.Create(*this).Start(p))
		sEventLoop(*this);
}

void Bobcat::Run()
{
	if(AddTerminal(settings.activeprofile))
		sEventLoop(*this);
}

void Bobcat::RunWithProfile(const String& name)
{
	if(AddTerminal(name))
		sEventLoop(*this);
}

void Bobcat::Close()
{
	if(window.IsOpen())
		window.Close();
}

void Bobcat::Settings()
{
	Profiles  profiles(*this);
	WithSettingsLayout<ParentCtrl> settingspane;

	settingspane.titlepos.Add("top", tt_("Top"));
	settingspane.titlepos.Add("bottom", tt_("Bottom"));
	settingspane.titlepos.GoBegin();
	
	settingspane.direction.Add("horizontal", tt_("Horizontal"));
	settingspane.direction.Add("vertical", tt_("Vertical"));
	settingspane.direction.GoBegin();

	CtrlRetriever cr;
	cr(settingspane.titlepos, settings.titlealignment);
	cr(settingspane.direction, settings.stackdirection);
	cr(settingspane.animation, settings.stackanimation);
	cr(settingspane.wheel, settings.stackwheel);
	cr(settingspane.showmenu, settings.showmenu);
	cr(settingspane.showtitle, settings.showtitle);
	cr(settingspane.savescreenshot, settings.savescreenshot);
	cr(settingspane.custominput, settings.custominputmethod);
	cr.Set();

	profiles.Load();

	TabDlg dlg;
	dlg.Add(profiles, tt_("Profiles"));
	dlg.Add(settingspane, tt_("General"));
	dlg.WhenClose = dlg.Acceptor(IDEXIT);
	if(dlg.OK().Cancel().Title(tt_("Settings")).ExecuteOK()) {
		cr.Retrieve();
		profiles.Store();
		Sync();
		SyncTerminalProfiles();
	}

}

Bobcat& Bobcat::Maximize(bool b)
{
	window.Maximize();
	return *this;
}

bool Bobcat::IsMaximized() const
{
	return window.IsMaximized();
}

Bobcat& Bobcat::Minimize(bool b)
{
	window.Minimize();
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

Bobcat& Bobcat::SetRect(Rect r)
{
	NoFullScreen();
	window.SetRect(r);
	return *this;
}

Bobcat& Bobcat::FullScreen(int mode)
{
	window.FullScreen(mode == 0 ? !IsFullScreen() : mode == 1);
	return *this;
}

Bobcat& Bobcat::ToggleFullScreen()
{
	window.FullScreen(!IsFullScreen());
	return *this;
}

Bobcat& Bobcat::NoFullScreen()
{
	if(IsFullScreen())
		window.FullScreen(false);
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
		GetActiveTerminal()->SetFocus();
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
		s << " [" << t->GetTitle() << "]";
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
	menu.Add(AK_KEYCONFIG, [this] { EditKeys(); });
}

void Bobcat::HelpMenu(Bar& menu)
{
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
		menu.Add(t.GetTitle(), [this, i] { stack.Goto(i); }).Radio(stack.GetCursor() == i);
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
	atxt.Replace("TEXT", DeQtf(GetBuildInfo()));
	about.txt.SetQTF(atxt);
	licenses.txt.SetQTF(GetTopic("topic://Bobcat/docs/licenses_en-us"));
	licenses.txt.SetZoom(Zoom(1, 2));
	dlg.OK().Execute();
}

Bobcat::Config::Config()
: titlealignment("top")
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
	   ("StackAnimationDirection", stackdirection)
	   ("StackAnimationDuration", stackanimation)
	   ("StackWheelMode", stackwheel)
	   ("ShowMenuBar", showmenu)
	   ("ShowTitleBar", showtitle)
	   ("SaveScreenshot", savescreenshot)
	   ("CustomInputMethod", custominputmethod)
	   ("SeralizePlacement", serializeplacement);
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
