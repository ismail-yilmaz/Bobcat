// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

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

static StaticMutex sFinderLock;

Finder::Finder(Terminal& t)
: term(t)
, mode(Finder::Mode::CaseSensitive)
, limit(SEARCH_MAX)
, cancel(false)
{
}

Finder::~Finder()
{
}

Finder& Finder::SetLimit(int n)
{
	limit = clamp(n, 1, SEARCH_MAX);
	return *this;
}

Finder& Finder::SetMode(Mode m)
{
	mode = m;
	return *this;
}

Finder& Finder::CaseSensitive()
{
	mode = Mode::CaseSensitive;
	return *this;
}

Finder& Finder::CaseInsensitive()
{
	mode = Mode::CaseInsensitive;
	return *this;
}

Finder& Finder::Regex()
{
	mode = Mode::Regex;
	return *this;
}

bool Finder::IsCaseSensitive() const
{
	return mode == Mode::CaseSensitive;
}

bool Finder::IsCaseInsensitive() const
{
	return mode == Mode::CaseInsensitive;
}

bool Finder::IsRegex() const
{
	return mode == Mode::Regex;
}

bool Finder::Find(const WString& text, bool co)
{
	foundtext.Clear();
	cancel = false;
	if(co) {
		term.CoFind(~text, false, THISFN(OnSearch));
	}
	else {
		term.Find(~text, false, THISFN(OnSearch));
	}
	return HasFound();
}

