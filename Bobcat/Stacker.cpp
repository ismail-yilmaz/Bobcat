// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

constexpr const int MAX_PANE_COUNT = 16;

Stacker::Stacker(Bobcat& ctx_)
: ctx(ctx_)
, activectrl(nullptr)
, duration(0)
, maxpanes(MAX_PANE_COUNT)
, viewmode(ViewMode::AutoSplit)
, wheel(false)
, vertical(false)
, splitvert(false)
, animating(false)
, swapping(false)
{
	WhenAction = [this]() { ctx.Sync(); };
	WhenSwap   = [this](int a, int b) { ctx.navigator.AnimateSwap(a, b); };

	ctx.view.Add(Animation().Wheel().Horz().SizePos());
}

void Stacker::ChildGotFocus()
{
	if(!animating) {
		int i = FindMatch(list, [](Ctrl *c) { return c && c->HasFocusDeep(); });
		if(i >= 0 && activectrl != list[i]) {
			activectrl = list[i];
			TrackFocus(activectrl);
			WhenAction();
		}
	}
	ParentCtrl::ChildGotFocus();
}

void Stacker::TrackFocus(Ctrl *ctrl)
{
	if(!ctrl)
		return;
	int ix = FindIndex(focushistory, ctrl);
	if(ix >= 0)
		focushistory.Remove(ix);
	focushistory.Insert(0, ctrl);
}

Ctrl *Stacker::GetPrevPane(MultiSplitter *scope, Ctrl *excl) const
{
	int ix = FindMatch(focushistory, [&](Ctrl *c) {
		return c && c != excl && FindIndex(list, c) >= 0
			&& (!scope || GetParentSplitter(c) == scope);
	});
	return ix >= 0 ? focushistory[ix] : nullptr;
}

Stacker& Stacker::Wheel(bool b)
{
	wheel = b;
	return *this;
}

Stacker& Stacker::Animation(int ms)
{
	duration = clamp(ms, 0, 1000);
	return *this;
}

int Stacker::GetDuration() const
{
	return duration;
}

Stacker& Stacker::MaxPanes(int n)
{
	maxpanes = max(1, n);
	return *this;
}

int Stacker::GetMaxPanes() const
{
	return maxpanes;
}

Stacker& Stacker::NoBackground(bool b)
{
	Transparent(b);
	for(MultiSplitter& s : splitters)
		s.Transparent(b);
	return *this;
}

Stacker& Stacker::Horz()
{
	vertical = false;
	return *this;
}

Stacker& Stacker::Vert()
{
	vertical = true;
	return *this;
}

Stacker& Stacker::HorzSplitter()
{
	splitvert = false;
	viewmode = ViewMode::RightSplit;
	return *this;
}

Stacker& Stacker::VertSplitert()
{
	splitvert = true;
	viewmode = ViewMode::BottomSplit;
	return *this;
}

Stacker& Stacker::AutoSplit()
{
	return SetViewMode(ViewMode::AutoSplit);
}

Stacker& Stacker::SplitLeft()
{
	return SetViewMode(ViewMode::LeftSplit);
}

Stacker& Stacker::SplitRight()
{
	return SetViewMode(ViewMode::RightSplit);
}

Stacker& Stacker::SplitTop()
{
	return SetViewMode(ViewMode::TopSplit);
}

Stacker& Stacker::SplitBottom()
{
	return SetViewMode(ViewMode::BottomSplit);
}

Stacker& Stacker::SetViewMode(ViewMode mode)
{
	viewmode = mode;
	if(mode == ViewMode::TopSplit || mode == ViewMode::BottomSplit)
		splitvert = true;
	else
	if(mode == ViewMode::LeftSplit || mode == ViewMode::RightSplit)
		splitvert = false;
	return *this;
}

ViewMode Stacker::GetViewMode() const
{
	return viewmode;
}

Stacker& Stacker::ToggleSplitterOrientation()
{
	if(MultiSplitter *s = GetParentSplitter(activectrl); s && activectrl)
		s->ToggleOrientation(*activectrl);
	WhenAction();
	return *this;
}

