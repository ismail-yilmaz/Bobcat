// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

#define KEYGROUPNAME "Finder"
#define KEYNAMESPACE FinderKeys
#define KEYFILE <Bobcat/Finder.key>
#include <CtrlLib/key_source.h>

using namespace FinderKeys;

Finder::Finder(Terminal& t)
: ctx(t)
, index(0)
, searchtype(Search::CaseSensitive)
{
	CtrlLayout(*this);
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
	Add(text.LeftPosZ(5, 180).TopPosZ(3, 18));
	text.NullText(t_("Type to search..."));
	text.AddFrame(mode);
	text.AddFrame(menu);
	text.WhenBar << THISFN(StdKeys);
	menu.Image(Images::Find());
	menu << [=] { MenuBar::Execute(THISFN(StdBar)); };
	mode.SetDisplay(StdCenterDisplay());
	mode <<= AttrText("C").Bold().Ink(SColorDisabled);
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
	if(!IsChild()) {
		timer.Kill();
		bool b = ctx.HasSizeHint();
		ctx.HideSizeHint();
		ctx.AddFrame(Height(StdFont().GetCy() + 16));
		ctx.WhenSearch << THISFN(OnSearch);
		ctx.WhenHighlight << THISFN(OnHighlight);
		text.WhenAction << THISFN(Search);
		ctx.ShowSizeHint(b);
	}
	text.SetFocus();
}

void Finder::Hide()
{
	if(IsChild()) {
		timer.Kill();
		bool b = ctx.HasSizeHint();
		ctx.HideSizeHint();
		ctx.RemoveFrame(*this);
		ctx.WhenSearch = Null;
		ctx.WhenHighlight = Null;
		text.WhenAction = Null;
		ctx.RefreshLayout();
		ctx.ShowSizeHint(b);
	}
	ctx.SetFocus();
}

void Finder::Goto(int i)
{
	if(i >= 0) {
		ctx.Goto(pos[i].row);;
		Sync();
	}
}

void Finder::Next()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = clamp(++index, 0, n - 1);
		Goto(index);
	}
}

void Finder::Prev()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = clamp(--index, 0, n - 1);
		Goto(index);
	}
}

void Finder::Begin()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = 0;
		Goto(index);
	}
}

void Finder::End()
{
	if(int n = pos.GetCount(); n >= 0) {
		index = n - 1;
		Goto(index);
	}
}

void Finder::CheckCase()
{
	searchtype = Search::CaseSensitive;
	mode <<= AttrText("C").Bold().Ink(SColorDisabled);
	Update();
}

void Finder::IgnoreCase()
{
	searchtype = Search::CaseInsensitive;
	mode <<= AttrText("I").Bold().Ink(SColorDisabled);
	Update();
}

void Finder::CheckPattern()
{
	searchtype = Search::Regex;
	mode <<= AttrText("R").Bold().Ink(SColorDisabled);
	Update();
}

void Finder::StdBar(Bar& menu)
{
	menu.Add(AK_CHECKCASE,      THISFN(CheckCase)).Radio(searchtype == Search::CaseSensitive);
	menu.Add(AK_IGNORECASE,     THISFN(IgnoreCase)).Radio(searchtype == Search::CaseInsensitive);
	menu.Add(AK_REGEX,          THISFN(CheckPattern)).Radio(searchtype == Search::Regex);
	StdKeys(menu);
}

void Finder::StdKeys(Bar& menu)
{
	menu.AddKey(AK_FIND_ALL,    [this] { bool b = showall; showall = !showall; Sync(); });
	menu.AddKey(AK_FIND_NEXT,   THISFN(Next));
	menu.AddKey(AK_FIND_PREV,   THISFN(Prev));
	menu.AddKey(AK_FIND_FIRST,  THISFN(Begin));
	menu.AddKey(AK_FIND_LAST,   THISFN(End));
	menu.AddKey(AK_HIDE_FINDER, THISFN(Hide));
}

bool Finder::Key(dword key, int count)
{
	MenuBar::Scan([this](Bar& menu) { StdBar(menu); }, key);
	return true;
}

void Finder::Sync()
{
	int cnt = pos.GetCount();
	index = clamp(index, 0, max(0, cnt - 1));
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
	pos.Clear();
	ctx.Find((WString)~text);
	Sync();
}

