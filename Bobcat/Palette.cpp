// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2025, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

constexpr const char *sSampleText = "AaZz09...";

struct DefaulPaletteNameDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		StdDisplay().Paint(w, r, AttrText(q).SetImage(Images::ColorSwatch()).Bold(), ink, paper, style);
	}
};

struct NormalPaletteNameDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		StdDisplay().Paint(w, r, AttrText(q).SetImage(Images::ColorSwatch()), ink, paper, style);
	}
};

struct NormalPaletteSampleDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const final
	{
		const Palette& p = q.To<Palette>();
		ink   = p.table[TerminalCtrl::COLOR_INK];
		paper = p.table[TerminalCtrl::COLOR_PAPER];
		StdCenterDisplay().Paint(w, r, AttrText(sSampleText).SetFont(Monospace()), ink, paper, style);
	}
};

const Display& DefaultPaletteNameDisplay()   { return Single<DefaulPaletteNameDisplayCls>();  }
const Display& NormalPaletteNameDisplay()    { return Single<NormalPaletteNameDisplayCls>();  }
const Display& NormalPaletteSampleDisplay()  { return Single<NormalPaletteSampleDisplayCls>();}

static const Vector<Tuple<Color, const char*, const char*>>& GetColorList()
{
	static Vector<Tuple<Color, const char*, const char*>> sColorTable = {
		{ Black(),               "Color_1", t_("Black") },
		{ Red(),                 "Color_2", t_("Red") },
		{ Green(),               "Color_3", t_("Green") },
		{ Yellow(),              "Color_4", t_("Yellow") },
		{ Blue(),                "Color_5", t_("Blue") },
		{ Magenta(),             "Color_6", t_("Magenta") },
		{ Cyan(),                "Color_7", t_("Cyan") },
		{ White(),               "Color_8", t_("White") },
		{ Gray(),                "Color_9", t_("Gray") },
		{ LtRed(),               "Color_10", t_("Bright Red") },
		{ LtGreen(),             "Color_11", t_("Bright Green") },
		{ LtYellow(),            "Color_12", t_("Bright Yellow") },
		{ LtBlue(),              "Color_13", t_("Bright Blue") },
		{ LtMagenta(),           "Color_14", t_("Bright Magenta") },
		{ LtCyan(),              "Color_15", t_("Bright Cyan") },
		{ White(),               "Color_16", t_("Bright White") },
		{ SColorText,            "Ink"     , t_("Ink") },
		{ SColorHighlightText,   "SelectionInk", t_("Selection Ink") },
		{ SColorPaper,           "Paper"       , t_("Paper") },
		{ SColorHighlight,       "SelectionPaper", t_("Selection Paper") },
		{ SYellow,               "AnnotationUnderline", t_("Annotation Underline") },
		{ LtRed(),               "HighlightInk", t_("Highlight Ink") },
		{ SColorHighlightText,   "HighlightCursorInk", t_("Highlight Cursor Ink") },
		{ Yellow(),              "HighlightPaper", t_("Highlight Paper") },
		{ SColorHighlight,       "HighlightCursorPaper", t_("Highlight Cursor Paper") },
	};
	return sColorTable;
}

Palette::Palette()
: order(0)
{
	const auto& lst = GetColorList();
	for(int i = 0; i < lst.GetCount(); i++)
		table[i] = lst[i].a;
}

void Palette::Jsonize(JsonIO& jio)
{
    jio("Name", name)
       ("Order", order);
	
	const auto& lst = GetColorList();
	
	if(jio.IsLoading()) {
		for(int i = 0; i < lst.GetCount(); i++) {
			String q;
			jio(lst[i].b, q);
			table[i] = Nvl((Color) ConvertHashColorSpec().Scan(q), lst[i].a);
		}
	}
	else
	if(jio.IsStoring()) {
		for(int i = 0; i < lst.GetCount(); i++) {
			String s = ConvertHashColorSpec().Format(table[i]);
			jio(lst[i].b, s);
		}
	}
}

Palettes::Palettes()
{
	int cx = Zx(GetTextSize(sSampleText, Monospace()).cx + Zx(8));
	Ctrl::Add(list.SizePos());
	list.AddColumn(tt_("Name"));
	list.AddColumn(tt_("Sample")).SetDisplay(NormalPaletteSampleDisplay()).HeaderTab().Fixed(cx);
	list.Moving().Removing().Track().NoAskRemove().NoHeader().AutoHideSb();
	list.VertGrid(false).HorzGrid(false).SetLineCy(24).SetFrame(0, toolbar);
	list.WhenLeftDouble = [this]() { GetCtrl() ? MakeActive() : Edit(); };
	list.WhenSel        = [this]() { Sync(); Action(); };
	list.WhenBar        = [this](Bar& bar) { ContextMenu(bar); };
	list.WhenDrag = [this] { Drag(); };
	list.WhenDropInsert = [=](int line, PasteClip& d) { DnDInsert(line, d); };
	Load();
	Sync();
}

