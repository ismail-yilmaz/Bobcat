// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_QuickText_h_
#define _Bobcat_QuickText_h_

class QuickText {
public:
    QuickText(Terminal& t);

    int  LoadText();
    
    void OnSelect();
    void OnCancel();

    void Popup();
	
    struct Config {
        Config();
        void Jsonize(JsonIO& jio);
        WithDeepCopy<Vector<String>> texts;
    };
    
private:
    Terminal& term;
};

class QuickTextSetup : public WithLinkifierProfileLayout<ParentCtrl> {
public:
    typedef QuickTextSetup CLASSNAME;
    
    QuickTextSetup();

    void        Sync();
    void        ContextMenu(Bar& bar);

    void        Drag();
    void        DnDInsert(int line, PasteClip& d);

    void        Load(const Profile& p);
    void        Store(Profile& p) const;

private:
    ToolBar     toolbar;
    EditStringNotNull edit;
};

const Display& QuickPasteDisplay();

#endif
