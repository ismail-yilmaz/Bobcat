// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

TabBar::TabBar(Bobcat& ctx_)
: ctx(ctx_)
, pos(-1, -1)
, cursor(-1)
, tabcy(GetStdFontCy() * 2 + 12)
, tabswidth(200)
, closetab(-1)
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

		Rect rect = RectC(0, y, sz.cx, tabcy);

		DisplayInfo info;
		info.title   = term.GetTitle();
		info.focus   = (i == q);
		info.hover   = (i == cursor);
		info.dnd     = (dnd && info.hover);
		info.showmsg = true;
		info.icon    = Images::Terminal();

		SetStatusDisplayInfo(term, info, false);

		if(info.hover & !dnd) {
			bool pressedbtn = (closetab == i);
			bool hot = GetCloseRect(i).Contains(pos);
			info.state = pressedbtn ? CTRL_PRESSED : hot ? CTRL_HOT : CTRL_NORMAL;
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

	auto HasAnim = [&](const Ctrl& c) -> bool {
		Terminal& t  = AsTerminal(const_cast<Ctrl&>(c));
		ProgramInfo pinfo = GetProgramInfo(t);
		return !t.IsRunning()
			 || t.IsRoot()
			 || t.InProgress()
			 || pinfo.IsWorking()
			 || pinfo.IsUrgent();
	};

	if(FindMatch(ctx.stack, HasAnim) >= 0) {
		if(!ExistsTimeCallback(TIMEID_BLINK))
			SetTimeCallback(-30, [this] { Refresh(); }, TIMEID_BLINK);
	}
	else {
		KillTimeCallback(TIMEID_BLINK);
	}

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
//    int h = GetTab(p);
//    if(h >= 0)
//        WhenClose(h);
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
		const DisplayInfo& info = q.To<DisplayInfo>();
		if(IsNull(info.title))
			return;

		const Button::Style& z = Button::StyleNormal();

		if(info.hover || info.focus)
			ChPaint(w, r, z.look[info.hover ? CTRL_HOT : CTRL_NORMAL]);

		Color cc = info.focus ? SColorHighlight : SColorShadow;
		Color barc = cc;
		if(info.urgent && info.showmsg && !IsNull(info.msgcolor))
			barc = info.msgcolor;

		w.DrawRect(r.left + 3, r.top + 6, 4, r.Height() - 12, barc);

		int tx = r.left + 15;
		int cx = max(0, r.right - 32 - tx);

		if(info.dnd) {
			const Image& dico = Images::DropClipHD();
			ChPaint(w, r, Colorize(z.look[CTRL_NORMAL], SColorHighlight));
			w.DrawImage(r.CenterRect(min(dico.GetSize(), r.GetSize())), dico);
		}

		Font tfont = StdFont().Bold(info.focus);

		AttrText t1;
		t1.Text(info.title).SetFont(tfont).Ink(ink);
		if(!IsNull(info.icon))
			t1.SetImage(info.icon);

		int th = max(GetTextSize(info.title, tfont).cy, IsNull(info.icon) ? 0 : info.icon.GetSize().cy);
		int ty = r.top;
		int tcy = r.Height();

		if(info.msg.GetCount()) {
			Font sfont = StdFont();
			int mh = max(GetTextSize(info.msg, sfont).cy, IsNull(info.mimg) ? 0 : info.mimg.GetSize().cy);
			ty = r.top + (r.Height() - th - mh - 2) / 2;
			tcy = th;

			if(info.showmsg) {
				AttrText t2;
				t2.Text(info.msg).SetFont(sfont).Ink(Nvl(info.msgcolor, ink));
				if(!IsNull(info.mimg))
					t2.SetImage(Colorize(info.mimg, Nvl(info.msgcolor, ink)));
				StdDisplay().Paint(w, RectC(tx, ty + th + 2, cx, mh), t2, ink, Null, 0);
			}
		}

		StdDisplay().Paint(w, RectC(tx, ty, cx, tcy), t1, ink, Null, 0);

		if(!info.dnd && info.hover) {
			Rect cr = RectC(r.right - 24, r.top + (r.Height() - 16) / 2, 16, 16);
			if(info.state != CTRL_NORMAL)
				ChPaint(w, cr, z.look[info.state]);
			w.DrawImage(cr.left, cr.top, Images::Delete());
		}
	}
};

const Display& StdTabDisplay()
{
	return Single<StdTabDisplayCls>();
}

}