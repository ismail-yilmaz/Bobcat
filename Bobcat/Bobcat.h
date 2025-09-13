// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_Bobcat_h
#define _Bobcat_Bobcat_h

#ifdef flagWEBGUI
#include <Turtle/Turtle.h>
#elif  flagSDLGUI
#include <VirtualGui/SDL2GL/SDL2GL.h>
#endif

#include <plugin/tif/tif.h>
#include <plugin/pcre/Pcre.h> // WebGui Windows compilation fix: This has to be above the other include files.

#include <MessageCtrl/MessageCtrl.h>
#include <Terminal/Terminal.h>
#include <PtyProcess/PtyProcess.h>

#ifdef PLATFORM_POSIX
#include <pwd.h>
#endif

namespace Upp {

#define TOPICFILE <Bobcat/docs.tpp/all.i>
#include <Core/topic_group.h>

#define KEYGROUPNAME "General"
#define KEYNAMESPACE AppKeys
#define KEYFILE <Bobcat/Application.key>
#include <CtrlLib/key_header.h>

#define KEYGROUPNAME TERMINALCTRL_KEYGROUPNAME
#define KEYNAMESPACE TerminalCtrlKeys
#define KEYFILE <Bobcat/Terminal.key>
#include <CtrlLib/key_header.h>

#define IMAGEFILE <Bobcat/Bobcat.iml>
#define IMAGECLASS Images
#include <Draw/iml_header.h>

// Forward declaration
struct Bobcat;
struct Terminal;
struct Profile;
struct ItemInfo;
struct HighlightInfo;

// Global context
Ptr<Bobcat> GetContext();
void SetContext(Bobcat& ctx);

#include "Palette.h"

#define LAYOUTFILE <Bobcat/Bobcat.lay>
#include <CtrlCore/lay.h>

#include "Stacker.h"
#include "WebSearch.h"
#include "QuickText.h"
#include "Linkifier.h"
#include "Finder.h"
#include "Navigator.h"
#include "Profile.h"
#include "Terminal.h"

struct Bobcat : Pte<Bobcat> {
    Bobcat();

    bool        AddTerminal(const String& key = Null, bool pane = false);
    bool        AddTerminal(const Profile& profile, bool pane = false);
    bool        NewTerminalFromActiveProfile(bool pane = false);
    void        RemoveTerminal(Terminal& t);
    void        ActivateTerminal();
    Terminal*   GetActiveTerminal();
    String      GetActiveProfile();

    Vector<Terminal*> GetTerminalGroup(hash_t id);
    Vector<Terminal*> GetTerminalGroup(const Profile& p);
    
    void        Run(const Profile& profile, Size size, bool fullscreen = false);
    
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
    Bobcat&     SetPageSize(Size sz);
    Bobcat&     SetRect(Rect r);

    Bobcat&     ShowMenuBar(bool b = true);
    Bobcat&     HideMenuBar();
    bool        HasMenuBar() const;
    
    Bobcat&     ToggleBars();
    Bobcat&     ToggleNavigator();

    void        Sync();
    void        SyncTitle();
    void        SyncBackground();
    void        SyncTerminalProfiles();
    
    void        MainMenu(Bar& menu);
    void        FileMenu(Bar& menu);
    void        EditMenu(Bar& menu);
    void        ViewMenu(Bar& menu);
    void        EmulationMenu(Bar& menu);
    void        SetupMenu(Bar& menu);
    void        HelpMenu(Bar& menu);
    void        TermMenu(Bar& menu);
    void        TermSubmenu(Bar& menu, const Vector<String>& list);
    void        ListMenu(Bar& menu);
    void        SizeMenu(Bar& menu);
    
    void        ScreenShot();
    void        About();
    void        Help();

    void        ProcessEvents();
    
