// ImageShowDlg.cpp: 实现文件
//

#include "pch.h"
#include "DupImgFinder.h"
#include "afxdialogex.h"
#include "ImageShowDlg.h"
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CImageShowDlg 对话框

IMPLEMENT_DYNAMIC(CImageShowDlg, CDialogEx)

CImageShowDlg::CImageShowDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_IMAGE_SHOW, pParent)
{

}

CImageShowDlg::~CImageShowDlg()
{
}

void CImageShowDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CImageShowDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_NCACTIVATE()
END_MESSAGE_MAP()

BOOL CImageShowDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ShowWindow(SW_MAXIMIZE);
    SetFocus();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CImageShowDlg::OnPaint()
{
    CPaintDC dc( this ); // device context for painting
    // TODO: 在此处添加消息处理程序代码
    // 不为绘图消息调用 CDialogEx::OnPaint()

    if( m_imgs[m_ImgIdx] )
    {
        CRect rect;
        GetClientRect( &rect );

        auto rw = rect.Width();
        auto rh = rect.Height();

        auto& img = *m_imgs[m_ImgIdx];
        auto w = img.GetWidth();
        auto h = img.GetHeight();

        auto 新h = (int)std::round(h*1.0/w*rw);
        auto 新w = rw;
        if( 新h > rh )
        {
            新w = (int)std::round(w*1.0/h*rh);
            新h = rh;
        }

        CRect 新rect;
        新rect.left = (rect.Width() - 新w) / 2;
        新rect.top = (rect.Height() - 新h) / 2;
        新rect.right = 新rect.left + 新w;
        新rect.bottom = 新rect.top + 新h;

        {
            CDC MemDC;
            CBitmap bm;
            MemDC.CreateCompatibleDC( &dc );
            bm.CreateCompatibleBitmap( &dc, rect.Width(), rect.Height() );
            CBitmap* pOldBitmap = MemDC.SelectObject( &bm );
            MemDC.SetWindowOrg( 0, 0 );
            MemDC.FillSolidRect( &rect, RGB(128,128,128) );

            Gdiplus::Graphics graphics( MemDC.GetSafeHdc() );
            graphics.DrawImage( &img, 新rect.left, 新rect.top, 新w, 新h );

            dc.BitBlt( 0, 0, rw, rh, &MemDC, 0, 0, SRCCOPY );

            MemDC.SelectObject(pOldBitmap);
            bm.DeleteObject();
        }

        //Gdiplus::Graphics graphics( dc.GetSafeHdc() );
        //graphics.DrawImage( &img, 新rect.left, 新rect.top, 新w, 新h );

        //if( 新rect.left != 0 )
        //{
        //    dc.FillSolidRect( 0, 0, 新rect.left, rh, RGB(128,128,128) );
        //    dc.FillSolidRect( 新rect.right, 0, rw-新rect.right, rh, RGB(128,128,128) );
        //}
        //if( 新rect.top != 0 )
        //{
        //    dc.FillSolidRect( 0, 0, rw, 新rect.top, RGB(128,128,128) );
        //    dc.FillSolidRect( 0, 新rect.bottom, rw, rh-新rect.bottom, RGB(128,128,128) );
        //}
    }
    else
        CDialogEx::OnPaint();
}

BOOL CImageShowDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    // zDelta > 0 表示向上滚动，zDelta < 0 表示向下滚动
    if( zDelta > 0 )
    {
        if( m_ImgIdx!=0 && m_imgs[0] )
        {
            m_ImgIdx = 0;
            Invalidate(FALSE);
            UpdateWindow();
        }
    }
    else
    {
        if( m_ImgIdx!=1 && m_imgs[1] )
        {
            m_ImgIdx = 1;
            Invalidate(FALSE);
            UpdateWindow();
        }
    }
    
    return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

void CImageShowDlg::OnLButtonDblClk( UINT nFlags, CPoint point )
{
    OnOK();

    //CDialogEx::OnLButtonDblClk( nFlags, point );
}

BOOL CImageShowDlg::OnNcActivate( BOOL bActive )
{
    if( !bActive )
        OnOK();

    return CDialogEx::OnNcActivate( bActive );
}
