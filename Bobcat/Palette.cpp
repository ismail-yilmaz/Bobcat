// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#include "Bobcat.h"

#define LLOG(x)  // RLOG(x)
#define LDUMP(x) // RDUMP(x)

namespace Upp {

constexpr const char *sSampleText = "AaZz09...";

struct DefaulPaletteNameDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const override
	{
		StdDisplay().Paint(w, r, AttrText(q).SetImage(Images::ColorSwatch()).Bold(), ink, paper, style);
	}
};

struct NormalPaletteNameDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const override
	{
		StdDisplay().Paint(w, r, AttrText(q).SetImage(Images::ColorSwatch()), ink, paper, style);
	}
};

struct NormalPaletteSampleDisplayCls : Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const override
	{
		const Palette& p = q.To<Palette>();
		ink   = p.table[TerminalCtrl::COLOR_INK];
		paper = p.table[TerminalCtrl::COLOR_PAPER];
		StdDisplay().Paint(w, r, AttrText(sSampleText).Right().SetFont(Monospace()), ink, paper, style);
	}
};

const Display& DefaultPaletteNameDisplay()   { return Single<DefaulPaletteNameDisplayCls>();  }
const Display& NormalPaletteNameDisplay()    { return Single<NormalPaletteNameDisplayCls>();  }
const Display& NormalPaletteSampleDisplay()  { return Single<NormalPaletteSampleDisplayCls>();}

static const Vector<Tuple<Color, const char*, const char*>>& GetColorList()
{
	static Vector<Tuple<Color, const char*, const char*>> sColorTable = {
		{ Black(),               "Color_1", t_("Color_1") },
		{ Red(),                 "Color_2", t_("Color_2") },
		{ Green(),               "Color_3", t_("Color_3") },
		{ Yellow(),              "Color_4", t_("Color_4") },
		{ Blue(),                "Color_5", t_("Color_5") },
		{ Magenta(),             "Color_6", t_("Color_6") },
		{ Cyan(),                "Color_7", t_("Color_7") },
		{ White(),               "Color_8", t_("Color_8") },
		{ Gray(),                "Color_9", t_("Color_9") },
		{ LtRed(),               "Color_10", t_("Color_10") },
		{ LtGreen(),             "Color_11", t_("Color_11") },
		{ LtYellow(),            "Color_12", t_("Color_12") },
		{ LtBlue(),              "Color_13", t_("Color_13") },
		{ LtMagenta(),           "Color_14", t_("Color_14") },
		{ LtCyan(),              "Color_15", t_("Color_15") },
		{ White(),               "Color_16", t_("Color_16") },
		{ SColorText,            "Ink"     , t_("Ink") },
		{ SColorHighlightText,   "SelectionInk", t_("Selection Ink") },
		{ SColorPaper,           "Paper"       , t_("Paper") },
		{ SColorHighlight,       "SelectionPaper", t_("Selection Paper") },
		{ LtRed(),               "HighlightInk", t_("Highlight Ink") },
		{ SColorHighlightText,   "HighlightCursorInk", t_("Highlight Cursor Ink") },
		{ Yellow(),              "HighlightPaper", t_("Highlight Paper") },
		{ SColorHighlight,       "HighlightCursorPaper", t_("Highlight Cursor Paper") },
	};
	return sColorTable;
}

Palette::Palette()
{
	const auto& lst = GetColorList();
	for(int i = 0; i < lst.GetCount(); i++)
		table[i] = lst[i].a;
}

void Palette::Jsonize(JsonIO& jio)
{
	jio("Name", name);
	
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
	int cx = Zx(GetSmartTextSize(sSampleText, Monospace()).cx);
	Ctrl::Add(list.SizePos());
	list.AddColumn(tt_("Name"));
	list.AddColumn(tt_("Sample")).SetDisplay(NormalPaletteSampleDisplay()).HeaderTab().Fixed(cx);
	list.Moving().Removing().Track().NoAskRemove().NoHeader().AutoHideSb();
	list.VertGrid(false).HorzGrid(false).SetLineCy(24).SetFrame(0, toolbar);
	list.WhenLeftDouble = [this]() { Edit(); };
	list.WhenSel        = [this]() { Sync(); Action(); };
	list.WhenBar        = [this](Bar& bar) { ContextMenu(bar); };
	Load();
	Sync();
}

void Palettes::ContextMenu(Bar& bar)
{
	bool b = list.IsCursor();
	bar.Add(tt_("Add palette"), Images::Add(), [this]() { Add(); }).Key(K_INSERT);
	bar.Add(b, tt_("Edit palette"), Images::Edit(), [this]() { Edit(); }).Key(K_SPACE);
	bar.Add(b, tt_("Remove palette"), Images::Delete(), [this]() { Remove(); }).Key(K_DELETE);
	if(bar.IsMenuBar()) {
		bar.Separator();
		bar.Add(b, tt_("Set as active palette"), [this]() { MakeActive(); }).Key(K_CTRL|K_D);
	}
}

void Palettes::Add()
{
	if(String s; EditTextNotNull(s, t_("New Profile"), t_("Name"))) {
		if(!list.FindSetCursor(s)) {
			list.Add(s, RawToValue(Palette(s)));
			list.GoEnd();
			Store();
		}
	}
}

void Palettes::Edit()
{
	if(list.IsCursor())
		SetPalette();
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
		Store();
	}
}

void Palettes::SetPalette()
{
	if(!list.IsCursor())
		return;

	WithColorPaletteLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, tt_("Color Profile"));

	dlg.colors.AddColumn(tt_("Description"));
	dlg.colors.AddColumn(tt_("Color")).Ctrls<ColorPusher>();
	dlg.colors.ColumnWidths("100 24");
	dlg.colors.EvenRowColor();

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
	data = list.Get(list.GetCursor(), 0);
	Sync();
}

void Palettes::Sync()
{
	toolbar.Set([this](Bar& bar) { ContextMenu(bar); });
	for(int i = 0; i < list.GetCount(); i++) {
		list.SetDisplay(i, 0,  list.Get(i, 0) == data ? DefaultPaletteNameDisplay() : NormalPaletteNameDisplay());
	}
	Action();
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
		list.FindSetCursor(data);
		Sync();
	}
	return rc;
}

void Palettes::Store()
{
	for(int i = 0; i < list.GetCount(); i++) {
		String  s = list.Get(i, 0).To<String>();
		Palette p = list.Get(i, 1).To<Palette>();
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
	Sync();
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