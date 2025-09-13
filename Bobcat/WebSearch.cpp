// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

#define KEYGROUPNAME TERMINALCTRL_KEYGROUPNAME
#define KEYNAMESPACE TerminalCtrlKeys
#define KEYFILE <Bobcat/Terminal.key>
#include <CtrlLib/key_source.h>

using namespace TerminalCtrlKeys;

struct DefaultSearchProviderDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		const auto& ti = q.To<WebSearch::Provider>();
		StdDisplay().Paint(w, r, AttrText(ti).Bold(true), ink, paper, style);
	}
};

struct NormalSearchProviderDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		const auto& ti = q.To<WebSearch::Provider>();
		StdDisplay().Paint(w, r, AttrText(ti).Bold(false), ink, paper, style);
	}
};


const Display& DefaultSearchProviderDisplay()   { return Single<DefaultSearchProviderDisplayCls>();  }
const Display& NormalSearchProviderDisplay()    { return Single<NormalSearchProviderDisplayCls>();  }

WebSearch::WebSearch(Terminal& t)
: term(t)
{
}

void WebSearch::SetConfig(const Profile& p)
{
	// Acquire the providers only once.
	if(auto& m = GetWebSearchProviders(); !IsNull(p.name) && m.Find(p.name) < 0) {
		m.Add(p.name).Append(p.websearch.providers);
	}
}

void WebSearch::ContextMenu(Bar& menu)
{
	auto& m = GetWebSearchProviders();

	if(int i = m.Find(term.profilename); i >= 0 && m[i].GetCount() > 0) {
		bool b = term.IsSelection();
		String txt = term.GetSelectedText().ToString();
		menu.Sub(b, t_("Search on web..."), Images::FindWeb(), [&, i, txt = pick(txt)](Bar& menu) {
			for(int j = 0; j < m[i].GetCount(); j++) {
				const WebSearch::Provider& wsp = m[i][j];
				Bar::Item& q = menu.Add(wsp.name, Images::Provider(), [&, txt] {
					String uri = wsp.uri;
					uri.Replace("%s", txt);
					LaunchWebBrowser(uri);
				});
				if(j == 0)
					q.Key(AK_WEBSEARCH);
			}
		});
		if(menu.IsPopUp())
			menu.Separator();
	}
}

WebSearch::Provider::operator AttrText() const
{
	return AttrText(Nvl(name, uri)).SetImage(Images::Provider());
}

void WebSearch::Provider::Jsonize(JsonIO& jio)
{
	jio("Name", name)("URI", uri);
}

String WebSearch::Provider::ToString() const
{
	return "Name: " << name << ", URI: " << uri;
}

WebSearch::Config::Config()
{
}

void WebSearch::Config::Jsonize(JsonIO& jio)
{
	providers.Jsonize(jio);
}

VectorMap<String, Vector<WebSearch::Provider>>& GetWebSearchProviders()
{
	return Single<VectorMap<String, Vector<WebSearch::Provider>>>();
}

WebSearchSetup::WebSearchSetup()
{
	CtrlLayout(*this);
	CtrlLayoutOKCancel(dlg, t_("Web search engine setup"));
	
	AddFrame(toolbar);
	list.AddColumn(t_("Text"));
	list.WhenBar = THISFN(ContextMenu);
	list.WhenSel = THISFN(Sync);
	list.WhenDrag = THISFN(Drag);
	list.WhenLeftDouble = THISFN(Edit);
	list.WhenDropInsert = THISFN(DnDInsert);
	Sync();
}

void WebSearchSetup::Add()
{
	dlg.alias <<= dlg.uri <<= Null;
	if(dlg.Sizeable().Zoomable().ExecuteOK()) {
		WebSearch::Provider wsp;
		wsp.name = ~dlg.alias;
		wsp.uri  = ~dlg.uri;
		list.Add(RawToValue(wsp));
		list.GoEnd();
		Sync();
	}
}

void WebSearchSetup::Edit()
{
	int cursor = list.GetCursor();
	if(cursor < 0)
		return;
	
	auto wsp = list.Get(cursor, 0).To<WebSearch::Provider>();

	CtrlRetriever cr;
	cr(dlg.alias, wsp.name)
	  (dlg.uri,  wsp.uri).Set();

	if(dlg.Sizeable().Zoomable().ExecuteOK()) {
		cr.Retrieve();
		list.Set(cursor, 0, RawToValue(wsp));
		Sync();
	}
}

void WebSearchSetup::Sync()
{
	toolbar.Set(THISFN(ContextMenu));
	for(int i = 0; i < list.GetCount(); i++) {
		list.SetDisplay(i, 0, i == 0 ? DefaultSearchProviderDisplay()
								     : NormalSearchProviderDisplay());
	}
	Action();
}

void WebSearchSetup::ContextMenu(Bar& bar)
{
	bool e = list.IsEditable();
	bool c = !list.IsEdit() && e;
	bool d = c && list.IsCursor();
	bool q = list.GetCursor() >= 0 && list.GetCursor() < list.GetCount() - 1;

	bool b = list.IsCursor();
	bar.Add(c, t_("Add provider"), Images::Add(), [this]() { Add(); }).Key(K_INSERT);
	bar.Add(d, t_("Edit provider"), Images::Edit(), [this]() { Edit(); }).Key(K_SPACE);
	bar.Add(d, t_("Remove provider"), Images::Delete(), [this]() { list.DoRemove(); }).Key(K_DELETE);
	bar.Separator();
	bar.Add(list.GetCursor() > 0, t_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, t_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	bar.Separator();
	bar.Add(list.GetCount() > 0, t_("Select all"), Images::SelectAll(), [this]() { list.DoSelectAll(); }).Key(K_CTRL_A);

}

void WebSearchSetup::Drag()
{
	if(list.DoDragAndDrop(InternalClip(list, "websearchproviderlist"), list.GetDragSample()) == DND_MOVE)
		list.RemoveSelection();
}

void WebSearchSetup::DnDInsert(int line, PasteClip& d)
{
	if(AcceptInternal<ArrayCtrl>(d, "websearchproviderlist")) {
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

void WebSearchSetup::Load(const Profile& p)
{
	list.Clear();
	for(const WebSearch::Provider& wsp : p.websearch.providers)
		if(!IsNull(wsp.uri))
			list.Add(RawToValue(wsp));
	list.SetCursor(0);
}

void WebSearchSetup::Store(Profile& p) const
{
	if(IsNull(p.name))
		return;
	for(int i = 0; i < list.GetCount(); i++)
		p.websearch.providers.Add() = list.Get(i, 0).To<WebSearch::Provider>();
}


	
}