Stacker& Stacker::Add(Ctrl& ctrl)
{
	return Insert(GetCount(), ctrl);
}

Stacker& Stacker::Insert(int i, Ctrl& ctrl)
{
	if(ctrl.InFrame())
		Ctrl::Add(ctrl);
	else {
		ctrl.Hide();
		Ctrl::Add(ctrl.SizePos());
		list.Insert(i, &ctrl);
		WhenInsert(i, ctrl);
		ActivatePane(&ctrl);
	}
	return *this;
}

Stacker& Stacker::Split(Ctrl& ctrl)
{
	ViewMode md = viewmode;
	if(md == ViewMode::Stacked || md == ViewMode::Windowed)
		md = splitvert ? ViewMode::BottomSplit : ViewMode::RightSplit;
	return Split(ctrl, md);
}

Stacker& Stacker::Split(Ctrl& ctrl, ViewMode mode)
{
	if(mode == ViewMode::Stacked || mode == ViewMode::Windowed)
		return Add(ctrl);

	int i = GetCursor();
	if(i < 0)
		return Add(ctrl);

	return Split(*list[i], ctrl, mode);
}

Stacker& Stacker::Split(Ctrl& target, Ctrl& ctrl, ViewMode mode)
{
	if(mode == ViewMode::Stacked || mode == ViewMode::Windowed)
		return Add(ctrl);

	int i = Find(target);
	if(i < 0)
		return Add(ctrl);

	MultiSplitter* sp = GetParentSplitter(list[i]);

	if(maxpanes < 2 || (sp && sp->GetCount() >= maxpanes))
		return Add(ctrl);

	Ctrl* anchor = list[i];
	if(mode == ViewMode::AutoSplit) {
		Size sz = anchor ? anchor->GetSize() : Size(0, 0);
		if(sz.cx <= 0 || sz.cy <= 0)
			sz = GetSize();
		if(sz.cx > 0 && sz.cy > 0)
			mode = sz.cx >= sz.cy ? ViewMode::RightSplit : ViewMode::BottomSplit;
		else
			mode = splitvert ? ViewMode::BottomSplit : ViewMode::RightSplit;
	}

	Ctrl* oldroot = sp ? sp->GetFirstPane() : anchor;
	String oldname;
	if(oldroot) {
		int idx = workspacenames.Find(oldroot);
		if(idx >= 0)
			oldname = workspacenames[idx];
	}

	bool before = (mode == ViewMode::LeftSplit || mode == ViewMode::TopSplit);
	int ins = before ? i : i + 1;
	list.Insert(ins, &ctrl);

	if(sp && anchor) {
		switch(mode) {
		case ViewMode::LeftSplit:
			sp->SplitLeft(*anchor, ctrl);
			break;
		case ViewMode::RightSplit:
			sp->SplitRight(*anchor, ctrl);
			break;
		case ViewMode::TopSplit:
			sp->SplitTop(*anchor, ctrl);
			break;
		case ViewMode::BottomSplit:
			sp->SplitBottom(*anchor, ctrl);
			break;
		default:
			break;
		}
	}
	else {
		if(anchor)
			anchor->Remove();

		MultiSplitter& s = splitters.Add();
		s.Transparent(IsTransparent());
		Ctrl::Add(s.SizePos());
		sp = &s;

		if(anchor) {
			switch(mode) {
			case ViewMode::LeftSplit:
				s.SplitLeft(*anchor, ctrl);
				break;
			case ViewMode::RightSplit:
				s.SplitRight(*anchor, ctrl);
				break;
			case ViewMode::TopSplit:
				s.SplitTop(*anchor, ctrl);
				break;
			case ViewMode::BottomSplit:
				s.SplitBottom(*anchor, ctrl);
				break;
			default:
				break;
			}
		}
	}

	if(sp && !oldname.IsEmpty()) {
		Ctrl* newroot = sp->GetFirstPane();
		if(newroot && newroot != oldroot) {
			workspacenames.RemoveKey(oldroot);
			workspacenames.GetAdd(newroot) = oldname;
		}
	}

	WhenInsert(ins, ctrl);
	ActivatePane(&ctrl);

	return *this;
}

