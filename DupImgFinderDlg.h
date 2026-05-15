
// DupImgFinderDlg.h: 头文件
//

#pragma once
#include "PairListCtrl.h"
#include "PairImgCtrl.h"
#include <thread>
#include <mutex>

// CDupImgFinderDlg 对话框
class CDupImgFinderDlg : public CDialogEx
{
// 构造
public:
	CDupImgFinderDlg(CWnd* pParent = nullptr);	// 标准构造函数
    bool SaveCache();
    bool LoadCache();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DUPIMGFINDER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;
    CSize m_csMinWin={0,0}; CSize m_csMinWin_PL, m_csMinWin_PI;
    CToolTipCtrl m_ToolTip;
    CComboBox m_Folders;
    CPairImgCtrl m_PairImg;
    CPairListCtrl m_PairList;

    std::jthread m_Thread;
    std::vector<std::wstring> m_Thread_Folders;
    std::mutex m_Thread_Mutex;
    std::wstring m_Thread_Progress;
    std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>> m_Thread_Result1;
    std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>> m_Thread_Result2;
    std::wstring m_Thread_Summary;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog() override;
    virtual BOOL DestroyWindow() override;
    virtual BOOL PreTranslateMessage( MSG* pMsg ) override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnGetMinMaxInfo( MINMAXINFO* lpMMI );
    CPoint m_MouseL;
    afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
    afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
    afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
    void UpdateBtnState( void );
    afx_msg void OnBnClickedBtnAppendFolder();
    afx_msg void OnBnClickedBtnRemoveFolder();
    afx_msg void OnBnClickedBtnRun();
    afx_msg void OnTimer( UINT_PTR nIDEvent );
    afx_msg void OnBnClickedBtnSwapPair();
    afx_msg void OnBnClickedBtnRemovePair();
    afx_msg void OnBnClickedBtnDeleteFile();
	DECLARE_MESSAGE_MAP()

};
