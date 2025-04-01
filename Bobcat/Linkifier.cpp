// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {


Linkifier::Linkifier(Terminal& t)
: term(t)
, cursor(-1)
, pos(-1)
, enabled(false)
{
}

void Linkifier::SetConfig(const Profile& p)
{
	// Acquire the patterns only once.
	if(auto& m = GetHyperlinkPatterns(); !IsNull(p.name) && m.Find(p.name) < 0) {
		m.Add(p.name).Append(p.linkifier.patterns);
	}
}

Linkifier& Linkifier::Enable(bool b)
{
	if(enabled = b; !enabled)
		Clear();
	term.SyncHighlight();
	return *this;
}

Linkifier& Linkifier::Disable()
{
	return Enable(false);
}

bool Linkifier::IsEnabled() const
{
	return enabled;
}

int Linkifier::GetCursor() const
{
	return cursor;
}

void Linkifier::ClearCursor()
{
	cursor = -1;
}

bool Linkifier::IsCursor() const
{
	return enabled && cursor >= 0 && cursor < GetCount();
}

int Linkifier::GetPos()
{
	return pos;
}

void Linkifier::UpdatePos()
{
	pos = term.GetMousePosAsIndex();
}

void Linkifier::ClearPos()
{
	pos = -1;
}

int Linkifier::GetCount() const
{
	return links.GetCount();
}

LinkInfo& Linkifier::operator[](int i)
{
	 ASSERT(i >= 0 && i < GetCount());
	 return links[i];
}

const LinkInfo& Linkifier::GetCurrentLinkInfo() const
{
	return links.Get(cursor, Single<LinkInfo>());
}

void Linkifier::Clear()
{
	pos    = -1;
	cursor = -1;
	links.Clear();
}

bool Linkifier::Sync()
{
	cursor = -1;
	if(!enabled || links.IsEmpty())
		return false;
	pos = term.GetMousePosAsIndex();
	cursor = FindMatch(links, [this](const LinkInfo& q) {
		int i = term.GetPosAsIndex(q.pos, true);
		return pos >= i && pos < i + q.length;
	});
	return true;
}

void Linkifier::Update()
{
	if(term.HasLinkifier())
		Search();
}

const LinkInfo *Linkifier::begin() const
{
	 return links.begin();
}

LinkInfo *Linkifier::begin()
{
	 return links.begin();
}

const LinkInfo *Linkifier::end() const
{
	 return links.end();
}

LinkInfo *Linkifier::end()
{
	 return links.end();
}

void Linkifier::Search()
{
	Clear();
	Scan();
	Sync();
}

void Linkifier::Scan()
{
	if(!term.HasLinkifier() || term.HasFinder() || !term.IsVisible())
		return;

	LTIMING("Linkifier::Scan");

	auto ScanRange = [&](VectorMap<int, VTLine>& m) {
		WString text;
		int offset = m.GetKey(0);
		for(const auto& q : m)  // Unwrap the line...
			text << q.ToWString();
	
		if(text.IsEmpty())
			return false;
	
		String s = ToUtf8(text);
		for(const PatternInfo& pi : GetHyperlinkPatterns().Get(term.profilename)) {
			RegExp r(pi.pattern);
			while(r.GlobalMatch(s)) {
				int o = r.GetOffset();
				LinkInfo& p = links.Add();
				p.pos.y = offset;
				p.pos.x = Utf32Len(~s, o);
				p.length = Utf32Len(~s + o, r.GetLength());
				p.url = s.Mid(o, r.GetLength());
			}
		}
		return false;
	};
	term.GetPage().FetchRange(term.GetPageRange(), ScanRange);
}
 
void Linkifier::OnHighlight(VectorMap<int, VTLine>& hl)
{
	if(!term.HasLinkifier() || term.HasFinder() || !term.IsVisible() || links.IsEmpty())
		return;

	LTIMING("Linkifier::OnHighlight");

	for(const LinkInfo& pt : links) {
		for(int row = 0, col = 0; row < hl.GetCount(); row++) {
			if(hl.GetKey(row) != pt.pos.y)
				continue;
			int offset = 0;
			int ipos = term.GetPosAsIndex(pt.pos, true);
			for(VTLine& l : hl)
				for(VTCell& c : l) {
					offset += c == 1; // Double width char, second half.
					if(!c.IsHypertext() && pt.pos.x + offset <= col && col < pt.pos.x + pt.length + offset) { // First, check if the cell isn't already a hypertext.
						if(pos >= ipos && pos < ipos + pt.length) {
							c.Hyperlink();
							c.data = 0;
						}
						else {
							c.Hyperlink();
							c.data = (dword) -1;
						}
					}
					col++;
				}
		}
	}
}

Linkifier::Config::Config()
{
}

void Linkifier::Config::Jsonize(JsonIO& jio)
{
	jio("Patterns", patterns);
}

LinkifierSetup::LinkifierSetup()
{
	CtrlLayout(*this);
	AddFrame(toolbar);
	list.AddColumn(tt_("Hyperlink patterns")).Edit(edit);
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
	list.WhenDrag = THISFN(Drag);
	list.WhenDropInsert = THISFN(DnDInsert);
	Sync();
}

void LinkifierSetup::Sync()
{
	toolbar.Set(THISFN(ContextMenu));
}

void LinkifierSetup::ContextMenu(Bar& bar)
{
	bool e = list.IsEditable();
	bool c = !list.IsEdit() && e;
	bool d = c && list.IsCursor();
	bool q = list.GetCursor() >= 0 && list.GetCursor() < list.GetCount() - 1;

	bool b = list.IsCursor();
	bar.Add(c, tt_("Add pattern"), Images::Add(), [this]() { list.DoAppend(); }).Key(K_INSERT);
	bar.Add(d, tt_("Edit pattern"), Images::Edit(), [this]() { list.DoEdit(); }).Key(K_SPACE);
	bar.Add(d, tt_("Remove pattern"), Images::Delete(), [this]() { list.DoRemove(); }).Key(K_DELETE);
	bar.Separator();
	bar.Add(list.GetCursor() > 0, tt_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, tt_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	bar.Separator();
	bar.Add(list.GetCount() > 0, tt_("Select all"), Images::SelectAll(), [this]() { list.DoSelectAll(); }).Key(K_CTRL_A);
}

void LinkifierSetup::Drag()
{
	if(list.DoDragAndDrop(InternalClip(list, "linkifierpatternlist"), list.GetDragSample()) == DND_MOVE)
		list.RemoveSelection();
}

void LinkifierSetup::DnDInsert(int line, PasteClip& d)
{
	if(AcceptInternal<ArrayCtrl>(d, "linkifierpatternlist")) {
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

void LinkifierSetup::Load(const Profile& p)
{
	list.Clear();
	for(const PatternInfo& s : p.linkifier.patterns)
		list.Add(s.pattern);
	list.SetCursor(0);
}

void LinkifierSetup::Store(Profile& p) const
{
	if(IsNull(p.name))
		return;
	for(int i = 0; i < list.GetCount(); i++)
		p.linkifier.patterns.Add().pattern = list.Get(i, 0);
}

VectorMap<String, Vector<PatternInfo>>& GetHyperlinkPatterns()
{
	return Single<VectorMap<String, Vector<PatternInfo>>>();
}

void PatternInfo::Jsonize(JsonIO& jio)
{
    jio("Command", cmd)
       ("Pattern", pattern);
}

String PatternInfo::ToString() const
{
	return "Command: " << cmd << ", Pattern: " << pattern;
}

}