// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME "General"
#define KEYNAMESPACE AppKeys
#define KEYFILE <Bobcat/Application.key>
#include <CtrlLib/key_source.h>

using namespace AppKeys;

static Ptr<Bobcat> sBobcatCtx;

Ptr<Bobcat> GetContext()
{
	return sBobcatCtx;
}

void SetContext(Bobcat& ctx)
{
	sBobcatCtx = &ctx;
}

FileSel& BobcatFs()
{
	return Single<FileSel>();
}

Bobcat::Bobcat()
: navigator(*this)
{
	SetContext(*this);
	SyncTitle();
	window.Sizeable().Zoomable().CenterScreen();
	window.WhenClose = [this]() { Close(); };
	window.Add(view.SizePos());
	window.Add(navigator.SizePos());
	window.Icon(Images::Icon);
	view.AddFrame(menubar);
	view.Add(stack.Animation().Wheel().Horz().SizePos());
	navigator.WhenBar        = [this](Bar& menu) { TermMenu(menu); };
	navigator.WhenClose      = [this]()          { ToggleNavigator(); };
	navigator.WhenGotoItem   = [this](Ctrl& c)   { ToggleNavigator(); stack.Goto(c); };
	navigator.WhenRemoveItem = [this](Ctrl& c)   { RemoveTerminal(AsTerminal(c)); };
	stack.WhenAction         = [this]()          { Sync(); };
	stack.WhenSwap           = [this](int a, int b) { navigator.AnimateSwap(a, b); };
	menubar.Set([this](Bar& menu) { MainMenu(menu); });
	GetNotificationDaemon().NoIcon().Append().Animation();
}

bool Bobcat::AddTerminal(const String& key, bool pane)
{
	bool ok = terminals.Create(*this).Start(key, pane);
	if(ok) Sync();
	return ok;
}

bool Bobcat::AddTerminal(const Profile& profile, bool pane)
{
	bool ok = terminals.Create(*this).Start(profile, pane);
	if(ok) Sync();
	return ok;
}

bool Bobcat::NewTerminalFromActiveProfile(bool pane)
{
	bool ok = terminals.Create(*this).Start(GetActiveTerminal(), pane);
	if(ok) Sync();
	return ok;
}

void Bobcat::RemoveTerminal(Terminal& t)
{
	if(t.pty && t.pty->IsRunning())
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
	return (Terminal*) stack.GetActiveCtrl();
}

String Bobcat::GetActiveProfile()
{
	if(Terminal *t = GetActiveTerminal(); t)
		return t->profilename;
	else
		return "";
}

Vector<Terminal*> Bobcat::GetTerminalGroup(hash_t id)
{
	Vector<Terminal*> v;
	for(Terminal& t : terminals) {
		if(t == id)
			v.Add(&t);
	}
	return v;
}

Vector<Terminal*> Bobcat::GetTerminalGroup(const Profile& p)
{
	return GetTerminalGroup(p.GetHashValue());
}

