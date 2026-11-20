// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#ifndef _Bobcat_Stacker_h_
#define _Bobcat_Stacker_h_

// Note: Stacker is a modified version of StackCtrl

class Stacker : public ParentCtrl {
public:
    Stacker(Bobcat& ctx);
    virtual ~Stacker() {}

    void ChildGotFocus() override;

    Stacker&    Wheel(bool b = true);
    Stacker&    Animation(int ms = 150);
    int         GetDuration() const;

    Stacker&    MaxPanes(int n);
    int         GetMaxPanes() const;

    Stacker&    NoBackground(bool b = true);

    Stacker&    Horz();
    Stacker&    Vert();

    Stacker&    HorzSplitter();
    Stacker&    VertSplitert();
    Stacker&    AutoSplit();
    Stacker&    SplitLeft();
    Stacker&    SplitRight();
    Stacker&    SplitTop();
    Stacker&    SplitBottom();
    Stacker&    SetViewMode(ViewMode mode);
    ViewMode    GetViewMode() const;
    Stacker&    ToggleSplitterOrientation();

    Stacker&    Add(Ctrl& ctrl);
    Stacker&    Insert(int i, Ctrl& ctrl);
    Stacker&    Split(Ctrl& ctrl);
    Stacker&    Split(Ctrl& ctrl, ViewMode mode);
    Stacker&    Split(Ctrl& target, Ctrl& ctrl, ViewMode mode = ViewMode::AutoSplit);
    Stacker&    SplitAuto(Ctrl& ctrl);
    Stacker&    SplitLeft(Ctrl& ctrl);
    Stacker&    SplitRight(Ctrl& ctrl);
    Stacker&    SplitTop(Ctrl& ctrl);
    Stacker&    SplitBottom(Ctrl& ctrl);

    void        Remove(Ctrl& ctrl);
    void        Remove(int i);

    Event<int, Ctrl&> WhenInsert;
    Event<int, Ctrl&> WhenRemove;
    Event<int, int>   WhenSwap;

    int         GetCount() const;
    int         GetCursor() const;
    Ctrl&       Get(int i) const;
    Ctrl&       operator[](int i) const;

    Ctrl*       GetActiveCtrl() const;

    int         Find(Ctrl& ctrl) const;

    // Workspace & Search API
    int         GetWorkspaceCount() const;
    int         FindWorkspace(Ctrl& ctrl) const;
    int         FindInWorkspace(int wsid, Ctrl& ctrl) const;

    Vector<Ctrl*> GetWorkspacePanes(int wsid) const;
    Vector<Ctrl*> GetWorkspacePanes(Ctrl& ctrl) const;

    // Workspace Identification
    Stacker&    SetWorkspaceName(int wsid, const String& name);
    Stacker&    SetWorkspaceName(Ctrl& ctrl, const String& name);

    String      GetWorkspaceName(int wsid) const;
    String      GetWorkspaceName(Ctrl& ctrl) const;

    int         FindWorkspaceByName(const String& name) const;

    // Workspace Arrangement
    void        MovePanePrev();
    void        MovePaneNext();

    void        Swap(int a, int b);
    void        Swap(Ctrl& a, Ctrl& b);
    void        SwapNext();
    void        SwapPrev();

    void        SwapPanes();
    void        ExpandTopLeftPane();
    void        ExpandBottomRightPane();
    void        ResetSplitterPos();

    Ctrl*       GetPassiveCtrl() const;

    void        Goto(int i);
    void        Goto(Ctrl& ctrl);
    void        Prev();
    void        Next();
    void        GoBegin();
    void        GoEnd();

private:
    Bobcat&        ctx;
    MultiSplitter* GetParentSplitter(Ctrl *c) const;
    void           RemoveSplitter(const MultiSplitter& s);
    bool           RemoveFromSplitter(int i);
    void           ActivatePane(Ctrl* ctrl);
    void           TrackFocus(Ctrl *ctrl);
    Ctrl*          GetPrevPane(MultiSplitter *scope, Ctrl *excl) const;

    // Animation related stuff.
    bool IsNext(Ctrl *next) const;
    void Animate(Ctrl *current, Ctrl *next, bool forward);

    Vector<Ctrl*>            list;
    Vector<Ctrl*>            focushistory;
    Array<MultiSplitter>     splitters;
    VectorMap<Ctrl*, String> workspacenames;
    Ctrl*                    activectrl;    // cursor
    int                      duration;
    int                      maxpanes;
    ViewMode                 viewmode;
    bool                     wheel:1;
    bool                     vertical:1;
    bool                     splitvert:1;
    bool                     animating:1;
    bool                     swapping:1;
};

#endif