void Palettes::ContextMenu(Bar& bar)
{
	bool b = list.IsCursor();
	bool q = b && list.GetCursor() < list.GetCount() - 1;
	bar.Add(tt_("Add palette"), Images::Add(), [this]() { Add(); }).Key(K_INSERT);
	bar.Add(tt_("Clone palette"), Images::Copy(), [this]() { Clone(); }).Key(K_CTRL|K_C);
	bar.Add(b, tt_("Edit palette"), Images::Edit(), [this]() { Edit(); }).Key(K_SPACE);
	bar.Add(tt_("Rename palette"), Images::Rename(), [this]() { Rename(); }).Key(K_F2);
	bar.Add(b, tt_("Remove palette"), Images::Delete(), [this]() { Remove(); }).Key(K_DELETE);
	bar.Add(list.GetCursor() > 0, tt_("Move up"), Images::Up(), [this]() { list.SwapUp(); }).Key(K_CTRL_UP);
	bar.Add(q, tt_("Move down"), Images::Down(), [this]() { list.SwapDown(); }).Key(K_CTRL_DOWN);
	if(bar.IsMenuBar()) {
		bar.Separator();
		bar.Add(b, tt_("Set as active palette"), [this]() { MakeActive(); }).Key(K_CTRL|K_D);
		bar.Separator();
		bar.Add(tt_("Open palettes directory"), Images::Directory(), []{ LaunchWebBrowser(PaletteDir()); }).Key(K_CTRL_HOME);

	}
}

void Palettes::Add()
{
	if(String s; EditTextNotNull(s, t_("New Palette"), t_("Name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Profile named \"" + s + "\" already exists.");
		} else {
			list.Add(s, RawToValue(Palette(s)));
			list.GoEnd();
			Edit();
		}
	}
}

void Palettes::Edit()
{
	SetPalette();
}

void Palettes::Clone()
{
	if(String s; EditTextNotNull(s, t_("Clone Palette"), t_("Name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Palette named \"" + s + "\" already exists.");
		} else {
			Palette source = list.Get(1).To<Palette>();
			Palette p(source);
			p.name = s;
			list.Add(s, RawToValue(p));
			list.GoEnd();
		}
	}
	Sync();
}

void Palettes::Rename()
{
	if(!list.IsCursor())
		return;
	if(String s = list.GetKey(); EditTextNotNull(s, t_("Rename Palette"), t_("New name"))) {
		if(list.Find(s) >= 0) {
			Exclamation("Palette named \"" + s + "\" already exists.");
		} else {
			Palette p = list.Get(1).To<Palette>();
			String f = PaletteFile(p.name);
			if(FileExists(f))
				DeleteFile(f);
			p.name = s;
			list.Set(0, p.name);
			list.Set(1, RawToValue(p));
		}
	}
	Sync();
}

void Palettes::Remove()
{
	const char* msg = t_("Do you really want to delete palette '%s'?");
	if(!list.IsCursor())
		return;
	String s = list.GetKey();
	if(PromptYesNo(DeQtf(Format(msg, s)))) {
		String f = PaletteFile(s);
		if(FileExists(f))
			DeleteFile(f);
		list.Remove(list.GetCursor());
	}
	Sync();
}

void Palettes::SetPalette()
{
	if(!list.IsCursor())
		return;

	WithColorPaletteLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, tt_("Color Profile"));
	dlg.Sizeable();

	dlg.colors.AddColumn(tt_("Description"));
	dlg.colors.AddColumn(tt_("Color")).Ctrls<ColorPusher>();
	dlg.colors.ColumnWidths("100 24");

	Palette p = list.Get(list.GetCursor(), 1).To<Palette>();
	
	auto LoadColors = [&dlg](const Palette& q)
	{
		int cursor = dlg.colors.GetCursor();
		dlg.colors.Clear();
		const auto& lst = GetColorList();
		for(int i = 0; i < Palette::MAX_COLOR_COUNT; i++)
			dlg.colors.Add(lst[i].c, q.table[i]);
		if(cursor >= 0)
			dlg.colors.SetCursor(cursor);
	};

	dlg.reset << [&] { LoadColors(Palette()); };

	LoadColors(p);

	if(dlg.Title(Format(tt_("Color Profile: %"), p.name)).ExecuteOK()) {
		for(int i = 0; i < dlg.colors.GetCount(); i++)
			p.table[i] = dlg.colors.Get(i, 1);
		if(list.FindSetCursor(p.name)) {
			list.Set(list.GetCursor(), 1, RawToValue(p));
		}
		else {
			list.Add(p.name, RawToValue(p));
			list.GoEnd();
		}
		Store();
	}
}

