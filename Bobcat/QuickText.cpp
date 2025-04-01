// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

struct QuickPasteDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		if(auto ctx = GetContext(); ctx)
			if(Terminal *t = ctx->GetActiveTerminal(); t)
				StdDisplay().Paint(w, r, AttrText(q).SetFont(t->GetFont()), ink, paper, style);
	}
};

static PopUpList& sGetTextList()
{
	return Single<PopUpList>();
}

QuickText::QuickText(Terminal& t)
: term(t)
{
	auto& q = sGetTextList();
	q.SetDisplay(Single<QuickPasteDisplayCls>());
	q.WhenSelect = [this] { OnSelect(); };
	q.WhenCancel = [this] { OnCancel(); };
}

void QuickText::OnSelect()
{
	auto& q = sGetTextList();
	if(int i = q.GetCursor(); i >= 0) {
		String txt = q.Get(i);
		term.PutText(txt.ToWString());
		term.SetFocus();
	}
}

void QuickText::OnCancel()
{
	term.SetFocus();
}

int QuickText::LoadText()
{
	auto& q = sGetTextList();
	int i = q.GetCursor();
	q.Clear();
	
	Profile p = LoadProfile(term.profilename);
	if(p.quicktext.texts.IsEmpty())
		return 0;
					
	Font f = term.GetFont();
	q.SetLineCy(f.GetCy());
	
	int width = 0;
	for(const String& s : p.quicktext.texts) {
		q.Add(s);
		int cx = GetTextSize(s, f).cx;
		width = max(Zx(cx), width);
	}

	q.SetCursor(i >= 0 && i < q.GetCount() ? i : 0);
	
	return width;
}

void QuickText::Popup()
{
	if(int width = LoadText(); width > 0){
		Size sz = term.GetCellSize();
		Rect rp = term.GetScreenView() + term.GetCursorPoint();
		sGetTextList().PopUp(&term, rp.left, rp.top + sz.cy, rp.top + sz.cy, width);
	}
}

QuickText::Config::Config()
{
}

void QuickText::Config::Jsonize(JsonIO& jio)
{
	 jio("Texts", texts);
}

QuickTextSetup::QuickTextSetup()
{
	CtrlLayout(*this);
	AddFrame(toolbar);
	list.AddColumn(tt_("Text")).Edit(edit);
	list.ColumnWidths("50 300");
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
	list.WhenDrag = THISFN(Drag);
	list.WhenDropInsert = THISFN(DnDInsert);
	Sync();
}

void QuickTextSetup::Sync()
{
	toolbar.Set(THISFN(ContextMenu));
}

void QuickTextSetup::ContextMenu(Bar& bar)
{
	bool e = list.IsEditable();
	bool c = !list.IsEdit() && e;
	bool d = c && list.IsCursor();
	bool q = list.GetCursor() >= 0 && list.GetCursor() < list.GetCount() - 1;

	bool b = list.IsCursor();
	bar.Add(c, tt_("Add text"), Images::Add(), [this]() { list.DoAppend(); }).Key(K_INSERT);
	bar.Add(d, tt_("Edit text"), Images::Edit(), [this]() { list.DoEdit(); }).Key(K_SPACE);
	bar.Add(d, tt_("Remove text"), Images::Delete(), [this]() { list.DoRemove(); }).Key(K_DELETE);
	bar.Separator();
	bar.Add(list.GetCursor() > 0, tt_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, tt_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	bar.Separator();
	bar.Add(list.GetCount() > 0, tt_("Select all"), Images::SelectAll(), [this]() { list.DoSelectAll(); }).Key(K_CTRL_A);
}

void QuickTextSetup::Drag()
{
	if(list.DoDragAndDrop(InternalClip(list, "quicktextlist"), list.GetDragSample()) == DND_MOVE)
		list.RemoveSelection();
}

void QuickTextSetup::DnDInsert(int line, PasteClip& d)
{
	if(AcceptInternal<ArrayCtrl>(d, "quicktextlist")) {
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

void QuickTextSetup::Load(const Profile& p)
{
	list.Clear();
	for(const String& text : p.quicktext.texts)
		list.Add(text);
	list.SetCursor(0);
}

void QuickTextSetup::Store(Profile& p) const
{
	if(IsNull(p.name))
		return;
	for(int i = 0; i < list.GetCount(); i++)
		p.quicktext.texts.Add() = list.Get(i, 0);
}

}
