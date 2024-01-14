// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME "Finder"
#define KEYNAMESPACE FinderKeys
#define KEYFILE <Bobcat/Finder.key>
#include <CtrlLib/key_source.h>

using namespace FinderKeys;

Finder::Finder(Terminal& t)
: ctx(t)
, index(0)
{
	CtrlLayout(*this);
	Add(close.RightPosZ(4, 12).VCenterPosZ(12, 0));
	close.Image(Images::Delete()).Tip(t_("Close finder"));
	next.Image(Images::Next());
	prev.Image(Images::Prev());
	begin.Image(Images::Begin());
	end.Image(Images::End());
	next  << THISFN(Next);
    prev  << THISFN(Prev);
    begin << THISFN(Begin);
    end   << THISFN(End);
    close << THISFN(Hide);
    showall << THISFN(Sync);
	text.NullText(t_("Type to search..."));
	Sync();
}

Finder::~Finder()
{
	ctx.WhenSearch = Null;
	ctx.WhenHighlight = Null;
}

void Finder::SetData(const Value& v)
{
	data = v;
	ctx.RefreshLayout();
}

Value Finder::GetData() const
{
	return data;
}

void Finder::FrameLayout(Rect& r)
{
	data == "top"
		? LayoutFrameTop(r, this, cy ? cy : r.Width())
		: LayoutFrameBottom(r, this, cy ? cy : r.Width()); // default
}

void Finder::Show()
{
	bool b = ctx.HasSizeHint();
	ctx.HideSizeHint();
	ctx.AddFrame(Height(StdFont().GetCy() + Zy(16)));
	ctx.WhenSearch << THISFN(OnSearch);
	ctx.WhenHighlight << THISFN(OnHighlight);
	text.WhenAction << THISFN(Search);
	text.SetFocus();
	ctx.ShowSizeHint(b);
}

void Finder::Hide()
{
	bool b = ctx.HasSizeHint();
	ctx.HideSizeHint();
	ctx.RemoveFrame(*this);
	ctx.WhenSearch = Null;
	ctx.WhenHighlight = Null;
	text.WhenAction = Null;
	ctx.SetFocus();
	ctx.RefreshLayout();
	ctx.ShowSizeHint(b);
}

void Finder::Next()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = clamp(++index, 0, n - 1);
		Sync();
	}
}

void Finder::Prev()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = clamp(--index, 0, n - 1);
		Sync();
	}
}

void Finder::Begin()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = 0;
		Sync();
	}
}

void Finder::End()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = n - 1;
		Sync();
	}
}

void Finder::StdBar(Bar& menu)
{
	menu.AddKey(AK_FIND_ALL,    [this] { bool b = showall; showall = !showall; Sync(); });
	menu.AddKey(AK_FIND_NEXT,   [this] { Next();  });
	menu.AddKey(AK_FIND_PREV,   [this] { Prev();  });
	menu.AddKey(AK_FIND_FIRST,  [this] { Begin(); });
	menu.AddKey(AK_FIND_LAST,   [this] { End();   });
	menu.AddKey(AK_HIDE_FINDER, [this] { Hide();  });
}

bool Finder::Key(dword key, int count)
{
	return MenuBar::Scan([this](Bar& menu) { StdBar(menu); }, key);
}

void Finder::Sync()
{
	int cnt = pos.GetCount();
	if(index >= 0 && index < cnt)
		ctx.Goto(pos[index].y);;
	if(text.GetLength() > 0)
		status = Format(t_("Found %d/%d"), cnt ? index +  1 : 0 , cnt);
	else
		status = "";
	prev.Enable(cnt > 0 && index > 0);
	next.Enable(cnt > 0 && index < cnt - 1);
	begin.Enable(cnt > 0 && index > 0);
	end.Enable(cnt > 0 && index < cnt - 1);
	ctx.Refresh();
}

void Finder::Search()
{
	index = 0;
	pos.Clear();
	ctx.Find((WString)~text);
	Sync();
}

bool Finder::OnSearch(const VectorMap<int, WString>& m, const WString& s)
{
	int slen = s.GetLength();
	int offset = m.GetKey(0);
	
	for(int row = 0; row < m.GetCount(); row++) { // Note: m.GetCount() > 1 == text is wrapped.
		for(int col = 0; col < m[row].GetLength(); col++) {
			if(m[row][col] == s[0]) {
				int trow = row, tcol = col, tlen = slen;
				// Check if the substring is present starting from the current position
				while(tlen > 0 && trow < m.GetCount()) {
					if(m[trow][tcol] == s[slen - tlen])
						tlen--;
					else
						break;
					if(tcol + 1 < m[trow].GetLength())
						tcol++;
					else {
						trow++;
						tcol = 0;
					}
				}
				// If tlen is 0, then the substring is found.
				if(!tlen) {
					pos.Add({ col, row + offset });
				}
			}
		}
	}
	
	return true;

}

void Finder::OnHighlight(VectorMap<int, VTLine>& hl)
{
	if(!pos.GetCount() || index < 0)
		return;

	WString s = ~text;
	int len = s.GetLength();
	Point p = pos[index];

	for(Point pt : pos)
		for(int row = 0, col = 0, ln = hl.GetKey(row); ln == pt.y && row < hl.GetCount(); row++) {
			for(auto& q : hl[row]) {
				if(pt.x <= col && col < pt.x + len) {
					if(pt == p) {
						q.Ink(SColorHighlightText);
						q.Paper(SColorHighlight);
					}
					else
					if(~showall) {
						q.Ink(LtRed());
						q.Paper(Yellow());
					}
				}
				col++;
			}
		}
}

}