Stacker& Stacker::SplitAuto(Ctrl& ctrl)
{
	Split(ctrl, ViewMode::AutoSplit);
	return *this;
}

Stacker& Stacker::SplitLeft(Ctrl& ctrl)
{
	Split(ctrl, ViewMode::LeftSplit);
	return *this;
}

Stacker& Stacker::SplitRight(Ctrl& ctrl)
{
	Split(ctrl, ViewMode::RightSplit);
	return *this;
}

Stacker& Stacker::SplitTop(Ctrl& ctrl)
{
	Split(ctrl, ViewMode::TopSplit);
	return *this;
}

Stacker& Stacker::SplitBottom(Ctrl& ctrl)
{
	Split(ctrl, ViewMode::BottomSplit);
	return *this;
}

void Stacker::Remove(Ctrl& ctrl)
{
	LLOG("Remove(" << &ctrl << ")");

	int i = Find(ctrl);
	int hi = FindIndex(focushistory, &ctrl);
	if(hi >= 0)
		focushistory.Remove(hi);

	if(!GetParentSplitter(&ctrl))
		workspacenames.RemoveKey(&ctrl);

	if(i < 0 || RemoveFromSplitter(i))
		return;

	// Only transfer focus if we are closing the active pane
	if(&ctrl == activectrl) {
		Ctrl *nx = GetPrevPane(nullptr, &ctrl);
		if(nx)
			Goto(*nx);
		else
		if(i + 1 < GetCount())
			Goto(i + 1);
		else
		if(i > 0)
			Goto(i - 1);
		else
			activectrl = nullptr;
	}

	WhenRemove(i, ctrl);
	list.Remove(i);
	ctrl.Remove();
}

void Stacker::Remove(int i)
{
	if(i >= 0 && i < GetCount())
		Remove(*list[i]);
}

bool Stacker::RemoveFromSplitter(int i)
{
	if(i < 0 || i >= list.GetCount())
		return false;

	Ctrl* ctrl = list[i];
	if(!ctrl)
		return false;

	if(MultiSplitter* s = GetParentSplitter(ctrl); s) {
		bool wasroot = (s->GetFirstPane() == ctrl);
		Vector<Ctrl*> oldpanes = s->GetPanes();
		int oldidx = FindIndex(oldpanes, ctrl);
		String oldname;

		if(wasroot) {
			int nameidx = workspacenames.Find(ctrl);
			if(nameidx >= 0) {
				oldname = workspacenames[nameidx];
				workspacenames.Remove(nameidx);
			}
		}

		s->Remove(*ctrl);

		if(wasroot && s->GetCount() > 0 && !oldname.IsEmpty()) {
			if(Ctrl* newroot = s->GetFirstPane())
				workspacenames.Add(newroot, oldname);
		}

		bool sync = false;
		if(s->GetCount() == 1) {
			Ctrl* remaining = s->GetFirstPane();
			RemoveChild(s);
			if(remaining) {
				s->Remove(*remaining);
				Ctrl::Add(remaining->SizePos());

				// Only activate the remaining pane if this splitter was the active workspace
				if(activectrl == ctrl || activectrl == remaining) {
					activectrl = remaining;
					ActivatePane(remaining);
					sync = true;
				}
				else
					remaining->Hide();
			}
			RemoveSplitter(*s);
		}
		else
		if(s->GetCount() > 1 && ctrl == activectrl) {
			Ctrl* nx = GetPrevPane(s, ctrl);
			if(!nx) {
				Vector<Ctrl*> panes = s->GetPanes();
				nx = oldidx >= 0 && oldidx < panes.GetCount() ? panes[oldidx] : panes.Top();
			}
			activectrl = nx;
			ActivatePane(activectrl);
			sync = true;
		}

		WhenRemove(i, *ctrl);
		list.Remove(i);
		if(sync)
			WhenAction();
		return true;
	}
	
	return false;
}

int Stacker::GetCount() const
{
	return list.GetCount();
}

int Stacker::GetCursor() const
{
	return FindIndex(list, activectrl);
}

