// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)    // RLOG(x)
#define LDUMP(x)   // RDUMP(x)
#define LTIMING(x) // RTIMING(x)

namespace Upp {

MultiSplitter::MultiSplitter()
: dragnode(nullptr)
, chstyle(&StyleDefault())
{
	SetFrame(NullFrame());
	NoWantFocus();
}

MultiSplitter::~MultiSplitter()
{
	Clear();
}

const MultiSplitter::Node *MultiSplitter::FindLeaf(const Ctrl& ctrl) const
{
	return FindNode(~root, [&](const Node& nd) {
		return nd.IsLeaf() && nd.ctrl == &ctrl;
	});
}

MultiSplitter::Node *MultiSplitter::FindLeaf(const Ctrl& ctrl)
{
	return FindNode(~root, [&](const Node& nd) {
		return nd.IsLeaf() && nd.ctrl == &ctrl;
	});
}

const MultiSplitter::Node *MultiSplitter::FindSplitOf(const Ctrl& ctrl) const
{
	return FindNode(~root, [&](const Node& nd) {
		return !nd.IsLeaf() && (nd.left->ctrl == &ctrl || nd.right->ctrl == &ctrl);
	});
}

MultiSplitter::Node *MultiSplitter::FindSplitOf(const Ctrl& ctrl)
{
	return FindNode(~root, [&](const Node& nd) {
		return !nd.IsLeaf() && (nd.left->ctrl == &ctrl || nd.right->ctrl == &ctrl);
	});
}

const MultiSplitter::Node *MultiSplitter::FindBar(Point p) const
{
	int pad = chstyle->width >> 1;
	return FindNode(~root, [&](const Node& nd) {
		if(nd.IsLeaf())
			return false;
		Rect r = nd.bar.Inflated(nd.vert ? Size(0, pad) : Size(pad, 0));
		return r.Contains(p);
	});
}

MultiSplitter::Node *MultiSplitter::FindBar(Point p)
{
	int pad = chstyle->width >> 1;
	return FindNode(~root, [&](const Node& nd) {
		if(nd.IsLeaf())
			return false;
		Rect r = nd.bar.Inflated(nd.vert ? Size(0, pad) : Size(pad, 0));
		return r.Contains(p);
	});
}

void MultiSplitter::LayoutNode(Node& nd, Rect r)
{
	nd.rect = r;
	if(nd.IsLeaf()) {
		if(nd.ctrl)
			nd.ctrl->SetRect(r);
		return;
	}

	int w = GetSplitWidth();
	if(nd.vert) {
		int h = max(0, r.GetHeight() - w);
		int y = r.top + h * nd.pos / 10000;
		int b = min(r.bottom, y + w);
		LayoutNode(*nd.left, Rect(r.left, r.top, r.right, y));
		nd.bar = Rect(r.left, y, r.right, b);
		LayoutNode(*nd.right, Rect(r.left, b, r.right, r.bottom));
	}
	else {
		int cx = max(0, r.GetWidth() - w);
		int x  = r.left + cx * nd.pos / 10000;
		int rt = min(r.right, x + w);
		LayoutNode(*nd.left, Rect(r.left, r.top, x, r.bottom));
		nd.bar = Rect(x, r.top, rt, r.bottom);
		LayoutNode(*nd.right, Rect(rt, r.top, r.right, r.bottom));
	}
}

void MultiSplitter::Layout()
{
	if(root)
		LayoutNode(*root, GetSize());
}

void MultiSplitter::PaintDots(Draw& w, const Rect& r, bool vert)
{
	int x = r.left + r.GetWidth() / 2;
	int y = r.top + r.GetHeight() / 2;
	(vert ? x : y) -= 8 * 4;
	for(int i = 0; i < 16; i++) {
		w.DrawRect(x - 1, y - 1, 1, 1, SColorShadow());
		w.DrawRect(x, y, 1, 1, SColorShadow());
		(vert ? x : y) += 4;
	}
}

void MultiSplitter::Paint(Draw& w)
{
	if(!IsTransparent())
		w.DrawRect(GetSize(), SColorFace);

	FindNode(~root, [&](const Node& nd) {
		if(!nd.IsLeaf() && w.IsPainting(nd.bar)) {
			const Value *ch = nd.vert ? chstyle->vert : chstyle->horz;
			if(HasCapture() && dragnode == &nd)
				ChPaint(w, nd.bar, ch[1]);
			else
			if(!IsTransparent())
				ChPaint(w, nd.bar, ch[0]);
			if(chstyle->dots)
				PaintDots(w, nd.bar, nd.vert);
		}
		return false;
	});
}

Image MultiSplitter::CursorImage(Point p, dword keyflags)
{
	Node *nd = dragnode ? dragnode : FindBar(p);
	return nd ? (nd->vert ? Image::SizeVert() : Image::SizeHorz()) : Image::Arrow();
}

void MultiSplitter::LeftDown(Point p, dword keyflags)
{
	if(dragnode = FindBar(p); dragnode) {
		SetCapture();
		Refresh(dragnode->bar);
	}
}

void MultiSplitter::LeftUp(Point, dword keyflags)
{
	if(dragnode) {
		Rect r = dragnode->bar;
		dragnode = nullptr;
		ReleaseCapture();
		Refresh(r);
		WhenSplitFinish();
	}
}

void MultiSplitter::MouseMove(Point p, dword keyflags)
{
	if(HasCapture() && dragnode) {
		int w = GetSplitWidth();
		Refresh(dragnode->bar);
		dragnode->pos = clamp(dragnode->vert
			? (p.y - dragnode->rect.top - w / 2) * 10000 / max(1, dragnode->rect.GetHeight() - w)
			: (p.x - dragnode->rect.left - w / 2) * 10000 / max(1, dragnode->rect.GetWidth() - w),
			0, 10000);
		Layout();
		Refresh(dragnode->bar);
	}
}

void MultiSplitter::Split(Ctrl *target, Ctrl& a, Ctrl& b, int pos, bool vert)
{
	Node *nd = target ? FindLeaf(*target) : nullptr;
	if(!nd) {
		Clear();
		nd = &root.Create();
	}
	else
	if(nd->ctrl && nd->ctrl != &a && nd->ctrl != &b)
		RemoveChild(nd->ctrl);

	if(a.GetParent() != this)
		Add(a);
	if(b.GetParent() != this)
		Add(b);

	nd->ctrl = nullptr;
	nd->vert = vert;
	nd->pos  = clamp(pos, 0, 10000);
	nd->left.Create().ctrl  = &a;
	nd->right.Create().ctrl = &b;
	Layout();
}

MultiSplitter& MultiSplitter::Horz(Ctrl& left, Ctrl& right, int pos)
{
	Split(nullptr, left, right, pos, false);
	return *this;
}

MultiSplitter& MultiSplitter::Vert(Ctrl& top, Ctrl& bottom, int pos)
{
	Split(nullptr, top, bottom, pos, true);
	return *this;
}

MultiSplitter& MultiSplitter::Horz(Ctrl& n, Ctrl& left, Ctrl& right, int pos)
{
	Split(&n, left, right, pos, false);
	return *this;
}

MultiSplitter& MultiSplitter::Vert(Ctrl& n, Ctrl& top, Ctrl& bottom, int pos)
{
	Split(&n, top, bottom, pos, true);
	return *this;
}

void MultiSplitter::Remove(Ctrl& ctrl)
{
	if(ctrl.GetParent() == this)
		RemoveChild(&ctrl);

	if(!root)
		return;

	if(root->IsLeaf()) {
		if(root->ctrl == &ctrl) {
			root.Clear();
			dragnode = nullptr;
		}
		return;
	}

	if(Node *nd = FindSplitOf(ctrl); nd) {
		if(dragnode == nd)
			dragnode = nullptr;
		One<Node> keep = pick(nd->left->ctrl == &ctrl ? nd->right : nd->left);
		*nd = pick(*keep);
		Layout();
	}
}

void MultiSplitter::Clear()
{
	while(GetFirstChild())
		RemoveChild(GetFirstChild());
	root.Clear();
	dragnode = nullptr;
}

int MultiSplitter::GetCount() const
{
	int n = 0;
	FindNode(~root, [&](const Node& nd) {
		n += nd.IsLeaf() && nd.ctrl;
		return false;
	});
	return n;
}

Vector<Ctrl*> MultiSplitter::GetPanes() const
{
	Vector<Ctrl*> list;
	FindNode(~root, [&](const Node& nd) {
		if(nd.IsLeaf() && nd.ctrl)
			list.Add(nd.ctrl);
		return false;
	});
	return list;
}

Ctrl *MultiSplitter::GetFirstPane() const
{
	const Node *nd = ~root;
	while(nd && !nd->IsLeaf())
		nd = ~nd->left;
	return nd ? nd->ctrl : nullptr;
}

Ctrl *MultiSplitter::GetSibling(const Ctrl& ctrl) const
{
	const Node *nd = FindSplitOf(ctrl);
	if(!nd)
		return nullptr;
	const Node *other = nd->left->ctrl == &ctrl ? ~nd->right : ~nd->left;
	while(other && !other->IsLeaf())
		other = ~other->left;
	return other ? other->ctrl : nullptr;
}

int MultiSplitter::FindPane(const Ctrl& ctrl) const
{
	return FindIndex(GetPanes(), const_cast<Ctrl*>(&ctrl));
}

void MultiSplitter::Swap(Ctrl& a, Ctrl& b)
{
	Node *na = FindLeaf(a);
	Node *nb = FindLeaf(b);
	if(na && nb && na != nb) {
		Upp::Swap(na->ctrl, nb->ctrl);
		Layout();
	}
}

void MultiSplitter::Replace(Ctrl& oldctrl, Ctrl& newctrl)
{
	if(Node *nd = FindLeaf(oldctrl); nd) {
		if(oldctrl.GetParent() == this)
			RemoveChild(&oldctrl);
		if(newctrl.GetParent() != this)
			Add(newctrl);
		newctrl.Show();
		nd->ctrl = &newctrl;
		Layout();
	}
}

void MultiSplitter::ToggleOrientation(Ctrl& ctrl)
{
	if(Node *nd = FindSplitOf(ctrl); nd) {
		nd->vert = !nd->vert;
		Layout();
	}
}

int MultiSplitter::GetPos(const Ctrl& ctrl) const
{
	const Node *nd = FindSplitOf(ctrl);
	return nd ? nd->pos : 5000;
}

void MultiSplitter::SetPos(Ctrl& ctrl, int pos)
{
	if(Node *nd = FindSplitOf(ctrl); nd) {
		nd->pos = clamp(pos, 0, 10000);
		Layout();
	}
}

MultiSplitter& MultiSplitter::SetStyle(const Splitter::Style& s)
{
	chstyle = &s;
	Layout();
	Refresh();
	return *this;
}

}