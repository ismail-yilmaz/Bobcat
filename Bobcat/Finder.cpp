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

static void sWriteToDisplay(FrameLR<DisplayCtrl>& f, const String& txt)
{
	int cx = max(GetStdFont().GetMonoWidth() * 2, GetTextSize(txt, GetStdFont()).cx);
	f.Width(cx) <<= AttrText(txt).Bold().Ink(SColorDisabled);
}

Finder::Finder(Terminal& t)
: term(t)
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
	term.WhenSearch = THISFN(OnSearch);
	term.WhenHighlight = THISFN(OnHighlight);
	Add(text.HSizePosZ(4, 200).TopPosZ(3, 18));
	text.NullText(t_("Type to search..."));
	text.AddFrame(mode);
	text.AddFrame(counter);
	text.AddFrame(menu);
	text.WhenBar << THISFN(StdKeys);
	text.WhenAction << THISFN(Search);
	menu.Image(Images::Find());
	menu << [=] { MenuBar::Execute(THISFN(StdBar)); };
	counter.SetDisplay(StdCenterDisplay());
	mode.SetDisplay(StdCenterDisplay());
	sWriteToDisplay(counter, "0/0");
	sWriteToDisplay(mode, "C");
	Sync();
}

Finder::~Finder()
{
}

void Finder::SetData(const Value& v)
{
	data = v;
	term.RefreshLayout();
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
		bool b = term.HasSizeHint();
		term.HideSizeHint();
		term.AddFrame(Height(StdFont().GetCy() + 16));
		term.SyncHighlight();
		term.ShowSizeHint(b);
	}
	text.SetFocus();
}

void Finder::Hide()
{
	if(IsChild()) {
		bool b = term.HasSizeHint();
		term.HideSizeHint();
		term.RemoveFrame(*this);
		term.SyncHighlight();
		term.RefreshLayout();
		term.ShowSizeHint(b);
	}
	term.SetFocus();
}

void Finder::Goto(int i)
{
	if(i >= 0 && i < foundtext.GetCount()) {
		term.Goto(foundtext[i].pos.y);;
		Sync();
	}
}

void Finder::Next()
{
	if(int n = foundtext.GetCount(); n >= 0) {
		index = clamp(++index, 0, n - 1);
		Goto(index);
	}
}

void Finder::Prev()
{
	if(int n = foundtext.GetCount(); n >= 0) {
		index = clamp(--index, 0, n - 1);
		Goto(index);
	}
}

void Finder::Begin()
{
	if(int n = foundtext.GetCount(); n >= 0) {
		index = 0;
		Goto(index);
	}
}

void Finder::End()
{
	if(int n = foundtext.GetCount(); n >= 0) {
		index = n - 1;
		Goto(index);
	}
}

void Finder::SetSearchMode(const String& mode)
{
	searchtype = decode(mode,
		"insensitive", Search::CaseInsensitive,
		"regex", Search::Regex,
		Search::CaseSensitive);
	Sync();
}

void Finder::CheckCase()
{
	searchtype = Search::CaseSensitive;
	Update();
}

void Finder::IgnoreCase()
{
	searchtype = Search::CaseInsensitive;
	Update();
}

void Finder::CheckPattern()
{
	searchtype = Search::Regex;
	Update();
}

bool Finder::IsCaseSensitive() const
{
	return searchtype == Search::CaseSensitive;
}

bool Finder::IsCaseInsensitive() const
{
	return searchtype == Search::CaseInsensitive;
}

bool Finder::IsRegex() const
{
	return searchtype == Search::Regex;
}

