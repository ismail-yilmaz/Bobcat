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

constexpr const int SEARCH_MAX = 256000;

static void sWriteToDisplay(FrameLR<DisplayCtrl>& f, const String& txt)
{
	int cx = max(GetStdFont().GetMonoWidth() * 2, GetTextSize(txt, GetStdFont()).cx);
	f.Width(cx) <<= AttrText(txt).Bold().Ink(SColorDisabled);
}

Finder::Finder(Terminal& t)
: term(t)
, index(0)
, limit(SEARCH_MAX)
, harvester(*this)
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

int Finder::GetCount() const
{
	return foundtext.GetCount();
}

bool Finder::HasFound() const
{
	return foundtext.GetCount() > 0;
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
	menu.AddKey(AK_FIND_ALL,     [this] { showall = !showall; Sync(); });
	menu.AddKey(AK_FIND_NEXT,    THISFN(Next));
	menu.AddKey(AK_FIND_PREV,    THISFN(Prev));
	menu.AddKey(AK_FIND_FIRST,   THISFN(Begin));
	menu.AddKey(AK_FIND_LAST,    THISFN(End));
	menu.AddKey(AK_HIDE_FINDER,  THISFN(Hide));
	menu.AddKey(AK_HARVEST_FILE, THISFN(SaveToFile));
	menu.AddKey(AK_HARVEST_CLIP, THISFN(SaveToClipboard));
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
	bool a = !term.IsSearching() && cnt > 0 && index > 0;
	bool b = !term.IsSearching() && cnt > 0 && index < cnt - 1;
	prev.Enable(a);
	next.Enable(b);
	begin.Enable(a);
	end.Enable(b);
	sWriteToDisplay(mode, decode(searchtype, Search::CaseInsensitive, "I", Search::Regex, "R", "C"));
	text.Error(!IsNull(~text) && !cnt);
	term.Refresh();
}

void Finder::Search()
{
	if(term.IsSearching())
		return;
	foundtext.Clear();
	term.Find(~text, false, THISFN(OnSearch));
	Sync();
}

void Finder::Update()
{
	if(IsChild())
		Search();
}

void Finder::SaveToFile()
{
	harvester.SaveToFile();
}

void Finder::SaveToClipboard()
{
	harvester.SaveToClipboard();
}

bool Finder::BasicSearch(const VectorMap<int, WString>& m, const WString& s)
{
	int slen = s.GetLength();
	int offset = m.GetKey(0);
	
	LTIMING("TrivialSearch");
	
	// Notes: 1) We are using this for search, because it is faster than using WString::Find() here.
	//        2) m.GetCount() > 1 == text is wrapped.
	
	auto ScanText = [&](Vector<TextAnchor>& v, int limit, bool tolower) {
		for(int row = 0, i = 0; row < m.GetCount(); row++) {
			for(int col = 0; col < m[row].GetLength(); col++, i++) {
				int a = m[row][col], b = s[0];
				if(tolower) {
					a = ToLower(a);
					b = ToLower(b);
				}
				if(a == b) {
					int trow = row, tcol = col, tlen = slen;
					// Check if the substring is present starting from the current position.
					while(tlen > 0 && trow < m.GetCount()) {
						a = m[trow][tcol], b = s[slen - tlen];
						if(tolower) {
							a = ToLower(a);
							b = ToLower(b);
						}
						if(a == b)
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
						if(v.GetCount() == limit)
							return true;
					}
				}
			}
		}
		return false;
	};
	
	return ScanText(foundtext, limit, IsCaseInsensitive());
}

bool Finder::RegexSearch(const VectorMap<int, WString>& m, const WString& s)
{
	LTIMING("RegexSearch");

	int offset = m.GetKey(0);
	
	WString q;
	for(const WString& ss : m)
		q << ss;

	if(q.IsEmpty())
		return false;
	
	auto ScanText = [&](Vector<TextAnchor>& v, int limit) {
		RegExp r(s.ToString());
		String ln = ToUtf8(q);
		while(r.GlobalMatch(ln)) {
			int o = r.GetOffset();
			TextAnchor& a = v.Add();
			a.pos.y = offset;
			a.pos.x = Utf32Len(~ln, o);
			a.length = Utf32Len(~ln + o, r.GetLength());
			if(v.GetCount() == limit)
				return true;
		}
		return false;
	};
	
	return ScanText(foundtext, limit);
}

bool Finder::OnSearch(const VectorMap<int, WString>& m, const WString& s)
{
	if(!term.HasFinder())
		return true;
	return IsRegex() ? RegexSearch(m, s) : BasicSearch(m, s);
}

