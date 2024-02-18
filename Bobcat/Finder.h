// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Finder_h_
#define _Bobcat_Finder_h_

class Finder : public FrameTB<WithFinderLayout<ParentCtrl>>
{
public:
    typedef Finder CLASSNAME;
    
    Finder(Terminal& t);
    virtual ~Finder();

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
    
    void        SetSearchMode(const String& mode);

    void        CheckCase();
    void        IgnoreCase();
    void        CheckPattern();
    
    bool        IsCaseSensitive() const;
    bool        IsCaseInsensitive() const;
    bool        IsRegex() const;
    
    void        Sync();

    void        Search();
    void        Update();
    void        Harvest();
    
    bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);
    void        OnHighlight(VectorMap<int, VTLine>& hl);

private:
    bool        CaseSensitiveSearch(const VectorMap<int, WString>& m, const WString& s);
    bool        CaseInsensitiveSearch(const VectorMap<int, WString>& m, const WString& s);
    bool        RegexSearch(const VectorMap<int, WString>& m, const WString& s);
    
    struct TextAnchor : Moveable<TextAnchor> {
        Point   pos = {0, 0};
        int     length = 0;
    };
    
    Vector<TextAnchor> foundtext;
    
    enum class Search {
        CaseSensitive,
        CaseInsensitive,
        Regex
    }   searchtype;

    struct SearchField : EditString {
        typedef SearchField CLASSNAME;
        SearchField();
        void SearchBar(Bar& menu);
        bool Key(dword key, int count) override;
    };
    
    int           index = 0;
    Terminal&     term;
    Value         data;
    SearchField   text;
    FrameLeft<ToolButton> menu;
    FrameRight<DisplayCtrl> mode;
    FrameRight<DisplayCtrl> counter;
};

#endif