bool Finder::BasicSearch(const VectorMap<int, WString>& m, const WString& s)
{
	int slen = s.GetLength();
	int offset = m.GetKey(0);
	
	LTIMING("TrivialSearch");
	
	// Notes: 1) We are using this for search, because it is faster than using WString::Find() here.
	//        2) m.GetCount() > 1 == text is wrapped.
	
	auto ScanText = [&](SortedIndex<ItemInfo>& v, int limit, bool tolower) {
		for(int row = 0, i = 0; row < m.GetCount(); row++) {
			for(int col = 0; col < m[row].GetLength(); col++, i++) {
				if(IsCanceled())
					return true;
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
						ItemInfo q;
						q.pos.y = offset;
						q.pos.x = i;
						q.length = slen;
						sFinderLock.Enter();
						v.Add(q);
						sFinderLock.Leave();
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
	
	RegExp r(s.ToString());
	String ln = ToUtf8(q);

	while(r.GlobalMatch(ln)) {
		if(IsCanceled())
			return true;
		ItemInfo a;
		int o = r.GetOffset();
		a.pos.y = offset;
		a.pos.x = Utf32Len(~ln, o);
		a.length = Utf32Len(~ln + o, r.GetLength());
		sFinderLock.Enter();
		foundtext.Add(a);
		sFinderLock.Leave();
		if(foundtext.GetCount() == limit)
			return true;
	}

	return false;
}

bool Finder::OnSearch(const VectorMap<int, WString>& m, const WString& s)
{
	return IsRegex() ? RegexSearch(m, s) : BasicSearch(m, s);
}

const SortedIndex<ItemInfo>& Finder::GetItems() const
{
	return foundtext;
}

const ItemInfo& Finder::Get(int i)
{
	ASSERT(i > 0 && i <= foundtext.GetCount());
	return foundtext[i];
}


const ItemInfo& Finder::operator[](int i)
{
	return Get(i);
}

int Finder::GetCount() const
{
	return foundtext.GetCount();
}

bool Finder::HasFound() const
{
	return foundtext.GetCount() > 0;
}

void Finder::Cancel()
{
	cancel = true;
}

bool Finder::IsCanceled() const
{
	return cancel;
}

Finder::Config::Config()
: searchmode("sensitive")
, searchlimit(65536)
, saveformat("txt")
, savemode("map")
, delimiter(",")
, showall(false)
, parallelize(false)
{
}

void Finder::Config::Jsonize(JsonIO& jio)
{
	jio("SearchMode",        searchmode)
	   ("SearchLimit",       searchlimit)
	   ("ShowAll",           showall)
	   ("ParallelSearch",    parallelize)
	   ("HarvestingFormat",  saveformat)
	   ("HarvestingMode",    savemode)
	   ("Delimiter",         delimiter)
	   ("Patterns",          patterns);
}

FinderBar::FinderBar(Terminal& t)
: Finder(t)
, term(t)
, index(0)
, showall(false)
, co(false)
, harvester(*this)
{
	CtrlLayout(*this);
	close.Image(Images::Delete()).Tip(t_("Close finder"));
	next.Image(Images::Up());
	prev.Image(Images::Down());
	begin.Image(Images::Begin());
	end.Image(Images::End());
	next  << THISFN(Next);
	prev  << THISFN(Prev);
	begin << THISFN(Begin);
	end   << THISFN(End);
	close << THISFN(Hide);
	text.NullText(t_("Type to search..."));
	text.AddFrame(display);
	text.AddFrame(menu);
	text.WhenBar = THISFN(SearchBar);
	text.WhenAction << [this] { Search(); };
	fsave.Image(Images::Reap());
	fsave << [=] { SaveToFile(); };
	csave.Image(Images::Paste());
	csave << [=] { SaveToClipboard(); };
	menu.Image(Images::Find());
	menu << [=] { MenuBar::Execute(THISFN(StdBar)); };
	display.SetDisplay(StdRightDisplay());
	Sync();
}

FinderBar::~FinderBar()
{
}

void FinderBar::SetConfig(const Profile& p)
{
	SetSearchMode(p.finder.searchmode);
	SetLimit(p.finder.searchlimit);
	showall = ~p.finder.showall;
	co = p.finder.parallelize;
	harvester.Format(p.finder.saveformat);
	harvester.Delimiter(p.finder.delimiter);
	harvester.Mode(p.finder.savemode);
	text.ClearList();
	for(const String& s: p.finder.patterns)
		text.AddList(s);
}

void FinderBar::SetData(const Value& v)
{
	data = v;
	term.RefreshLayout();
}

Value FinderBar::GetData() const
{
	return data;
}

void FinderBar::FrameLayout(Rect& r)
{
	data == "top"
		? LayoutFrameTop(r, this, cy ? cy : r.Width())
		: LayoutFrameBottom(r, this, cy ? cy : r.Width()); // default
}

void FinderBar::Show()
{
	if(!IsChild()) {
		bool b = term.HasSizeHint();
		term.HideSizeHint();
		term.AddFrame(Height(GetStdBarHeight()));
		term.SyncHighlight();
		term.ShowSizeHint(b);
	}
	text.SetFocus();
}

void FinderBar::Hide()
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

void FinderBar::Goto(int i)
{
	if(i >= 0 && i < GetCount()) {
		term.Goto(Get(i).pos.y);;
		Sync();
	}
}

void FinderBar::Next()
{
	if(int n = GetCount(); n >= 0) {
		index = clamp(++index, 0, n - 1);
		Goto(index);
	}
}

void FinderBar::Prev()
{
	if(int n = GetCount(); n >= 0) {
		index = clamp(--index, 0, n - 1);
		Goto(index);
	}
}

void FinderBar::Begin()
{
	if(int n = GetCount(); n >= 0) {
		index = 0;
		Goto(index);
	}
}

void FinderBar::End()
{
	if(int n = GetCount(); n >= 0) {
		index = n - 1;
		Goto(index);
	}
}

void FinderBar::SetSearchMode(const String& mode)
{
	SetMode(decode(mode,
			"regex", Finder::Mode::Regex,
				"insensitive", Finder::Mode::CaseInsensitive,
					/* sensitive */		Finder::Mode::CaseSensitive));
	Sync();
}

void FinderBar::CheckCase()
{
	CaseSensitive();
	Update();
}

void FinderBar::IgnoreCase()
{
	CaseInsensitive();
	Update();
}

void FinderBar::CheckPattern()
{
	Regex();
	Update();
}

void FinderBar::ShowAll(bool b)
{
	showall = b;
	Sync();
}

void FinderBar::Co(bool b)
{
	co = b;
	Sync();
}

void FinderBar::Undo()
{
	text.Undo();
}

void FinderBar::Cut()
{
	text.Cut();
}

void FinderBar::Copy()
{
	text.Copy();
}

void FinderBar::Paste()
{
	text.Paste();
}

void FinderBar::SelectAll()
{
	text.SelectAll();
}

void FinderBar::StdBar(Bar& menu)
{
	menu.Add(AK_CHECKCASE, THISFN(CheckCase)).Radio(IsCaseSensitive());
	menu.Add(AK_IGNORECASE,THISFN(IgnoreCase)).Radio(IsCaseInsensitive());
	menu.Add(AK_REGEX,     THISFN(CheckPattern)).Radio(IsRegex());
	menu.Separator();
	menu.Add(AK_FIND_ALL,   [this]{ ShowAll(!showall); }).Check(showall);
	menu.Separator();
	menu.Add(AK_PARALLELIZE,[this]{ Co(!co); }).Check(co);
	StdKeys(menu);
}

void FinderBar::StdKeys(Bar& menu)
{
	menu.AddKey(AK_FIND_ALL,     [this] { ShowAll(!showall); });
	menu.AddKey(AK_FIND_NEXT,    THISFN(Next));
	menu.AddKey(AK_FIND_PREV,    THISFN(Prev));
	menu.AddKey(AK_FIND_FIRST,   THISFN(Begin));
	menu.AddKey(AK_FIND_LAST,    THISFN(End));
	menu.AddKey(AK_HIDE_FINDER,  THISFN(Hide));
	menu.AddKey(AK_HARVEST_FILE, THISFN(SaveToFile));
	menu.AddKey(AK_HARVEST_CLIP, THISFN(SaveToClipboard));
	menu.AddKey(AK_HARVEST_LIST, [this] { harvester.Mode("list"); });
	menu.AddKey(AK_HARVEST_MAP,  [this] { harvester.Mode("map");  });
}

void FinderBar::SearchBar(Bar& menu)
{
	bool ed = text.IsEditable();
	bool sel = text.IsSelection();
	menu.Add(ed, AK_FIND_UNDO, Images::Undo(), THISFN(Undo));
	menu.Separator();
	menu.Add(ed && sel, AK_FIND_CUT, Images::Cut(), THISFN(Cut));
	menu.Add(sel, AK_FIND_COPY, Images::Copy(), THISFN(Copy));
	menu.Add(ed && IsClipboardAvailableText(), AK_FIND_PASTE, Images::Paste(), THISFN(Paste));
	menu.Separator();
	menu.Add(text.GetLength(), AK_FIND_SELECTALL, Images::SelectAll(), THISFN(SelectAll));
	StdKeys(menu);
}

bool FinderBar::Key(dword key, int count)
{
	MenuBar::Scan([this](Bar& menu) { StdBar(menu); }, key);
	return true;
}

void FinderBar::Sync()
{
	int cnt = GetCount();
	index = clamp(index, 0, max(0, cnt - 1));
	bool err = !IsNull(~text) && !cnt;
	String s;
	if(!err) {
		if(!IsNull(~text))
			s << (cnt ? index + 1 : 0) << "/" << cnt << " ";
		if(IsCaseSensitive()) {
			s << "C ";
			display.Tip(t_("Case sensitive mode"));
		}
		else
		if(IsCaseInsensitive()) {
			s << "I ";
			display.Tip(t_("Case insensitive mode"));
		}
		else
		if(IsRegex()) {
			s << "R ";
			display.Tip(t_("Regex mode"));
		}
		else
			NEVER();
	}

	SetSearchStatusText(display, s);

	String k;
	k = " (" + GetKeyDesc(FinderKeys::AK_HARVEST_FILE().key[0]) + ") ";
	fsave.Tip(t_("Save to file") + k);
	k = " (" + GetKeyDesc(FinderKeys::AK_HARVEST_CLIP().key[0]) + ") ";
	csave.Tip(t_("Copy to clipboard") + k);
	
	if(cnt && IsRegex()) {
		if(!fsave.IsChild())
			text.InsertFrame(2, fsave); // FIXME: Yes, ugly...
		if(!csave.IsChild())
			text.InsertFrame(2, csave);
	}
	else {
		if(fsave.IsChild())
			text.RemoveFrame(fsave);
		if(csave.IsChild())
			text.RemoveFrame(csave);
	}
	
	bool a = !term.IsSearching() && cnt > 0 && index > 0;
	bool b = !term.IsSearching() && cnt > 0 && index < cnt - 1;
	prev.Enable(a);
	next.Enable(b);
	begin.Enable(a);
	end.Enable(b);
	text.Error(err);
	term.Refresh();
}

void FinderBar::Search()
{
	Search(~text);
}

void FinderBar::Search(const WString& txt)
{
	if(term.IsSearching()) {
		Cancel();
		return;
	}
	Find(txt, co);
	Sync();
}


void FinderBar::Update()
{
	if(IsChild())
		Search();
}

void FinderBar::SaveToFile()
{
	harvester.SaveToFile();
}

void FinderBar::SaveToClipboard()
{
	harvester.SaveToClipboard();
}

void FinderBar::OnHighlight(HighlightInfo& hl)
{
	if(!term.HasFinder() || term.IsSearching() || !HasFound() || index < 0)
		return;

	LTIMING("FinderBar::OnHighlight");

	hl.adjusted = true;
	term.DoHighlight(GetItems(), hl, [&](HighlightInfo& hl) {
		const ItemInfo& p = Get(index);
		const ItemInfo* q = hl.iteminfo;
		int   o = hl.offset;
		for(auto cell : hl.highlighted) {
			if(q->pos.y == p.pos.y && q->pos.x + o == p.pos.x + o) {
				cell->Normal()
					.Ink(term.highlight[1]).Paper(term.highlight[3]);
			}
			else
			if(showall) {
				cell->Normal()
					.Ink(term.highlight[0]).Paper(term.highlight[2]);
			}
		}
	});
}

FinderBar::Harvester::Harvester(FinderBar& f)
: finder(f)
, format(Fmt::Csv)
, delimiter(",")
{
}

FinderBar::Harvester& FinderBar::Harvester::Format(const String& fmt)
{
	format = decode(fmt, "csv", Harvester::Fmt::Csv, Harvester::Fmt::Txt);
	return *this;
}

FinderBar::Harvester& FinderBar::Harvester::Mode(const String& md)
{
	if(md == "list")
		delimiter = "\r\n";
	return *this;
}

FinderBar::Harvester& FinderBar::Harvester::Delimiter(const String& delim)
{
	delimiter = delim;
	return *this;
}

bool FinderBar::Harvester::Reap(Stream& s)
{
	bool aborted = false;
	Progress pi(&finder.term);
	pi.Title(t_("Harvester"));
	pi.Set(0, finder.GetCount());
	const char *status = t_("Saved %d of %d item(s). [%s]");
	finder.term.Find(~finder.text, false, [&](const VectorMap<int, WString>& m, const WString& /* NIL*/) {
		WString txt;
		for(const WString& s : m)
			txt << s;
		if(txt.IsEmpty())
			return false;
		String reaped;
		for(const ItemInfo& a : finder.GetItems()) {
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

bool FinderBar::Harvester::IsReady() const
{
	if(finder.term.IsSearching())
		return false;

	if(!finder.HasFound()) {
		Exclamation(t_("Nothing to harvest."));
		return false;
	}

	if(!finder.IsRegex()) {
		Exclamation(t_("Cannot harvest.&Finder is not in regexp (R) mode."));
		return false;
	}
	
	return true;
}

void FinderBar::Harvester::SaveToClipboard()
{
	if(!IsReady())
		return;

	if(StringStream ss; Reap(ss))
		AppendClipboardText(ss);
}

void FinderBar::Harvester::SaveToFile()
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
	parallelize = false;
	list.InsertFrame(0, toolbar);
	list.AddColumn(t_("Predefined search patterns")).Edit(edit).SetDisplay(FinderSetupListDisplay());
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
	list.WhenDrag = THISFN(Drag);
	list.WhenDropInsert = THISFN(DnDInsert);
	Sync();

}
void FinderSetup::Sync()
{
	toolbar.Set(THISFN(ContextMenu));
}

void FinderSetup::ContextMenu(Bar& bar)
{
	bool e = list.IsEditable();
	bool c = !list.IsEdit() && e;
	bool d = c && list.IsCursor();
	bool q = list.GetCursor() >= 0 && list.GetCursor() < list.GetCount() - 1;

	bar.Add(c, t_("Add pattern"), Images::Add(), [this]() { list.DoAppend(); }).Key(K_INSERT);
	bar.Add(d, t_("Edit pattern"), Images::Edit(), [this]() { list.DoEdit(); }).Key(K_SPACE);
	bar.Add(d, t_("Remove pattern"), Images::Delete(), [this]() { list.DoRemove(); }).Key(K_DELETE);
	bar.Separator();
	bar.Add(list.GetCursor() > 0, t_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, t_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	bar.Separator();
	bar.Add(list.GetCount() > 0, t_("Select all"), Images::SelectAll(), [this]() { list.DoSelectAll(); }).Key(K_CTRL_A);
}

void FinderSetup::Drag()
{
	if(list.DoDragAndDrop(InternalClip(list, "finderpatternlist"), list.GetDragSample()) == DND_MOVE)
		list.RemoveSelection();
}

void FinderSetup::DnDInsert(int line, PasteClip& d)
{
	if(AcceptInternal<ArrayCtrl>(d, "finderpatternlist")) {
		const ArrayCtrl& src = GetInternal<ArrayCtrl>(d);
		bool self = &src == &list;
		Vector<Vector<Value>> data;
		for(int i = 0; i < src.GetCount(); i++)
			if(src.IsSel(i)) {
				data.Add().Add(src.Get(i, 0));
			}
		list.InsertDrop(line, data, d, self);
		list.SetFocus();
	}
}

void FinderSetup::Load(const Profile& p)
{
	list.Clear();
	searchmode  <<= p.finder.searchmode;
	maxsearch   <<= p.finder.searchlimit;
	showall     <<= p.finder.showall;
	saveformat  <<= p.finder.saveformat;
	savemode    <<= p.finder.savemode;
	delimiter   <<= p.finder.delimiter;
	parallelize <<= p.finder.parallelize;
	for(const String& s : p.finder.patterns)
		list.Add(s);
	list.SetCursor(0);
}

void FinderSetup::Store(Profile& p) const
{
	p.finder.searchmode  = ~searchmode;
	p.finder.searchlimit = ~maxsearch;
	p.finder.showall     = ~showall;
	p.finder.saveformat  = ~saveformat;
	p.finder.savemode    = ~savemode;
	p.finder.delimiter   = ~delimiter;
	p.finder.parallelize = ~parallelize;
	for(int i = 0; i < list.GetCount(); i++)
		p.finder.patterns.Add(list.Get(i, 0));
}
}

