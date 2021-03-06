/*

    Image Uploader -  free application for uploading images/files to the Internet

    Copyright 2007-2015 Sergey Svistunov (zenden2k@gmail.com)

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

*/

#include "ImageConverter.h"

#include <cassert>
#include "atlheaders.h"
#include "Func/Common.h"
#include "Gui/Dialogs/LogWindow.h"
#include "Core/Utils/StringUtils.h"
#include "Core/3rdpart/parser.h"
#include "Core/3rdpart/pcreplusplus.h"
#include "3rdpart/QColorQuantizer.h"
#include <Func/IuCommonFunctions.h>

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

using namespace Gdiplus;

ImageConvertingParams::ImageConvertingParams()
{
	StrokeColor = RGB(0, 0, 0);
	SmartConverting = false;
	AddLogo  = false;
	AddText = false;
	Format = 1;
	Quality = 85;
	SaveProportions = true;
	ResizeMode = irmFit;
	LogoPosition = 0;
	LogoBlend = 0;
	Text = APPNAME;
	TextPosition = 5;
	TextColor = 0x00ffffff;
	StringToFont(_T("Tahoma,8,,204"), &Font);
	PreserveExifInformation = true;
}

CImageConverter::CImageConverter()
{
	m_generateThumb = false;
	thumbnail_template_ = 0;
	processing_enabled = true;
}

void CImageConverter::setDestinationFolder(const CString& folder)
{
	m_destinationFolder = folder;
}

void CImageConverter::setGenerateThumb(bool generate)
{
	m_generateThumb = generate;
}

CRect CenterRect(CRect r1, CRect intoR2)
{
	r1.OffsetRect(( intoR2.Width()  - r1.Width()) / 2, ( intoR2.Height()  - r1.Height()) / 2);
	return r1;
}

