// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2026, İsmail Yılmaz

#ifndef _MultiSplitter_h_
#define _MultiSplitter_h_
	
class MultiSplitter : public Ctrl {
public:
	MultiSplitter();
	virtual ~MultiSplitter();
	
	Event<> WhenSplitFinish;

	MultiSplitter& Horz(Ctrl& left, Ctrl& right, int pos = 5000);
	MultiSplitter& Vert(Ctrl& top, Ctrl& bottom, int pos = 5000);
	MultiSplitter& Horz(Ctrl& n, Ctrl& left, Ctrl& right, int pos = 5000);
	MultiSplitter& Vert(Ctrl& n, Ctrl& top, Ctrl& bottom, int pos = 5000);

	MultiSplitter& SplitLeft(Ctrl& n, Ctrl& ctrl, int pos = 5000)   { return Horz(n, ctrl, n, pos); }
	MultiSplitter& SplitRight(Ctrl& n, Ctrl& ctrl, int pos = 5000)  { return Horz(n, n, ctrl, pos); }
	MultiSplitter& SplitTop(Ctrl& n, Ctrl& ctrl, int pos = 5000)    { return Vert(n, ctrl, n, pos); }
	MultiSplitter& SplitBottom(Ctrl& n, Ctrl& ctrl, int pos = 5000) { return Vert(n, n, ctrl, pos); }

	void Remove(Ctrl& ctrl);
	void RemovePane(Ctrl& ctrl)                          { Remove(ctrl); }
	void Clear();

	int           GetCount() const;
	Vector<Ctrl*> GetPanes() const;
	Ctrl*         GetFirstPane() const;
	Ctrl*         GetSibling(const Ctrl& ctrl) const;
	int           FindPane(const Ctrl& ctrl) const;

	void          Swap(Ctrl& a, Ctrl& b);
	void          Replace(Ctrl& oldctrl, Ctrl& newctrl);

	void          ToggleOrientation(Ctrl& ctrl);
	int           GetPos(const Ctrl& ctrl) const;
	void          SetPos(Ctrl& ctrl, int pos);

	int            GetSplitWidth() const                 { return chstyle->width; }
	MultiSplitter& SetStyle(const Splitter::Style& s);

	static const Splitter::Style& StyleDefault()         { return Splitter::StyleDefault(); }

	void  Layout() override;
	void  Paint(Draw& w) override;
	void  MouseMove(Point p, dword keyflags) override;
	void  LeftDown(Point p, dword keyflags) override;
	void  LeftUp(Point p, dword keyflags) override;
	Image CursorImage(Point p, dword keyflags) override;
	
private:
	struct Node {
		Ctrl     *ctrl = nullptr;
		One<Node> left;
		One<Node> right;
		Rect      rect;
		Rect      bar;
		int       pos  = 5000;
		bool      vert = false;
		bool IsLeaf() const { return !left && !right; }
	};

	template <class T, class Fn>
	static T* FindNode(T *nd, Fn&& fn)
	{
		if(!nd)
			return nullptr;
		if(fn(*nd))
			return nd;
		if(T *r = FindNode(~nd->left, fn))
			return r;
		return FindNode(~nd->right, fn);
	}

	const Node* FindLeaf(const Ctrl& ctrl) const;
	Node*      FindLeaf(const Ctrl& ctrl);
	const Node* FindSplitOf(const Ctrl& ctrl) const;
	Node*      FindSplitOf(const Ctrl& ctrl);
	const Node* FindBar(Point p) const;
	Node*       FindBar(Point p);
	void        LayoutNode(Node& nd, Rect r);
	void        Split(Ctrl *target, Ctrl& a, Ctrl& b, int pos, bool vert);

	static void PaintDots(Draw& w, const Rect& r, bool vert);

	One<Node>              root;
	Node                  *dragnode;
	Node                  *hoverbar;
	const Splitter::Style *chstyle;
};

#endif