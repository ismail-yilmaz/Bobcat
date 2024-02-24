// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Profile_h_
#define _Bobcat_Profile_h_

// Terminal profiles.

struct Profile : Moveable<Profile> {
    Profile();
    Profile(const String& s) : Profile() { name = s; }
    String      name;
    String      user;
    String      command;
    String      address;
    String      env;
    bool        noenv;
    Font        font;
    bool        bell;
    bool        blinktext;
    int         blinkinterval;
    String      palette;
    bool        lightcolors;
    bool        adjustcolors;
    bool        intensify;
    bool        dynamiccolors;
    String      cursorstyle;
    bool        lockcursor;
    bool        blinkcursor;
    bool        inlineimages;
    bool        hyperlinks;
    bool        windowactions;
    bool        windowreports;
    bool        clipboardread;
    bool        clipboardwrite;
    String      functionkeystyle;
    bool        altescapeskeys;
    bool        altshiftskeys;
    bool        keynavigation;
    int         mousewheelstep;
    bool        dontscrolltoend;
    bool        alternatescroll;
    bool        autohidemouse;
    bool        history;
    int         historysize;
    bool        delayedrefresh;
    bool        lazyresize;
    String      encoding;
    String      erasechar;
    int         linespacing;
    String      overridetracking;
    String      onexit;
    String      wordselchars;
    bool        filterctrl;
    bool        sizehint;
    int         order;
    Finder::Config finder;
    hash_t      GetHashValue() const;
    void        Serialize(Stream& s);
    void        Jsonize(JsonIO& jio);
};

class Profiles : public WithProfilesLayout<ParentCtrl> {
public:
    Profiles(Bobcat& ctx);

    void        Add();
    void        Clone();
    void        Rename();
    void        Remove();
    void        Sync();
    void        SetDefault();
    void        ContextMenu(Bar& bar);

    int         Load();
    void        Store();
    
private:
    struct Setup : ParentCtrl {
        Setup();
        void            SetData(const Value& data) override;
        Value           GetData() const override;
        void            MapData(CtrlMapper& m, Profile& p) const;
        void            Sync();
        String          name;
        TabCtrl         tabs;
        FileSelButton   filesel;
        SelectDirButton dirsel;
        mutable         WithGeneralProfileLayout<ParentCtrl>   general;
        mutable         WithVisualProfileLayout<ParentCtrl>    visuals;
        mutable         WithEmulationProfileLayout<ParentCtrl> emulation;
        mutable         FinderSetup                            finder;
        mutable         LinkifierSetup                         linkifier;
        mutable         Palettes                               palettes;
    };
    
    ToolBar            toolbar;
    Bobcat&            ctx;

public:
    Setup       setup;
};

dword          GetModifierKey(String s);
String         GetModifierKeyDesc(dword keyflags);

Profile        LoadProfile(const String& name);
int            LoadProfiles(VectorMap<String, Profile>& v);

String         ProfilesDir();
String         ProfileFile(const String& name);
Vector<String> GetProfileFilePaths();
Vector<String> GetProfileNames();

String         ShortcutKeysFile();
bool           LoadShortcutKeys();
void           SaveShortcutKeys();

Font SelectFont(Font f, dword type = Font::FIXEDPITCH);
const Display& FontProfileDisplay();

// Operators

inline bool operator==(const Profile& p, const Profile& q) { return p.name == q.name; }
inline bool operator==(const Profile& p, const String& s)  { return p.name == s; }
inline bool operator==(const String& s, const Profile& p)  { return p == s; }


#endif