void Finder::OnHighlight(VectorMap<int, VTLine>& hl)
{
	if(!term.HasFinder() || term.IsSearching() || !foundtext.GetCount() || index < 0)
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

void Finder::SetConfig(const Finder::Config& cfg)
{
	SetSearchMode(cfg.searchmode);
	limit = clamp(cfg.searchlimit, 1, SEARCH_MAX);
	showall = cfg.showall;
	harvester.Format(cfg.saveformat).Delimiter(cfg.delimiter).Mode(cfg.savemode);
}

Finder::Harvester::Harvester(Finder& f)
: finder(f)
, format(Fmt::Csv)
, delimiter(",")
{
}

Finder::Harvester& Finder::Harvester::Format(const String& fmt)
{
	format = decode(fmt, "csv", Harvester::Fmt::Csv, Harvester::Fmt::Txt);
	return *this;
}

Finder::Harvester& Finder::Harvester::Mode(const String& md)
{
	if(md == "list")
		delimiter = "\r\n";
	return *this;
}

Finder::Harvester& Finder::Harvester::Delimiter(const String& delim)
{
	delimiter = delim;
	return *this;
}

bool Finder::Harvester::Reap(Stream& s)
{
	bool aborted = false;
	Progress pi(&finder.term);
	pi.Title(t_("Harvester"));
	pi.Set(0, finder.foundtext.GetCount());
	const char *status = t_("Saved %d of %d item(s). [%s]");
	finder.term.Find(~finder.text, false, [&](const VectorMap<int, WString>& m, const WString& /* NIL*/) {
		WString txt;
		for(const WString& s : m)
			txt << s;
		if(txt.IsEmpty())
			return false;
		String reaped;
		for(const TextAnchor& a : finder.foundtext) {
			if(m.GetKey(0) != a.pos.y)
				continue;
			if((aborted = pi.StepCanceled()))
				return true;
			String q = ToUtf8(txt.Mid(a.pos.x, a.length));
			if(!q.IsEmpty())
				reaped << (format == Fmt::Csv ? CsvString(q) : q) << delimiter;
		}
		if(!reaped.IsEmpty()) {
			reaped.TrimEnd(delimiter);
			s.Put(reaped);
			s.PutCrLf();
		}
		pi.SetText(Upp::Format(status, pi.GetPos(), pi.GetTotal(), FormatFileSize(s.GetSize())));
		return false;
	});
	s.Close();
	return !aborted;
}

bool Finder::Harvester::IsReady() const
{
	if(finder.term.IsSearching())
		return false;

	if(finder.foundtext.IsEmpty()) {
		Exclamation(t_("Nothing to harvest."));
		return false;
	}

	if(!finder.IsRegex()) {
		Exclamation(t_("Cannot harvest.&Finder is not in regexp (R) mode."));
		return false;
	}
	
	return true;
}

void Finder::Harvester::SaveToClipboard()
{
	if(!IsReady())
		return;

	if(StringStream ss; Reap(ss))
		AppendClipboardText(ss);
}

void Finder::Harvester::SaveToFile()
{
	if(!IsReady())
		return;

	String fmt = decode(format,	Fmt::Csv,  "*.csv", "*.txt");
	if(String path = SelectFileSaveAs(fmt); !path.IsEmpty()) {
		String tmp = GetTempFileName();
		if(FileOut fo(tmp); fo && Reap(fo))
			FileCopy(tmp, path);
		DeleteFile(tmp);
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

Finder::Config::Config()
: searchmode("sensitive")
, searchlimit(65536)
, saveformat("txt")
, savemode("map")
, delimiter(",")
, showall(false)
{
}

void Finder::Config::Jsonize(JsonIO& jio)
{
	jio("SearchMode",       searchmode)
	   ("SearchLimit",      searchlimit)
	   ("ShowAll",          showall)
	   ("HarvestingFormat", saveformat)
	   ("HarvestingMode",   savemode)
	   ("Delimiter",        delimiter);
}

FinderSetup::FinderSetup()
{
	CtrlLayout(*this);
	searchmode.Add("sensitive",   t_("Case sensitive"));
	searchmode.Add("insensitive", t_("Case insensitive"));
	searchmode.Add("regex",       t_("Regex"));
	searchmode.SetIndex(0);
	saveformat.Add("txt", t_("Plain text"));
	saveformat.Add("csv", "Csv");
	saveformat.SetIndex(0);
	savemode.Add("map",  t_("Map"));
	savemode.Add("list", t_("List"));
	savemode.SetIndex(0);
	maxsearch <<= 65536;
	showall = false;
}
void FinderSetup::Sync()
{
	// TODO
}

void FinderSetup::ContextMenu(Bar& bar)
{
	// TODO
}

}

