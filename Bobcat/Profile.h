// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

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
    bool        shellintegration;
    String      ptybackend;
    bool        warnonrootaccess;
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
    bool        annotations;
    bool        progress;
    bool        windowactions;
    bool        windowreports;
    bool        clipboardread;
    bool        clipboardwrite;
    bool        findselectedtext;
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
    bool        ambiguoustowide;
    String      erasechar;
    int         linespacing;
    String      overridetracking;
    String      onexit;
    String      wordselmode;
    String      wordselchars;
    String      wordselpattern;
    String      pathtranslation;
    String      pathdelimiter;
    String      answerbackmsg;
    bool        addtopath;
    bool        filterctrl;
    bool        sizehint;
    int         order;
    Finder::Config finder;
    Linkifier::Config linkifier;
    QuickText::Config quicktext;
    WebSearch::Config websearch;
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
    
    void        Drag();
    void        DnDInsert(int line, PasteClip& d);

    int         Load();
    void        Store();
    
private:
    struct Setup : ParentCtrl {
        struct EmulationProfileSetup : WithEmulationProfileLayout<ParentCtrl> {
            EmulationProfileSetup();
            WithSelectionLayout<ParentCtrl> selection;
            WithPasteLayout<ParentCtrl>     paste;
        };
        
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
        mutable         EmulationProfileSetup                  emulation;
        mutable         FinderSetup                            finder;
        mutable         LinkifierSetup                         linkifier;
        mutable         QuickTextSetup                         quicktext;
        mutable         WebSearchSetup                         websearch;
        mutable         Palettes                               palettes;
    };
    
    ToolBar            toolbar;
    Bobcat&            ctx;

public:
    Setup       setup;
};

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
