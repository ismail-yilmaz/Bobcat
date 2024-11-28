// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

using namespace Upp;

void PrintHelp()
{
#ifdef PLATFORM_POSIX
	const String sHelpText = t_(
		"Usage:\n"
		"   bobcat [OPTIONS] -- [COMMAND...]"
		"\n\n"
		"General options:\n"
		"   -h, --help                     Show help.\n"
		"   -l, --list                     List available profiles.\n"
		"   -p, --profile PROFILE          Run with the given PROFILE (Names are case-sensitive).\n"
		"   -s, --settings                 Open settings window.\n"
		"   -b, --show-bars                Show the menu and title bar.\n"
		"   -B, --hide-bars                Hide the menu and title bar.\n"
		"       --show-menubar             Show the menu bar.\n"
		"       --hide-menubar             Hide the menu bar.\n"
		"       --show-titlebar            Show the title bar.\n"
		"       --hide-titlebar            Hide the title bar.\n"
		"\n"
		"Environment options:\n"
		"   -r, --restart                  Restart the command on exit.\n"
		"   -R, --restart-failed           Restart the command when it fails.\n"
		"   -k, --keep                     Don't close the terminal on exit.\n"
		"   -K, --dont-keep                Close the terminal on exit.\n"
		"   -y, --ask                      Ask what to do on exit.\n"
		"   -n, --no-environment           Don't inherit the environment.\n"
		"   -d, --working-dir PATH         Set the working directory to PATH.\n"
		"   -f, --fullscreen               Full screen mode.\n"
		"   -m, --maximize                 Maximize the window.\n"
		"   -g, --geometry GEOMETRY        Set the initial window geometry. (E.g. 80x24, 132x24)\n"
		"\n"
		"Emulation options:\n"
		"   -q, --vt-style-fkeys           Use VT-style function keys.\n"
		"   -Q, --pc-style-fkeys           Use PC-style function keys.\n"
		"   -w, --window-reports           Enable window reports.\n"
		"   -W, --no-window-reports        Disable window reports.\n"
		"   -a, --window-actions           Enable window actions.\n"
		"   -A, --no-window-actions        Disable window actions.\n"
		"       --hyperlinks               Enable hyperlink detection (OSC 52).\n"
		"       --no-hyperlinks            Disable hyperlink detection.\n"
		"       --inline-images            Enable inline images support (sixel, iterm2, jexer).\n"
		"       --no-inline-images         Disable inline images support.\n"
	);
	Cout() << sHelpText;
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

void BobcatAppMain()
{
	StdLogSetup(LOG_FILE);
	
	Size page_size(80, 24);
	bool fullscreen = false;
	
	Bobcat app;
	LoadConfig(app);
	
	// Try loading the default profile, if any, and fallback to default configuration on failure.
	Profile p = LoadProfile(app.settings.defaultprofile);
	
	const Vector<String>& cmd = CommandLine();

	for(int i = 0, n = cmd.GetCount(); i < n; i++)
	{
		String s = cmd[i];
		if(findarg(s, "-l", "--list") != -1) {
			PrintProfileList();
			return;
		}
		else
		if(findarg(s, "-s", "--settings") != -1) {
			app.Settings();
			return;
		}
		else
		if(findarg(s, "-p", "--profile") != -1) {
			if(++i < n)
				p = LoadProfile(cmd[i]);
		}
		else
		if(findarg(s, "-b", "--show-bars") != -1) {
			app.settings.showmenu = true;
			app.settings.showtitle = true;
		}
		else
		if(findarg(s, "-B", "--hide-bars") != -1) {
			app.settings.showmenu = false;
			app.settings.showtitle = false;
		}
		else
		if(findarg(s, "--show-menubar") != -1) {
			app.settings.showmenu = true;
		}
		else
		if(findarg(s, "--hide-menubar") != -1) {
			app.settings.showmenu = false;
		}
		else
		if(findarg(s, "--show-titlebar") != -1) {
			app.settings.showtitle = true;
		}
		else
		if(findarg(s, "--hide-titlebar") != -1) {
			app.settings.showtitle = false;
		}
		else
		if(findarg(s, "-r", "--restart") != -1) {
			p.onexit = "restart";
		}
		else
		if(findarg(s, "-R", "--restart-failed") != -1) {
			p.onexit = "restart";
		}
		else
		if(findarg(s, "-k", "--keep") != -1) {
			p.onexit = "keep";
		}
		else
		if(findarg(s, "-K", "--dont-keep") != -1) {
			p.onexit = "exit";
		}
		else
		if(findarg(s, "-y", "--ask") != -1) {
			p.onexit = "ask";
		}
		else
		if(findarg(s, "-n", "--no-environment") != -1) {
			p.noenv = true;
		}
		else
		if(findarg(s, "-d", "--working-dir") != -1) {
			if(++i < n)
				p.address = cmd[i];
		}
		else
		if(findarg(s, "-f", "--fullscreen") != -1) {
			fullscreen = true;
			app.Maximize(false);
		}
		else
		if(findarg(s, "-m", "--maximize") != -1) {
			app.Maximize();
			fullscreen = false;
		}
		else
		if(findarg(s, "-g", "--geometry") != -1) {
			if(String r, c; ++i < n && SplitTo(ToLower(cmd[i]), "x", c, r)) {
				page_size.cx = clamp(StrInt(c), 10, 300);
				page_size.cy = clamp(StrInt(r), 10, 300);
				app.Maximize(false);
				fullscreen = false;
			}
		}
		else
		if(findarg(s, "-q", "--vt-style-fkeys") != -1) {
			p.functionkeystyle = "vt";
		}
		else
		if(findarg(s, "-Q", "--pc-style-fkeys") != -1) {
			p.functionkeystyle = "pc";
		}
		else
		if(findarg(s, "-w", "--window-reports") != -1) {
			p.windowreports = true;
		}
		else
		if(findarg(s, "-W", "--no-window-reports") != -1) {
			p.windowreports = false;
		}
		else
		if(findarg(s, "-a", "--window-actions") != -1) {
			p.windowactions = true;
		}
		else
		if(findarg(s, "-A", "--no-window-actions") != -1) {
			p.windowactions = false;
		}
		else
		if(findarg(s, "--hyperlinks") != -1) {
			p.hyperlinks = true;
		}
		else
		if(findarg(s, "--no-hyperlinks") != -1) {
			p.hyperlinks = false;
		}
		else
		if(findarg(s, "--inline-images") != -1) {
			p.inlineimages = true;
		}
		else
		if(findarg(s, "--no-inline-images") != -1) {
			p.inlineimages = false;
		}
		else
		if(findarg(s, "--sdl-screen-size") != 1) {
			// NOOP
		}
		else
		if(s.IsEqual("--"))	{
			String s;
			for(int j = ++i; j < n; j++)
				s << cmd[j] << ' ';
			if(!s.IsEmpty())
				p.command = TrimBoth(s);
			break;
		}
		else {
			PrintHelp();
			return;
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
