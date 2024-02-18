// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Linkifier_h_
#define _Bobcat_Linkifier_h_

struct LinkInfo : Moveable<LinkInfo> {
    Point pos  = { 0, 0 };
    int length = 0;
    String url;
};

class Linkifier {
public:
    typedef Linkifier CLASSNAME;
    
    Linkifier(Terminal& t);

    Linkifier&  Enable(bool b = true);
    Linkifier&  Disable();
    bool        IsEnabled() const;

    int         GetCursor() const;
    void        ClearCursor();
    bool        IsCursor() const;

    int         GetPos();
    void        UpdatePos();
    void        ClearPos();

    int         GetCount() const;
    LinkInfo&   operator[](int i);

    const LinkInfo& GetCurrentLinkInfo() const;
    
    void        Clear();
    bool        Sync();

    void        Search();
    void        Update();
    bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);
    void        OnHighlight(VectorMap<int, VTLine>& hl);
    
    const LinkInfo* begin() const;
    LinkInfo*       begin();
    const LinkInfo* end() const;
    LinkInfo*       end();
    
private:
    Terminal&   term;
    int         cursor;
    int         pos;
    bool        enabled:1;
    Vector<LinkInfo> links;
};

class LinkifierSetup : public WithLinkifierProfileLayout<ParentCtrl> {
public:
    typedef LinkifierSetup CLASSNAME;
    
    LinkifierSetup();

    void        Sync();
    void        ContextMenu(Bar& bar);

    void        Load();
    void        Store() const;

    void        SetData(const Value& data) override;
    Value       GetData() const override;

private:
    String      name;
    ToolBar     toolbar;
    EditStringNotNull edit;
};


#endif
