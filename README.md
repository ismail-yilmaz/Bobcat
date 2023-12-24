# Bobcat - Terminal Emulator

![info](resources/bobcat-about-linux.png)

 
Bobcat is a cross-platform terminal emulator built on the [U++](https://www.ultimatepp.org/index.html) rapid application development framework and [TerminalCtrl](https://github.com/ismail-yilmaz/Terminal), a powerful vt widget. It leverages these technologies to provide a versatile and user-friendly command-line interface.

*Disclaimer: Bobcat is a work in progress. Although it *should be* pretty stable by now, use it with caution.*

## Table of Contents

 1. [Features](#Features)
 2. [Requirements](#Requirements)
 3. [Installation](#Installation)
 4. [To Do](#ToDo)
 5. [Screenshots](#Screenshots)
 6. [Acknowledgements](#Acknowledgements)
 7. [License](#License)

## [Features](#Features)

*For a full range of supported VT features, see the [Features](https://github.com/ismail-yilmaz/Terminal#features) section of TerminalCtrl's docs.*

*For a full range of supported VT sequences, modes and control commands, see the [technical specifications document](https://github.com/ismail-yilmaz/Terminal/blob/master/Terminal/Specs.md) of TerminalCtrl.*

- **Cross-Platform:** Bobcat can basically run on any OS that U++ can be compiled on: Linux, Windows, MacOS, and BSD.
- **High Compatibility:** Recognizes and supports a wide range of terminal sequences (ESC, CSI, DCS, OSC, APC) sequences, ensuring high VT (DEC/ANSI) and xterm compatibility. 
- **Image Support:** Supports both legacy (sixel) and modern image formats (png, jpg, bmp, tiff) for inline image display, and recognizes sixel, iterm2 & jexer protocols.
- **OSC Extensions:** Implements newer OSC extensions, including hyperlinks, clipboard manipulation protocol, etc.
- **Versatile Copy/Paste:** Supports copy/paste/drag-and-drop operations on texts, links, and inline-images.
- **Multiple Profiles:** Supports multiple terminal and color profiles.
- **Configurable UI:** Offers a configurable user interface, allowing for a bare-bone terminal or fully-fledged GUI.
- **Stacked Interface:** Uses a stacked interface to reduce UI clutter, including a terminal manager called Navigator to navigate between open/stacked terminals.
- **Simple Configuration:** Uses a simple JSON format for configuration files.
- **Multiple Pty Support on Windows:** Can use ConPty or WinPty (compile-time feature, with plans to make it runtime) on Windows. WinPty backend is statically linked and *does not* require msys2 or cygwin environments.
- **Mouse Events:** Supports mouse events for almost all protocols.
- **Configurable Keys:** Supports configurable keyboard shortcuts for a personalized experience.
- **Emoji Support:** Can display color emojis (depending on the font).
- **Colorful Experience:** Supports 16, 256, and 24-Bit colors, xterm's dynamic colors feature, and even recognizes CMY and CMYK color spec formats.
- **Performance:** Reasonably fast and less memory-hungry.


## [Requirements](#Requirements)

Requirements are not heavy.  Below are the tools you need to compile Bobcat.

- CLANG/GCC with at least C++17.
- [U++](https://www.ultimatepp.org) framework.
- [TerminalCtrl](https://github.com/ismail-yilmaz/Terminal)
- [StackCtrl](https://github.com/ismail-yilmaz/StackCtrl)  

## [Installation](#Installation)

To be written...

## [To Do](#ToDo)

- Macro infrastructure, based on Upp::Esc scripting language.
- A "lite" plugin framework.

## [Screenshots](#Screenshots)

Below you can find a handful of screenshots of Bobcat, running on Linux & Windows.

Multiple terminal profile support (linux, btop)

![bobcat-profiles](resources/bobcat-profiles-linux.png)

Inline images support (linux, jexer)

![bobcat-sixel](resources/bobcat-jexer-sixel-linux.png)

Navigator (linux)

![bobcat-navigator](resources/bobcat-navigator-linux.png)

Settings windows (linux)

![bobcat-settings](resources/bobcat-settings-linux.png)

Far manager running on Bobcat (Windows 10)

![bobcat-far-manager](resources/bobcat-far-manager-windows.png)

## [Acknowledgements](#Acknowledgements)

To be written...

## [License](#License)

Copyright (c) 2023, İsmail Yılmaz

Bobcat is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Bobcat is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for details. You should have received a copy of the GNU General Public License along with Bobcat. If not, see [gnu licenses](http://www.gnu.org/licenses/)


