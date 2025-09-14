// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_Terminal_h_
#define _Bobcat_Terminal_h_

struct Terminal : TerminalCtrl {
    typedef Terminal CLASSNAME;

    Terminal(Bobcat& ctx);
    ~Terminal();

    void        PostParse() override;
    void        SetData(const Value& data) override;
    Value       GetData() const override;

    void        MouseEnter(Point pt, dword keyflags) override;
    void        MouseLeave() override;
    void        MouseWheel(Point pt, int zdelta, dword keyflags) override;
    void        MouseMove(Point pt, dword keyflags) override;
    void        LeftDouble(Point pt, dword keyflags) override;
    Image       CursorImage(Point pt, dword keyflags) override;
    
    void        GotFocus() override;
    
    bool        StartPty(const Profile& profile, bool pane = false);
    bool        Start(const String& profile_name, bool pane = false);
    bool        Start(const Profile& profile, bool pane = false);
    bool        Start(Terminal *term, bool pane = false);
    void        Stop();
    bool        Do();
    void        Restart();
    void        Reset();
    
    bool        IsRunning();
    bool        IsFailure();
    bool        IsSuccess();
    bool        IsAsking();
    
    bool        IsRoot();
    
    void        DontExit();
    void        KeepAsking();
    void        ScheduleExit();
    void        ScheduleRestart();
    
    bool        ShouldAsk();
    bool        ShouldExit();
    bool        ShouldRestart();

    void        AskRestartExit();
    
    hash_t      GetHashValue() const;
    
    void        Update();
    Terminal&   Sync();
    void        SyncHighlight();
    
    void        Layout() override;
    
    Terminal&   SetProfile(const Profile& p);
    Terminal&   SetPalette(const Palette& p);
    Terminal&   SetExitMode(const String& s);
    Terminal&   SetLocale(const String& s);
    Terminal&   SetEraseKey(const String& s);
    Terminal&   SetCursorStyle(const String& s);
    Terminal&   SetFontZoom(int n);
    Terminal&   SetLineSpacing(int n);
    Terminal&   SetWordSelectionFilter(const String& s);
    Terminal&   SetWordSelectionPattern(const String& s);
    void        SetWorkingDirectory(const String& s);
    Terminal&   SetPathTranslationMode(const String& s);
    Terminal&   SetPathDelimiter(const String& s);
        
    String      GetWorkingDirectory() const;
    
    void        MakeTitle(const String& txt);
    String      GetTitle() const;

    void        ShowTitleBar(bool b = true);
    void        HideTitleBar();
    bool        HasTitleBar() const;
    
    void        EnableResize(bool b = true);
    void        DisableResize();
    bool        CanResize() const;
    
    void        ShowFinder(bool b);
    void        HideFinder();
    bool        HasFinder() const;
    
    bool        IsEditable();
    
    void        CopyImage();
    void        OpenImage();

    String      GetLink();
    void        CopyLink(const String& s);
    void        CopyLink();
    void        OpenLink(const String& s);
    void        OpenLink();
    void        OnLink(const String& s);
    
    void        Hyperlinks(bool b);
    bool        HasHyperlinks() const;
    bool        HasLinkifier() const;
    bool        IsMouseOverExplicitHyperlink();
    bool        IsMouseOverImplicitHyperlink();
    bool        IsMouseOverLink();
    
    void        CopyAnnotation(const String& s);
    bool        OnAnnotation(Point pt, String& s);
    
    void        OnNotification(const String& text);
    
    void        OnProgress(int type, int data);
    bool        InProgress() const;

    bool        OnSelectorScan(dword key);
    
    void        DragAndDrop(Point pt, PasteClip& d) override;
    
    Terminal&   PutText(const WString& txt);

    void        FindText(const WString& txt);
    void        OpenFinder();
    
    const VTPage& GetPage() const;
    int         GetPosAsIndex(Point pt, bool relative = false) const;
    int         GetMousePosAsIndex() const;
    
    void        OnHighlight(VectorMap<int, VTLine>& hl);
    void        DoHighlight(const SortedIndex<ItemInfo>& items, HighlightInfo& hl, const Event<HighlightInfo&>& cb);
    
    WString     GetSelectedText() const;
    
    bool        GetWordSelection(const Point& pt, Point& pl, Point& ph) const override;
    bool        GetWordSelectionByPattern(const Point& pt, Point& pl, Point& ph) const;
    
