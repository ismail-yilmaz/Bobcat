// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

struct TabInfo : Moveable<TabInfo> {
	Ptr<Terminal> term;
	bool focus:1;
	bool hover:1;
	bool dnd:1;
	bool msg:1;
	int  state;
	TabInfo() : focus(false), hover(false), dnd(false), msg(false), state(CTRL_NORMAL) {}
};

TabBar::TabBar(Bobcat& ctx_)
: ctx(ctx_)
, pos(-1, -1)
, cursor(-1)
, tabcy(GetStdFontSize().cy * 2 + Zy(12))
, tabswidth(200)
, closetab(-1)
, blinking(false)
, pressed(false)
, dnd(false)
{
	sframe.Left(SizePos(), 200).Hide();
	sb.WhenScroll = [this] { Refresh(); };
	AddFrame(sb.AutoHide());
	WhenClose = THISFN(OnClose);
	WhenBar = THISFN(OnBar);
	ctx.view.AddFrame(sframe);
}

void TabBar::Paint(Draw& w)
{
	Size sz = GetSize();
	w.DrawRect(sz, SColorFace());
	int q = ctx.stack.GetCursor();

	w.Clip(sz);
	for(int i = 0, sc = sb; i < ctx.stack.GetCount(); i++) {
		Terminal& term = AsTerminal(ctx.stack[i]);
		int y = i * tabcy - sc;

		if(y > sz.cy)
			break;

		if(y + tabcy < 0)
			continue;

		Rect rect(0, y, sz.cx, y + tabcy);

		TabInfo info;
		info.term  = &term;
		info.focus = (i == q);
		info.hover = (i == cursor);
		info.dnd   = (dnd && info.hover);
		info.msg   = blinking;

		if(info.hover) {
			bool pressed = (closetab == i);
			bool hot = GetCloseRect(i).Contains(pos);
			info.state = pressed ? CTRL_PRESSED : hot ? CTRL_HOT : CTRL_NORMAL;
		}

		StdTabDisplay().Paint(w, rect, RawToValue(info), SColorText, SColorFace, 0);
	}
	w.End();
}

void TabBar::Animate()
{
	if(!IsVisible()) {
		KillTimeCallback(TIMEID_BLINK);
		return;
	}

	auto HasMessage = [&](const Ctrl& c) -> bool {
		Terminal& t  = AsTerminal(const_cast<Ctrl&>(c));
		return !t.IsRunning() || t.IsRoot();
	};

	if(!ExistsTimeCallback(TIMEID_BLINK) && FindMatch(ctx.stack, HasMessage) >= 0)
		SetTimeCallback(-500, [this] { blinking ^= 1; Refresh(); }, TIMEID_BLINK);
	
	Refresh();
}

void TabBar::Layout()
{
	Sync();
}

Rect TabBar::GetCloseRect(int i) const
{
	return RectC(GetSize().cx - 24, i * tabcy - sb + (tabcy - 16) / 2, 16, 16);
}

void TabBar::SyncSb()
{
	sb.SetTotal(ctx.stack.GetCount() * tabcy);
	sb.SetPage(GetSize().cy);
	
	int q = ctx.stack.GetCursor();
	if(q >= 0 && q < ctx.stack.GetCount()) {
		int y = q * tabcy;
		int page = GetSize().cy;
		if(y < (int)sb)
			sb = y;
		else
		if(y + tabcy > sb + page)
			sb = max(y, y + tabcy - page);
	}
}

int TabBar::GetTab(Point p) const
{
	Size sz = GetSize();
	if(p.x >= 0 && p.x < sz.cx) {
		int i = (p.y + sb) / tabcy;
		if(i >= 0 && i < ctx.stack.GetCount()) {
			return i;
		}
	}
	return -1;
}

void TabBar::SyncHot()
{
	int h = GetTab(pos);
	if(h != cursor) {
		cursor = h;
		Refresh();
	}

	if(IsReadOnly())
		cursor = -1;
}

void TabBar::LeftDown(Point p, dword keyflags)
{
	if(!IsEditable())
		return;

	int h = GetTab(p);
	if(h >= 0) {
		if(GetCloseRect(h).Contains(p)) {
			closetab = h;
			Refresh();
			return;
		}
		if(ctx.stack[h].IsEnabled()) {
			ctx.stack.Goto(h);
			SyncSb();
		}
	}
	pressed = true;
}

void TabBar::LeftUp(Point p, dword keyflags)
{
	if(closetab >= 0) {
		if(GetCloseRect(closetab).Contains(p))
			WhenClose(closetab);
		closetab = -1;
		Refresh();
	}
	pressed = false;
}

void TabBar::MouseMove(Point p, dword keyflags)
{
	pos = p;
	SyncHot();
}

void TabBar::MouseWheel(Point p, int zdelta, dword keyflags)
{
	sb.Wheel(zdelta, 16);
}

