// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2023-2024, İsmail Yılmaz

#ifndef _Bobcat_Finder_h_
#define _Bobcat_Finder_h_

// TODO: Make this an abstract base class and derive different types of "search engines" from it
// (E.g. SimpleFinder, RegexFinder. etc.)

class Finder
{
public:
	typedef Finder CLASSNAME;
	
	Finder(TerminalCtrl& t);
	~Finder();
	
	void		Sync();

	void		DoSearch();
	void		Search();

	bool		OnSearch(const VectorMap<int, WString>& m, const WString& s);
	void		OnHighlight(VectorMap<int, VTLine>& hl);

private:
	int			index = 0;
	TerminalCtrl& ctx;
	Vector<Point> pos;
	WithFinderLayout<TopWindow> dlg;
};


#endif
