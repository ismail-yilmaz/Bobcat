// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

#ifndef _Bobcat_Bobcat_h
#define _Bobcat_Bobcat_h

#include <StackCtrl/StackCtrl.h>
#include <Terminal/Terminal.h>
#include <PtyProcess/PtyProcess.h>

namespace Upp {

#define TOPICFILE <Bobcat/docs.tpp/all.i>
#include <Core/topic_group.h>

#define KEYGROUPNAME "General"
#define KEYNAMESPACE AppKeys
#define KEYFILE <Bobcat/Application.key>
#include <CtrlLib/key_header.h>

#define IMAGEFILE <Bobcat/Bobcat.iml>
#define IMAGECLASS Images
#include <Draw/iml_header.h>

#include "Palette.h"

#define LAYOUTFILE <Bobcat/Bobcat.lay>
#include <CtrlCore/lay.h>

// Forward declaration
struct Bobcat;

#include "Profile.h"
#include "Terminal.h"
#include "Navigator.h"

struct Bobcat {
    Bobcat();

    bool        AddTerminal(const Value& key = Null);
    void        RemoveTerminal(Terminal& t);
    void        ActivateTerminal();
    Terminal*   GetActiveTerminal();

    Vector<Terminal*> GetTerminalGroup(hash_t id);
    Vector<Terminal*> GetTerminalGroup(const Profile& p);
    
    void        Run();
    void        RunCommand(const String& cmd);
    void        RunWithProfile(const String& name);
    
    void        Close();
    void        Settings();

    Bobcat&     Maximize(bool b = true);
    bool        IsMaximized() const;
    Bobcat&     Minimize(bool b = true);
    bool        IsMinimized() const;

    Bobcat&     FullScreen(int mode);
    Bobcat&     ToggleFullScreen();
    Bobcat&     NoFullScreen();
    bool        IsFullScreen() const;

    Bobcat&     Resize(Size sz);
    Bobcat&     SetRect(Rect r);

    Bobcat&     ShowMenuBar(bool b = true);
    Bobcat&     HideMenuBar();
    bool        HasMenuBar() const;
    
    Bobcat&     ToggleNavigator();

    void        Sync();
    void        SyncTitle();
    void        SyncTerminalProfiles();
    
    void        MainMenu(Bar& menu);
    void        FileMenu(Bar& menu);
    void        EditMenu(Bar& menu);
    void        ViewMenu(Bar& menu);
    void        SetupMenu(Bar& menu);
    void        HelpMenu(Bar& menu);
    void        TermMenu(Bar& menu);
    void        ListMenu(Bar& menu);
    
    void        ScreenShot();
    void        About();
//  void        Help();

    struct  Config {
        Config();
        String      activeprofile;
        String      titlealignment;
        String      stackdirection;
        int         stackanimation;
        bool        stackwheel;
        bool        savescreenshot;
        bool        custominputmethod;
        bool        showmenu;
        bool        showtitle;
        bool        serializeplacement;
        void        Jsonize(JsonIO& jio);
    };
    
    TopWindow  window;
    MenuBar    menubar;
    Navigator  navigator;
    ParentCtrl view;
    StackCtrl  stack;
    Config     settings;
    Array<Terminal> terminals;
};

FileSel& BobcatFs();

void LoadConfig(Bobcat& ctx);
void SaveConfig(Bobcat& ctx);

String GetBuildInfo();
}
#endif
