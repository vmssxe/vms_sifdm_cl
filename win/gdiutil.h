#pragma once

inline void vmsFillSolidRect (HDC hdc, LPCRECT lpRect, COLORREF clr)
{
	assert (hdc);
	assert (lpRect);
	::SetBkColor (hdc, clr);
	::ExtTextOut (hdc, 0, 0, ETO_OPAQUE, lpRect, NULL, 0, NULL);
}

inline void vmsFillSolidRect (HDC hdc, int left, int top, int width, int height, COLORREF clr)
{
	vmsWinRect rc (left, top, left + width, top + height);
	vmsFillSolidRect (hdc, &rc, clr);
}

inline void vmsGetBitmap (HBITMAP hbmp, BITMAP &bm)
{
	GetObject (hbmp, sizeof (BITMAP), &bm);
}

inline SIZE vmsGetTextExtent (HDC hdc, const std::wstring& wstr)
{
	SIZE s = {0, 0};
	GetTextExtentPoint32 (hdc, wstr.c_str (), (int)wstr.length (), &s);
	return s;
}

inline HBITMAP vmsCreateX32Bitmap (int cx, int cy)
{
	assert (cx > 0 && cy > 0);

	BITMAPINFO bmi; ZeroMemory (&bmi, sizeof (bmi));
	bmi.bmiHeader.biSize = sizeof (bmi.bmiHeader);
	bmi.bmiHeader.biWidth = cx;
	bmi.bmiHeader.biHeight = - cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;

	LPDWORD pdwPixels = nullptr;

	return CreateDIBSection (NULL, &bmi, DIB_RGB_COLORS, (LPVOID*)&pdwPixels, NULL, 0);
}