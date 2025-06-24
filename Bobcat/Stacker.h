// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_Stacker_h_
#define _Bobcat_Stacker_h_

// Note: Stacker is a modified version of StackCtrl

class Stacker : public ParentCtrl {
public:
    Stacker();
    virtual ~Stacker() {}

    Stacker&    Wheel(bool b = true);
    Stacker&    Animation(int ms = 150);
    int         GetDuration() const;
    
    Stacker&    NoBackground(bool b = true);

    Stacker&    Horz();
    Stacker&    Vert();
    
    Stacker&    HorzSplitter();
    Stacker&    VertSplitert();
    Stacker&    ToggleSplitterOrientation();

    Stacker&    Add(Ctrl& ctrl);
    Stacker&    Insert(int i, Ctrl& ctrl);
    Stacker&    Split(Ctrl& ctrl);

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

    void        Swap(int a, int b);
    void        Swap(Ctrl& a, Ctrl& b);
    void        SwapNext();
    void        SwapPrev();
    
    void        SwapPanes();
    void        ExpandTopLeftPane();
    void        ExpandBottomRightPane();
    void        ResetSplitterPos();

    void        Goto(int i);
    void        Goto(Ctrl& ctrl);
    void        Prev();
    void        Next();
    void        GoBegin();
    void        GoEnd();

private:
    Splitter*   GetParentSplitter(Ctrl *c) const;
    void        RemoveSplitter(const Splitter& s);
    bool        RemoveFromSplitter(int i);
    void        Activate(Ctrl* ctrl);

    // Animation related stuff.
    bool IsNext(Ctrl *next) const;
    void Animate(Ctrl *current, Ctrl *next, bool forward);

    Vector<Ctrl*> list;
    Array<Splitter> splitters;
    Ctrl*        activectrl;    // cursor
    int          duration;
    bool         wheel:1;
    bool         vertical:1;
    bool         splitvert:1;
    bool         animating:1;
};

#endif
