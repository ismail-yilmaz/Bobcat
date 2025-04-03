// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

struct QuickTextDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		if(auto ctx = GetContext(); ctx)
			if(Terminal *t = ctx->GetActiveTerminal(); t) {
				const auto& ti = q.To<QuickText::Item>();
				StdDisplay().Paint(w, r, AttrText(ti).SetFont(t->GetFont()), ink, paper, style);
			}
	}
};

struct QuickTextSetupListDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		const auto& ti = q.To<QuickText::Item>();
		StdDisplay().Paint(w, r, AttrText(ti), ink, paper, style);
	}
};

QuickText::QuickText(Terminal& t)
: term(t)
{
	plist.SetDisplay(Single<QuickTextDisplayCls>());
	plist.WhenSelect = [this] { OnSelect(); };
	plist.WhenCancel = [this] { OnCancel(); };
}

void QuickText::OnSelect()
{
	if(int i = plist.GetCursor(); i >= 0) {
		if(auto ti = plist.Get(i).To<QuickText::Item>(); !IsNull(ti.text)) {
			if(ti.IsCommand()) {
				ti.text = TrimBoth(ti.text);
				#ifdef PLATFORM_WIN32
				ti.text << '\r';
				#endif
				ti.text << '\n';
			}
			term.PutText(ti.text.ToWString());
		}
		term.SetFocus();
	}
}

void QuickText::OnCancel()
{
	term.SetFocus();
}

int QuickText::LoadText()
{
	int i = plist.GetCursor();
	plist.Clear();
	
	Profile p = LoadProfile(term.profilename);
	if(p.quicktext.texts.IsEmpty())
		return 0;
					
	Font f = term.GetFont();
	plist.SetLineCy(f.GetCy());
	
	int width = 0;
	for(const QuickText::Item& s : p.quicktext.texts) {
		if(IsNull(s.text))
			continue;
		plist.Add(RawToValue(s));
		int ix = Images::Prompt().GetSize().cx;
		int cx = GetTextSize(Nvl(s.alias, s.text), f).cx + ix;
		width = max(Zx(cx), width);
	}

	plist.SetCursor(i >= 0 && i < plist.GetCount() ? i : 0);
	
	return width;
}

void QuickText::Popup()
{
	if(int width = LoadText(); width > 0){
		Size sz = term.GetCellSize();
		Rect rp = term.GetScreenView() + term.GetCursorPoint();
		plist.PopUp(&term, rp.left, rp.top + sz.cy, rp.top + sz.cy, width);
	}
}

bool QuickText::Item::IsCommand() const
{
	return type == "command";
}

QuickText::Item::operator AttrText() const
{
	return AttrText(Nvl(alias, text))
			.SetImage(IsCommand() ? Images::Prompt() : Images::Text());
}

void QuickText::Item::Jsonize(JsonIO& jio)
{
	jio("Type", type)
	   ("Alias", alias)
	   ("Text", text);
}

String QuickText::Item::ToString() const
{
	return "Type: " << type << ", Alias: " << alias << ", String: " << text;
}

QuickText::Config::Config()
{
}

void QuickText::Config::Jsonize(JsonIO& jio)
{
	 texts.Jsonize(jio);
}

QuickTextSetup::QuickTextSetup()
{
	CtrlLayout(*this);
	CtrlLayoutOKCancel(dlg, t_("QuickText Editor"));
	
	AddFrame(toolbar);
	list.AddColumn(t_("Text")).SetDisplay(Single<QuickTextSetupListDisplayCls>());
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
	list.WhenDrag = THISFN(Drag);
	list.WhenLeftDouble = THISFN(Edit);
	list.WhenDropInsert = THISFN(DnDInsert);

	dlg.type.Add("command", t_("Command"));
	dlg.type.Add("text", t_("Text"));
	dlg.type.GoBegin();

	dlg.text.SetFont(Monospace());
	Sync();
}

void QuickTextSetup::Add()
{
	dlg.type.GoBegin();
	dlg.alias <<= dlg.text <<= Null;
	if(dlg.Sizeable().Zoomable().ExecuteOK()) {
		QuickText::Item ti;
		ti.type  = ~dlg.type;
		ti.alias = ~dlg.alias;
		ti.text  = ~dlg.text;
		list.Add(RawToValue(ti));
		list.GoEnd();
		Sync();
	}
}

void QuickTextSetup::Edit()
{
	int cursor = list.GetCursor();
	if(cursor < 0)
		return;
	
	auto ti = list.Get(cursor, 0).To<QuickText::Item>();

	CtrlRetriever cr;
	cr(dlg.type,  ti.type)
	  (dlg.alias, ti.alias)
	  (dlg.text,  ti.text).Set();

	if(dlg.Sizeable().Zoomable().ExecuteOK()) {
		cr.Retrieve();
		list.Set(cursor, 0, RawToValue(ti));
		Sync();
	}
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
	bar.Add(c, t_("Add text"), Images::Add(), [this]() { Add(); }).Key(K_INSERT);
	bar.Add(d, t_("Edit text"), Images::Edit(), [this]() { Edit(); }).Key(K_SPACE);
	bar.Add(d, t_("Remove text"), Images::Delete(), [this]() { list.DoRemove(); }).Key(K_DELETE);
	bar.Separator();
	bar.Add(list.GetCursor() > 0, t_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, t_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	bar.Separator();
	bar.Add(list.GetCount() > 0, t_("Select all"), Images::SelectAll(), [this]() { list.DoSelectAll(); }).Key(K_CTRL_A);
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
	for(const QuickText::Item& ti : p.quicktext.texts)
		if(!IsNull(ti.text))
			list.Add(RawToValue(ti));
	list.SetCursor(0);
}

void QuickTextSetup::Store(Profile& p) const
{
	if(IsNull(p.name))
		return;
	for(int i = 0; i < list.GetCount(); i++)
		p.quicktext.texts.Add() = list.Get(i, 0).To<QuickText::Item>();
}

}
