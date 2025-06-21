// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

#define KEYGROUPNAME "Navigator"
#define KEYNAMESPACE NavigatorKeys
#define KEYFILE <Bobcat/Navigator.key>
#include <CtrlLib/key_source.h>

using namespace NavigatorKeys;

Navigator::Item::Item()
: blinking(false)
, dnd(false)
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
		if(ctrl) {
			if(ctrl->IsRunning()) {
				if(dnd) {
					const Image& ico = Images::DropClipHD();
					w.DrawRect(q, SColorHighlight);
					w.DrawImage(q.CenterRect(ico.GetSize()), ico);
				}
				// Always overlay
				if(blinking && ctrl->IsRoot()) {
					const char *txt = t_("Privilege escalation!");
					Point pt = q.CenterPos(GetTextSize(txt, StdFont().Bold()));
					w.DrawText(pt.x, pt.y, txt, StdFont().Bold(), LtRed());
				}
			}
			else
			if(blinking) {
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
	dnd = false;
	Refresh();
}

void Navigator::Item::MouseMove(Point pt, dword keyflags)
{
	if(Rect r = GetCloseButtonRect(); r.Contains(pt)) {
		pos = pt;
		Refresh(r);
	}
	else
	if(!IsNull(pos)) {
		Refresh(r);
		pos = Null;
	}
}

void Navigator::Item::DragAndDrop(Point pt, PasteClip& d)
{
	if(ctrl) {
		ctrl->DragAndDrop(pt, d);
		dnd = d.IsAccepted();
		Refresh();
	}
}

void Navigator::Item::CancelMode()
{
	dnd = false;
	Refresh();
}

void Navigator::Item::DragLeave()
{
	CancelMode();
}

Rect Navigator::Item::GetCloseButtonRect()
{
	Rect r = GetView();
	return RectC(r.right - 20, r.top + 4, 16, 16);
}

Navigator::Navigator(Bobcat& ctx_)
: ctx(ctx_)
, swapanim(false)
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
	searchbar.search << [this] { Search();  };
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
	sb.WhenScroll = [this] { (void) SyncItemLayout(); Refresh(); };
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

void Navigator::Search()
{
	searchbar.search.Error(SyncItemLayout() == 0);
	Refresh();
}

bool Navigator::FilterItem(const Item& item)
{
	if(item.ctrl) {
		WString s = ~*item.ctrl;
		WString q = ~searchbar.search;
		return IsNull(q) || ToLower(s).Find(ToLower(q)) >= 0;
	}
	return true;
}

int Navigator::SyncItemLayout()
{
	for(auto& m : items)
		m.Hide();
	
	auto v = FilterRange(items, [=](const Item& item) { return FilterItem(item); });
	if(!v.GetCount())
		return v.GetCount();

	int cnt = max(v.GetCount(), 1);
	int fcy = GetStdFontSize().cy;

	Point margins = { 16, fcy * 2 };
	
	Size viewsize = GetSize() + Size(0, searchbar.GetHeight());
	Size cellsize = GetRatioSize(viewsize, 200, 0);
	
	Size gridsize;
	gridsize.cx = min(max(1, viewsize.cx / (cellsize.cx + margins.x)), min(4, cnt));
	gridsize.cy = (cnt + gridsize.cx - 1) / gridsize.cx;
	
	Size totalsize = gridsize * cellsize + (gridsize + 1) * margins;
	
	Point offset = max({ 8, 8 }, Rect(viewsize).CenterRect(totalsize).TopLeft());
	
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
	return v.GetCount();
}

void Navigator::Sync()
{
	if(!IsShown())
		return;
	
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
			if(!m.ctrl->IsRunning() || m.ctrl->IsRoot())
				blinking++;
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
			KillSetTimeCallback(-500, [this] { Animate(); }, Navigator::TIMEID_BLINK);

		Refresh();
	};

	String k = " (" + GetKeyDesc(NavigatorKeys::AK_NAVSEARCH().key[0]) + ") ";
	searchbar.search.NullText(t_("Search terminal") + k);

	SetTimeCallback(100, ScheduledSync, TIMEID_SYNC);
}

void Navigator::Animate()
{
	for(Item& m : items)
		if(m.ctrl && (!m.ctrl->IsRunning() || m.ctrl->IsRoot())) {
			m.blinking ^= 1;
			m.Refresh();
		}
}

void Navigator::Layout()
{
	if(IsVisible())
		Sync();
}

int Navigator::GetCursor()
{
	return FindMatch(items, [](const Item& m) { return m.HasFocus() ; });
}

void Navigator::SwapItem(int i, int ii)
{
	ctx.stack.Swap(*items[i].ctrl, *items[ii].ctrl);
}

void Navigator::SwapFirst()
{
	if(int i = GetCursor(); i > 0)
		SwapItem(i, 0);
}

void Navigator::SwapLast()
{
	if(int i = GetCursor(); i >= 0 && i < items.GetCount() - 1)
		SwapItem(i, items.GetCount() - 1);
}

void Navigator::SwapPrev()
{
	if(int i = GetCursor(); i > 0)
		SwapItem(i, i - 1);
}

void Navigator::SwapNext()
{
	if(int i = GetCursor(); i >= 0 && i + 1 < items.GetCount())
		SwapItem(i, i + 1);
}

void Navigator::AnimateSwap(int i, int ii)
{
	if(!IsShown() || swapanim)
		return;
	
	swapanim = true;
	
	Item& a = items[i];
	Item& b = items[ii];
	
	Rect ra = a.GetRect();
	Rect rb = b.GetRect();
	
	constexpr const int duration = 100;
	
	for(int start = msecs();;) {
		int elapsed = msecs(start);
		if(elapsed > duration) {
			a.SetRect(ra);
			b.SetRect(rb);
			break;
		}
		Rect r1 = ra, r2 = rb;
		r1 += (rb - ra) * elapsed / duration;
		r2 += (ra - rb) * elapsed / duration;
		a.SetRect(r1);
		b.SetRect(r2);
#ifdef PLATFORM_POSIX
		a.Sync();
		b.Sync();
#else
		a.Refresh();
		b.Refresh();
#endif
		Ctrl::ProcessEvents();
		GuiSleep(0);
	}
	
	Swap(a.ctrl, b.ctrl);
	Swap(a.img, b.img);

	b.SetFocus();
	Refresh();

	swapanim = false;
}

void Navigator::Paint(Draw& w)
{
	Size sz = GetSize(), fsz = GetStdFontSize();
	w.Clip(sz);
	for(const Item& m : items) {
		if(m.ctrl && m.IsVisible() && !swapanim) {
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
	if(key == K_ESCAPE) {
		WhenClose();
	}
	else
	if(key == K_RETURN) {
		if(Ctrl *c = GetFocusCtrl(); c)
			c->Action();
	}
	else
	if(Match(AK_SWAPFIRST, key)) {
		SwapFirst();
	}
	else
	if(Match(AK_SWAPLAST, key)) {
		SwapLast();
	}
	else
	if(Match(AK_SWAPPREV, key)) {
		SwapPrev();
	}
	else
	if(Match(AK_SWAPNEXT, key)) {
		SwapNext();
	}
	else
	if(Match(AK_NAVSEARCH, key)) {
		searchbar.search.SetFocus();
	}
	else
	if(!MenuBar::Scan([this](Bar& menu) { WhenBar(menu); }, key))
		return false;
		
	return true;
}


}