void Bobcat::ProcessEvents()
{
	window.ProcessEvents();
	auto& we = Single<PtyWaitEvent>();
	we.Clear();
	for(Terminal& t : terminals)
		we.Add(*(t.pty), WAIT_READ | WAIT_IS_EXCEPTION);
	we.Wait(clamp(settings.ptywaitinterval, 0, 100));
	for(Terminal& t : terminals)
		if(!t.Do())
			RemoveTerminal(t);
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
	
	settingspane.notifierpos.Add("top", st);
	settingspane.notifierpos.Add("bottom", sb);
	settingspane.notifierpos.GoBegin();
	
	settingspane.direction.Add("horizontal", tt_("Horizontal"));
	settingspane.direction.Add("vertical", tt_("Vertical"));
	settingspane.direction.GoBegin();
	
	settingspane.splitterorientation.Add("horizontal", tt_("Horizontal"));
	settingspane.splitterorientation.Add("vertical", tt_("Vertical"));
	settingspane.splitterorientation.GoBegin();
	
	settingspane.imagemode.Add("normal", tt_("Normal"));
	settingspane.imagemode.Add("centered", tt_("Centered"));
	settingspane.imagemode.Add("stretched", tt_("Stretched"));
	settingspane.imagemode.Add("tiled", tt_("Tiled"));
	settingspane.direction.GoBegin();
	
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
	
	FileSelButton filesel;
	filesel.Types("*.jpg *.png *.bmp");
	filesel.Attach(settingspane.imagepath);

	settingspane.imageblur.MinMax(0, 20).Step(1) <<= 0;
	
	settingspane.backgroundimage.WhenAction = [&] {
		bool b = ~settingspane.backgroundimage;
		settingspane.imagemode.Enable(b);
		settingspane.imageblur.Enable(b);
		settingspane.imagepath.Enable(b);
	};
	
	settingspane.backgroundimage.WhenAction();
	
	CtrlRetriever cr;
	cr(settingspane.titlepos, settings.titlealignment);
	cr(settingspane.finderpos, settings.finderalignment);
	cr(settingspane.notifierpos, settings.notificationalignment);
	cr(settingspane.notifieranimation, settings.notificationanimation);
	cr(settingspane.direction, settings.stackdirection);
	cr(settingspane.animation, settings.stackanimation);
	cr(settingspane.wheel, settings.stackwheel);
	cr(settingspane.splitterorientation, settings.splitterorientation);
	cr(settingspane.showmenu, settings.showmenu);
	cr(settingspane.showtitle, settings.showtitle);
	cr(settingspane.frameless, settings.frameless);
	cr(settingspane.savescreenshot, settings.savescreenshot);
	cr(settingspane.custominput, settings.custominputmethod);
	cr(settingspane.pagesizes, settings.custompagesizes);
	cr(settingspane.chstyle, settings.guitheme);
	cr(settingspane.guifont, settings.guifont);
	cr(settingspane.backgroundimage, settings.backgroundimage);
	cr(settingspane.imagemode, settings.backgroundimagemode);
	cr(settingspane.imagepath, settings.backgroundimagepath);
	cr(settingspane.imageblur, settings.backgroundimageblur);
	cr(settingspane.waitinterval, settings.ptywaitinterval);
	cr.Set();

	profiles.Load();

	// ** EXPERIMENTAL WAYLAND SUPPORT **
	settingspane.wayland.Hide();
	#ifdef GUI_GTK
	settingspane.wayland.Show();
	settingspane.wayland <<= IsWaylandEnabled();
	#endif

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
			EnableWayland(~settingspane.wayland);
			if(window.IsOpen()) {
				GetHyperlinkPatterns().Clear();     // Drop all patterns. Terminals will reacquire them.
				GetWordSelectionPatterns().Clear(); // Drop all patterns.
				GetWebSearchProviders().Clear();    // Drop all providers.
				view <<= Null;                      // Drop any existing background image.
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

Bobcat& Bobcat::ToggleBars()
{
	settings.showmenu  ^= 1;
	settings.showtitle ^= 1;
	Sync();
	return *this;
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
	window.FrameLess(settings.frameless);
	settings.stackdirection == "horizontal"	? stack.Horz() : stack.Vert();
	settings.splitterorientation == "horizontal" ? stack.HorzSplitter() : stack.VertSplitert();
	auto& m = GetNotificationDaemon();
	settings.notificationalignment == "top" ? m.Top() : m.Bottom();
	m.Animation(settings.notificationanimation);
	stack.Wheel(settings.stackwheel);
	stack.Animation(settings.stackanimation);
	SyncBackground();
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

void Bobcat::SyncBackground()
{
	bool bkimg = settings.backgroundimage;

	if(bkimg) {
		bkimg &= FileExists(settings.backgroundimagepath);
		if(bkimg && IsNull(view.data)) {
			view.mode = settings.backgroundimagemode;
			int n = clamp(settings.backgroundimageblur, 0, 20);
			Image img = StreamRaster::LoadFileAny(settings.backgroundimagepath);
			view <<= n ? GaussianBlur(img, n) : img;
		}
	}
	else
		view <<= Null;
	
	stack.NoBackground(bkimg);
	for(int i = 0; i < stack.GetCount(); i++)
		AsTerminal(stack[i]).Sync().NoBackground(bkimg);
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
	menu.Sub(t_("File"),     [this](Bar& menu) { FileMenu(menu);  });
	menu.Sub(t_("Edit"),     [this](Bar& menu) { EditMenu(menu);  });
	menu.Sub(t_("View"),     [this](Bar& menu) { ViewMenu(menu);  });
	menu.Sub(t_("Emulation"),[this](Bar& menu) { EmulationMenu(menu); });
	menu.Sub(t_("List"),     [this](Bar& menu) { ListMenu(menu);  });
	menu.Sub(t_("Setup"),    [this](Bar& menu) { SetupMenu(menu); });
	menu.Sub(t_("Help"),     [this](Bar& menu) { HelpMenu(menu);  });
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
	
	menu.Add(AK_FRAMELESS,     [this] { settings.frameless ^= 1; Sync(); }).Check(settings.frameless);
	menu.Add(AK_MENUBAR,       [this] { settings.showmenu  ^= 1; Sync(); }).Check(settings.showmenu);
	menu.Add(AK_TITLEBAR,      [this] { settings.showtitle ^= 1; Sync(); }).Check(settings.showtitle);
	menu.AddKey(AK_TOGGLEBARS, [this] { ToggleBars(); });
	menu.Add(AK_FULLSCREEN,    [this] { FullScreen(0); }).Check(window.IsFullScreen());
	menu.AddKey(AK_MAXIMIZE,   [this] { Maximize(!window.IsMaximized()); });
	menu.AddKey(AK_MINIMIZE,   [this] { Minimize(!window.IsMinimized()); });
	menu.Separator();
	menu.Add(enable, AK_PREV, Images::Prev(),  [this] { stack.Prev(); });
	menu.Add(enable, AK_NEXT, Images::Next(),  [this] { stack.Next(); });
	menu.Add(enable, AK_BEGIN,Images::Begin(), [this] { stack.GoBegin(); });
	menu.Add(enable, AK_END,  Images::End(),   [this] { stack.GoEnd(); });
	menu.AddKey(AK_SPLITTER_TOGGLE,            [this] { stack.ToggleSplitterOrientation(); });
	menu.AddKey(AK_SPLITTER_EXPANDTOPLEFT,     [this] { stack.ExpandTopLeftPane(); });
	menu.AddKey(AK_SPLITTER_EXPANDBOTTOMRIGHT, [this] { stack.ExpandBottomRightPane(); });
	menu.AddKey(AK_SPLITTER_RESETPOS,          [this] { stack.ResetSplitterPos(); });
	menu.AddKey(AK_SPLITTER_SWAPPANES,         [this] { stack.SwapPanes(); });

	if(Terminal *t = GetActiveTerminal(); t)
		t->ViewMenu(menu);
}

void Bobcat::EmulationMenu(Bar& menu)
{
	if(Terminal *t = GetActiveTerminal(); t)
		t->EmulationMenu(menu);
}

void Bobcat::SetupMenu(Bar& menu)
{
	menu.Add(AK_SETTINGS,  [this] { Settings(); });
	menu.Add(AK_KEYCONFIG, [] { EditKeys(); SaveShortcutKeys(); });
}

void Bobcat::HelpMenu(Bar& menu)
{
	menu.Add(t_("Help"),  [this] { Help();  });
	menu.Add(t_("About"), [this] { About(); });
}

void Bobcat::TermMenu(Bar& menu)
{
	menu.Add(AK_NEWTAB, Images::Terminal(),    [this] { NewTerminalFromActiveProfile(); });
	menu.AddKey(AK_NEWPANE,                    [this] { NewTerminalFromActiveProfile(true); });
	
	Vector<String> pnames = GetProfileNames();
	if(!pnames.GetCount())
		return;
	menu.Sub(t_("New terminal from..."), [this, pnames = pick(pnames)](Bar& menu) { TermSubmenu(menu, pnames); });
	menu.AddKey(AK_NAVIGATOR, [this] { ToggleNavigator(); });
}

void Bobcat::TermSubmenu(Bar& menu, const Vector<String>& list)
{
	for(int i = 0; i < list.GetCount(); i++) {
		const String& name = list[i];
		auto& item = menu.Add(name, [this, name] { AddTerminal(name); });
		item.Image(name == settings.defaultprofile ? Images::DefaultTerminal() : Images::Terminal());
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
			menu.AddKey(decode(i,
				0, (KeyInfo& (*)()) AK_SPROFILE1,
				1, (KeyInfo& (*)()) AK_SPROFILE2,
				2, (KeyInfo& (*)()) AK_SPROFILE3,
				3, (KeyInfo& (*)()) AK_SPROFILE4,
				4, (KeyInfo& (*)()) AK_SPROFILE5,
				5, (KeyInfo& (*)()) AK_SPROFILE6,
				6, (KeyInfo& (*)()) AK_SPROFILE7,
				7, (KeyInfo& (*)()) AK_SPROFILE8,
				8, (KeyInfo& (*)()) AK_SPROFILE9,
				(KeyInfo& (*)()) AK_SPROFILE10), [this, name] { AddTerminal(name, true); });
		}
	}
}

void Bobcat::ListMenu(Bar& menu)
{
	menu.Add(AK_NAVIGATOR, Images::Navigator(), [this] { ToggleNavigator(); });
	menu.Separator();
	for(int i = 0; i < stack.GetCount(); i++) {
		Terminal& t = AsTerminal(stack[i]);
		menu.Add(t.GetTitle(), [this, i] { stack.Goto(i); SyncTitle(); }).Radio(stack.GetCursor() == i);
	}
}

void Bobcat::SizeMenu(Bar& menu)
{
	menu.Separator();
	menu.Add(AK_GEOM_80_24,  [this] { SetPageSize(Size(80, 24));  });
	menu.Add(AK_GEOM_80_48,  [this] { SetPageSize(Size(80, 48));  });
	menu.Add(AK_GEOM_132_24, [this] { SetPageSize(Size(132, 24)); });
	menu.Add(AK_GEOM_132_48, [this] { SetPageSize(Size(132, 48)); });
	StringStream ss(settings.custompagesizes);
	while(!ss.IsEof()) {
		if(Size sz = ParsePageSize(ss.GetLine()); !IsNull(sz)) {
			menu.Add(AsString(sz.cx) + "x" + AsString(sz.cy), [this, sz] { SetPageSize(sz); });
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
	WithAboutLayout<ParentCtrl> about, contributors, licenses;
	CtrlLayout(about);
	CtrlLayout(contributors);
	CtrlLayout(licenses);
	dlg.Add(about, tt_("About"));
	dlg.Add(contributors, tt_("Contributors"));
	dlg.Add(licenses, tt_("Licenses"));
	String atxt =  GetTopic("topic://Bobcat/docs/about_en-us");
	atxt.Replace("$(BUILD)", DeQtf(GetBuildInfo()));
	about.txt.SetQTF(atxt);
	about.txt.NoLazy();
	contributors.txt.SetQTF(GetTopic("topic://Bobcat/docs/contributors_en-us"));
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

Bobcat::ViewCtrl::ViewCtrl()
{
}

void Bobcat::ViewCtrl::SetData(const Value& v)
{
	data = v;
}

Value Bobcat::ViewCtrl::GetData() const
{
	return data;
}

void Bobcat::ViewCtrl::Paint(Draw& w)
{
	Rect r = GetRect();
	
	if(IsNull(data))
		w.DrawRect(r, SColorFace);
	else {
		if(mode == "normal")
			NormalImageDisplay().Paint(w, r, data, SColorText, SColorPaper, 0);
		else
		if(mode == "centered")
			CenteredImageDisplay().Paint(w, r, data, SColorText, SColorPaper, 0);
		else
		if(mode == "stretched")
			ImageDisplay().Paint(w, r, data, SColorText, SColorPaper, 0);
		else
		if(mode == "tiled")
			TiledImageDisplay().Paint(w, r, data, SColorText, SColorPaper, 0);
	}
}

Bobcat::Config::Config()
: guitheme("host")
, guifont(GetStdFont())
, titlealignment("top")
, finderalignment("bottom")
, notificationalignment("top")
, notificationanimation(true)
, stackdirection("horizontal")
, stackanimation(150)
, stackwheel(true)
, showmenu(true)
, showtitle(false)
, frameless(false)
, savescreenshot(false)
, custominputmethod(false)
, serializeplacement(false)
, backgroundimage(false)
, backgroundimagemode("normal")
, backgroundimageblur(0)
, ptywaitinterval(10)
, splitterorientation("horizontal")
{
}

void Bobcat::Config::Jsonize(JsonIO& jio)
{
	jio("DefaultProfile", defaultprofile)
	   ("TitleBarAlignment", titlealignment)
	   ("FinderBarAlignment", finderalignment)
	   ("NotificationAlignment", notificationalignment)
	   ("NotificationAnimation", notificationanimation)
	   ("StackAnimationDirection", stackdirection)
	   ("StackAnimationDuration", stackanimation)
	   ("StackWheelMode", stackwheel)
	   ("SplitterOrientation", splitterorientation)
	   ("ShowMenuBar", showmenu)
	   ("ShowTitleBar", showtitle)
	   ("FramelessWindow", frameless)
	   ("SaveScreenshot", savescreenshot)
	   ("CustomInputMethod", custominputmethod)
	   ("CustomPageSizes", custompagesizes)
	   ("BackgroundImage", backgroundimage)
	   ("BackgroundImagePath", backgroundimagepath)
	   ("BackgroundImageMode", backgroundimagemode)
	   ("BackgroundImageBlur", backgroundimageblur)
	   ("PtyMonitoringInterval", ptywaitinterval)
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
