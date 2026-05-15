#pragma once
#include "afxdialogex.h"
#include "PairListCtrl.h"
#include "PairImgCtrl.h"
#include <utility>
#include <string>
#include <vector>

// CDupImgFinder2Dlg 对话框

class CDupImgFinder2Dlg : public CDialogEx
{
	DECLARE_DYNAMIC(CDupImgFinder2Dlg)

public:
    std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>> t_lhs, t_rhs;
	CDupImgFinder2Dlg( CWnd* pParent
        , std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>> lhs
        , std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>> rhs );
	virtual ~CDupImgFinder2Dlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DUPIMGFINDER2_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持
    virtual BOOL OnInitDialog() override;
    afx_msg void OnDestroy();
    afx_msg void OnGetMinMaxInfo( MINMAXINFO* lpMMI );
    CPoint m_MouseL;
    afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
    afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
    afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
    DECLARE_MESSAGE_MAP()

    HICON m_hIcon;
    CSize m_csMinWin = {0,0}; CSize m_csMinWin_PL, m_csMinWin_PI;
    static CSize m_csLastWin_sz; static bool m_csLastWin_zd;
    CPairImgCtrl m_PairImg;
    CPairListCtrl m_PairList;

public:
    void Init();
    afx_msg void OnBnClickedBtnSwapPair();
    afx_msg void OnBnClickedBtnDeleteFile();
};
