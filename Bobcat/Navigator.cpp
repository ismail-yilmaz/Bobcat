// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

Size ScaleDown(Size r, double factor)
{
	return Size(double(r.cx), double(r.cy)) * min(max(factor, 0.0), 1.0);
}

Navigator::Item::Item()
{
	WhenAction = [this] { if(ctrl) WhenItem(*(ctrl)); };
}

void Navigator::Item::GotFocus()
{
	Refresh();
}

void Navigator::Item::LostFocus()
{
	Refresh();
}

void Navigator::Item::Paint(Draw& w)
{
	if(Rect q = GetView(); q.Width() - 16 >= 1 && q.GetHeight() - 16 >= 1) {
		Rect r = GetCloseButtonRect();
		w.Clip(q);
		w.DrawRect(q, SColorPaper);
		w.DrawImage(q.Deflated(8), img);
		w.DrawImage(r, r.Contains(pos) ? Images::DeleteHL2() : Images::DeleteHL());
		Color c = HasMouse() ? SColorText : HasFocus() ? SColorHighlight : Color(30, 30, 30);
		DrawFrame(w, q.Deflated(2), Color(50, 50, 50));
		DrawFrame(w, q.Deflated(1), c);
		w.End();
	}
}

void Navigator::Item::LeftUp(Point pt, dword keyflags)
{
	if(GetCloseButtonRect().Contains(pt) && ctrl)
		WhenClose(*ctrl);
	else
		Action();
}

void Navigator::Item::MouseEnter(Point pt, dword keyflags)
{
	pos = pt;
	Refresh();
}

void Navigator::Item::MouseLeave()
{
	pos = Null;
	Refresh();
}

void Navigator::Item::MouseMove(Point pt, dword keyflags)
{
	Rect r = GetCloseButtonRect();
	if(r.Contains(pt)) {
		pos = pt;
		Refresh(r);
	}
	else
	if(!IsNull(pos)) {
		Refresh(r);
		pos = Null;
	}
}

Rect Navigator::Item::GetCloseButtonRect()
{
	Rect r = GetView();
	return RectC(r.right - 20, r.top + 4, 16, 16);
}

Navigator::Navigator(Bobcat& ctx_)
: ctx(ctx_)
, columns(4)
{
	Hide();
	AddFrame(sb);
	sb.AutoHide();
	sb.WhenScroll = [this] { SyncItemLayout(); Refresh(); };
	CtrlLayout(searchbar);
	icon.SetDisplay(StdCenterDisplay());
	icon <<= Images::Find();
	searchbar.search.AddFrame(icon);
	searchbar.search.WantFocus();
	AddFrame(searchbar.Height(Zy(22)));
	searchbar.newterm.Image(Images::Add());
	searchbar.newterm << [this] { ctx.NewTerminalFromActiveProfile(); };
	searchbar.search.NullText(t_("Search terminal (Ctrl+S)..."));
	searchbar.search << [this] { SyncItemLayout(); Refresh();  };
	searchbar.close.Image(Images::Delete()).Tip(t_("Close navigator"));
	searchbar.close  << [this] { WhenClose(); };
}

Navigator::~Navigator()
{
	KillTimeCallback(TIMEID_SYNC);
}

Navigator& Navigator::Show(bool ok)
{
	if(!ok) {
		Ctrl::Hide();
		items.Clear();
		searchbar.search.Clear();
		return *this;
	}

	Ctrl::Show();
	Sync();
	return *this;
}

Navigator& Navigator::Hide()
{
	return Show(false);
}

bool Navigator::FilterItem(const Item& item)
{
	if(item.ctrl) {
		WString s = ~*item.ctrl;
		return s.Find((const WString&) ~searchbar.search) >= 0;
	}
	return true;
}

void Navigator::SyncItemLayout()
{
	int cnt = ctx.stack.GetCount();
	if(!cnt)
		return;
	int  tcy = searchbar.GetHeight();
	Size wsz = GetSize() + Size(0, tcy);
	Size tsz = ScaleDown(wsz, 1.0 / max(1, columns));
	for(auto& m : items)
		m.Hide();
	int row = 0, col = 0, fcy = GetStdFontSize().cy;
	auto v = FilterRange(items, [=](const Item& item) { return FilterItem(item); });
	cnt = max(v.GetCount(), 1);
	for(Item& m : v) {
		Rect r = RectC(col * tsz.cx, row * (tsz.cy + fcy), tsz.cx, tsz.cy);
		m.SetRect(r.Deflated(4).OffsetedVert(-sb));
		m.Show();
		if(++col == columns) {
			col = 0;
			row++;
		}
	}
	sb.Set(sb.Get(), wsz.cy - tcy, (tsz.cy + fcy) * (cnt / columns + (cnt % columns != 0)));
}

void Navigator::Sync()
{
	auto ScheduledSync = [this]
	{
		int cnt = ctx.stack.GetCount();
		if(!cnt || !IsVisible())
			return;
		items.SetCount(cnt);
		SyncItemLayout();
		Size fsz = GetStdFontSize();
		for(int i = 0; i < cnt; i++) {
			Item& m = items[i];
			m.ctrl  = &ctx.stack[i];
			Size csz = max(Size(1, 1), m.ctrl->GetSize());
			Size isz = max(Size(1, 1), m.GetSize());
			ImageDraw w(csz);
			m.ctrl->Paint(w);
			m.img = Rescale(w, max(1, isz.cx - 4), max(1, isz.cy - fsz.cy));
			if(!m.IsChild())
				Add(m);
			m.WantFocus();
			if(ctx.stack.GetActiveCtrl() == m.ctrl)
				m.SetFocus();
			m.WhenItem = WhenGotoItem;
			m.WhenClose = WhenRemoveItem;
			m.Update();
		}
		Refresh();
	};

	SetTimeCallback(100, ScheduledSync, TIMEID_SYNC);
}

void Navigator::Layout()
{
	if(IsVisible())
		Sync();
}

void Navigator::Paint(Draw& w)
{
	Size sz = GetSize(), fsz = GetStdFontSize();
	w.Clip(sz);
	w.DrawRect(GetSize(), SColorPaper);
	for(const Item& m : items) {
		if(m.ctrl && m.IsVisible()) {
			Rect r = m.GetRect();
			r = Rect(r.left, r.bottom, r.right, r.bottom + fsz.cy);
			StdCenterDisplay().Paint(w, r, ~*m.ctrl, SColorText, SColorPaper, 0);
		}
	}
	w.End();
}

void Navigator::RightDown(Point pt, dword keyflags)
{
	MenuBar::Execute([this](Bar& menu) { WhenBar(menu); });
}

void Navigator::MouseWheel(Point pt, int zdelta, dword keyflags)
{
	sb.Wheel(zdelta, 16);
}

bool Navigator::Key(dword key, int count)
{
	switch(key) {
	case K_ESCAPE:
		WhenClose();
		return true;
	case K_RETURN:
		if(Ctrl *c = GetFocusCtrl(); c)
			c->Action();
		return true;
	case K_CTRL_S:
		searchbar.search.SetFocus();
		return true;
	default:
		if(MenuBar::Scan([this](Bar& menu) { WhenBar(menu); }, key))
			return true;
	}
	return Ctrl::Key(key, count);
}


}