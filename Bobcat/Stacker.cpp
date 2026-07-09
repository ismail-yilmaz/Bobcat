// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

Stacker::Stacker()
: activectrl(nullptr)
, duration(0)
, maxpanes(2)
, wheel(false)
, vertical(false)
, splitvert(false)
, animating(false)
, swapping(false)
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
	Splitter *sp = GetParentSplitter(activectrl);

	if(sp && sp->GetCount() >= maxpanes)
		return *this;

	if(int i = GetCursor(); i >= 0) {
		list.Insert(i + 1, &ctrl);

		if(sp)
			sp->Add(ctrl);
		else {
			if(list[i])
				list[i]->Remove();

			Splitter& s = splitters.Add();
			splitvert ? s.Vert() : s.Horz();
			Ctrl::Add(s.SizePos());

			if(list[i])
				s.Add(*list[i]);

			s.Add(ctrl);
		}
		WhenInsert(i + 1, ctrl);
		ActivatePane(&ctrl);
	}

	return *this;
}

void Stacker::Remove(Ctrl& ctrl)
{
	LLOG("Remove(" << &ctrl << ")");

	int i = Find(ctrl);

	if(!GetParentSplitter(&ctrl))
		workspacenames.RemoveKey(&ctrl);

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
	if(i < 0 || i >= list.GetCount())
		return false;

	Ctrl *ctrl = list[i];
	if(!ctrl)
		return false;

	if(Splitter *s = GetParentSplitter(ctrl); s) {
		bool wasroot = (s->GetFirstChild() == ctrl);
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
			Ctrl* newroot = s->GetFirstChild();
			if(newroot)
				workspacenames.Add(newroot, oldname);
		}

		if(s->GetCount() == 1) {
			Ctrl* remaining = s->GetFirstChild();
			RemoveChild(s);
			if(remaining) {
				s->Remove(*remaining);
				if(int j = Find(*remaining); j >= 0) {
					activectrl = list[j];
					if(activectrl) {
						Ctrl::Add(activectrl->SizePos());
						ActivatePane(activectrl);
					}
				}
			}
			RemoveSplitter(*s);
		}
		else
		if(s->GetCount() > 1 && ctrl == activectrl)
			ActivatePane(s->GetFirstChild());

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
		Splitter* sp = GetParentSplitter(list[i]);
		if(!sp || sp->GetFirstChild() == list[i])
			count++;
	}
	return count;
}

int Stacker::FindWorkspace(Ctrl& ctrl) const
{
	Splitter* sp = GetParentSplitter(&ctrl);
	Ctrl* root = sp ? sp->GetFirstChild() : &ctrl;
	if(!root)
		return -1;
	return FindIndex(list, root);
}

int Stacker::FindInWorkspace(int wsid, Ctrl& ctrl) const
{
	if(wsid < 0 || wsid >= list.GetCount())
		return -1;

	Splitter* sp = GetParentSplitter(list[wsid]);
	if(!sp)
		return (&ctrl == list[wsid]) ? 0 : -1;

	int idx = 0;
	for(Ctrl* c = sp->GetFirstChild(); c; c = c->GetNext(), ++idx)
		if(c == &ctrl)
			return idx;

	return -1;
}

Vector<Ctrl*> Stacker::GetWorkspacePanes(int wsid) const
{
	Vector<Ctrl*> panes;
	if(wsid >= 0 && wsid < list.GetCount()) {
		Splitter* sp = GetParentSplitter(list[wsid]);
		if(sp) {
			for(Ctrl* c = sp->GetFirstChild(); c; c = c->GetNext())
				if(c)
					panes.Add(c);
		}
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
		Ctrl* root = list[wsid];
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
		Ctrl* root = list[wsid];
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
			Ctrl* root = workspacenames.GetKey(i);
			return FindIndex(list, root);
		}
	}
	return -1;
}

