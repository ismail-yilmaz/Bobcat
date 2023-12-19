// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023, İsmail Yılmaz

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

static const Tuple<Color, const char*> sColorTable[TerminalCtrl::MAX_COLOR_COUNT] {
	{ Black(),               "Color_1" },
	{ Red(),                 "Color_2" },
	{ Green(),               "Color_3" },
	{ Yellow(),              "Color_4" },
	{ Blue(),                "Color_5" },
	{ Magenta(),             "Color_6" },
	{ Cyan(),                "Color_7" },
	{ White(),               "Color_8" },
	{ Gray(),                "Color_9" },
	{ LtRed(),               "Color_10" },
	{ LtGreen(),             "Color_11" },
	{ LtYellow(),            "Color_12" },
	{ LtBlue(),              "Color_13" },
	{ LtMagenta(),           "Color_14" },
	{ LtCyan(),              "Color_15" },
	{ White(),               "Color_16" },
	{ SColorText(),          "Ink"      },
	{ SColorHighlightText(), "SelectionInk" },
	{ SColorPaper(),         "Paper"        },
	{ SColorHighlight(),     "SelectionPaper" }
};

Palette::Palette()
: table {
	Black(),
	Red(),
	Green(),
	Yellow(),
	Blue(),
	Magenta(),
	Cyan(),
	White(),
	Gray(),
	LtRed(),
	LtGreen(),
	LtYellow(),
	LtBlue(),
	LtMagenta(),
	LtCyan(),
	White(),
	SColorText(),
	SColorHighlightText(),
	SColorPaper(),
	SColorHighlight()
}
{
}

void Palette::Jsonize(JsonIO& jio)
{
	jio("Name", name);
	
	if(jio.IsLoading()) {
		for(int i = 0; i < TerminalCtrl::MAX_COLOR_COUNT; i++) {
			String q;
			jio(sColorTable[i].b, q);
			table[i] = ConvertHashColorSpec().Scan(q);
		}
	}
	else
	if(jio.IsStoring()) {
		for(int i = 0; i < TerminalCtrl::MAX_COLOR_COUNT; i++) {
			String s = ConvertHashColorSpec().Format(table[i]);
			jio(sColorTable[i].b, s);
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
		bar.Add(b, tt_("Set as active palette"), [this]() { data = list.Get(list.GetCursor(), 0); Sync(); }).Key(K_CTRL|K_D);
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
	
	for(int i = 0; i < TerminalCtrl::MAX_COLOR_COUNT; i++) {
		dlg.colors.Add(
			decode(i,
				TerminalCtrl::COLOR_INK, tt_("Ink"),
				TerminalCtrl::COLOR_PAPER, tt_("Paper"),
				TerminalCtrl::COLOR_INK_SELECTED, tt_("Selection Ink"),
				TerminalCtrl::COLOR_PAPER_SELECTED, tt_("Selection Paper"),
				Format(t_("Color %d"), i + 1)),
			p.table[i]);
	}

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
		Vector<Value> v = list.GetLine(i);
		list.Remove(i);
		list.Insert(0, v);
		list.SetCursor(0);
		SetModify();
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
		MakeActive();
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
			if(i == 0) {
				data = s;
				RDUMP(data);
			}
		}
		catch(const JsonizeError& e)
		{
			RLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			RLOG("Value type error: " << e);
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
			RLOG("Jsonization error: " << e);
		}
		catch(const ValueTypeError& e)
		{
			RLOG("Value type error: " << e);
		}
		catch(const CParser::Error& e)
		{
			RLOG("Parser error: " << e);
		}
		catch(...)
		{
			RLOG("Unknown exception");
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
			RLOG(e);
		}
		catch(const ValueTypeError& e)
		{
			failures++;
			RLOG(e);
		}
		catch(const CParser::Error& e)
		{
			failures++;
			RLOG(e);
		}
		catch(...)
		{
			RLOG("Unknown exception");
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