bool CImageConverter::Convert(const CString& sourceFile)
{
	m_sourceFile = sourceFile;
	int fileformat;
	double width, height, imgwidth, imgheight, newwidth, newheight;
	CString imageFile = sourceFile;
	Image bm(sourceFile);
	Image* thumbSource = &bm;
	std::auto_ptr<Bitmap> BackBuffer;
	imgwidth = float(bm.GetWidth());
	imgheight = float(bm.GetHeight());
	double NewWidth = _wtof(m_imageConvertingParams.strNewWidth);
	double NewHeight = _wtof(m_imageConvertingParams.strNewHeight);
	if (m_imageConvertingParams.strNewWidth.Right(1) == _T("%"))
		NewWidth = NewWidth * imgwidth / 100;
	
	

	if (m_imageConvertingParams.strNewHeight.Right(1) == _T("%"))
	{
		NewHeight = NewHeight * imgheight / 100;
	}

	width = float(NewWidth);
	height = float(NewHeight);

	if (m_imageConvertingParams.Format < 1 || !processing_enabled)
		fileformat = GetSavingFormat(sourceFile);
	else
		fileformat = m_imageConvertingParams.Format - 1;

	if (fileformat == GetSavingFormat(sourceFile) && imgwidth < width && imgheight < height)
		processing_enabled = false;

	newwidth = imgwidth;
	newheight = imgheight;

	// ���� �������� ����� "�������� ��� ���������", ������ �������� ��� ��������� �����
	if (!processing_enabled)
		m_resultFileName = sourceFile;
	else
	{
		UINT propertyItemsSize = 0;
		UINT propertyItemsCount = 0;
		PropertyItem* pPropBuffer = NULL;
		if ( m_imageConvertingParams.PreserveExifInformation ) {
			bm.GetPropertySize(&propertyItemsSize, &propertyItemsCount);
			if ( propertyItemsSize ) {
				pPropBuffer = (PropertyItem*)malloc(propertyItemsSize);
				bm.GetAllPropertyItems(propertyItemsSize, propertyItemsCount, pPropBuffer);
			}
		}
		if (m_imageConvertingParams.ResizeMode == ImageConvertingParams::irmFit)
		{
			if (width && height && ( imgwidth > width || imgheight > height) )
			{
				double S = min(width / imgwidth, height / imgheight);
				newwidth = S * imgwidth;
				newheight = S  * imgheight;
			}
			else
			if (width && imgwidth > width)
			{
				newwidth = width;
				newheight = newwidth / imgwidth * imgheight;
			}
			else
			if (height && imgheight > height)
			{
				newheight = height;
				newwidth = newheight / imgheight * imgwidth;
			}
		}
		else
		{
			if ( imgwidth > width || imgheight > height)
			{
				if (width > 0)
					newwidth = width;
				if (height > 0)
					newheight = height;
			}
			else
			{
				newwidth = imgwidth;
				newheight = imgheight;
			}
		}
		BackBuffer.reset(new Bitmap((int)newwidth, ( int)newheight));

		Graphics gr(BackBuffer.get());
		if (fileformat != 2) /* not GIF */
			gr.Clear(Color(0, 255, 255, 255));
		else
			gr.Clear(Color(255, 255, 255, 255));

		gr.SetInterpolationMode(InterpolationModeHighQualityBicubic );

		gr.SetPixelOffsetMode(PixelOffsetModeHalf);

		ImageAttributes attr;
		attr.SetWrapMode( WrapModeTileFlipXY);
		if (m_imageConvertingParams.ResizeMode == ImageConvertingParams::irmFit || m_imageConvertingParams.ResizeMode ==
		    ImageConvertingParams::irmStretch)
		{
			if ((!width && !height) || ((int)newwidth == (int)imgwidth && (int)newheight == (int)imgheight))
				gr.DrawImage(/*backBuffer*/ &bm, (int)0, (int)0, (int)newwidth, (int)newheight);
			else
				gr.DrawImage(&bm,
				             RectF(0.0, 0.0, float(newwidth), float(newheight)),
				             0,
				             0,
				             float(bm.GetWidth()),
				             float(bm.GetHeight()),
				             UnitPixel,
				             &attr);
			// gr.DrawImage(/*backBuffer*/&bm, (int)-1, (int)-1, (int)newwidth+2,(int)newheight+2);
		}
		else
		if (m_imageConvertingParams.ResizeMode == ImageConvertingParams::irmCrop)
		{
			int newVisibleWidth = 0;
			int newVisibleHeight = 0;
			double k = 1;
			if (newwidth > newheight)
			{
				newVisibleWidth = static_cast<int>(min(newwidth, imgwidth));
				k = newVisibleWidth / imgwidth;
				newVisibleHeight = static_cast<int>(newVisibleWidth / imgwidth * imgheight);
			}
			else
			{
				newVisibleHeight = static_cast<int>(min(newheight, imgheight));
				k = newVisibleHeight / imgheight;
				newVisibleWidth = static_cast<int>(newVisibleHeight / imgheight * imgwidth);
			}
			CRect r(0, 0, newVisibleWidth, newVisibleHeight);
			CRect destRect = CenterRect(r, CRect(0, 0, static_cast<int>(newwidth), static_cast<int>(newheight)));
			CRect croppedRect;
			croppedRect.IntersectRect(CRect(0, 0, int(newwidth), int(newheight)), destRect);
			CRect sourceRect = croppedRect;
			sourceRect.OffsetRect(/*(destRect.left < 0)?*/ -destRect.left, /*(destRect.top < 0)?*/ -destRect.top);
			sourceRect.left = int(sourceRect.left / k);
			sourceRect.top = int(sourceRect.top / k);
			sourceRect.right = int(sourceRect.right / k);
			sourceRect.bottom = int(sourceRect.bottom / k);
			// = sourceRect.MulDiv(1, k);
			gr.DrawImage(&bm,
			             RectF(float(croppedRect.left), float(croppedRect.top), float(croppedRect.Width()),
			                   float(croppedRect.Height()))
			             , float(sourceRect.left), float(sourceRect.top), float(sourceRect.Width()),
			             float(sourceRect.Height()), UnitPixel,
			             &attr);
		}
		RectF bounds(0, 0, float(newwidth), float(newheight));

		// ��������� ����� �� �������� (���� ����� ��������)
		if (m_imageConvertingParams.AddText)
		{
			SolidBrush brush(Color(GetRValue(m_imageConvertingParams.TextColor), GetGValue(
			                          m_imageConvertingParams.TextColor), GetBValue(m_imageConvertingParams.TextColor)));

			int HAlign[6] = {0, 1, 2, 0, 1, 2};
			int VAlign[6] = {0, 0, 0, 2, 2, 2};

			m_imageConvertingParams.Font.lfQuality = m_imageConvertingParams.Font.lfQuality | ANTIALIASED_QUALITY;
			HDC dc = ::GetDC(0);
			Font font(/*L"Tahoma", 10, FontStyleBold*/ dc, &m_imageConvertingParams.Font);
			SolidBrush brush2(Color(70, 0, 0, 0));
			RectF bounds2(1, 1, float(newwidth), float(newheight) + 1);
			ReleaseDC(0, dc);
			DrawStrokedText(gr, m_imageConvertingParams.Text, bounds2, font, MYRGB(255,
			                                                                       m_imageConvertingParams.TextColor),
			                MYRGB(180,
			                      m_imageConvertingParams.StrokeColor), HAlign[m_imageConvertingParams.TextPosition],
			                VAlign[m_imageConvertingParams.TextPosition], 1);
		}

		if (m_imageConvertingParams.AddLogo)
		{
			Bitmap logo(m_imageConvertingParams.LogoFileName);
			if (logo.GetLastStatus() == Ok)
			{
				int x, y;
				int logowidth, logoheight;
				logowidth = logo.GetWidth();
				logoheight = logo.GetHeight();
				if (m_imageConvertingParams.LogoPosition < 3)
					y = 0;
				else
					y = int(newheight - logoheight);
				if (m_imageConvertingParams.LogoPosition == 0 || m_imageConvertingParams.LogoPosition == 3)
					x = 0;
				if (m_imageConvertingParams.LogoPosition == 2 || m_imageConvertingParams.LogoPosition == 5)
					x = int(newwidth - logowidth);
				if (m_imageConvertingParams.LogoPosition == 1 || m_imageConvertingParams.LogoPosition == 4)
					x = int((newwidth - logowidth) / 2);

				gr.DrawImage(&logo, (int)x, (int)y, logowidth, logoheight);
			}
		}
		thumbSource = BackBuffer.get();
		if ( m_imageConvertingParams.PreserveExifInformation ) {
			for ( int k = 0; k < propertyItemsCount; k++ ) {
				BackBuffer->SetPropertyItem(pPropBuffer + k );
			}
		}
		free(pPropBuffer);
		MySaveImage(BackBuffer.get(), GenerateFileName(L"img%md5.jpg", 1,
		                                               CPoint()), m_resultFileName, fileformat,
		            m_imageConvertingParams.Quality);
		imageFile = m_resultFileName;
	}

	if (!processing_enabled)
	{
		CString Ext = GetFileExt(sourceFile);
		if (Ext == _T("png"))
			fileformat = 1;
		else
			fileformat = 0;
	}
	if (m_generateThumb)
	{
		// ������������� ��������� � �������� � ��������� �������
		int thumbFormat = fileformat;
		if (m_thumbCreatingParams.Format == ThumbCreatingParams::tfJPEG)
			thumbFormat = 0;
		else
		if (m_thumbCreatingParams.Format == ThumbCreatingParams::tfPNG)
			thumbFormat = 1;
		else
		if (m_thumbCreatingParams.Format == ThumbCreatingParams::tfGIF)
			thumbFormat = 2;
		createThumb(thumbSource, imageFile, thumbFormat);
	}
	return true;
}

