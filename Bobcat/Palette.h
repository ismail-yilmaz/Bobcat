// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Palette_h_
#define _Bobcat_Palette_h_

struct Palette : Moveable<Palette> {
    enum {
        MAX_COLOR_COUNT = TerminalCtrl::MAX_COLOR_COUNT + 4
    };
    Palette();
    Palette(const String& s) : Palette() { name = s; }
    void        Jsonize(JsonIO& jio);
    String      name;
    Color       table[MAX_COLOR_COUNT];
};

class Palettes : public ParentCtrl {
public:
    Palettes();
    void        Add();
    void        Edit();
    void        Remove();
    void        Reset();
    void        Sync();
    void        MakeActive();
    void        ContextMenu(Bar& bar);

    int         Load();
    void        Store();

    void        SetPalette();

    void        SetData(const Value& data) override;
    Value       GetData() const override;

private:
    Value       data;
    ArrayCtrl   list;
    ToolBar     toolbar;
};

// Global functions

Palette        LoadPalette(const String& name);
int            LoadPalettes(VectorMap<String, Palette>& v);

String         PaletteDir();
String         PaletteFile(const String& name);
Vector<String> GetPaletteFilePaths();
Vector<String> GetPaletteNames();

// Operators

inline bool operator==(const Palette& p, const Palette& q) { return p.name == q.name; }
inline bool operator==(const Palette& p, const String& s)  { return p.name == s; }
inline bool operator==(const String& s, const Palette& p)  { return p == s; }

#endif
