// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

// Note: Stacker is a modified version of StackCtrl

Stacker::Stacker()
: activectrl(nullptr)
, duration(0)
, vertical(false)
, wheel(false)
, animating(false)
, splitvert(false)
{
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

Stacker& Stacker::NoBackground(bool b)
{
	Transparent(b);
	if(splitters.GetCount() > 0)
		for(Splitter& s : splitters)
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
	return *this;
}

Stacker& Stacker::VertSplitert()
{
	splitvert = true;
	return *this;
}

Stacker& Stacker::ToggleSplitterOrientation()
{
	if(Splitter *s = GetParentSplitter(activectrl); s)
		s->IsHorz() ? s->Vert() : s->Horz();
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
		Activate(&ctrl);
	}
	return *this;
}

Stacker& Stacker::Split(Ctrl& ctrl)
{
	if(GetParentSplitter(activectrl))
		return *this;

	if(int i = GetCursor(); i >= 0) {
		list[i]->Remove();
		list.Insert(i + 1, &ctrl);
		Splitter& s = splitters.Add();
		splitvert ? s.Vert() : s.Horz();
		Ctrl::Add(s.SizePos());
		s.Add(*list[i]);
		s.Add(*list[i + 1]);
		WhenInsert(i + 1, ctrl);
		Activate(&ctrl);
	}
	
	return *this;
}

