// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

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
	CtrlLayout(*this);
	WhenAction = [this] { if(ctrl) WhenItem(*(ctrl)); };
	close.Image(Images::Delete());
	close << [this] { WhenClose(*ctrl); };
	close.Tip(t_("Close terminal"));
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
		w.Clip(q);
		w.DrawRect(q, SColorFace);
		w.DrawImage(q.Deflated(8), img);
		if(ctrl) {
			if(ctrl->IsRunning()) {
				if(dnd) {
					const Image& ico = Images::DropClipHD();
					w.DrawRect(q, SColorHighlight);
					Size sz = min(ico.GetSize(), q.GetSize());
					w.DrawImage(q.CenterRect(sz), ico);
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
						ico = Images::ExclamationHD();
					else
						ico = Images::ErrorHD();
				}
				else
				if(ctrl->IsAsking())
					ico = Images::QuestionHD();
				else
				if(ctrl->IsSuccess())
					ico = Images::OkHD();
				Size sz = min(ico.GetSize(), q.GetSize());
				w.DrawImage(q.CenterRect(sz), ico);
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

Navigator::Navigator(Bobcat& ctx_)
: ctx(ctx_)
, swapanim(false)
{
	Hide();
	SetFrame(NullFrame());
	CtrlLayout(bar);
	icon.SetDisplay(StdCenterDisplay());
	status.SetDisplay(StdRightDisplay());
	icon <<= Images::Find();
	bar.search.AddFrame(icon);
	bar.search.AddFrame(status);
	bar.search.WantFocus();
	bar.newterm.Image(Images::Add());
	bar.newterm.Tip(t_("Open new terminal"));
	bar.close.Image(Images::Delete()).Tip(t_("Close navigator"));
	bar.profiles.Image(Images::ChevronDown());
	bar.profiles.Tip(t_("Open new terminal from..."));
	bar.newterm << [this] { ctx.NewTerminalFromActiveProfile(); };
	bar.profiles << [this] { OpenProfileMenu(ctx); };
	bar.search << [this] { Search();  };
	bar.close << [this] { WhenClose(); };
	AddFrame(bar.Height(GetStdBarHeight()));
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
		bar.search.Clear();
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
	int n = SyncItemLayout();
	SetSearchResults(n);
	bar.search.Error(!n);
	Refresh();
}

void Navigator::SetSearchResults(int n)
{
	String txt;
	int total = ctx.stack.GetCount();
	if(!IsNull(~bar.search) && n > 0)
		txt << n << "/" << total;
	SetSearchStatusText(status, txt);
}

bool Navigator::FilterItem(const Item& item)
{
	if(item.ctrl) {
		WString q = ~bar.search;
		WString s = ~*item.ctrl;
		return IsNull(q) || ToLower(s).Find(ToLower(q)) >= 0;
	}
	return true;
}

int Navigator::SyncItemLayout()
{
	for(auto& m : items)
		m.Hide();

	constexpr const int mincsz = 200;
	constexpr const int maxcol = 4;

	auto v = FilterRange(items, [this](const Item& item) { return FilterItem(item); });
	int cnt = v.GetCount();
	if(!cnt)
		return 0;

	Point margins = { 16, GetStdFontSize().cy * 2 };
	Size viewsize = GetSize() + Size(0, bar.GetHeight());
	Size cellsize = GetRatioSize(viewsize, mincsz, 0);

	Size gridsize;
	gridsize.cx = min(max(1, viewsize.cx / (cellsize.cx + margins.x)), min(maxcol, cnt));
	gridsize.cy = (cnt + gridsize.cx - 1) / gridsize.cx;

	Size totalsize = gridsize * cellsize + (gridsize + 1) * margins;
	Point offset = max({ 8, 8 }, Rect(viewsize).CenterRect(totalsize).TopLeft());
	offset.x += AddFrameSize({0, 0}).cx;

	sb.SetTotal(totalsize.cy);
	sb.SetPage(viewsize.cy);

	int row = 0, col = 0;
	for(Item& m : v) {
		m.SetRect(Rect(offset + Point(col, row) * (cellsize + margins), m.AddFrameSize(cellsize)).OffsetedVert(-sb));
		m.Show();
		if(++col >= gridsize.cx) {
			col = 0;
			row++;
		}
	}
	return cnt;
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
		if(SyncItemLayout() == 0)
			return;
		Size fsz = GetStdFontSize();
		int blinking = 0;
		for(int i = 0; i < cnt; i++) {
			Item& m = items[i];
			m.ctrl = &AsTerminal(ctx.stack[i]);
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
			if(ctx.stack.GetActiveCtrl() == m.ctrl && !bar.search.HasFocus())
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
	bar.search.NullText(t_("Search terminal") + k);
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
	auto *t = static_cast<const Terminal*>(ctx.stack.GetActiveCtrl());
	int count = 0, total = items.GetCount();
	AttrText txt;
	for(const Item& m : items) {
		if(m.ctrl && m.IsVisible() && !swapanim) {
			Rect r = m.GetRect();
			r = Rect(r.left, r.bottom, r.right, r.bottom + fsz.cy);
			bool highlight = m.ctrl == t && total != 1;
			txt.Text(m.ctrl->GetTitle()).Bold(highlight);
			TerminalTitleDisplay().Paint(w, r, txt, SColorText, SColorFace, 0);
			count++;
		}
	}
	if(items.GetCount() && !count && !swapanim) {
		const String s = t_("No matches found.");
		Size sz = GetTextSize(s, StdFont().Bold());
		Rect r =  GetRect().CenterRect(sz);
		w.DrawText(r.left, r.top, s, StdFont().Bold(), SRed);
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
		bar.search.SetFocus();
	}
	else
	if(Match(AK_NAVALIAS, key)) {
		if(int i = GetCursor(); i >= 0)
			items[i].ctrl->SetAlias();
	}
	else
	if(key < K_CHAR_LIM && key != K_TAB) {
		bar.search.SetFocus();
		return !bar.search.Key(key, count);
	}

	return MenuBar::Scan([this](Bar& menu) { WhenBar(menu); }, key);
}

}