    void        EmulationMenu(Bar& menu);
    void        FileMenu(Bar& menu);
    void        EditMenu(Bar& menu);
    void        ViewMenu(Bar& menu);
    void        ContextMenu(Bar& menu);


    Splitter*   GetParentSplitter() const;
    bool        IsSplitterPane() const;
    
    Time        GetUpTime() const;
    
    int         GetExitCode()                   { return pty->GetExitCode();     }
    String      GetExitMessage()                { return pty->GetExitMessage();  }

    enum class ExitMode {
        Keep,
        KeepAsking,
        Restart,
        RestartFailed,
        Ask,
        Exit
    };

    enum class PathMode {
        Native,
        Unix,
        Windows
    };
    
    Bobcat&          ctx;
    One<APtyProcess> pty;
    One<FrameTop<ProgressIndicator>> pi;
    bool         bell:1;
    bool         filter:1;
    bool         canresize:1;
    bool         smartwordsel:1;
    bool         shellintegration:1;
    bool         findselectedtext:1;
    bool         warnonrootaccess:1;
    ExitMode     exitmode;
    String       profilename;
    String       workingdir;
    PathMode     pathmode;
    Time         starttime;
    String       pathdelimiter;
    Value        data;
    Finder       finder;
    Linkifier    linkifier;
    QuickText    quicktext;
    WebSearch    websearch;
    Color        highlight[4];
    TimeCallback timer;

    struct TitleBar : FrameTB<Ctrl> {
        TitleBar(Terminal& ctx);
        void        SetData(const Value& v) override;
        Value       GetData() const override;
        void        FrameLayout(Rect& r) override;

        void        Show();
        void        Hide();
        void        Menu();

        Terminal&   term;
        ToolButton  close;
        ToolButton  menu;
        ToolButton  newterm;
        ToolButton  navlist;
        StaticText  title;
        Value       data;
    }  titlebar;
    
    struct ProgressBar : FrameTB<ProgressIndicator> {
        ProgressBar(Terminal& t);
        void        SetData(const Value& v) override;
        Value       GetData() const override;
        void        FrameLayout(Rect& r) override;

        void        Show(int percent);
        void        Hide();

        Terminal&    term;
        Value        data;
        TimeCallback timer;
    } progressbar;
};

// Global functions
Terminal&                  AsTerminal(Ctrl& c);
VectorMap<String, String>& GetWordSelectionPatterns();
void                       InsertUnicodeCodePoint(Terminal& term);
bool                       AnnotationEditor(String& s, const char *title);
int                        AdjustLineOffset(Terminal& term, const Vector<int>& in, Vector<int>& out);

// Global highlight callback
void SetHighlightCallback(Terminal& t, VectorMap<int, VTLine>& line, const Vector<ItemInfo>& items,
                                            const Event<VTCell&, const ItemInfo&, int>& cb);

// Terminal specific notifications
Ptr<MessageBox> AskRestartExitOK(Ptr<Terminal> t, const Profile& p);
Ptr<MessageBox> AskRestartExitOK(Ptr<Terminal> t);
Ptr<MessageBox> AskRestartExitError(Ptr<Terminal> t, const Profile& p);
Ptr<MessageBox> AskRestartExitError(Ptr<Terminal> t);

// Linux specific functions
#ifdef PLATFORM_LINUX
pid_t               GetProcessGroupId(APtyProcess& pty);
Tuple<uid_t, uid_t> GetUserIdsFromProcess(pid_t pid);
String              GetUsernameFromUserId(uid_t uid);
String              GetRunningProcessName(APtyProcess& pty);

class LinuxPtyProcess : public PosixPtyProcess {
public:
    bool            IsRoot();
    bool            CheckPrivileges(Terminal& t);
private:
    uid_t euid   = 0;
    uid_t ruid   = 0;
    bool  isroot = false;
    Ptr<MessageBox> notification;
};
#endif

// Operators

inline bool operator==(const Terminal& p, const Terminal& q)  { return p.GetHashValue() == q.GetHashValue(); }
inline bool operator==(const Terminal& t, const Profile& p)   { return t.GetHashValue() == p.GetHashValue(); }
inline bool operator==(const Profile& p, const Terminal& t)   { return t == p; }
inline bool operator==(const Terminal& t,hash_t id)           { return t.GetHashValue() == id; }
inline bool operator==(hash_t id, const Terminal& t)          { return t == id; }

#endif