void Stacker::Remove(Ctrl& ctrl)
{
	LLOG("Remove(" << &ctrl << ")");

	int i = Find(ctrl);
	
	if(i < 0 || RemoveFromSplitter(i))
		return;
	
	if(i > 0)
		Goto(i - 1);
	else
		Goto(i + 1);
	
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
	Ctrl *ctrl = list[i];
	if(Splitter *s = GetParentSplitter(ctrl); s) {
		RemoveChild(s);
		s->Remove(*ctrl);
		if(s->GetCount()) {
			if(int j = Find(*s->GetFirstChild()); j >= 0) {
				activectrl = list[j];
				Ctrl::Add(activectrl->SizePos());
				Activate(activectrl);
			}
		}
		RemoveSplitter(*s);
		WhenRemove(i, *ctrl);
		list.Remove(i);
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
	int i  = FindIndex(list, activectrl);
	return i >= 0 ? list[i] : nullptr;
}

int Stacker::Find(Ctrl& ctrl) const
{
	return FindIndex(list, &ctrl);
}

void Stacker::Swap(int a, int b)
{
	if (a == b || a < 0 || a >= list.GetCount() || b < 0 || b >= list.GetCount())
		return;

	Ctrl *ctrl_a = list[a];
	Ctrl *ctrl_b = list[b];
	
	Splitter *splitter_a = GetParentSplitter(ctrl_a);
	Splitter *splitter_b = GetParentSplitter(ctrl_b);
	
	int idx_a = -1, idx_b = -1, pos_a = 0, pos_b = 0;

	if(splitter_a) {
		idx_a = splitter_a->GetChildIndex(ctrl_a);
		pos_a = splitter_a->GetPos();
	}

	if(splitter_b) {
		idx_b = splitter_b->GetChildIndex(ctrl_b);
		pos_b = splitter_b->GetPos();
	}

	if(splitter_a && splitter_b) {
		splitter_a->Remove(*ctrl_a);
		splitter_b->Remove(*ctrl_b);
	
		if(splitter_a == splitter_b) {
			splitter_a->Set(idx_a < idx_b ? *ctrl_b : *ctrl_a,
			                idx_a < idx_b ? *ctrl_a : *ctrl_b);
		}
		else {
			Ctrl* a_first = splitter_a->GetFirstChild();
			Ctrl* b_first = splitter_b->GetFirstChild();
	
			splitter_a->Set(idx_a == 0 ? *ctrl_b : *a_first,
			                idx_a == 0 ? *a_first : *ctrl_b);
	
			splitter_b->Set(idx_b == 0 ? *ctrl_a : *b_first,
			                idx_b == 0 ? *b_first : *ctrl_a);
		}
		splitter_a->SetPos(pos_a);
		splitter_b->SetPos(pos_b);
		
	}
	else
	if(splitter_a && !splitter_b) {
		ctrl_b->Remove();
		ctrl_a->Hide();
		splitter_a->Remove(*ctrl_a);
		Ctrl::Add(ctrl_a->SizePos());
		Ctrl* a_first = splitter_a->GetFirstChild();
		splitter_a->Set(idx_a == 0 ? *ctrl_b : *a_first,
						idx_a == 0 ? *a_first : *ctrl_b);
		splitter_a->SetPos(pos_a).Show();
	}
	else
	if(splitter_b && !splitter_a) {
		ctrl_a->Remove();
		ctrl_b->Hide();
		Ctrl::Add(ctrl_b->SizePos());
		Ctrl* b_first = splitter_b->GetFirstChild();
		splitter_b->Set(idx_b == 0 ? *ctrl_a : *b_first,
						idx_b == 0 ? *b_first : *ctrl_a);
		splitter_b->SetPos(pos_b).Show();
	}
	
	WhenSwap(a, b);
	list.Swap(a, b);
}

void Stacker::Swap(Ctrl& a, Ctrl& b)
{
	Swap(Find(a), Find(b));
}

void Stacker::SwapNext()
{
	int i = Find(*activectrl);
	Swap(i, i + 1);
}

void Stacker::SwapPrev()
{
	int i = Find(*activectrl);
	Swap(i, i - 1);
}

void Stacker::SwapPanes()
{
	if(Splitter *sp = GetParentSplitter(activectrl); sp) {
		if(Ctrl *l = sp->GetFirstChild(), *r = sp->GetLastChild(); l && r) {
			activectrl = l->HasFocus() ? l  : r;
			Swap(Find(*l), Find(*r));
			activectrl->SetFocus();
		}
	}
}

void Stacker::ExpandTopLeftPane()
{
	if(Splitter *sp = GetParentSplitter(activectrl); sp)
		sp->SetPos(clamp(sp->GetPos() + 500, 0, 10000));
}

void Stacker::ExpandBottomRightPane()
{
	if(Splitter *sp = GetParentSplitter(activectrl); sp)
		sp->SetPos(clamp(sp->GetPos() - 500, 0, 10000));
}

void Stacker::ResetSplitterPos()
{
	if(Splitter *sp = GetParentSplitter(activectrl); sp)
		sp->SetPos(5000);
}

void Stacker::Goto(int i)
{
	if(i >= 0 && i < GetCount()) {
		Activate(list[i]);
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

void Stacker::Activate(Ctrl *ctrl)
{
	if(!ctrl)
		return;
	
	if(!activectrl)
		activectrl = ctrl;
	
	Splitter *currentsp = GetParentSplitter(activectrl);
	Splitter *nextsp = GetParentSplitter(ctrl);

	if(activectrl == ctrl) {
		currentsp ? currentsp->Show() : activectrl->Show();
		activectrl->SetFocus();
		return;
	}
	
	GuiLock __;

	if(ctrl != activectrl && ((currentsp != nextsp) || (!currentsp && !nextsp)) && duration >= 100) {
		// Animate the splitter if present, otherwise animate the control
		Ctrl* nextctrl = nextsp ? static_cast<Ctrl*>(nextsp) : ctrl;
		Ctrl* currentctrl = currentsp ? static_cast<Ctrl*>(currentsp) : activectrl;
		Animate(currentctrl, nextctrl, IsNext(ctrl));
	}

	currentsp
		? currentsp->Hide()
			: activectrl->Hide();

	activectrl = ctrl;

	nextsp
		? nextsp->Show()
			: activectrl->Show();

	activectrl->SetFocus();
}

bool Stacker::IsNext(Ctrl *nextctrl) const
{
	// Handle cyclic navigation
	int curr  = FindIndex(list, activectrl);
	int next  = FindIndex(list, nextctrl);
	int total = list.GetCount();
	return (curr < next) || (total > 2 && curr == total - 1 && next == 0);
}

void Stacker::Animate(Ctrl *currentctrl, Ctrl *nextctrl, bool forward)
{
	if(animating)
		return;

	animating = true;
	
	Rect view = GetView();
	Size size = view.GetSize();
	
	// Prepare source and destination rectangles
	Rect rsrc1 = view;
	Rect rdst1 = view;
	Rect rsrc2 = view;
	Rect rdst2 = view;

	// Offset rectangles based on animation direction
	if(vertical) {
		rdst1.OffsetVert(forward ? -size.cy :  size.cy);
		rsrc2.OffsetVert(forward ?  size.cy : -size.cy);
	}
	else {
		rdst1.OffsetHorz(forward ? -size.cx :  size.cx);
		rsrc2.OffsetHorz(forward ?  size.cx : -size.cx);
	}

	// Prepare new control
	nextctrl->SetRect(rsrc2);
	nextctrl->Show();

	// Animation loop
	for(int start = msecs();;) {
		int elapsed = msecs(start);
		if(elapsed > duration)
			break;
		Rect r1 = rsrc1, r2 = rsrc2;
		r1 += (rdst1 - rsrc1) * elapsed / duration; // Lerp
		r2 += (rdst2 - rsrc2) * elapsed / duration; // Lerp
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

Splitter *Stacker::GetParentSplitter(Ctrl *c) const
{
	return c ? dynamic_cast<Splitter*>(c->GetParent()) : nullptr;
}

void Stacker::RemoveSplitter(const Splitter& s)
{
	if(int i = FindMatch(splitters, [&](const Splitter& q) { return &q == &s; }); i >= 0)
		splitters.Remove(i);
}

}
