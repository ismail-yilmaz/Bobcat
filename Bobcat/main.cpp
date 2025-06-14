// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

using namespace Upp;

void PrintHelp()
{
#ifdef PLATFORM_POSIX

	String txt;
	txt << t_("Usage:") << "\n"
		<< "\tbobcat [" << t_("OPTIONS") << "] -- [" << t_("COMMAND") << "...]\n";
	
	CmdArgType type = CmdArgType::General;
	
	for(auto t : { CmdArgType::General, CmdArgType::Environment, CmdArgType::Emulation, CmdArgType::Appearance }) {
			if(type != t && type == CmdArgType::General) {
				txt << "\n" << t_("Profile Specific Options") << "\n";
				type = t;
			}
			txt << "\n" << GetCmdArgTypeName(t) << "\n";
			for(const CmdArg *a : FindCmdArgs(t)) {
				txt << "\t";
				String sopt = a->sopt;
				String lopt = a->lopt;
				
				if(!sopt.IsEmpty()) {
					txt << "-" << sopt;
					if(!lopt.IsEmpty())
						txt << ", ";
				}
				else {
					txt << "    ";
				}
				
				if(!lopt.IsEmpty())
					txt << "--" << lopt;
				
				if(!String(a->arg).IsEmpty())
					txt << " " << a->arg;
		
		        int padding = 30 - (txt.GetLength() - txt.ReverseFind('\n') - 1);
		        txt << String(' ', max(1, padding)) << a->desc << "\n";
			}
	}
	Cout() << txt;
	
#else
	PromptOK(GetTopic("Bobcat/docs/usage_en-us"));
#endif
}

void PrintProfileList()
{
	String list;
	for(const String& s : GetProfileNames())
		list << s << "\n";
#ifdef PLATFORM_POSIX
	Cout() << list;
#else
	PromptOK(DeQtf(list));
#endif
}

void PrintPaletteList()
{
	String list;
	for(const String& s : GetPaletteNames())
		list << s << "\n";
#ifdef PLATFORM_POSIX
	Cout() << list;
#else
	PromptOK(DeQtf(list));
#endif
}

void PrintGuiThemeList()
{
	String list;
	for(const auto& theme: GetAllGuiThemes())
		list << theme.b << "\n";
#ifdef PLATFORM_POSIX
	Cout() << list;
#else
	PromptOK(DeQtf(list));
#endif
}

void PrintFontList()
{
	String list;
	for(int i = 0; i < Font::GetFaceCount(); i++)
		if((Font::GetFaceInfo(i) & Font::FIXEDPITCH)) {
			list << Font::GetFaceName(i) << "\n";
		}
#ifdef PLATFORM_POSIX
	Cout() << list;
#else
	PromptOK(DeQtf(list));
#endif
}

