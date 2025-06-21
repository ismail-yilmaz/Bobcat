// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_Navigator_h_
#define _Bobcat_Navigator_h_

struct Navigator : ParentCtrl {
    Navigator(Bobcat& ctx);
    ~Navigator();
    
    Navigator&    Show(bool ok = true);
    Navigator&    Hide();
    void          Search();
    int           SyncItemLayout();
    void          Sync();
    void          Animate();
    void          Layout() override;

    void          Paint(Draw& w) override;

    void          RightDown(Point pt, dword keyflags) override;
    void          MouseWheel(Point pt, int zdelta, dword keyflags) override;
    bool          Key(dword key, int count) override;

    int           GetCursor();
    
    void          SwapItem(int i, int ii);
    void          SwapFirst();
    void          SwapLast();
    void          SwapPrev();
    void          SwapNext();
    
    void          AnimateSwap(int i, int ii);
    
    Event<Bar&>   WhenBar;
    Event<>       WhenClose;
    Event<Ctrl&>  WhenGotoItem;
    Event<Ctrl&>  WhenRemoveItem;

    struct Item : public Ctrl {
    public:
        Item();
        Event<Ctrl&> WhenItem;
        Event<Rect>  WhenFocus;
        Event<Ctrl&> WhenClose;

        Rect         GetCloseButtonRect();

        void         GotFocus() override;
        void         LostFocus() override;
        void         Paint(Draw& w) override;
        void         LeftUp(Point pt, dword keyflags) override;
        void         MouseEnter(Point pt, dword keyflags) override;
        void         MouseLeave() override;
        void         MouseMove(Point pt, dword keyflags) override;
        void         DragAndDrop(Point pt, PasteClip& d) override;
        void         CancelMode() override;
        void         DragLeave() override;
        
        Ptr<Terminal> ctrl;
        Image         img;
        bool          blinking:1;
        
    private:
        Point         pos = {0, 0};
        bool          dnd:1;
    };

    enum TimerId
    {
        TIMEID_SYNC = Ctrl::TIMEID_COUNT,
        TIMEID_BLINK,
        TIMEID_COUNT
    };

    bool          FilterItem(const Item& item);
    
    Bobcat&     ctx;
    VScrollBar  sb;
    bool        swapanim:1;
    int         cursor;
    Array<Item> items;
    FrameLeft<DisplayCtrl> icon;
    FrameTop<WithNavigatorLayout<ParentCtrl>> searchbar;
};

#endif
