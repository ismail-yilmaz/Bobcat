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

	struct Item : Moveable<Item> {
		String   type;
		String   alias;
		String   text;
		bool     IsCommand() const;
		operator AttrText() const;
		void     Jsonize(JsonIO& jio);
		String   ToString() const;
	};
	
    struct Config {
        Config();
        void Jsonize(JsonIO& jio);
        WithDeepCopy<Vector<Item>> texts;
    };
    
private:
    Terminal& term;
    PopUpList plist;
};

class QuickTextSetup : public WithQuickTextListLayout<ParentCtrl> {
public:
    typedef QuickTextSetup CLASSNAME;
    
    QuickTextSetup();

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
	WithQuickTextLayout<TopWindow> dlg;
};

#endif