void BobcatAppMain()
{
	Size page_size(80, 24);
	bool fullscreen = false;
	
	Bobcat app;
	LoadConfig(app);
	
	// Try loading the default profile, if any, and fallback to default configuration on failure.
	Profile p = LoadProfile(app.settings.defaultprofile);
	
	if(const Vector<String>& cmd = CommandLine(); !cmd.IsEmpty()) {
		CmdArgList arglist;
		String cmdargerr;
		if(CmdArgParser argparser(GetCmdArgs()); argparser.Parse(cmd, arglist, cmdargerr)) {
			// Note that selected profile should be loaded before any profile related options.
			if(arglist.HasOption("help"))
			{
				PrintHelp();
				return;
			}
			if(arglist.HasOption("settings"))
			{
				app.Settings();
				return;
			}
			if(arglist.HasOption("list"))
			{
				PrintProfileList();
				return;
			}
			if(arglist.HasOption("version"))
			{
				Cout() << GetBuildInfo() << "\n";
				return;
			}
			if(arglist.HasOption("list-palettes"))
			{
				PrintPaletteList();
				return;
			}
			if(arglist.HasOption("list-fonts"))
			{
				PrintFontList();
				return;
			}
			if(arglist.HasOption("list-gui-themes"))
			{
				PrintGuiThemeList();
				return;
			}
			if(const String& q = arglist.Get("gui-theme"); !IsNull(q))
			{
				app.settings.guitheme = q;
			}
			if(arglist.HasOption("show-bars"))
			{
				app.settings.showmenu = true;
				app.settings.showtitle = true;
			}
			if(arglist.HasOption("hide-bars"))
			{
				app.settings.showmenu = false;
				app.settings.showtitle = false;
			}
			if(arglist.HasOption("show-menubar"))
			{
				app.settings.showmenu = true;
			}
			if(arglist.HasOption("hide-menubar"))
			{
				app.settings.showmenu = false;
			}
			if(arglist.HasOption("show-titlebar"))
			{
				app.settings.showtitle = true;
			}
			if(arglist.HasOption("hide-titlebar"))
			{
				app.settings.showtitle = false;
			}
			if(arglist.HasOption("show-borders"))
			{
				app.settings.frameless = false;
			}
			if(arglist.HasOption("hide-borders"))
			{
				app.settings.frameless = true;
			}
			if(arglist.HasOption("fullscreen"))
			{
				fullscreen = true;
				app.Maximize(false);
			}
			if(arglist.HasOption("maximize"))
			{
				app.Maximize();
				fullscreen = false;
			}
			if(const String& q = arglist.Get("geometry"); !IsNull(q))
			{
				if(Size sz = ParsePageSize(q); !IsNull(sz)) {
					page_size = sz;
					app.Maximize(false);
					fullscreen = false;
				}
			}
			if(const String& q = arglist.Get("profile"); !IsNull(q))
			{
				p = LoadProfile(q);
			}
#ifdef PLATFORM_WIN32
			if(const String& q = arglist.Get("pty-backend"); !IsNull(q))
			{
				p.ptybackend = q;
			}
#endif
			if(arglist.HasOption("restart"))
			{
				p.onexit = "restart";
			}
			if(arglist.HasOption("restart-failed"))
			{
				p.onexit = "restart_failed";
			}
			if(arglist.HasOption("keep"))
			{
				p.onexit = "keep";
			}
			if(arglist.HasOption("dont-keep"))
			{
				p.onexit = "exit";
			}
			if(arglist.HasOption("ask"))
			{
				p.onexit = "ask";
			}
			if(arglist.HasOption("environment"))
			{
				p.noenv = false;
			}
			if(arglist.HasOption("no-environment"))
			{
				p.noenv = true;
			}
			if(const String& q = arglist.Get("working-dir"); !IsNull(q))
			{
				p.address = q;
			}
			if(arglist.HasOption("vt-style-fkeys"))
			{
				p.functionkeystyle = "vt";
			}
			if(arglist.HasOption("pc-style-fkeys"))
			{
				p.functionkeystyle = "pc";
			}
			if(arglist.HasOption("window-reports"))
			{
				p.windowreports = true;
			}
			if(arglist.HasOption("no-window-reports"))
			{
				p.windowreports = false;
			}
			if(arglist.HasOption("window-actions"))
			{
				p.windowactions = true;
			}
			if(arglist.HasOption("no-window-actions"))
			{
				p.windowactions = false;
			}
			if(arglist.HasOption("hyperlinks"))
			{
				p.hyperlinks = true;
			}
			if(arglist.HasOption("no-hyperlinks"))
			{
				p.hyperlinks = false;
			}
			if(arglist.HasOption("inline-images"))
			{
				p.inlineimages = true;
			}
			if(arglist.HasOption("no-inline-images"))
			{
				p.inlineimages = false;
			}
			if(arglist.HasOption("annotations"))
			{
				p.annotations = true;
			}
			if(arglist.HasOption("no-annotations"))
			{
				p.annotations = false;
			}
			if(arglist.HasOption("clipboard-access"))
			{
				p.clipboardread = true;
				p.clipboardwrite = true;
			}
			if(arglist.HasOption("no-clipboard-access"))
			{
				p.clipboardread = false;
				p.clipboardwrite = false;
			}
			if(const String& q = arglist.Get("palette"); !IsNull(q))
			{
				p.palette = q;
			}
			if(const String& q = arglist.Get("font-family"); !IsNull(q))
			{
				if(Font f = p.font; f.FaceName(q).IsFixedPitch())
					p.font = f;
			}
			if(const String& q = arglist.Get("font-size"); !IsNull(q))
			{
				p.font.Height(clamp(StrInt(q), 6, 128));
			}
			if(arglist.HasOption("bell"))
			{
				p.bell = true;
			}
			if(arglist.HasOption("no-bell"))
			{
				p.bell = false;
			}
			if(!IsNull(arglist.command))
			{
				p.command = arglist.command;
			}
		}
		else {
			Cout() << t_("Command line error.");
			if(!IsNull(cmdargerr)) {
				Cout() << " [" << cmdargerr << "]\n\n";
			}
			PrintHelp();
			Exit();
		}
	}

	LoadShortcutKeys();
	LoadGuiFont(app);
	LoadGuiTheme(app);
	app.Run(p, page_size, fullscreen);
	SaveShortcutKeys();
}

#ifdef flagWEBGUI
CONSOLE_APP_MAIN
{
#ifdef _DEBUG
	TurtleServer::DebugMode();
#endif

	String host  = "localhost";
	int port     = 8888;
	int maxconn  = 15;
	int maxmemkb = 100000000;
	
	const Vector<String>& cmd = CommandLine();

	if(int n = FindIndex(cmd, "--url"); n >= 0 && ++n < cmd.GetCount()) {
		UrlInfo url(cmd[n]);
		if(!IsNull(url.host))
			host = url.host;
		if(!IsNull(url.port))
			port = StrInt(url.port);
		if(int q = url.array_parameters.Find("max_connection"); q >= 0)
			maxconn = StrInt(url.parameters[q]);
		if(int q = url.array_parameters.Find("memory_limit"); q >= 0)
			maxmemkb = StrInt(url.parameters[q]);
	}

	MemoryLimitKb(maxmemkb); // Can aid preventing DDoS attacks.

	TurtleServer guiserver;
	guiserver.Host(host);
	guiserver.HtmlPort(port);
	guiserver.MaxConnections(maxconn);
	RunTurtleGui(guiserver, BobcatAppMain);
}
#elif flagSDLGUI
CONSOLE_APP_MAIN
{
	Rect screen(0, 0, 1024, 768);

	const Vector<String>& cmd = CommandLine();

	if(int n = FindIndex(cmd, "--sdl-screen-size"); n >= 0 && ++n < cmd.GetCount()) {
		if(String cx, cy; SplitTo(cmd[n], 'x', cx, cy)) {
			screen.SetSize(
				clamp(StrInt(cx), 1024, 4096),
				clamp(StrInt(cy), 768,  4096)
			);
		}
	}
	SDL2GUI gui;
	gui.Create(screen, t_("Bobcat [SDL2-GUI]"));
	RunVirtualGui(gui,BobcatAppMain);
}
#else
GUI_APP_MAIN
{
	BobcatAppMain();
}
#endif