void Finder::Update()
{
	timer.KillSet(20, THISFN(Search));
}

bool Finder::CaseSensitiveSearch(const VectorMap<int, WString>& m, const WString& s)
{
	int slen = s.GetLength();
	int offset = m.GetKey(0);
	
	LTIMING("CaseSensitiveSearch");

	for(int row = 0, i = 0; row < m.GetCount(); row++) { // Note: m.GetCount() > 1 == text is wrapped.
		for(int col = 0; col < m[row].GetLength(); col++, i++) {
			if(m[row][col] == s[0]) {
				int trow = row, tcol = col, tlen = slen;
				// Check if the substring is present starting from the current position.
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
					Pos& sp = pos.Add();
					sp.row = offset;
					sp.col = i;
					sp.length = slen;
				}
			}
		}
	}
	return true;
}

bool Finder::CaseInsensitiveSearch(const VectorMap<int, WString>& m, const WString& s)
{
	LTIMING("CaseInsensitiveSearch");

	int offset = m.GetKey(0);
	
	WString u = ToLower(s), q;
	for(const WString& ss : m)
		q << ss;

	q = ToLower(q);

	int i = 0, len = u.GetLength();
	while((i = q.Find(u, i)) >= 0) {
		Pos& sp = pos.Add();
		sp.row = offset;
		sp.col = i;
		sp.length = len;
		i += len;
	}

	return true;
}

bool Finder::RegexSearch(const VectorMap<int, WString>& m, const WString& s)
{
	LTIMING("RegexSearch");

	int offset = m.GetKey(0);
	
	WString q;
	for(const WString& ss : m)
		q << ss;

	RegExp r(s.ToString());
	String ln = ToUtf8(q);
	while(r.GlobalMatch(ln)) {
		int o = r.GetOffset();
		Pos& p = pos.Add();
		p.row = offset;
		p.col = Utf32Len(~ln, o);
		p.length = Utf32Len(~ln + o, r.GetLength());
	}

	return true;
}


bool Finder::OnSearch(const VectorMap<int, WString>& m, const WString& s)
{
	LTIMING("OnSearch");

	switch(searchtype) {
	case Search::CaseInsensitive:
		CaseInsensitiveSearch(m, s);
		break;
	case Search::CaseSensitive:
		CaseSensitiveSearch(m, s);
		break;
	case Search::Regex:
		RegexSearch(m, s);
		break;
	default:
		break;
	}
	return true;
}

void Finder::OnHighlight(VectorMap<int, VTLine>& hl)
{
	if(!pos.GetCount() || index < 0)
		return;

	Pos p = pos[index];

	LTIMING("OnHighlight");

	for(const Pos& pt : pos)
		for(int row = 0, col = 0; row < hl.GetCount(); row++) {
			if(hl.GetKey(row) != pt.row)
				continue;
			for(VTLine& l : hl) {
				for(VTCell& c : l) {
					if(pt.col <= col && col < pt.col + pt.length) {
						if(pt.row == p.row && pt.col == p.col) {
							c.Normal();
							c.Ink(ctx.highlight[1]);
							c.Paper(ctx.highlight[3]);
						}
						else
						if(~showall) {
							c.Normal();
							c.Ink(ctx.highlight[0]);
							c.Paper(ctx.highlight[2]);
						}
					}
					col++;
				}
			}
		}
}

Finder::SearchField::SearchField()
{
	WhenBar = THISFN(SearchBar);
}

void Finder::SearchField::SearchBar(Bar& menu)
{
	menu.Add(IsEditable(), AK_FIND_UNDO, Images::Undo(), THISFN(Undo));
	menu.Separator();
	menu.Add(IsEditable() && IsSelection(), AK_FIND_CUT, Images::Cut(), THISFN(Cut));
	menu.Add(IsSelection(), AK_FIND_COPY, Images::Copy(),THISFN(Copy));
	menu.Add(IsEditable() && IsClipboardAvailableText(), AK_FIND_PASTE, Images::Paste(), THISFN(Paste));
	menu.Separator();
	menu.Add(text.GetLength(), AK_FIND_SELECTALL, Images::SelectAll(), THISFN(SelectAll));
}

bool Finder::SearchField::Key(dword key, int count)
{
	if(MenuBar::Scan(WhenBar, key))
		return true;
	return EditField::Key(key, count);
}

}
