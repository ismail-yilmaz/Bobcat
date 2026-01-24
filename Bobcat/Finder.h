// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#ifndef _Bobcat_Finder_h_
#define _Bobcat_Finder_h_

class Finder : public Moveable<Finder> {
public:
	typedef Finder CLASSNAME;
	
    enum class Mode { CaseSensitive, CaseInsensitive, Regex };
    
	Finder(Terminal& t);
	virtual ~Finder();
	
	Finder&     SetLimit(int n);
	
	Finder&     SetMode(Mode m);
	Finder&     CaseSensitive();
	Finder&     CaseInsensitive();
	Finder&     Regex();
	
	bool        IsCaseSensitive() const;
	bool        IsCaseInsensitive() const;
	bool        IsRegex() const;

	bool		Find(const WString& text, bool co = false);
	bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);

	const SortedIndex<ItemInfo>& GetItems() const;
	
	const ItemInfo& Get(int i);
	const ItemInfo& operator[](int i);
	
	int         GetCount() const;
	bool        HasFound() const;

	void        Cancel();
	bool        IsCanceled() const;

	void        SetConfig(const Profile& p);

    struct Config {
        Config();
        void    Jsonize(JsonIO& jio);
        String  searchmode;
        int     searchlimit;
        String  saveformat;
        String  savemode;
        String  delimiter;
        bool    showall;
        bool    parallelize;
        WithDeepCopy<Vector<String>> patterns;
    };
    
private:
    bool        BasicSearch(const VectorMap<int, WString>& m, const WString& s);
    bool        RegexSearch(const VectorMap<int, WString>& m, const WString& s);

    SortedIndex<ItemInfo> foundtext;
    
    Terminal&   term;
	Mode        mode;
	int         limit;
	bool        cancel:1;
};

class FinderBar : public Finder, public FrameTB<WithFinderBarLayout<ParentCtrl>>
{
public:
    typedef FinderBar CLASSNAME;

    FinderBar(Terminal& t);
    virtual ~FinderBar();

    void        SetConfig(const Profile& p);
    
    void        SetData(const Value& v) override;
    Value       GetData() const override;
    void        FrameLayout(Rect& r) override;

    void        StdBar(Bar& menu);
    void        StdKeys(Bar& menu);
    void        SearchBar(Bar& menu);

    bool        Key(dword key, int count) override;

    void        Show();
    void        Hide();

    void        Next();
    void        Prev();
    void        Begin();
    void        End();
    void        Goto(int i);

	void		Undo();
	void        Cut();
	void		Copy();
	void		Paste();
	void		SelectAll();
	
    void        SetSearchMode(const String& mode);

    void        CheckCase();
    void        IgnoreCase();
    void        CheckPattern();
    
    void        ShowAll(bool b = true);

    void        Co(bool b = true);
  
    void        Sync();

    void        Search();
    void        Search(const WString& txt);
    void        Updated() override;

    void        SaveToFile();
    void        SaveToClipboard();

    void        OnHighlight(HighlightInfo& hl);

private:

	void       Search0(const WString& txt);
	
    SortedIndex<ItemInfo> foundtext;

    struct Harvester {
        Harvester(FinderBar& f);
        Harvester& Format(const String& fmt);
        Harvester& Mode(const String& mode);
        Harvester& Delimiter(const String& delim);
        void       SaveToClipboard();
        void       SaveToFile();
        bool       Reap(Stream& s);
        bool       IsReady() const;
        enum class Fmt  { Txt, Csv };
        FinderBar& finder;
        Fmt        format;
        String     delimiter;
    } harvester;

    int           index;
    Terminal&     term;
    Value         data;
    bool          showall:1;
    bool          co:1;
    FrameLeft<ToolButton> menu;
    FrameRight<DisplayCtrl> display;
    FrameRight<ToolButton> fsave, csave;
};

class FinderSetup : public WithFinderProfileLayout<ParentCtrl> {
public:
    typedef FinderSetup CLASSNAME;
    
    FinderSetup();

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

// Displays
const Display& FinderSetupListDisplay();

#endif
