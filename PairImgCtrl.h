#pragma once

#include "ImageShowDlg.h"

#include <filesystem>
#include <memory>
#include <gdiplus.h>

// CPairImgCtrl

class CPairImgCtrl : public CStatic
{
	DECLARE_DYNAMIC(CPairImgCtrl)

public:
    CImageShowDlg m_ImgDlg;

	CPairImgCtrl();
	virtual ~CPairImgCtrl();
    virtual void PreSubclassWindow() override;
    afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
    void ShowImagesFullScreen( bool left_first );
    struct ImageInfo
    {
        std::wstring filepath;
        std::wstring imgfmt;
        int width, height;
        uintmax_t filesize;
        std::wstring lastwritetime;

        CRect rect;
        CRect rect_img;
    };
    std::unique_ptr<Gdiplus::Bitmap> Load_Image( const std::filesystem::path& imgpath, std::vector<BYTE>& buffer, ImageInfo& info ) const;
    virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct ) override;
    bool SetImagePath( const std::filesystem::path& lhs, const std::filesystem::path& rhs );

protected:
	DECLARE_MESSAGE_MAP()

    std::vector<BYTE> m_lhs_buf; std::unique_ptr<Gdiplus::Bitmap> m_lhs; ImageInfo m_lhs_info={};
    std::vector<BYTE> m_rhs_buf; std::unique_ptr<Gdiplus::Bitmap> m_rhs; ImageInfo m_rhs_info={};
};