bool CImageConverter::createThumb(Gdiplus::Image* bm, const CString& imageFile, int fileformat)
{
	bool result = false;
	HDC dc = ::GetDC(0);
	int newwidth = bm->GetWidth();
	int newheight = bm->GetHeight();
	int64_t FileSize = IuCoreUtils::getFileSize(WCstringToUtf8((imageFile)));


	// Saving thumbnail (without template)
	Image* res = 0;
	if (createThumbnail(bm, &res, FileSize, fileformat))
	{
		result = MySaveImage(res, _T("thumb"), m_thumbFileName, fileformat, m_thumbCreatingParams.Quality);
		delete res;
	}
	// }
	ReleaseDC(0, dc);
	return result;
}

// hack for stupid GDIplus
void changeAplhaChannel(Bitmap& source, Bitmap& dest, int sourceChannel, int destChannel )
{
	Rect r( 0, 0, source.GetWidth(), source.GetHeight() );
	BitmapData bdSrc;
	BitmapData bdDst;
	source.LockBits( &r,  ImageLockModeRead, PixelFormat32bppARGB, &bdSrc);
	dest.LockBits( &r,  ImageLockModeWrite, PixelFormat32bppARGB, &bdDst);

	BYTE* bpSrc = (BYTE*)bdSrc.Scan0;
	BYTE* bpDst = (BYTE*)bdDst.Scan0;
	bpSrc += (int)sourceChannel;
	bpDst += (int)destChannel;

	for ( int i = r.Height * r.Width; i > 0; i-- )
	{
		// if(*bpSrc != 255)
		{
			*bpDst = static_cast<BYTE>((float(255 - *bpSrc) / 255) *  *bpDst);
		}

		/*if(*bpDst == 0)
		{
		   bpDst -=(int)destChannel;
		   *bpDst = 0;
		   *(bpDst+1) = 0;
		   *(bpDst+2) = 0;
		   bpDst +=(int)destChannel;
		}*/
		bpSrc += 4;
		bpDst += 4;
	}
	source.UnlockBits( &bdSrc );
	dest.UnlockBits( &bdDst );
}

