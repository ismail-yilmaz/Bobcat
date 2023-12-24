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

Navigator::Navigator(Bobcat& ctx)
: ctx(ctx)
, cursor(-1)
, columns(4)
{
	Hide();
	ctx.window.Add(SizePos());
	AddFrame(sb);
	sb.AutoHide();
	sb.WhenScroll = [=] { Refresh(); };
	lastsize = GetSize();
}

Navigator::~Navigator()
{
	KillTimeCallback(TIMEID_SYNC);
}

Navigator& Navigator::Show(bool ok)
{
	if(!ok) {
		Ctrl::Hide();
		shots.Clear();
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

void Navigator::Sync()
{
	auto ScheduledSync = [this] {
		int cnt = ctx.stack.GetCount();
		if(!cnt || !IsVisible())
			return;
	
		Size wsz = ctx.stack[0].GetSize();
		Size tsz = ScaleDown(wsz, 1.0 / max(1, columns));
		Size fsz = GetStdFontSize();
		int  mcx = max(1, wsz.cx / max(tsz.cx, 1));
		shots.SetCount(cnt);
		for(int i = 0, row = 0, col = 0; i < cnt; i++) {
			Terminal& t = static_cast<Terminal&>(ctx.stack[i]);
			if(&ctx.stack[i] == ctx.stack.GetActiveCtrl())
				cursor = i;
			int x = col * tsz.cx;
			int y = row * (tsz.cy + fsz.cy);
			Snapshot& w = shots[i];
			w.name  = t.GetTitle();
			w.irect = RectC(x, y, tsz.cx, tsz.cy).Deflated(4);
			w.nrect = w.irect;
			w.nrect.top = w.irect.bottom;
			w.nrect.bottom = w.irect.bottom + fsz.cy;
			ImageDraw iw(wsz);
			t.PaintPage(iw);
			w.img = CachedRescale(iw, tsz);
			if(++col == mcx) {
				col = 0;
				row++;
			}
		}

		sb.Set(0, wsz.cy, (tsz.cy + fsz.cy) * (cnt / columns + (cnt % columns != 0)));
		Refresh();
	};

	SetTimeCallback(20, ScheduledSync, TIMEID_SYNC);
}

void Navigator::Layout()
{
	Sync();
}

void Navigator::Paint(Draw& w)
{
	w.DrawRect(GetSize(), SColorPaper);
	int n = ctx.stack.GetCursor();
	for(int i = 0, y = -sb; i < shots.GetCount(); i++) {
		const Snapshot& o = shots[i];
		Rect r = o.irect.OffsetedVert(y);
		Color tc = SColorText, fc1, fc2;
		if(r.Contains(mouse) || cursor == i) {
			fc1 = Color(150, 150, 150);
			fc2 = Color(130, 130, 130);
		}
		else{
			if(n == i) {
				fc1 = SLtRed;
				fc2 = SRed;
				tc  = SRed;
			}
			else {
				fc1 = Color(50, 50, 50);
				fc2 = Color(30, 30, 30);
			}
		}
		w.DrawImage(r, o.img);
		StdCenterDisplay().Paint(w, o.nrect.OffsetedVert(y), o.name, tc, SColorPaper, 0);
		DrawFrame(w, r.Inflated(1), fc1);
		DrawFrame(w, r.Inflated(2), fc2);
	}
}

void Navigator::LeftDown(Point pt, dword keyflags)
{
	for(int i = 0; i < shots.GetCount(); i++) {
		const Rect& r = shots[i].irect;
		if(r.Contains(pt)) {
			ctx.ToggleNavigator();
			ctx.stack.Goto(i);
			break;
		}
	}
}

void Navigator::RightDown(Point pt, dword keyflags)
{
	MenuBar::Execute([this](Bar& menu) { ContextMenu(menu); });
}

void Navigator::MouseMove(Point pt, dword keyflags)
{
	mouse = pt;
	Refresh();
}

void Navigator::MouseWheel(Point pt, int zdelta, dword keyflags)
{
	sb.Wheel(zdelta, 16);
}

bool Navigator::Key(dword key, int count)
{
	int n = shots.GetCount();

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
		ctx.ToggleNavigator();
		break;
	case K_RETURN:
		if(cursor >= 0) {
			ctx.ToggleNavigator();
			ctx.stack.Goto(cursor);
		}
		break;
	default:
		if(MenuBar::Scan([this](Bar& menu) { ContextMenu(menu); }, key))
			return true;
		return Ctrl::Key(key, count);
	}

	Refresh();
	return true;
}

void Navigator::ContextMenu(Bar& menu)
{
	ctx.TermMenu(menu);
}

}