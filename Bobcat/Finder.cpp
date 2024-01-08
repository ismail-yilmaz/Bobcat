// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

Finder::Finder(TerminalCtrl& t)
: ctx(t)
, index(0)
{
	CtrlLayout(dlg);
	dlg.begin << dlg.Breaker(100);
	dlg.end   << dlg.Breaker(101);
	dlg.next  << dlg.Breaker(102);
	dlg.prev  << dlg.Breaker(103);
	dlg.next.SetImage(Images::Next());
	dlg.prev.SetImage(Images::Prev());
	dlg.begin.SetImage(Images::Begin());
	dlg.end.SetImage(Images::End());
	ctx.WhenSearch << THISFN(OnSearch);
	ctx.WhenHighlight << THISFN(OnHighlight);
	dlg.text.WhenAction  << THISFN(DoSearch);
	dlg.text.NullText(t_("Type to search..."));
	dlg.Title(t_("Finder"));
	Sync();
}

Finder::~Finder()
{
	ctx.WhenSearch = Null;
	ctx.WhenHighlight = Null;
}

void Finder::Sync()
{
	int cnt = pos.GetCount();
	if(index >= 0 && index < cnt)
		ctx.Goto(pos[index].y);;
	if(dlg.text.GetLength() > 0)
		dlg.status = Format(t_("%d instances found. (%d/%d)"), cnt, cnt ? index +  1 : 0 , cnt);
	else
		dlg.status = "";
	dlg.prev.Enable(cnt > 0 && index > 0);
	dlg.next.Enable(cnt > 0 && index < cnt - 1);
	dlg.begin.Enable(cnt > 0 && index > 0);
	dlg.end.Enable(cnt > 0 && index < cnt - 1);
	ctx.Refresh();
}

void Finder::DoSearch()
{
	index = 0;
	pos.Clear();
	ctx.Find((WString)~dlg.text);
	Sync();

}

void Finder::Search()
{
	dlg.Open();
	while(dlg.IsOpen()) {
		dlg.ProcessEvents();
		int rc = dlg.Run();
		if(rc < 100 || rc > 103)
			return;
		int cnt = pos.GetCount();
		if(cnt) {
			switch(rc) {
			case 100: index = 0;  break;
			case 101: index = cnt - 1;   break;
			case 102: index = clamp(++index, 0, cnt - 1); break;
			case 103: index = clamp(--index, 0, cnt - 1); break;
			default: return;
			}
		}
		Sync();
	}
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
	WString s = ~dlg.text;
	int len = s.GetLength();
	for(int i = 0; i < hl.GetCount(); i++) { // Note: hl.GetCount() > 1 == line is wrapped.
		Point p = pos[index];
		for(int row = i, ln = hl.GetKey(i), col = 0; ln == p.y && row < hl.GetCount(); row++) {
			for(auto& q : hl[row]) {
				if(p.x <= col && col < p.x + len) {
					q.Ink(LtRed());
					q.Paper(Yellow());
				}
				col++;
			}
		}
	}
}

}