bool CImageConverter::createThumbnail(Gdiplus::Image* image, Gdiplus::Image** outResult, int64_t fileSize,
                                      int fileformat)
{
	assert(thumbnail_template_);
	bool result = false;
	const Thumbnail::ThumbnailData* data = thumbnail_template_->getData();
	CDC dc = ::GetDC(0);
	int newwidth = image->GetWidth();
	int newheight = image->GetHeight();
	int FileSize = MyGetFileSize(m_sourceFile);
	/*TCHAR SizeBuffer[100]=_T("\0");
	if(FileSize>0)
	   NewBytesToString(FileSize,SizeBuffer,sizeof(SizeBuffer));*/

	CString sizeString = Utf8ToWCstring(IuCoreUtils::fileSizeToString(fileSize));
	CString ThumbnailText = m_thumbCreatingParams.Text; // Text that will be drawn on thumbnail

	ThumbnailText.Replace(_T("%width%"), IntToStr(newwidth)); // Replacing variables names with their values
	ThumbnailText.Replace(_T("%height%"), IntToStr(newheight));
	ThumbnailText.Replace(_T("%size%"), sizeString);

	int thumbwidth = m_thumbCreatingParams.Size;
	int thumbheight = m_thumbCreatingParams.Size;
	Graphics g1(dc);
	CString filePath = Utf8ToWCstring(thumbnail_template_->getSpriteFileName());
	Image templ(filePath);
	int ww = templ.GetWidth();
	CString s;

	LOGFONT lf;
	StringToFont(_T("Tahoma,7,b,204"), &lf);
	if (thumbnail_template_->existsParam("Font"))
	{
		std::string font = thumbnail_template_->getParamString("Font");
		CString wide_text = Utf8ToWCstring(font);
		StringToFont(wide_text, &lf);
	}
	Font font(dc, &lf);
	RectF TextRect;

	/*StringFormat * f = StringFormat::GenericTypographic()->Clone();
	f->SetTrimming(StringTrimmingNone);
	f->SetFormatFlags( StringFormatFlagsMeasureTrailingSpaces);*/

	StringFormat format;
	FontFamily ff;
	font.GetFamily(&ff);
	g1.SetPageUnit(UnitWorld);
	g1.MeasureString(_T("test"), -1, &font, PointF(0, 0), &format, &TextRect);
//		delete f;
	m_Vars["TextWidth"] = IuCoreUtils::toString((int)TextRect.Width);
	m_Vars["TextHeight"] =  IuCoreUtils::toString((int)TextRect.Height);
	m_Vars["UserText"] =  WCstringToUtf8(ThumbnailText);
	std::string textTempl = thumbnail_template_->getParamString("Text");
	if (textTempl.empty())
		textTempl =  "$(UserText)";
	CString textToDraw = Utf8ToWCstring(ReplaceVars(textTempl));

	for (std::map<std::string, std::string>::const_iterator it = data->colors_.begin(); it != data->colors_.end(); ++it)
	{
		m_Vars[it->first] = it->second;
	}

	m_Vars["DrawText"] = IuCoreUtils::toString(m_Vars["DrawText"] == "1" && m_thumbCreatingParams.AddImageSize);

	if (!m_thumbCreatingParams.ResizeMode == ThumbCreatingParams::trByHeight)
	{
		thumbwidth -= EvaluateExpression(thumbnail_template_->getWidthAddition());
		if (thumbwidth < 10)
			thumbwidth = 10;
		thumbheight = int((float)thumbwidth / (float)newwidth * newheight);
	}
	else
	{
		thumbheight -= EvaluateExpression(thumbnail_template_->getHeightAddition());
		if (thumbheight < 10)
			thumbheight = 10;
		thumbwidth = int((float)thumbheight / (float)newheight * newwidth);
	}

	int LabelHeight = int(TextRect.Height + 1);
	int RealThumbWidth = thumbwidth + EvaluateExpression(thumbnail_template_->getWidthAddition());
	int RealThumbHeight = thumbheight + EvaluateExpression(thumbnail_template_->getHeightAddition());

	m_Vars["Width"] = IuCoreUtils::toString(RealThumbWidth);
	m_Vars["Height"] = IuCoreUtils::toString(RealThumbHeight);
	Bitmap* ThumbBuffer = new Bitmap(RealThumbWidth, RealThumbHeight, &g1);
	Graphics thumbgr(ThumbBuffer);
	thumbgr.SetPageUnit(UnitPixel);

	if (fileformat == 0 || fileformat == 2)
		thumbgr.Clear(MYRGB(255, m_thumbCreatingParams.BackgroundColor));
	// thumbgr.Clear(Color(255,255,255,255));
	RectF thu((float)(m_thumbCreatingParams.DrawFrame ? 1 : 0), (float)(m_thumbCreatingParams.DrawFrame ? 1 : 0),
	          (float)thumbwidth,
	          (float)thumbheight);
	thumbgr.SetInterpolationMode(InterpolationModeHighQualityBicubic  );
	// thumbgr.SetPixelOffsetMode(PixelOffsetModeHighQuality );
	// thumbgr.SetSmoothingMode( SmoothingModeHighQuality);
	// //	thumbgr.SetSmoothingMode(SmoothingModeAntiAlias);
	thumbgr.SetSmoothingMode(SmoothingModeNone);
	// thumbgr.SetPixelOffsetMode(PixelOffsetModeHighQuality );
	// thumbgr.SetCompositingQuality(CompositingQualityHighQuality);

	Bitmap* MaskBuffer = new Bitmap(RealThumbWidth, RealThumbHeight, PixelFormat32bppARGB);
	Graphics maskgr(MaskBuffer);

	for (size_t i = 0; i < data->drawing_operations_.size(); i++)
	{
		bool performOperation = true;
		if ( !data->drawing_operations_[i].condition.empty())
		{
			performOperation = EvaluateExpression(data->drawing_operations_[i].condition) != 0;
		}
		if (!performOperation)
			continue;
		Graphics* gr = &thumbgr;
		std::string type = data->drawing_operations_[i].type;
		CRect rc;
		EvaluateRect(data->drawing_operations_[i].rect, &rc);
		if (data->drawing_operations_[i].destination == "mask")
		{
			gr = &maskgr;
		}
		if (type == "text")
		{
			std::string colorsStr = data->drawing_operations_[i].text_colors;
			std::vector<std::string> tokens;
			IuStringUtils::Split(colorsStr, " ", tokens, 2);
			unsigned int color1 = 0xB3FFFFFF;
			unsigned int strokeColor = 0x5A000000;
			if (tokens.size() > 0)
				color1 =    EvaluateColor(tokens[0]);
			if (tokens.size() > 1)
				strokeColor = EvaluateColor(tokens[1]);
			RectF TextBounds((float)rc.left, (float)rc.top, (float)rc.right, (float)rc.bottom);
			thumbgr.SetPixelOffsetMode(PixelOffsetModeDefault );
			DrawStrokedText(*gr, /* Buffer*/ textToDraw, TextBounds, font, Color(color1), Color(
			                   strokeColor) /*params.StrokeColor*/, 1, 1, 1);
			thumbgr.SetPixelOffsetMode(PixelOffsetModeNone );
		}
		else
		if (type == "fillrect")
		{
			Brush* fillBr = CreateBrushFromString(data->drawing_operations_[i].brush, rc);
			gr->FillRectangle(fillBr, rc.left, rc.top, rc.right, rc.bottom);
			delete fillBr;
		}
		else
		if (type == "drawrect")
		{
			GraphicsPath path;
			// path.AddRectangle(
			std::vector<std::string> tokens;
			IuStringUtils::Split(data->drawing_operations_[i].pen, " ", tokens, 2);
			if (tokens.size() > 1)
			{
				unsigned int color = EvaluateColor(tokens[0]);
				int size = EvaluateExpression( tokens[1]);
				Gdiplus::Pen p(color, float(size));
				if (size == 1)
				{
					rc.right--;
					rc.bottom--;
				}
				p.SetAlignment( PenAlignmentInset );
				gr->DrawRectangle(&p, rc.left, rc.top, rc.right, rc.bottom);
			}
		}
		else
		if (type == "blt")
		{
			RECT sourceRect;
			EvaluateRect(data->drawing_operations_[i].source_rect, &sourceRect);
			Rect t(int(rc.left), int(rc.top), int(rc.right), int(rc.bottom));

			if (data->drawing_operations_[i].source == "image")
			{
				SolidBrush whiteBr(Color(160, 130, 100));
				// thumbgr.FillRectangle(&whiteBr, (int)t.X, (int) t.Y/*(int)item->DrawFrame?1:0-1*/,thumbwidth,thumbheight);
				// thumbgr.SetInterpolationMode(InterpolationModeNearestNeighbor  );
				ImageAttributes attr;
				attr.SetWrapMode( WrapModeTileFlipXY);
				Rect dest(t.X, t.Y, thumbwidth, thumbheight);

				Bitmap tempImage(RealThumbWidth, RealThumbHeight, PixelFormat32bppARGB);
				Graphics tempGr(&tempImage);
				tempGr.SetInterpolationMode(InterpolationModeHighQualityBicubic  );
				tempGr.SetSmoothingMode(SmoothingModeHighQuality);
				tempGr.SetPixelOffsetMode(PixelOffsetModeHighQuality );
				tempGr.DrawImage(image, dest, (INT)0, (int)0, (int)image->GetWidth(),
				                 (int)image->GetHeight(), UnitPixel, &attr);
				changeAplhaChannel(*MaskBuffer, tempImage, 3, 3);
				gr->DrawImage(&tempImage, 0, 0);
				tempGr.SetSmoothingMode(SmoothingModeNone);
			}
			else
			{
				ImageAttributes attr;
				attr.SetWrapMode(WrapModeClamp);    // we need this to avoid some glitches on the edges of interpolated image
				thumbgr.SetInterpolationMode(InterpolationModeHighQualityBicubic  );
				thumbgr.SetSmoothingMode(SmoothingModeHighQuality);
				gr->DrawImage(&templ, t, (int)sourceRect.left, (int)sourceRect.top, (int)sourceRect.right,
				              (int)sourceRect.bottom, UnitPixel,
				              &attr);
				thumbgr.SetSmoothingMode(SmoothingModeNone);
			}
		}
	}

	delete MaskBuffer;
	*outResult = ThumbBuffer;

	return true;
}