Ctrl& Stacker::Get(int i) const
{
	ASSERT(i >= 0 && i < GetCount());
	return *list[i];
}

Ctrl& Stacker::operator[](int i) const
{
	return Get(i);
}

Ctrl *Stacker::GetActiveCtrl() const
{
	int i = FindIndex(list, activectrl);
	return i >= 0 ? list[i] : nullptr;
}

int Stacker::Find(Ctrl& ctrl) const
{
	return FindIndex(list, &ctrl);
}

int Stacker::GetWorkspaceCount() const
{
	int count = 0;
	for(int i = 0; i < list.GetCount(); ++i) {
		MultiSplitter *sp = GetParentSplitter(list[i]);
		if(!sp || sp->GetFirstPane() == list[i])
			count++;
	}
	return count;
}

int Stacker::FindWorkspace(Ctrl& ctrl) const
{
	MultiSplitter *sp = GetParentSplitter(&ctrl);
	Ctrl *root = sp ? sp->GetFirstPane() : &ctrl;
	if(!root)
		return -1;
	return FindIndex(list, root);
}

int Stacker::FindInWorkspace(int wsid, Ctrl& ctrl) const
{
	if(wsid < 0 || wsid >= list.GetCount())
		return -1;

	MultiSplitter *sp = GetParentSplitter(list[wsid]);
	if(!sp)
		return (&ctrl == list[wsid]) ? 0 : -1;

	return sp->FindPane(ctrl);
}

Vector<Ctrl*> Stacker::GetWorkspacePanes(int wsid) const
{
	Vector<Ctrl*> panes;
	if(wsid >= 0 && wsid < list.GetCount()) {
		MultiSplitter *sp = GetParentSplitter(list[wsid]);
		if(sp)
			panes = sp->GetPanes();
		else
		if(list[wsid])
			panes.Add(list[wsid]);
	}
	return panes;
}

Vector<Ctrl*> Stacker::GetWorkspacePanes(Ctrl& ctrl) const
{
	int wsid = FindWorkspace(ctrl);
	if(wsid >= 0)
		return GetWorkspacePanes(wsid);
	return Vector<Ctrl*>();
}

Stacker& Stacker::SetWorkspaceName(int wsid, const String& name)
{
	if(wsid >= 0 && wsid < list.GetCount()) {
		Ctrl *root = list[wsid];
		int idx = workspacenames.Find(root);
		if(idx >= 0)
			workspacenames[idx] = name;
		else
			workspacenames.Add(root, name);
	}
	return *this;
}

Stacker& Stacker::SetWorkspaceName(Ctrl& ctrl, const String& name)
{
	int wsid = FindWorkspace(ctrl);
	if(wsid >= 0)
		SetWorkspaceName(wsid, name);
	return *this;
}

String Stacker::GetWorkspaceName(int wsid) const
{
	if(wsid >= 0 && wsid < list.GetCount()) {
		Ctrl *root = list[wsid];
		int idx = workspacenames.Find(root);
		if(idx >= 0)
			return workspacenames[idx];
	}
	return Null;
}

String Stacker::GetWorkspaceName(Ctrl& ctrl) const
{
	int wsid = FindWorkspace(ctrl);
	if(wsid >= 0)
		return GetWorkspaceName(wsid);
	return Null;
}

int Stacker::FindWorkspaceByName(const String& name) const
{
	for(int i = 0; i < workspacenames.GetCount(); ++i) {
		if(workspacenames[i] == name) {
			Ctrl *root = workspacenames.GetKey(i);
			return FindIndex(list, root);
		}
	}
	return -1;
}

void Stacker::MovePanePrev()
{
	if(!activectrl)
		return;

	if(GetParentSplitter(activectrl)) {
		int wsid = FindWorkspace(*activectrl);
		int localidx = FindInWorkspace(wsid, *activectrl);

		if(localidx > 0) {
			Vector<Ctrl*> panes = GetWorkspacePanes(wsid);
			if(localidx < panes.GetCount() && panes[localidx - 1])
				Swap(*activectrl, *panes[localidx - 1]);
		}
	}
}