void TabBar::RightDown(Point p, dword keyflags)
{
	int h = GetTab(p);
	if(h >= 0) {
		if(ctx.stack[h].IsEnabled()) {
			ctx.stack.Goto(h);
			SyncSb();
		}
	}
	MenuBar::Execute([this, h](Bar& menu) { WhenBar(h, menu); });
}

void TabBar::MiddleDown(Point p, dword keyflags)
{
//	int h = GetTab(p);
//	if(h >= 0)
//		WhenClose(h);
}

void TabBar::DragAndDrop(Point p, PasteClip& d)
{
	int h = GetTab(p);
	if(h >= 0) {
		cursor = h;
		Ctrl& ctrl = ctx.stack[h];
		ctrl.DragAndDrop(p, d);
		dnd = d.IsAccepted();
		Refresh();
	}
	else {
		cursor = -1;
		dnd = false;
		Refresh();
	}
}

void TabBar::DragLeave()
{
	CancelMode();
}

void TabBar::CancelMode()
{
	cursor = -1;
	closetab = -1;
	pressed = false;
	dnd = false;
	Refresh();
}

void TabBar::MouseLeave()
{
	CancelMode();
}

void TabBar::Sync()
{
	auto type = decode(
		ctx.settings.tabsalignment, "right",
		SplitterFrame::RIGHT,
		SplitterFrame::LEFT
	);

	if(sframe.GetType() != type)
		sframe.Set(SizePos(), tabswidth, type);

	sframe.Show(ctx.settings.showtabs);

	SyncSb();
	Animate();
}

void TabBar::OnClose(int i)
{
	ctx.RemoveTerminal(AsTerminal(ctx.stack[i]));
}

void TabBar::OnBar(int i, Bar& bar)
{
	if(i >= 0 && i < ctx.stack.GetCount())
		AsTerminal(ctx.stack[i]).ContextMenu(bar);
	else
		ctx.TermMenu(bar);
}

struct StdTabDisplayCls : Display
{
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const override
	{
		const TabInfo& info = q.To<TabInfo>();
		if(!info.term)
			return;
		
		const Button::Style& z = Button::StyleNormal();
	
		if(info.hover || info.focus)
			ChPaint(w, r, z.look[info.hover ? CTRL_HOT : CTRL_NORMAL]);

		String msg;
		Color msgcolor = Null;
		bool isblink = false;

		auto AddAlert = [&](String m, Color c) {
			msg << (msg.GetCount() ? " - " : "") << m;
			msgcolor = Nvl(msgcolor, c);
			isblink = true;
		};

		if(info.term->IsRunning()) {
			if(info.term->IsRoot())
				AddAlert(t_("Privilege escalation!"), SRed);
		}
		else {
			if(info.term->IsFailure())
				AddAlert(t_("Failure!"), SRed);
			if(info.term->IsAsking())
				AddAlert(t_("Input required"), Yellow());
			if(info.term->IsSuccess() && msg.IsEmpty())
				AddAlert(t_("Completed"), SLtGreen);
		}

		Color cc = info.focus ? SColorHighlight : SColorShadow;
		w.DrawRect(r.left + 3, r.top + 6, 4, r.Height() - 12, Nvl(info.msg ? msgcolor : cc, cc));

		int tx = r.left + 15;
		int max_cx = max(0, r.right - 32 - tx);

		if(info.dnd) {
			const Image& dico = Images::DropClipHD();
			ChPaint(w, r, Colorize(z.look[CTRL_NORMAL], SColorHighlight));
			w.DrawImage(r.CenterRect(min(dico.GetSize(), r.GetSize())), dico);
		}

		Font tfont = StdFont().Bold(info.focus);
		String title = info.term->GetTitle();
		Size tsz = GetTextSize(title, tfont);
		int ty = r.top + (r.Height() - tsz.cy) / 2;

		if(msg.GetCount()) {
			Font sfont = StdFont();
			Size msz = GetTextSize(msg, sfont);
			ty = r.top + (r.Height() - tsz.cy - msz.cy - 2) / 2;

			if(!isblink || info.msg)
				DrawTextEllipsis(w, tx, ty + tsz.cy + 2, max_cx, msg, "...", sfont, Nvl(msgcolor, ink));
		}

		DrawTextEllipsis(w, tx, ty, max_cx, title, "...", tfont, ink);

		if(info.hover) {
			Rect cr = RectC(r.right - 24, r.top + (r.Height() - 16) / 2, 16, 16);
			if(info.state != CTRL_NORMAL)
				ChPaint(w, cr,  z.look[info.state]);
			w.DrawImage(cr.left, cr.top, Images::Delete());
		}
	}
};

const Display& StdTabDisplay()
{
	return Single<StdTabDisplayCls>();
}

}