void CImageConverter::setImageConvertingParams(const ImageConvertingParams& params)
{
	m_imageConvertingParams = params;
}

void CImageConverter::setThumbCreatingParams(const ThumbCreatingParams& params)
{
	m_thumbCreatingParams = params;
}

bool MySaveImage(Image* img, const CString& szFilename, CString& szBuffer, int Format, int Quality, LPCTSTR Folder)
{
	if (Format == -1)
		Format = 0;
	std::auto_ptr<Bitmap> quantizedImage;
	TCHAR szImgTypes[3][4] = {_T("jpg"), _T("png"), _T("gif")};
	TCHAR szMimeTypes[3][12] = {_T("image/jpeg"), _T("image/png"), _T("image/gif")};
	CString szNameBuffer;
	TCHAR szBuffer2[MAX_PATH];
	if (IsImage(szFilename) )
		szNameBuffer = GetOnlyFileName(szFilename);
	else
		szNameBuffer = szFilename;
	CString userFolder;
	if (Folder)
		userFolder = Folder;
	if (userFolder.Right(1) != _T('\\'))
		userFolder += _T('\\');
	wsprintf(szBuffer2, _T(
		"%s%s.%s"), (LPCTSTR)(Folder ? userFolder : IuCommonFunctions::IUTempFolder), (LPCTSTR)szNameBuffer,
	         /*(int)GetTickCount(),*/ szImgTypes[Format]);
	CString resultFilename = GetUniqFileName(szBuffer2);
	IU_CreateFilePath(resultFilename);
	CLSID clsidEncoder;
	EncoderParameters eps;
	eps.Count = 1;

	if (Format == 0) // JPEG
	{
		eps.Parameter[0].Guid = EncoderQuality;
		eps.Parameter[0].Type = EncoderParameterValueTypeLong;
		eps.Parameter[0].NumberOfValues = 1;
		eps.Parameter[0].Value = &Quality;
	}
	else
	if (Format == 1)      // PNG
	{
		eps.Parameter[0].Guid = EncoderCompression;
		eps.Parameter[0].Type = EncoderParameterValueTypeLong;
		eps.Parameter[0].NumberOfValues = 1;
		eps.Parameter[0].Value = &Quality;
	}
	else
	if (Format == 2)
	{
		QColorQuantizer quantizer;
		quantizedImage.reset ( quantizer.GetQuantized(img, QColorQuantizer::Octree, (Quality < 50) ? 16 : 256) );
		if (quantizedImage.get() != 0)
			img = quantizedImage.get();
	}

	Gdiplus::Status result;

	if (GetEncoderClsid(szMimeTypes[Format], &clsidEncoder) != -1)
	{
		if (Format == 0)
			result = img->Save(resultFilename, &clsidEncoder, &eps);
		else
			result = img->Save(resultFilename, &clsidEncoder);
	}
	else
		return false;
	if (result != Ok)
	{
		WriteLog(logError, _T("Image Converter"), _T("Could not save image at path \r\n") + resultFilename + L"\r\nGdiplus status=" + IntToStr(result));
		return false;
	}
	szBuffer = resultFilename;
	return true;
}

