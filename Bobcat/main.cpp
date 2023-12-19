// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

using namespace Upp;

void PrintHelp()
{
#ifdef PLATFORM_POSIX
	const String sHelpText = t_(
		"Usage:\n"
		"\tbobcat [OPTIONS] -- [COMMAND...]"
		"\n\n"
		"Options:\n"
		"\t-h, --help                     Show help\n"
		"\t-l, --list                     List available profiles\n"
		"\t-p, --profile PROFILE          Run with the given PROFILE (Names are case-sensitive)\n"
		"\t-s, --settings                 Open settings window\n"
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

void OpenSettings(Bobcat& app)
{
	LoadConfig(app);
	app.Settings();
	SaveConfig(app);
}

void RunWithProfile(Bobcat& app, const Vector<String>& cmdline, int i)
{
	if(i < cmdline.GetCount()) {
		if(FindIndex(GetProfileNames(), cmdline[i]) < 0)
			Cout() << Format(t_("Couldn't find profile '%s'. Falling back to active profile.\n"), cmdline[i]);
		LoadConfig(app);
		app.RunWithProfile(cmdline[i]);
		SaveConfig(app);
	}
}

void ExecuteCommand(Bobcat& app, const Vector<String>& cmdline, int i)
{
	if(i < cmdline.GetCount()) {
		const auto r = SubRange(cmdline, i, cmdline.GetCount() - 1);
		String cmd = Join((const Vector<String>&) r, " ", true);
		LoadConfig(app);
		app.RunCommand(cmd);
		SaveConfig(app);
	}
}

GUI_APP_MAIN
{
	StdLogSetup(LOG_FILE);
	
	Bobcat app;
	const Vector<String>& cmd = CommandLine();

	// TODO: Rudimentary. Fine tune this part.
	
	for(int i = 0; i < cmd.GetCount(); i++)
	{
		if(findarg(cmd[i], "-l", "--list") != -1)
		{
			PrintProfileList();
			return;
		}
		else
		if(findarg(cmd[i], "-s", "--settings") != -1)
		{
			OpenSettings(app);
			return;
		}
		else
		if(findarg(cmd[i], "-p", "--profile") != -1)
		{
			RunWithProfile(app, cmd, ++i);
			return;
		}
		else
		if(findarg(cmd[i], "--") != -1)
		{
			ExecuteCommand(app, cmd, ++i);
			return;
		}
		else
		{
			PrintHelp();
			return;
		}
			
	}
	
	LoadConfig(app);
	app.Run();
	SaveConfig(app);
}
