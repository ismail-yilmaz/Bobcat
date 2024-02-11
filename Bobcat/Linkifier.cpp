// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {
	
Linkifier::Linkifier(Terminal& ctx)
: ctx(ctx)
, cursor(-1)
, pos(-1)
, enabled(false)
{
	ctx.WhenSearch << THISFN(OnSearch);
	ctx.WhenHighlight << THISFN(OnHighlight);
}

Linkifier& Linkifier::Enable(bool b)
{
	if(enabled = b; !enabled)
		Clear();
	ctx.SyncHighlight();
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
	pos = ctx.GetMousePosAsIndex();
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
	pos = ctx.GetMousePosAsIndex();
	cursor = FindMatch(links, [this](const LinkInfo& q) {
		int i = ctx.GetPosAsIndex(q.pos);
		return pos >= i && pos < i + q.length;
	});
	return true;
}

void Linkifier::Update()
{
	if(ctx.HasLinkifier())
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
	ctx.Find(WString("NIL"), true); // We don't really search for a specific string.
	Sync();
}

bool Linkifier::OnSearch(const VectorMap<int, WString>& m, const WString& /* NIL */)
{
	if(!ctx.HasLinkifier())
		return true;

	LTIMING("Linkifier::OnSearch");
	
	WString text;
	for(const WString& s : m) // Unwrap the line...
		text << s;

	int offset = m.GetKey(0);

	for(const PatternInfo& pi : GetHyperlinkPatterns().Get(ctx.profilename)) {
		RegExp r(pi.pattern);
		String s = ToUtf8(text);
		while(r.GlobalMatch(s)) {
			int o = r.GetOffset();
			LinkInfo& p = links.Add();
			p.pos.y = offset;
			p.pos.x = Utf32Len(~s, o);
			p.length = Utf32Len(~s + o, r.GetLength());
			p.url = s.Mid(o, r.GetLength());
		}
	}
	return true;
}
  
void Linkifier::OnHighlight(VectorMap<int, VTLine>& hl)
{
	if(!ctx.HasLinkifier())
		return;

	LTIMING("Linkifier::OnHighlight");
	
	for(const LinkInfo& pt : links)
		for(int row = 0, col = 0; row < hl.GetCount(); row++) {
			if(hl.GetKey(row) != pt.pos.y)
				continue;
			int ipos = ctx.GetPosAsIndex(pt.pos);
			for(VTLine& l : hl) {
				for(VTCell& c : l) {
					if(!c.IsHyperlink() && pt.pos.x <= col && col < pt.pos.x + pt.length) {     // First, check if the cell isn't already a hyperlink.
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
LinkifierSetup::LinkifierSetup()
{
	CtrlLayout(*this);
	AddFrame(toolbar);
	list.AddColumn(tt_("Hyperlink patterns")).Edit(edit);
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
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

void LinkifierSetup::Load()
{
	if(IsNull(name))
		return;
	list.Clear();
	auto& m = GetHyperlinkPatterns();
	if(int i = m.FindAdd(name); i >= 0) {
		for(const PatternInfo& pi : m[i])
			list.Add(pi.pattern);
		list.SetCursor(0);
	}
}

void LinkifierSetup::Store() const
{
	if(IsNull(name))
		return;
	auto& m = GetHyperlinkPatterns();
	if(int i = m.FindAdd(name); i >= 0) {
		m[i].Clear();
		for(int j = 0; j < list.GetCount(); j++) {
			m[i].Add().pattern = list.Get(j, 0);
		}
	}
}

void LinkifierSetup::SetData(const Value& data)
{
	name = data;
	Load();
}

Value LinkifierSetup::GetData() const
{
	Store();
	return name;
}

}