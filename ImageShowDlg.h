#pragma once
#include "afxdialogex.h"


// CImageShowDlg 对话框

class CImageShowDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CImageShowDlg)

public:
	CImageShowDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CImageShowDlg();

    size_t m_ImgIdx = 0;
    Gdiplus::Bitmap* m_imgs[2];

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IMAGE_SHOW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持
    virtual BOOL OnInitDialog() override;
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnPaint();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
    afx_msg BOOL OnNcActivate( BOOL bActive );
};
