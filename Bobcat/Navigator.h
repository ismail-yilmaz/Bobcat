// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Navigator_h_
#define _Bobcat_Navigator_h_

struct Navigator : ParentCtrl {
    Navigator(StackCtrl& sctrl);
    ~Navigator();

    Navigator&  Show(bool ok = true);
    Navigator&  Hide();
    void        Geometry();
    void        Sync();
    void        Layout() override;

    void        Paint(Draw& w) override;

    void        RightDown(Point pt, dword keyflags) override;
    void        MouseWheel(Point pt, int zdelta, dword keyflags) override;
    bool        Key(dword key, int count) override;

    Event<Bar&>   WhenBar;
    Event<>       WhenClose;
    Event<Ctrl&>  WhenGotoItem;
    Event<Ctrl&>  WhenRemoveItem;

    struct Item : public Ctrl {
    public:
        Item();
        Event<Ctrl&> WhenItem;
        Event<Ctrl&> WhenClose;

        Rect         GetCloseButtonRect();

        void         Paint(Draw& w) override;
        void         LeftUp(Point pt, dword keyflags) override;
        void         MouseEnter(Point pt, dword keyflags) override;
        void         MouseLeave() override;
        void         MouseMove(Point pt, dword keyflags) override;

        Ptr<Ctrl>    ctrl;
        Image        img;

    private:
        Point        pos = {0, 0};
    };

    enum TimerId
    {
        TIMEID_SYNC = Ctrl::TIMEID_COUNT,
        TIMEID_COUNT
    };

    StackCtrl&  stack;
    int         columns;
    int         cursor;
    VScrollBar  sb;
    Array<Item> items;
    FrameTop<WithNavigatorLayout<ParentCtrl>> searchbar;
};

#endif