void Stacker::MovePaneNext()
{
	if(!activectrl)
		return;

	if(GetParentSplitter(activectrl)) {
		int wsid = FindWorkspace(*activectrl);
		int localidx = FindInWorkspace(wsid, *activectrl);
		Vector<Ctrl*> panes = GetWorkspacePanes(wsid);

		if(localidx >= 0 && localidx < panes.GetCount() - 1)
			if(panes[localidx + 1])
				Swap(*activectrl, *panes[localidx + 1]);
	}
}

void Stacker::Swap(int a, int b)
{
	if(swapping || a == b || a < 0 || a >= list.GetCount() || b < 0 || b >= list.GetCount())
		return;

	swapping = true;

	Ctrl *ctrla = list[a];
	Ctrl *ctrlb = list[b];

	if(!ctrla || !ctrlb) {
		swapping = false;
		return;
	}

	MultiSplitter *splittera = GetParentSplitter(ctrla);
	MultiSplitter *splitterb = GetParentSplitter(ctrlb);

	if(splittera && splitterb) {
		if(splittera == splitterb)
			splittera->Swap(*ctrla, *ctrlb);
		else {
			splittera->Replace(*ctrla, *ctrlb);
			splitterb->Replace(*ctrlb, *ctrla);
		}
	}
	else
	if(splittera && !splitterb) {
		ctrlb->Remove();
		ctrla->Hide();
		Ctrl::Add(ctrla->SizePos());
		splittera->Replace(*ctrla, *ctrlb);
		splittera->Show();
	}
	else
	if(splitterb && !splittera) {
		ctrla->Remove();
		ctrlb->Hide();
		Ctrl::Add(ctrlb->SizePos());
		splitterb->Replace(*ctrlb, *ctrla);
		splitterb->Show();
	}

	WhenSwap(a, b);
	list.Swap(a, b);

	swapping = false;
}

void Stacker::Swap(Ctrl& a, Ctrl& b)
{
	Swap(Find(a), Find(b));
}

void Stacker::SwapNext()
{
	if(!activectrl)
		return;

	int i = Find(*activectrl);
	if(i >= 0 && i < GetCount() - 1)
		Swap(i, i + 1);
}

void Stacker::SwapPrev()
{
	if(!activectrl)
		return;

	int i = Find(*activectrl);
	if(i > 0)
		Swap(i, i - 1);
}

void Stacker::SwapPanes()
{
	if(MultiSplitter *sp = GetParentSplitter(activectrl); sp && sp->GetCount() >= 2 && activectrl) {
		Ctrl *other = sp->GetSibling(*activectrl);
		if(other) {
			Swap(*activectrl, *other);
			activectrl->SetFocus();
		}
	}
}

void Stacker::ExpandTopLeftPane()
{
	if(MultiSplitter *sp = GetParentSplitter(activectrl); sp && activectrl)
		sp->SetPos(*activectrl, clamp(sp->GetPos(*activectrl) + 500, 0, 10000));
}

void Stacker::ExpandBottomRightPane()
{
	if(MultiSplitter *sp = GetParentSplitter(activectrl); sp && activectrl)
		sp->SetPos(*activectrl, clamp(sp->GetPos(*activectrl) - 500, 0, 10000));
}

void Stacker::ResetSplitterPos()
{
	if(MultiSplitter *sp = GetParentSplitter(activectrl); sp && activectrl)
		sp->SetPos(*activectrl, 5000);
}

Ctrl* Stacker::GetPassiveCtrl() const
{
	MultiSplitter *sp = GetParentSplitter(activectrl);
	if(!sp || !activectrl)
		return nullptr;

	return sp->GetSibling(*activectrl);
}

void Stacker::Goto(int i)
{
	if(i >= 0 && i < GetCount()) {
		ActivatePane(list[i]);
		WhenAction();
	}
}

void Stacker::Goto(Ctrl& ctrl)
{
	Goto(Find(ctrl));
}

void Stacker::GoBegin()
{
	Goto(0);
}

void Stacker::GoEnd()
{
	Goto(GetCount() - 1);
}