void DrawGradient(Graphics& gr, Rect rect, Color& Color1, Color& Color2)
{
	Bitmap bm(rect.Width, rect.Height, &gr);
	Graphics temp(&bm);
	LinearGradientBrush
	brush(/*TextBounds*/ Rect(0, 0, rect.Width, rect.Height), Color1, Color2, LinearGradientModeVertical);

	temp.FillRectangle(&brush, Rect(0, 0, rect.Width, rect.Height));
	gr.DrawImage(&bm, rect.X, rect.Y);
}

void DrawRect(Bitmap& gr, Color& color, Rect rect)
{
	int i;
	SolidBrush br(color);
	for (i = rect.X; i < rect.Width; i++)
	{
		gr.SetPixel(i, 0, color);
		gr.SetPixel(i, rect.Height - 1, color);
	}

	for (i = rect.Y; i < rect.Height; i++)
	{
		gr.SetPixel(0, i, color);
		gr.SetPixel(rect.Width - 1, i, color);
	}
}

const CString CImageConverter::getThumbFileName()
{
	return m_thumbFileName;
}

const CString CImageConverter::getImageFileName()
{
	return m_resultFileName;
}

Rect MeasureDisplayString(Graphics& graphics, CString text, RectF boundingRect, Font& font) {
	CharacterRange charRange(0, text.GetLength());
	Region pCharRangeRegions;
	StringFormat strFormat;
	strFormat.SetMeasurableCharacterRanges(1, &charRange);
	graphics.MeasureCharacterRanges (text, text.GetLength(), &font, boundingRect, &strFormat, 1, &pCharRangeRegions);
	Rect rc;
	pCharRangeRegions.GetBounds (&rc, &graphics);

	return rc;
}