void Finder::StdBar(Bar& menu)
{
	menu.Add(AK_CHECKCASE, THISFN(CheckCase)).Radio(IsCaseSensitive());
	menu.Add(AK_IGNORECASE,THISFN(IgnoreCase)).Radio(IsCaseInsensitive());
	menu.Add(AK_REGEX,     THISFN(CheckPattern)).Radio(IsRegex());
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
	int cnt = foundtext.GetCount();
	index = clamp(index, 0, max(0, cnt - 1));
	if(text.GetLength() > 0)
		sWriteToDisplay(counter, Format("%d/%d", cnt ? index + 1 : 0, cnt));
	else
		sWriteToDisplay(counter, "0/0");
	prev.Enable(cnt > 0 && index > 0);
	next.Enable(cnt > 0 && index < cnt - 1);
	begin.Enable(cnt > 0 && index > 0);
	end.Enable(cnt > 0 && index < cnt - 1);
	sWriteToDisplay(mode, decode(searchtype, Search::CaseInsensitive, "I", Search::Regex, "R", "C"));
	term.Refresh();
}

void Finder::Search()
{
	if(term.IsSearching())
		return;

	foundtext.Clear();
	term.Find(~text);
	Sync();
}

void Finder::Update()
{
	if(IsChild())
		Search();
}

bool Finder::CaseSensitiveSearch(const VectorMap<int, WString>& m, const WString& s)
{
	int slen = s.GetLength();
	int offset = m.GetKey(0);
	
	LTIMING("CaseSensitiveSearch");
	
	// Notes: 1) We are using this for CS search, because it is faster than WString::Find().
	//        2) m.GetCount() > 1 == text is wrapped.
	
	auto ScanText = [&m, &s, slen, offset](Vector<TextAnchor>& v) {
		for(int row = 0, i = 0; row < m.GetCount(); row++) {
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
						TextAnchor& a = v.Add();
						a.pos.y = offset;
						a.pos.x = i;
						a.length = slen;
					}
				}
			}
		}
		return !v.IsEmpty();
	};
	
	ScanText(foundtext);
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
	
	auto ScanText = [&q, &u, &offset](Vector<TextAnchor>& v) {
		int i = 0, len = u.GetLength();
		while((i = q.Find(u, i)) >= 0) {
			TextAnchor& a = v.Add();
			a.pos.y = offset;
			a.pos.x = i;
			a.length = len;
			i += len;
		}
		return !v.IsEmpty();
	};

	ScanText(foundtext);
	return true;
}

bool Finder::RegexSearch(const VectorMap<int, WString>& m, const WString& s)
{
	LTIMING("RegexSearch");

	int offset = m.GetKey(0);
	
	WString q;
	for(const WString& ss : m)
		q << ss;

	auto ScanText = [&q, &s, &offset](Vector<TextAnchor>& v) {
		RegExp r(s.ToString());
		String ln = ToUtf8(q);
		while(r.GlobalMatch(ln)) {
			int o = r.GetOffset();
			TextAnchor& a = v.Add();
			a.pos.y = offset;
			a.pos.x = Utf32Len(~ln, o);
			a.length = Utf32Len(~ln + o, r.GetLength());
		}
		return !v.IsEmpty();
	};

	ScanText(foundtext);
	return true;
}

bool Finder::OnSearch(const VectorMap<int, WString>& m, const WString& s)
{
	if(!term.HasFinder())
		return true;
	
	LTIMING("Finder::OnSearch");
	
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
	if(!IsChild() || term.IsSearching() || !foundtext.GetCount() || index < 0)
		return;

	TextAnchor p = foundtext[index];

	LTIMING("Finder::OnHighlight");

	for(const TextAnchor& a : foundtext)
		for(int row = 0, col = 0; row < hl.GetCount(); row++) {
			if(hl.GetKey(row) != a.pos.y)
				continue;
			int offset = 0;
			for(VTLine& l : hl) {
				for(VTCell& c : l) {
					offset += c == 1; // Double width char, second half.
					if(a.pos.x + offset <= col && col < a.pos.x + offset + a.length) {
						if(a.pos.y == p.pos.y && a.pos.x + offset == p.pos.x + offset) {
							c.Normal();
							c.Ink(term.highlight[1]);
							c.Paper(term.highlight[3]);
						}
						else
						if(~showall) {
							c.Normal();
							c.Ink(term.highlight[0]);
							c.Paper(term.highlight[2]);
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
