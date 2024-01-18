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
    bool        Key(dword key, int count) override;
    
    void        Show();
    void        Hide();

    void        Next();
    void        Prev();
    void        Begin();
    void        End();
    void        Goto(int i);
    
    void        CheckCase();
    void        IgnoreCase();
    void        CheckPattern();
    
    void        Sync();

    void        Search();
    void        Update();
    
    bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);
    void        OnHighlight(VectorMap<int, VTLine>& hl);

private:
    bool        CaseSensitiveSearch(const VectorMap<int, WString>& m, const WString& s);
    bool        CaseInsensitiveSearch(const VectorMap<int, WString>& m, const WString& s);
    bool        RegexSearch(const VectorMap<int, WString>& m, const WString& s);
    
    struct Pos : Moveable<Pos> {
        int row = 0;
        int col = 0;
        int length = 0;
    };
    
    Vector<Pos> pos;
    
    enum class Search {
        CaseSensitive,
        CaseInsensitive,
        Regex
    }   searchtype;

    int           index = 0;
    Terminal&     ctx;
    Value         data;
    ToolButton    close;
    FrameLeft<ToolButton>    menu;
    TimeCallback  timer;
};


#endif
