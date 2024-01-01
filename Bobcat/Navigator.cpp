// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

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

void Navigator::Item::Paint(Draw& w)
{
	Rect r = GetCloseButtonRect(), q = GetView();
	w.DrawRect(q, SColorPaper);
	w.DrawImage(q.Deflated(8), img);
	w.DrawImage(r, r.Contains(pos) ? Images::DeleteHL() : Images::Delete());
	Color c = HasMouse() ? SColorText : HasFocus() ? SColorHighlight : Color(30, 30, 30);
	DrawFrame(w, q.Deflated(2), Color(50, 50, 50));
	DrawFrame(w, q.Deflated(1), c);
}

void Navigator::Item::LeftDown(Point pt, dword keyflags)
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

Navigator::Navigator(StackCtrl& sctrl)
: stack(sctrl)
, cursor(-1)
, columns(4)
{
	Hide();
	AddFrame(sb);
	sb.AutoHide();
	sb.WhenScroll = [=] { Geometry(); Refresh(); };
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

void Navigator::Geometry()
{
	int cnt = stack.GetCount();
	if(!cnt)
		return;
	Size wsz = GetSize();
	Size tsz = ScaleDown(wsz, 1.0 / max(1, columns));
	Size fsz = GetStdFontSize();
	for(int i = 0, row = 0, col = 0; i < items.GetCount(); i++) {
		Item& m = items[i];
		Rect r = RectC(col * tsz.cx, row * (tsz.cy + fsz.cy), tsz.cx, tsz.cy);
		m.SetRect(r.Deflated(4).OffsetedVert(-sb));
		if(++col == columns) {
			col = 0;
			row++;
		}
	}
	sb.Set(sb.Get(), wsz.cy, (tsz.cy + fsz.cy) * (cnt / columns + (cnt % columns != 0)));
}

void Navigator::Sync()
{
	auto ScheduledSync = [this]
	{
		int cnt = stack.GetCount();
		if(!cnt || !IsVisible())
			return;
		items.SetCount(cnt);
		Geometry();
		Size fsz = GetStdFontSize();
		for(int i = 0; i < cnt; i++) {
			Item& m = items[i];
			m.ctrl  = &stack[i];
			ImageDraw w(m.ctrl->GetSize());
			m.ctrl->Paint(w);
			Size sz = m.GetSize();
			m.img = Upp::Rescale(w, sz.cx - 4, sz.cy - fsz.cy);
			if(!m.IsChild())
				Add(m);
			if(stack.GetActiveCtrl() == m.ctrl) {
				m.SetFocus();
				cursor = i;
			}
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
		Rect r = m.GetRect();
		r = Rect(r.left, r.bottom, r.right, r.bottom + fsz.cy);
		if(m.ctrl)
			StdCenterDisplay().Paint(w, r, ~*m.ctrl, SColorText, SColorPaper, 0);
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
	int n = items.GetCount();

	switch(key) {
	case K_TAB:
		cursor = ++cursor % n;
		break;
	case K_LEFT:
		cursor = clamp(--cursor, 0, n - 1);
		break;
	case K_RIGHT:
		cursor = clamp(++cursor, 0, n - 1);
		break;
	case K_UP:
		cursor = cursor - columns < 0 ? cursor : cursor - columns;
		break;
	case K_DOWN:
		cursor = cursor + columns >= n ? cursor : cursor + columns;
		break;
	case K_ESCAPE:
		WhenClose();
		return true;
	case K_RETURN:
		if(cursor >= 0 && cursor < n) {
			items[cursor].Action();
			return true;
		}
		break;
	default:
		if(MenuBar::Scan([this](Bar& menu) { WhenBar(menu); }, key))
			return true;
		return Ctrl::Key(key, count);
	}

	if(cursor >= 0 && cursor < n) {
		items[cursor].SetFocus();
		items[cursor].Update();
	}
	Refresh();
	return true;
}


}