    struct  Config {
        Config();
        String      guitheme;
        Font        guifont;
        String      defaultprofile;
        String      titlealignment;
        String      notificationalignment;
        bool        notificationanimation;
        String      finderalignment;
        String      stackdirection;
        int         stackanimation;
        bool        stackwheel;
        bool        savescreenshot;
        bool        custominputmethod;
        String      custompagesizes;
        bool        showmenu;
        bool        showtitle;
        bool        frameless;
        bool        serializeplacement;
        bool        backgroundimage;
        String      backgroundimagepath;
        String      backgroundimagemode;
        int         backgroundimageblur;
        int         ptywaitinterval;
        String      splitterorientation;
        void        Jsonize(JsonIO& jio);
    };
    
    struct ViewCtrl : ParentCtrl {
        ViewCtrl();
        void        SetData(const Value& v) final;
        Value       GetData() const final;
        void        Paint(Draw& w) final;
        Value       data;
        String      mode;
    };

    TopWindow  window;
    MenuBar    menubar;
    Navigator  navigator;
    ViewCtrl   view;
    Stacker    stack;
    Config     settings;
    Array<Terminal> terminals;
};

// Generic terminal buffer item information. (For highlighting and other markers)

struct ItemInfo : Moveable<ItemInfo> {
    Point pos    = { 0, 0 };
    int   length = 0;
    Value data;
    hash_t GetHashValue() const { return pos.y; }
};

inline bool operator==(const ItemInfo& a, const ItemInfo& b) { return a.pos.y == b.pos.y; };
inline bool operator<(const ItemInfo& a, const ItemInfo& b)  { return a.pos.y < b.pos.y; };

// Generic highlighting structure.

struct HighlightInfo : Moveable<HighlightInfo> {
    VectorMap<int, VTLine>* line = nullptr;
    Vector<VTCell*> highlighted;
    const ItemInfo* iteminfo;
    int             posindex = 0;
    int             offset   = 0;
    bool            adjusted = false;
};

// Command line arguments/options parsing stuff

enum class CmdArgType {
    General,
    Environment,
    Emulation,
    Appearance
};

struct CmdArg {
    CmdArgType  type;
    const char *sopt;
    const char *lopt;
    const char *arg;
    const char *desc;
};

struct CmdArgList {
    String command;
    VectorMap<String, String> options;
    bool HasOption(const char *id) const;
    const String& Get(const char *id, const String& defval = Null);
};

class CmdArgParser {
public:
    CmdArgParser(const Array<CmdArg>& args);
    bool Parse(const Vector<String>& cmdline, CmdArgList& list, String& error);

private:
    const CmdArg *Find(const String &arg) const;
    const Array<CmdArg>& args;
};

const Array<CmdArg>&  GetCmdArgs();
Vector<const CmdArg*> FindCmdArgs(CmdArgType t);
const char*           GetCmdArgTypeName(CmdArgType t);

// Global functions.

FileSel& BobcatFs();

void LoadConfig(Bobcat& ctx);
void SaveConfig(Bobcat& ctx);

dword  GetModifierKey(String s);
String GetModifierKeyDesc(dword keyflags);
dword  GetAKModifierKey(const KeyInfo& k, int index = 0);

String GetUpTime(Time t);
String GetDefaultShell();
String GetVersion();
String GetBuildInfo();

Size ParsePageSize(const String& s);

const Display& StdBackgroundDisplay();
const Display& NormalImageDisplay();
const Display& TiledImageDisplay();

void LoadGuiTheme(Bobcat& ctx);
void LoadGuiFont(Bobcat& ctx);
Vector<Tuple<void (*)(), String, String>> GetAllGuiThemes();

bool IsWaylandEnabled();
void EnableWayland(bool b);

MessageCtrl& GetNotificationDaemon();

Ptr<MessageBox> AskYesNo(Ctrl& ctrl, const String& text, const String& yes, const String& no,
                            MessageBox::Type type, const Event<int>& action);
Ptr<MessageBox> Warning(Ctrl& ctrl, const String& text, int timeout = 0);
Ptr<MessageBox> Error(Ctrl& ctrl, const String& text, int timeout = 0);
}
#endif