/*
void DrawStrokedText(Graphics &gr, LPCTSTR Text,RectF Bounds,Font &font,Color &ColorText,Color &ColorStroke,int HorPos,int VertPos, int width)
{
   RectF OriginalTextRect;
   //	, NewTextRect;
   FontFamily ff;
   font.GetFamily(&ff);

   gr.SetPageUnit(UnitPixel);
   gr.MeasureString(Text,-1,&font,PointF(0,0),&OriginalTextRect);



   Font NewFont(&ff,48,font.GetStyle(),UnitPixel);
   Rect realSize = MeasureDisplayString(gr, Text, RectF(0,0,1000,200), NewFont);

   GraphicsPath path;
   StringFormat format;

   //format.SetAlignment(StringAlignmentCenter);
   //format.SetLineAlignment ( StringAlignmentCenter);
   StringFormat * f = format.GenericTypographic()->Clone();
   f->SetTrimming(StringTrimmingNone);
   f->SetFormatFlags( StringFormatFlagsMeasureTrailingSpaces);

   path.AddString(Text, -1,&ff, (int)NewFont.GetStyle(), NewFont.GetSize(), Point(0,0), f);
   RectF realTextBounds;
   path.GetBounds(&realTextBounds);

   float k = 2*width*realTextBounds.Height/OriginalTextRect.Height;
   Pen pen(ColorStroke,(float)k);
   realTextBounds.Inflate(k, k);
   pen.SetAlignment(PenAlignmentCenter);
   //path.GetBounds(&realTextBounds,0,&pen);
   k = 2*width*realTextBounds.Height/OriginalTextRect.Height; // recalculate K
   //Matrix matrix(1.0f, 0.0f, 0.0f, 1.0f, -realTextBounds.X,-realTextBounds.Y );
   //path.Transform(&matrix);
   //path.GetBounds(&realTextBounds,0,&pen);
   //gr.SetPageUnit(UnitPixel);


   //Font NewFont(&ff,48,font.GetStyle(),UnitPixel);

   RectF NewTextRect;

gr.MeasureString(Text,-1,&NewFont,RectF(0,0,5000,1600),f, &NewTextRect);

   RectF RectWithPaddings;
   StringFormat f2;
gr.MeasureString(Text,-1,&NewFont,RectF(0,0,5000,1600),&f2, &RectWithPaddings);


   OriginalTextRect.Height = OriginalTextRect.Height-OriginalTextRect.Y;
   float newwidth,newheight;
   newheight = OriginalTextRect.Height;
   newwidth=OriginalTextRect.Height/realTextBounds.Height*realTextBounds.Width;

   SolidBrush br(ColorText);

   Bitmap temp((int)(realTextBounds.Width+realTextBounds.X),(int)(realTextBounds.Height+realTextBounds.Y),&gr);
   //Bitmap temp((int)NewTextRect.Width,(int)NewTextRect.Height,&gr);

   Graphics gr_temp(&temp);

   gr_temp.SetPageUnit(UnitPixel);
//	GraphicsPath path;
   gr_temp.SetSmoothingMode( SmoothingModeHighQuality);
   //path.AddString(Text, -1,&ff, (int)NewFont.GetStyle(), NewFont.GetSize(), Point(0,0), &format);
   gr_temp.SetSmoothingMode( SmoothingModeHighQuality);


   SolidBrush br2(Color(255,0,0));
   float x,y;
   //gr_temp.FillRectangle(&br2, (int)realTextBounds.X, (int)realTextBounds.Y,(int)(realTextBounds.Width),(int)(realTextBounds.Height));
   //gr_temp.Set( SmoothingModeHighQuality);
   gr_temp.SetInterpolationMode(InterpolationModeHighQualityBicubic  );
   gr_temp.DrawPath(&pen, &path);
   gr_temp.FillPath(&br, &path);

   gr.SetSmoothingMode( SmoothingModeHighQuality);
   gr.SetInterpolationMode(InterpolationModeHighQualityBicubic  );

   if(HorPos == 0)
      x = 2;
   else if(HorPos == 1)
      x = (Bounds.Width-newwidth)/2;
   else x=(Bounds.Width-newwidth)-2;

   if(VertPos==0)
      y=2;
   else if(VertPos==1)
      y=(Bounds.Height-newheight)/2;
   else y=(Bounds.Height-newheight)-2;


   Rect destRect ((int)(Bounds.GetLeft()+x), (int)(Bounds.GetTop()+y), (int)(newwidth),(int)(newheight));
//	gr.SolidBrush br3(Color(0,255,0));

   //gr.FillRectangle(&br2, 0,0,(int)(realTextBounds.Width+realTextBounds.X),(int)(realTextBounds.Height+realTextBounds.Y));

   gr.DrawImage(&temp, destRect,(int)realTextBounds.X, (int)realTextBounds.Y,(int)(realTextBounds.Width),(int)(realTextBounds.Height), UnitPixel);
}*/

void DrawStrokedText(Graphics& gr, LPCTSTR Text, RectF Bounds, Font& font, Color& ColorText, Color& ColorStroke,
                     int HorPos, int VertPos,
                     int width)
{
	RectF OriginalTextRect, NewTextRect;
	FontFamily ff;
	font.GetFamily(&ff);
	gr.SetPageUnit(UnitPixel);
	gr.MeasureString(Text, -1, &font, PointF(0, 0), &OriginalTextRect);

	Font NewFont(&ff, 48, font.GetStyle(), UnitPixel);
	gr.MeasureString(Text, -1, &NewFont, RectF(0, 0, 5000, 1600), &NewTextRect);
	OriginalTextRect.Height = OriginalTextRect.Height - OriginalTextRect.Y;
	float newwidth, newheight;
	newheight = OriginalTextRect.Height;
	newwidth = OriginalTextRect.Height / NewTextRect.Height * NewTextRect.Width;
	float k = 2 * width * NewTextRect.Height / OriginalTextRect.Height;
	SolidBrush br(ColorText);
	Bitmap temp((int)NewTextRect.Width, (int)NewTextRect.Height, &gr);

	Graphics gr_temp(&temp);
	StringFormat format;
	gr_temp.SetPageUnit(UnitPixel);
	GraphicsPath path;
	gr_temp.SetSmoothingMode( SmoothingModeHighQuality);
	path.AddString(Text, -1, &ff, (int)NewFont.GetStyle(), NewFont.GetSize(), Point(0, 0), &format);

	Pen pen(ColorStroke, (float)k);
	pen.SetAlignment(PenAlignmentCenter);

	float x, y;
	gr_temp.DrawPath(&pen, &path);
	gr_temp.FillPath(&br, &path);
	gr.SetSmoothingMode( SmoothingModeHighQuality);
	gr.SetInterpolationMode(InterpolationModeHighQualityBicubic  );

	if (HorPos == 0)
		x = 2;
	else
	if (HorPos == 1)
		x = (Bounds.Width - newwidth) / 2;
	else
		x = (Bounds.Width - newwidth) - 2;

	if (VertPos == 0)
		y = 2;
	else
	if (VertPos == 1)
		y = (Bounds.Height - newheight) / 2;
	else
		y = (Bounds.Height - newheight) - 2;

	gr.DrawImage(&temp, (int)(Bounds.GetLeft() + x), (int)(Bounds.GetTop() + y), (int)(newwidth), (int)(newheight));
}


