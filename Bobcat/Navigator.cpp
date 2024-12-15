// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

Navigator::Item::Item()
: blinking(false)
{
	WhenAction = [this] { if(ctrl) WhenItem(*(ctrl)); };
}

void Navigator::Item::GotFocus()
{
	WhenFocus(GetRect());
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
		w.DrawRect(q, SColorFace);
		w.DrawImage(q.Deflated(8), img);
		w.DrawImage(r, r.Contains(pos) ? Images::DeleteHL2() : Images::DeleteHL());
		if(ctrl && !ctrl->IsRunning() && blinking) {
			Image ico;
			if(ctrl->IsFailure()) {
				if(ctrl->IsAsking())
					ico = Images::Exclamation();
				else
					ico = Images::Error();
			}
			else
			if(ctrl->IsAsking())
				ico = Images::Question();
			else
			if(ctrl->IsSuccess())
				ico = Images::OK();
			w.DrawImage(q.CenterRect(Size(24, 24)), ico);
		}
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
{
	Hide();
	CtrlLayout(searchbar);
	icon.SetDisplay(StdCenterDisplay());
	icon <<= Images::Find();
	searchbar.search.AddFrame(icon);
	searchbar.search.WantFocus();
	AddFrame(searchbar.Height(Zy(22)));
	searchbar.newterm.Image(Images::Add());
	searchbar.newterm.Tip(t_("Open new terminal"));
	searchbar.newterm << [this] { ctx.NewTerminalFromActiveProfile(); };
	searchbar.search.NullText(t_("Search terminal (Ctrl+S)..."));
	searchbar.search << [this] { SyncItemLayout(); Refresh();  };
	searchbar.close.Image(Images::Delete()).Tip(t_("Close navigator"));
	searchbar.close  << [this] { WhenClose(); };
	searchbar.profiles.Image(CtrlImg::down_arrow());
	searchbar.profiles.Tip(t_("Open new terminal from..."));
	searchbar.profiles << [this] {
		Vector<String> pnames = GetProfileNames();
		if(pnames.GetCount())
		    MenuBar::Execute([this, &pnames](Bar& bar) {
				ctx.TermSubmenu(bar, pnames);
			});
	};
	AddFrame(sb);
	sb.AutoHide();
	sb.WhenScroll = [this] { SyncItemLayout(); Refresh(); };
}

Navigator::~Navigator()
{
	KillTimeCallback(TIMEID_SYNC);
	KillTimeCallback(TIMEID_BLINK);
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
    for(auto& m : items)
        m.Hide();
    
    auto v = FilterRange(items, [=](const Item& item) { return FilterItem(item); });
 
    int cnt = max(v.GetCount(), 1);
    int fcy = GetStdFontSize().cy;

    Point margins = { 16, fcy * 2};
    
    Size viewsize = GetSize() + Size(0, searchbar.GetHeight());
    Size cellsize = GetRatioSize(viewsize, 200, 0);
    
    Size gridsize;
    gridsize.cx = min(max(1, viewsize.cx / (cellsize.cx + margins.x)), min(4, cnt));
    gridsize.cy = (cnt + gridsize.cx - 1) / gridsize.cx;
    
	Size totalsize = gridsize * cellsize + (gridsize + 1) * margins;
       
    Point offset = max({0, 8}, Rect(viewsize).CenterRect(totalsize).TopLeft());
    
    sb.SetTotal(totalsize.cy);
    sb.SetPage(viewsize.cy);
    
    int row = 0, col = 0;
    for(Item& m : v) {
        Rect r(offset + Point(col, row) * (cellsize + margins), cellsize);
        m.SetRect(r.OffsetedVert(-sb));
        m.Show();
        if(++col >= gridsize.cx) {
            col = 0;
            row++;
        }
    }
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
		int blinking = 0;
		for(int i = 0; i < cnt; i++) {
			Item& m = items[i];
			m.ctrl  = &AsTerminal(ctx.stack[i]);
			Size csz = max(Size(1, 1), m.ctrl->GetSize());
			Size isz = max(Size(1, 1), m.GetSize());
			ImageDraw w(csz);
			m.ctrl->Paint(w);
			m.img = Rescale(w, max(1, isz.cx - 4), max(1, isz.cy - fsz.cy));
			if(!m.ctrl->IsRunning()) {
				blinking++;
			}
			if(!m.IsChild())
				Add(m);
			m.WantFocus();
			if(ctx.stack.GetActiveCtrl() == m.ctrl)
				m.SetFocus();
			m.WhenItem = WhenGotoItem;
			m.WhenClose = WhenRemoveItem;
			m.WhenFocus = [&](Rect r) {
				Rect rr = GetRect();
				if(r.top < rr.top)
					sb.Set(r.TopLeft().y);
				else
				if(r.bottom > rr.bottom)
					sb.Set(r.BottomLeft().y);
			};
			m.Update();
		}
		if(blinking)
			KillSetTimeCallback(-500, [this]
			{
				for(Item& m : items) {
					if(m.ctrl && !m.ctrl->IsRunning()) {
						m.blinking ^= 1;
						m.Refresh();
					}
				}
		
			}, Navigator::TIMEID_BLINK);

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
	for(const Item& m : items) {
		if(m.ctrl && m.IsVisible()) {
			Rect r = m.GetRect();
			r = Rect(r.left, r.bottom, r.right, r.bottom + fsz.cy);
			StdCenterDisplay().Paint(w, r, ~*m.ctrl, SColorText, SColorFace, 0);
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