void Stacker::Prev()
{
	int i = FindIndex(list, activectrl);
	if(i < 0)
		return;

	if(i > 0)
		Goto(i - 1);
	else
	if(wheel)
		GoEnd();
}

void Stacker::Next()
{
	int i = FindIndex(list, activectrl);
	if(i < 0)
		return;

	if(i < GetCount() - 1)
		Goto(i + 1);
	else
	if(wheel)
		GoBegin();
}

void Stacker::ActivatePane(Ctrl *ctrl)
{
	if(!ctrl || animating)
		return;

	if(!activectrl)
		activectrl = ctrl;

	MultiSplitter *currentsp = GetParentSplitter(activectrl);
	MultiSplitter *nextsp = GetParentSplitter(ctrl);

	if(activectrl == ctrl) {
		TrackFocus(ctrl);
		currentsp ? currentsp->Show() : activectrl->Show();
		if(!activectrl->HasFocusDeep())
			activectrl->SetFocus();
		return;
	}

	GuiLock __;

	if(ctrl != activectrl && ((currentsp != nextsp) || (!currentsp && !nextsp)) && duration >= 100) {
		Ctrl *nextctrl = nextsp ? static_cast<Ctrl*>(nextsp) : ctrl;
		Ctrl *currentctrl = currentsp ? static_cast<Ctrl*>(currentsp) : activectrl;
		Animate(currentctrl, nextctrl, IsNext(ctrl));
	}

	if(currentsp != nextsp)
		currentsp ? currentsp->Hide() : activectrl->Hide();
	else
	if(!currentsp)
		activectrl->Hide();
	activectrl = ctrl;
	TrackFocus(ctrl);
	nextsp ? nextsp->Show() : activectrl->Show();
	if(!activectrl->HasFocusDeep())
		activectrl->SetFocus();
}

bool Stacker::IsNext(Ctrl *nextctrl) const
{
	int curr  = FindIndex(list, activectrl);
	int next  = FindIndex(list, nextctrl);
	int total = list.GetCount();
	return (curr < next) || (total > 2 && curr == total - 1 && next == 0);
}

void Stacker::Animate(Ctrl *currentctrl, Ctrl *nextctrl, bool forward)
{
	if(animating || !currentctrl || !nextctrl)
		return;

	animating = true;

	Rect view = GetView();
	Size size = view.GetSize();

	Rect rsrc1 = view, rsrc2 = view, rdst1 = view, rdst2 = view;

	if(vertical) {
		rdst1.OffsetVert(forward ? -size.cy :  size.cy);
		rsrc2.OffsetVert(forward ?  size.cy : -size.cy);
	}
	else {
		rdst1.OffsetHorz(forward ? -size.cx :  size.cx);
		rsrc2.OffsetHorz(forward ?  size.cx : -size.cx);
	}

	nextctrl->SetRect(rsrc2);
	nextctrl->Show();

	for(int start = msecs();;) {
		int elapsed = msecs(start);
		if(elapsed > duration)
			break;

		Rect r1 = rsrc1, r2 = rsrc2;
		r1 += (rdst1 - rsrc1) * elapsed / duration;
		r2 += (rdst2 - rsrc2) * elapsed / duration;
		currentctrl->SetRect(r1);
		nextctrl->SetRect(r2);

#ifdef PLATFORM_POSIX
		currentctrl->Sync();
		nextctrl->Sync();
#else
		currentctrl->Refresh();
		nextctrl->Refresh();
#endif
		if(IsMainThread()) {
			Ctrl::ProcessEvents();
			GuiSleep(0);
		}
	}

	currentctrl->SizePos();
	nextctrl->SizePos();
	animating = false;
}

MultiSplitter *Stacker::GetParentSplitter(Ctrl *c) const
{
	return c ? dynamic_cast<MultiSplitter*>(c->GetParent()) : nullptr;
}

void Stacker::RemoveSplitter(const MultiSplitter& s)
{
	if(int i = FindMatch(splitters, [&](const MultiSplitter& q) { return &q == &s; }); i >= 0)
		splitters.Remove(i);
}

}