topic "Command Line Options";
[a83;*R6 $$1,0#31310162474203024125188417583966:caption]
[ $$0,0#00000000000000000000000000000000:Default]
[{_} 
[s0; [* Usage:]&]
[s0; [C@3         bobcat `[OPTIONS`] `-`- `[COMMAND...`]]&]
[s0;* &]
[s0; [* General Options]&]
[s0; [C1    ][C@3;1  `-h, `-`-help][C1                    ][1 Show help.]&]
[s0; [C1     ][C@3;1 `-v, `-`-version ][C1                ][1 Show build 
information.]&]
[s0; [C1    ][C@3;1  `-l, `-`-list   ][C1                 ][1 List available 
terminal profiles.]&]
[s0; [C1    ][C@3;1  `-p, `-`-profile PROFILE  ][C1       ][1 Run with the 
given terminal PROFILE. (Names are case`-sensitive)]&]
[s0; [C1     ][C@3;1 `-s, `-`-settings       ][C1         ][1 Open settings 
window.]&]
[s0; [C1       ][C@3;1   `-`-list`-palettes][C1           ][1 List available 
color palettes]&]
[s0; [C1        ][C@3;1  `-`-list`-gui`-themes][C1         ][1 List available 
GUI themes.]&]
[s0; [C1         ][C@3;1 `-`-gui`-theme THEME ][C1        ][1 Set the GUI 
theme to THEME.]&]
[s0; [C1     ][C@3;1 `-b, `-`-show`-bars    ][C1           ][1 Show the menu 
and title bar.]&]
[s0; [C1     ][C@3;1 `-B, `-`-hide`-bars   ][C1            ][1 Hide the menu 
and title bar.]&]
[s0; [C1      ][C@3;1    `-`-show`-menubar][C1            ][1 Show the menu 
bar.]&]
[s0; [C1         ][C@3;1 `-`-hide`-menubar  ][C1          ][1 Hide the menu 
bar.]&]
[s0; [C1    ][C@3;1      `-`-show`-titlebar   ][C1        ][1 Show the title 
bar.]&]
[s0; [C1        ][C@3;1  `-`-hide`-titlebar   ][C1        ][1 Hide the title 
bar.]&]
[s0; [C1         ][C@3;1 `-`-show`-borders ][C1           ][1 Show window 
borders.]&]
[s0; [C1         ][C@3;1 `-`-hide`-borders][C1            ][1 Hide window 
borders. (Frameless window mode)]&]
[s0; [C@3;1     `-f, `-`-fullscreen ][C1             ][1 Full screen mode.]&]
[s0; [C1    ][C@3;1  `-m, `-`-maximize  ][C1              ][1 Maximize the 
window.]&]
[s0; [C1    ][C@3;1  `-g, `-`-geometry GEOMETRY ][C1      ][1 Set the initial 
window geometry. (E.g. 80x24, 132x24)]&]
[s0;* &]
[s0; [* Profile specific options:]&]
[s0;* &]
[s0; [*2 Environment:]&]
[s0; [C1   ][C@3;1  `-z, `-`-pty`-backend ][C1            ][1 Set the pseudoconsole 
backend to be used. (Windows specific)]&]
[s0; [C1   ][C@3;1  `-r, `-`-restart ][C1                ][1 Restart the 
command on exit.]&]
[s0; [C@3;1    `-R, `-`-restart`-failed    ][C1      ][1 Restart the command 
when it fails.]&]
[s0; [C@3;1    `-k, `-`-keep          ][C1          ][1 Don`'t close the 
terminal on exit.]&]
[s0; [C@3;1    `-K, `-`-dont`-keep    ][C1           ][1 Close the terminal 
on exit.]&]
[s0; [C@3;1    `-y, `-`-ask     ][C1                ][1 Ask what to do 
on exit.]&]
[s0; [C1   ][C@3;1  `-n, `-`-environment ][C1            I][1 nherit the 
environment.]&]
[s0; [C1   ][C@3;1  `-N, `-`-no`-environment ][C1         ][1 Don`'t inherit 
the environment.]&]
[s0; [C1   ][C@3;1  `-d, `-`-working`-dir PATH][C1        ][1 Set the working 
directory to PATH.]&]
[s0;* &]
[s0; [*2 Emulation:]&]
[s0; [C1    ][C@3;1 `-q, `-`-vt`-style`-fkeys  ][C1        ][1 Use VT`-style 
function keys.]&]
[s0; [C1   ][C@3;1  `-Q, `-`-pc`-style`-fkeys   ][C1       ][1 Use PC`-style 
function keys.]&]
[s0; [C1   ][C@3;1  `-w, `-`-window`-reports ][C1         ][1 Enable window 
reports.]&]
[s0; [C1    ][C@3;1 `-W, `-`-no`-window`-reports ][C1      ][1 Disable window 
reports.]&]
[s0; [C1   ][C@3;1  `-a, `-`-window`-actions  ][C1        ][1 Enable window 
actions.]&]
[s0; [C1    ][C@3;1 `-A, `-`-no`-window`-actions ][C1      ][1 Disable window 
actions.]&]
[s0; [C1    ][C@3;1 `-c, `-`-clipboard`-access  ][C1      ][1 Enable application 
clipboard access.]&]
[s0; [C1    ][C@3;1 `-C, `-`-no`-clipboard`-access ][C1    Disable][1  application 
clipboard access.]&]
[s0; [C1      ][C@3;1   `-`-hyperlinks   ][C1           ][1 Enable hyperlink 
detection. (OSC 52 `+ Linkifier)]&]
[s0; [C1    ][C@3;1     `-`-no`-hyperlinks  ][C1         ][1 Disable hyperlink 
detection.]&]
[s0; [C1      ][C@3;1   `-`-inline`-images    ][C1       ][1 Enable inline 
images support. (Sixel, iTerm2, Jexer)]&]
[s0; [C1      ][C@3;1   `-`-no`-inline`-images  ][C1      ][1 Disable inline 
images support.]&]
[s0; [C1      ][C@3;1   `-`-annotations...   ][C1       Enable][1  ][^https`:`/`/www`.ultimatepp`.org`/srcdoc`$RichText`$QTF`_en`-us`.html^1 r
ich`-text][1  annotations.]&]
[s0; [C1      ][C@3;1   `-`-no`-annotations   ][C1       ][1 Disable ][^https`:`/`/www`.ultimatepp`.org`/srcdoc`$RichText`$QTF`_en`-us`.html^1 r
ich`-text][1  annotations.]&]
[s0;1 &]
[s0; [*2 Appearance:]&]
[s0; [C1      ][C@3;1   `-`-palette PALETTE ][C1        ][1 Set color palette 
to PALETTE.]&]
[s0; [C1      ][C@3;1   `-`-bell         ][C1           ][1 Enable notification 
bell.]&]
[s0; [C1      ][C@3;1   `-`-no`-bell      ][C1           Disable][1  notification 
bell.]&]
[s0;1 ]]