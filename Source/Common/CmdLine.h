/*
    Image Uploader - program for uploading images/files to Internet
    Copyright (C) 2007-2010 ZendeN <zenden2k@gmail.com>
	 
    HomePage:    http://zenden.ws/imageuploader

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <atlmisc.h>
#include <atlcoll.h>
typedef CAtlArray<CString> CStringList;

class CCmdLine
{
	private:
		CStringList m_Params;
		CString m_OnlyParams;	
	public:
		CCmdLine();
		CCmdLine& operator=(const CCmdLine &p);
		CCmdLine(LPCTSTR szCmdLine);
		void Parse(LPCTSTR szCmdLine);
		int AddParam(LPCTSTR szParam);
		CString OnlyParams();
		CString operator[](int nIndex);
		CString ModuleName();
		bool GetNextFile(CString &FileName, int &nIndex);
		bool IsOption(LPCTSTR Option, bool bUsePrefix = true);
		size_t GetCount();
};

extern CCmdLine CmdLine;