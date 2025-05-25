// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#ifndef _Bobcat_Finder_h_
#define _Bobcat_Finder_h_

class Finder : public FrameTB<WithFinderLayout<ParentCtrl>>
{
public:
    typedef Finder CLASSNAME;

    Finder(Terminal& t);
    virtual ~Finder();

    void        SetConfig(const Profile& p);
    
    void        SetData(const Value& v) override;
    Value       GetData() const override;
    void        FrameLayout(Rect& r) override;

    void        StdBar(Bar& menu);
    void        StdKeys(Bar& menu);

    bool        Key(dword key, int count) override;

    void        Show();
    void        Hide();

    void        Next();
    void        Prev();
    void        Begin();
    void        End();
    void        Goto(int i);

    int         GetCount() const;
    bool        HasFound() const;

    void        SetSearchMode(const String& mode);

    void        CheckCase();
    void        IgnoreCase();
    void        CheckPattern();

    bool        IsCaseSensitive() const;
    bool        IsCaseInsensitive() const;
    bool        IsRegex() const;
  
    void        Sync();

    void        Search();
    void        SearchText(const WString& text);
    void        CancelSearch();
    bool        IsSearchCanceled() const;
    void        Update();

    void        SaveToFile();
    void        SaveToClipboard();

    bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);
    void        OnHighlight(HighlightInfo& hl);

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

    enum class Search {
        CaseSensitive,
        CaseInsensitive,
        Regex
    }   searchtype;

    struct Harvester {
        Harvester(Finder& f);
        Harvester& Format(const String& fmt);
        Harvester& Mode(const String& mode);
        Harvester& Delimiter(const String& delim);
        void       SaveToClipboard();
        void       SaveToFile();
        bool       Reap(Stream& s);
        bool       IsReady() const;
        enum class Fmt  { Txt, Csv };
        Finder&    finder;
        Fmt        format;
        String     delimiter;
    } harvester;

    struct SearchField : WithDropChoice<EditString> {
        typedef SearchField CLASSNAME;
        SearchField();
        void SearchBar(Bar& menu);
        bool Key(dword key, int count) final;
    };

    int           index;
    int           limit;
    Terminal&     term;
    Value         data;
    SearchField   text;
    bool          cancel:1;
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

#endif
