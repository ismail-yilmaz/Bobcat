// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Finder_h_
#define _Bobcat_Finder_h_

// TODO: Make this an abstract base class and derive different types of "search engines" from it
// (E.g. SimpleFinder, RegexFinder. etc.)

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
    
    void        Sync();

    void        Search();
    
    bool        OnSearch(const VectorMap<int, WString>& m, const WString& s);
    void        OnHighlight(VectorMap<int, VTLine>& hl);

private:
    int           index = 0;
    Terminal&     ctx;
    Vector<Point> pos;
    Value         data;
    ToolButton    close;
};


#endif
