// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_WebSearch_h_
#define _Bobcat_WebSearch_h_

class WebSearch {
public:
    WebSearch(Terminal& t);

    void        SetConfig(const Profile& p);

    void        ContextMenu(Bar& menu);
    
    struct Provider : Moveable<Provider> {
        String   name;
        String   uri;
        operator AttrText() const;
        void     Jsonize(JsonIO& jio);
        String   ToString() const;
    };
    
    struct Config {
        Config();
        void Jsonize(JsonIO& jio);
        WithDeepCopy<Vector<Provider>> providers;
    };
    
private:
    Terminal& term;
};

class WebSearchSetup : public WithWebSearchListLayout<ParentCtrl> {
public:
    typedef WebSearchSetup CLASSNAME;
    
    WebSearchSetup();

    void        Add();
    void        Edit();
    
    void        Sync();
    void        ContextMenu(Bar& bar);

    void        Drag();
    void        DnDInsert(int line, PasteClip& d);

    void        Load(const Profile& p);
    void        Store(Profile& p) const;

private:
    ToolBar     toolbar;
    WithWebSearchLayout<TopWindow> dlg;
};

VectorMap<String, Vector<WebSearch::Provider>>& GetWebSearchProviders();

#endif
