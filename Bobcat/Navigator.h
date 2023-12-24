// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

#ifndef _Bobcat_Navigator_h_
#define _Bobcat_Navigator_h_

struct Navigator : Ctrl {
    Navigator(Bobcat& ctx);
    ~Navigator();

    Navigator&  Show(bool ok = true);
    Navigator&  Hide();
    void        Sync();
    
    void        Layout() override;
    void        Paint(Draw& w) override;

    void        LeftDown(Point pt, dword keyflags) override;
    void        RightDown(Point pt, dword keyflags) override;
    void        MouseMove(Point pt, dword keyflags) override;
    void        MouseWheel(Point pt, int zdelta, dword keyflags) override;
    bool        Key(dword key, int count) override;

    void        ContextMenu(Bar& menu);
    
    struct Snapshot : Moveable<Snapshot> {
        String name;
        Rect   nrect;
        Image  img;
        Rect   irect;
    };

    enum TimerId
    {
        TIMEID_SYNC = Ctrl::TIMEID_COUNT,
        TIMEID_COUNT
    };
    
    Bobcat&     ctx;
    Point       mouse;
    int         columns;
    int         cursor;
    VScrollBar  sb;
    Size        lastsize;
    Vector<Snapshot> shots;
};

#endif
