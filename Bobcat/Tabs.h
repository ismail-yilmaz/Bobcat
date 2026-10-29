// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#ifndef _Bobcat_Tabs_h_
#define _Bobcat_Tabs_h_

struct TabBar : ParentCtrl {
	typedef TabBar CLASSNAME;

    TabBar(Bobcat& ctx);

    enum TimerId
    {
        TIMEID_BLINK = Ctrl::TIMEID_COUNT,
        TIMEID_COUNT
    };

    int          GetTab(Point p) const;
    Rect         GetCloseRect(int i) const;
    void         SyncSb();
    void         SyncHot();
    void         Paint(Draw& w) override;
    void         Animate();
    void         Layout() override;
    void         Sync();
    void         LeftDown(Point p, dword keyflags) override;
    void         LeftUp(Point p, dword keyflags) override;
    void         RightDown(Point p, dword keyflags) override;
    void         MiddleDown(Point p, dword keyflags) override;
    void         MouseMove(Point p, dword keyflags) override;
    void         MouseWheel(Point p, int zdelta, dword keyflags) override;
    void         DragAndDrop(Point p, PasteClip& d) override;
    void         DragLeave() override;
    void         CancelMode() override;
    void         MouseLeave() override;
    
    void         OnClose(int i);
    void         OnBar(int i, Bar& bar);

    SplitterFrame sframe;
    Bobcat&       ctx;
    VScrollBar    sb;
    Point         pos;
    int           cursor;
    int           tabcy;
    int           tabswidth;
    int           closetab;
    bool          blinking:1;
    bool          pressed:1;
    bool          dnd:1;

    Event<int>    WhenClose;
    Event<int, Bar&> WhenBar;

};

const Display& StdTabDisplay();

#endif
