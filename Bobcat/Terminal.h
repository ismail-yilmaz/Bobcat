// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Terminal_h_
#define _Bobcat_Terminal_h_

struct Terminal : TerminalCtrl {
    Terminal(Bobcat& ctx);

	void		PostParse() override;
    void        SetData(const Value& data) override;
    Value       GetData() const override;

    bool        Start(const String& profile_name);
    bool        Start(const Profile& profile);
    void        Stop();
    int         Do();
    void        Reset();
    bool        CanExit() const;

    void        Search();
    
    hash_t      GetHashValue() const;
    
    Terminal&   Sync();
    
    Terminal&   SetProfile(const Profile& p);
    Terminal&   SetPalette(const Palette& p);
    Terminal&   SetLocale(const String& s);
    Terminal&   SetEraseKey(const String& s);
    Terminal&   SetCursorStyle(const String& s);
    Terminal&   SetFontZoom(int n);
    Terminal&   SetLineSpacing(int n);
    
    void        MakeTitle(const String& txt);
    String      GetTitle() const;

    void        ShowTitleBar(bool b);
    void        HideTitleBar();
    bool        HasTitleBar() const;
    
    void        ShowFinder(bool b);
    void        HideFinder();
    bool        HasFinder() const;
    
    void        CopyImage();
    void        OpenImage();

    void        CopyLink();
    void        OpenLink();
    
    void        EmulationMenu(Bar& menu);
    void        FileMenu(Bar& menu);
    void        EditMenu(Bar& menu);
    void        ViewMenu(Bar& menu);
    void        ContextMenu(Bar& menu);

    int         GetExitCode()                   { return pty.GetExitCode();     }
    String      GetExitMessage()                { return pty.GetExitMessage();  }
    
    Bobcat&     ctx;
    PtyProcess  pty;
    bool        bell;
    bool        keep;
    bool        filter;
    String      profilename;
    Value       data;
    Finder      finder;

    struct TitleBar : FrameTB<Ctrl> {
        TitleBar(Terminal& ctx);
        void        SetData(const Value& v) override;
        Value       GetData() const override;
        void        FrameLayout(Rect& r) override;
 
        void        Show();
        void        Hide();

        Terminal&   ctx;
        ToolButton  close;
        ToolButton  menu;
        ToolButton  newterm;
        StaticText  title;
        Value       data;
    }  titlebar;
};


// Global functions
Terminal& AsTerminal(Ctrl& c);
void      InsertUnicodeCodePoint(Terminal& term);

// Operators

inline bool operator==(const Terminal& p, const Terminal& q)  { return p.GetHashValue() == q.GetHashValue(); }
inline bool operator==(const Terminal& t, const Profile& p)   { return t.GetHashValue() == p.GetHashValue(); }
inline bool operator==(const Profile& p, const Terminal& t)   { return t == p; }
inline bool operator==(const Terminal& t,hash_t id)           { return t.GetHashValue() == id; }
inline bool operator==(hash_t id, const Terminal& t)          { return t == id; }


#endif