void Stacker::MovePanePrev()
{
	if(!activectrl)
		return;

	if(Splitter* sp = GetParentSplitter(activectrl)) {
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

	if(Splitter* sp = GetParentSplitter(activectrl)) {
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

	Splitter *splittera = GetParentSplitter(ctrla);
	Splitter *splitterb = GetParentSplitter(ctrlb);

	Vector<Ctrl*> childrena, childrenb;
	Vector<int> posesa, posesb;

	if(splittera) {
		for(Ctrl *c = splittera->GetFirstChild(); c; c = c->GetNext())
			childrena.Add(c);
		for(int i = 0; i < childrena.GetCount() - 1; ++i)
			posesa.Add(splittera->GetPos(i));
	}

	if(splitterb && splitterb != splittera) {
		for(Ctrl *c = splitterb->GetFirstChild(); c; c = c->GetNext())
			childrenb.Add(c);
		for(int i = 0; i < childrenb.GetCount() - 1; ++i)
			posesb.Add(splitterb->GetPos(i));
	}

	if(splittera && splitterb) {
		if(splittera == splitterb) {
			int idxa = FindIndex(childrena, ctrla);
			int idxb = FindIndex(childrena, ctrlb);

			if(idxa >= 0 && idxb >= 0) {
				for(Ctrl *c : childrena)
					splittera->Remove(*c);

				childrena.Swap(idxa, idxb);

				for(Ctrl *c : childrena)
					splittera->Add(*c);
				for(int i = 0; i < posesa.GetCount(); ++i)
					splittera->SetPos(posesa[i], i);
			}
		}
		else {
			int idxa = FindIndex(childrena, ctrla);
			int idxb = FindIndex(childrenb, ctrlb);

			if(idxa >= 0 && idxb >= 0) {
				for(Ctrl *c : childrena)
					splittera->Remove(*c);
				for(Ctrl *c : childrenb)
					splitterb->Remove(*c);

				childrena[idxa] = ctrlb;
				childrenb[idxb] = ctrla;

				for(Ctrl *c : childrena)
					splittera->Add(*c);
				for(int i = 0; i < posesa.GetCount(); ++i)
					splittera->SetPos(posesa[i], i);

				for(Ctrl *c : childrenb)
					splitterb->Add(*c);
				for(int i = 0; i < posesb.GetCount(); ++i)
					splitterb->SetPos(posesb[i], i);
			}
		}
	}
	else
	if(splittera && !splitterb) {
		int idxa = FindIndex(childrena, ctrla);

		if(idxa >= 0) {
			for(Ctrl *c : childrena)
				splittera->Remove(*c);

			ctrlb->Remove();
			ctrla->Hide();
			Ctrl::Add(ctrla->SizePos());

			childrena[idxa] = ctrlb;

			for(Ctrl *c : childrena)
				splittera->Add(*c);
			for(int i = 0; i < posesa.GetCount(); ++i)
				splittera->SetPos(posesa[i], i);

			splittera->Show();
		}
	}
	else
	if(splitterb && !splittera) {
		int idxb = FindIndex(childrenb, ctrlb);

		if(idxb >= 0) {
			for(Ctrl *c : childrenb)
				splitterb->Remove(*c);

			ctrla->Remove();
			ctrlb->Hide();
			Ctrl::Add(ctrlb->SizePos());

			childrenb[idxb] = ctrla;

			for(Ctrl *c : childrenb)
				splitterb->Add(*c);
			for(int i = 0; i < posesb.GetCount(); ++i)
				splitterb->SetPos(posesb[i], i);

			splitterb->Show();
		}
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
	if(Splitter *sp = GetParentSplitter(activectrl); sp && sp->GetCount() >= 2) {
		Ctrl *first = sp->GetFirstChild();
		Ctrl *last = sp->GetLastChild();
		if(first && last) {
			activectrl = first->HasFocus() ? first : last;
			if(activectrl) {
				Swap(*first, *last);
				activectrl->SetFocus();
			}
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

Ctrl* Stacker::GetPassiveCtrl() const
{
	Splitter *sp = GetParentSplitter(activectrl);
	if(!sp)
		return nullptr;

	return sp->GetFirstChild() == activectrl ? sp->GetFirstChild()->GetNext() : sp->GetFirstChild();
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

	Splitter *currentsp = GetParentSplitter(activectrl);
	Splitter *nextsp = GetParentSplitter(ctrl);

	if(activectrl == ctrl) {
		currentsp ? currentsp->Show() : activectrl->Show();
		activectrl->SetFocus();
		return;
	}

	GuiLock __;

	if(ctrl != activectrl && ((currentsp != nextsp) || (!currentsp && !nextsp)) && duration >= 100) {
		Ctrl* nextctrl = nextsp ? static_cast<Ctrl*>(nextsp) : ctrl;
		Ctrl* currentctrl = currentsp ? static_cast<Ctrl*>(currentsp) : activectrl;
		Animate(currentctrl, nextctrl, IsNext(ctrl));
	}

	currentsp ? currentsp->Hide() : activectrl->Hide();
	activectrl = ctrl;
	nextsp ? nextsp->Show() : activectrl->Show();
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