void Palettes::MakeActive()
{
	if(int i = list.GetCursor(); i >= 0) {
		data = list.Get(i, 0);
		Sync();
	}
}

void Palettes::Sync()
{
	toolbar.Set([this](Bar& bar) { ContextMenu(bar); });
	for(int i = 0; i < list.GetCount(); i++) {
		list.SetDisplay(i, 0,  list.Get(i, 0) == data ? DefaultPaletteNameDisplay() : NormalPaletteNameDisplay());
	}
	Action();
}

void Palettes::Drag()
{
	if(list.DoDragAndDrop(InternalClip(list, "palettelist"), list.GetDragSample()) == DND_MOVE)
		list.RemoveSelection();
}

void Palettes::DnDInsert(int line, PasteClip& d)
{
	if(AcceptInternal<ArrayCtrl>(d, "palettelist")) {
		const ArrayCtrl& src = GetInternal<ArrayCtrl>(d);
		bool self = &src == &list;
		Vector<Vector<Value>> data;
		for(int i = 0; i < src.GetCount(); i++)
			if(src.IsSel(i)) {
				Value pname = src.Get(i, 0);
				Value pbody = src.Get(i, 1);
				auto& q = data.Add();
				q.Add(pname);
				q.Add(pbody);
			}
		list.InsertDrop(line, data, d, self);
		list.SetFocus();
	}
}

void Palettes::SetData(const Value& data_)
{
	if(data = data_; !list.FindSetCursor(data)) {
		list.GoBegin();
		if(list.GetCount())
			data = list.Get(0, 0);
	}
}

Value Palettes::GetData() const
{
	Store();
	return data;
}

int Palettes::Load()
{
	VectorMap<String, Palette> palettes;
	int rc = LoadPalettes(palettes);
	if(palettes.GetCount()) {
		list.Clear();
		for(const auto& p : ~palettes)
			list.Add(p.key, RawToValue(clone(p.value)));
		list.Sort(1, [](const Value& a, const Value& b) -> int { return a.To<Palette>().order - b.To<Palette>().order; });
		list.FindSetCursor(data);
		Sync();
	}
	return rc;
}

void Palettes::Store() const
{
	for(int i = 0; i < list.GetCount(); i++) {
		String  s = list.Get(i, 0).To<String>();
		Palette p = list.Get(i, 1).To<Palette>();
		p.order = i;
		try
		{
			JsonIO jio;
			p.Jsonize(jio);
			String path = PaletteFile(s);
			if(!FileExists(path))
				RealizePath(path);
			SaveFile(path, (String) AsJSON(jio.GetResult(), true));
		}
		catch(const JsonizeError& e)
		{
			LLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			LLOG("Value type error: " << e);
		}
	}
}

Palette LoadPalette(const String& name)
{
	Palette p;
	for(const FindFile& f : FindFile(PaletteFile(name))) {
		try
		{
			Value q = ParseJSON(LoadFile(f.GetPath()));
			JsonIO jio(q);
			p.Jsonize(jio);
		}
		catch(const JsonizeError& e)
		{
			LLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			LLOG("Value type error: " << e);
		}
		catch(const CParser::Error& e)
		{
			LLOG("Parser error: " << e);
		}
		catch(...)
		{
			LLOG("Unknown exception");
		}
	}
	return pick(p);
}

int LoadPalettes(VectorMap<String, Palette>& v)
{
	int failures = 0;
	for(const String& s : GetPaletteFilePaths()) {
		try
		{
			Value q = ParseJSON(LoadFile(s));
			JsonIO jio(q);
			v.GetAdd(GetFileTitle(s)).Jsonize(jio);
		}
		catch(const JsonizeError& e)
		{
			failures++;
			LLOG(e);
		}
		catch(const ValueTypeError& e)
		{
			failures++;
			LLOG(e);
		}
		catch(const CParser::Error& e)
		{
			failures++;
			LLOG(e);
		}
		catch(...)
		{
			LLOG("Unknown exception");
		}
	}
	return v.GetCount() ? failures : -1;
}

String PaletteDir()
{
	return ConfigFile("palettes");
}

String PaletteFile(const String& name)
{
	return AppendFileName(PaletteDir(), name + ".palette");
}

Vector<String> GetPaletteFilePaths()
{
	return FindAllPaths(PaletteDir(), "*.palette");
}

Vector<String> GetPaletteNames()
{
	Vector<String> q;
	for(const String& s : GetPaletteFilePaths())
		q.Add(GetFileTitle(s));
	return q;
}

}