void CImageConverter::setThumbnail(Thumbnail* thumb)
{
	thumbnail_template_ = thumb;
}

bool CImageConverter::EvaluateRect(const std::string& rectStr,  RECT* out)
{
	std::vector<std::string> values;
	IuStringUtils::Split(rectStr, ";", values, 4);
	if (values.size() != 4)
		return false;
	int* target = reinterpret_cast<int*>(out);
	for (size_t i = 0; i < values.size(); i++)
	{
		int coord = EvaluateExpression(IuStringUtils::Trim(values[i]));
		*target = coord;
		target++;
	}
	return true;
}

int CImageConverter::EvaluateExpression(const std::string& expr)
{
	std::string processedExpr = ReplaceVars(expr);
	return static_cast<int>(EvaluateSimpleExpression(processedExpr));
}

int64_t CImageConverter::EvaluateSimpleExpression(const std::string& expr) const
{
	TParser parser;
	int64_t res = 0;
	try
	{
		parser.Compile(expr.c_str());
		res = static_cast<int64_t>(parser.Evaluate());
	}
	catch (...)
	{
	}
	return res;
}

std::string CImageConverter::ReplaceVars(const std::string& expr)
{
	std::string Result =  expr;

	pcrepp::Pcre reg("\\$\\(([A-z0-9_]*?)\\)", "imc");
	std::string str = (expr);
	size_t pos = 0;
	while (pos < str.length())
	{
		if ( reg.search(str, pos))
		{
			pos = reg.get_match_end() + 1;
			std::string vv = reg[0];
			std::string value = m_Vars[vv];

			Result = IuStringUtils::Replace(Result, std::string("$(") + vv + std::string(")"), value);
		}
		else
			break;
	}

	// Result  = IuStringUtils::Replace(Result,std::string("#"), "0x");
	{
		pcrepp::Pcre reg("\\#([0-9A-Fa-f]+)", "imc");
		std::string str = (Result);
		size_t pos = 0;
		while (pos < str.length())
		{
			if ( reg.search(str, pos))
			{
				pos = reg.get_match_end() + 1;
				std::string vv = reg[0];
				unsigned int res = strtoul(vv.c_str(), 0, 16);
				std::string value = m_Vars[vv];

				Result = IuStringUtils::Replace(Result, std::string("#") + vv, IuCoreUtils::toString(res));
			}
			else
				break;
		}
	}
	return Result;
}

Gdiplus::Brush* CImageConverter::CreateBrushFromString(const std::string& brStr, RECT rect)
{
	std::vector<std::string> tokens;
	IuStringUtils::Split(brStr, ":", tokens, 10);
	if (tokens[0] == "solid")
	{
		unsigned int color = 0;
		if (tokens.size() >= 2)
		{
			color = EvaluateColor(tokens[1]);
		}
		SolidBrush* br = new SolidBrush(Color(color));
		return br;
	}
	else
	if (tokens[0] == "gradient" && tokens.size() > 1)
	{
		std::vector<std::string> params;
		IuStringUtils::Split(tokens[1], " ", params, 10);
		if (params.size() >= 3)
		{
			unsigned int color1 = EvaluateColor(params[0]);
			unsigned int color2 = EvaluateColor(params[1]);
			int type = -1;
			std::string gradType = params[2];
			if (gradType == "vert")
			{
				type = LinearGradientModeVertical;
			}
			else
			if (gradType == "hor")
			{
				type = LinearGradientModeHorizontal;
			}
			else
			if (gradType == "diag1")
			{
				type = LinearGradientModeForwardDiagonal;
			}
			else
			if (gradType == "diag2")
			{
				type = LinearGradientModeBackwardDiagonal;
			}
			// return new LinearGradientBrush(Rect(0,0, /*rect.left+*/rect.right , /*rect.top+*/rect.bottom ), Color(color1), Color(color2), LinearGradientMode(type));
			return new LinearGradientBrush(RectF(float(rect.left), float(-0.5 + rect.top), float(rect.right),
			                                     /*rect.top+*/ float(rect.bottom) ), Color(color1), Color(
			                                  color2), LinearGradientMode(type));
		}
	}
	SolidBrush*   br = new SolidBrush(0);
	return br;
}

unsigned int CImageConverter::EvaluateColor(const std::string& expr)
{
	unsigned int color  = 0;
	color = EvaluateExpression(expr);
	color = ((color << 8) >> 8) | ((255 - (color >> 24)) << 24);
	return color;
}

void CImageConverter::setEnableProcessing(bool enable)
{
	processing_enabled = enable;
}
