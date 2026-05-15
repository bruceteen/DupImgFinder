// DupImgFinder2Dlg.cpp: 实现文件
//

#include "pch.h"
#include "DupImgFinder.h"
#include "afxdialogex.h"
#include "DupImgFinder2Dlg.h"

// CDupImgFinder2Dlg 对话框

CSize CDupImgFinder2Dlg::m_csLastWin_sz = { 0, 0 };
bool CDupImgFinder2Dlg::m_csLastWin_zd = false;

IMPLEMENT_DYNAMIC(CDupImgFinder2Dlg, CDialogEx)

CDupImgFinder2Dlg::CDupImgFinder2Dlg( CWnd* pParent
        , std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>> lhs
        , std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>> rhs )
    : CDialogEx(IDD_DUPIMGFINDER2_DIALOG, pParent)
    , m_PairList(m_PairImg,true)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    t_lhs = std::move(lhs);
    t_rhs = std::move(rhs);
}

CDupImgFinder2Dlg::~CDupImgFinder2Dlg()
{
}

void CDupImgFinder2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
    DDX_Control( pDX, IDC_LIST_PAIR, m_PairList );
    DDX_Control( pDX, IDC_IMG, m_PairImg );
}

BEGIN_MESSAGE_MAP(CDupImgFinder2Dlg, CDialogEx)
    ON_WM_DESTROY()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED( IDC_BTN_SWAP_PAIR, &CDupImgFinder2Dlg::OnBnClickedBtnSwapPair )
    ON_BN_CLICKED( IDC_BTN_DELETE_FILE, &CDupImgFinder2Dlg::OnBnClickedBtnDeleteFile )
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

// CDupImgFinder2Dlg 消息处理程序

void CDupImgFinder2Dlg::Init()
{
    SetDlgItemTextW( IDC_CURRENT_TIP, L"" );
    SetDlgItemTextW( IDC_LEFT, (L"左: "+t_lhs.first).c_str() );
    SetDlgItemTextW( IDC_RIGHT, (L"右: "+t_rhs.first).c_str() );

    std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>> t_pair;
    t_pair.reserve( t_lhs.second.size() * t_rhs.second.size() );
    for( size_t i=0; i!=t_lhs.second.size(); ++i )
    {
        if( t_lhs.second[i].first.empty() ) continue;
        for( size_t j=0; j!=t_rhs.second.size(); ++j )
        {
            if( t_rhs.second[j].first.empty() ) continue;
            auto cmp_result = sizeof(uint64_t)*CHAR_BIT - std::popcount( t_lhs.second[i].second  ^ t_rhs.second[j].second  );
            t_pair.emplace_back( 0,i, (int)cmp_result, 1,j, 0 );
        }
    }

    std::ranges::stable_sort( t_pair, [](const auto& a, const auto& b){ return std::get<2>(a) > std::get<2>(b); } );
    for( size_t i=0; i!=t_pair.size(); ++i )
    {
        auto rgn = std::ranges::remove_if( t_pair.begin()+i+1, t_pair.end()
            , [&](const auto& v){
                return std::get<1>(v)==std::get<1>(t_pair[i]) || std::get<4>(v)==std::get<4>(t_pair[i]);
            } );
        t_pair.erase( rgn.begin(), rgn.end() );
    }
    if( t_lhs.second.size() > t_pair.size() )
    {
        for( size_t i=0; i!=t_lhs.second.size(); ++i )
        {
            if( t_lhs.second[i].first.empty() ) continue;
            auto itor = std::ranges::find_if( t_pair, [i](const auto& v){ return std::get<1>(v)==i; } );
            if( itor == t_pair.end() )
                t_pair.emplace_back( 0, i, INT_MAX, size_t(-1), 0, 0 );
        }
    }
    if( t_rhs.second.size() > t_pair.size() )
    {
        for( size_t i=0; i!=t_rhs.second.size(); ++i )
        {
            if( t_rhs.second[i].first.empty() ) continue;
            auto itor = std::ranges::find_if( t_pair, [i](const auto& v){ return std::get<4>(v)==i; } );
            if( itor == t_pair.end() )
                t_pair.emplace_back( size_t(-1), 0, INT_MAX, 1, i, 0 );
        }
    }

    auto t_raw = std::vector{ std::move(t_lhs), std::move(t_rhs) };
    m_PairList.SetData( std::move(t_raw), std::move(t_pair) );
}

BOOL CDupImgFinder2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

    CRect rect;
    GetWindowRect( &rect );
    m_csMinWin = rect.Size();
    {
        CRect tmp;
        m_PairList.GetWindowRect( &tmp );
        m_csMinWin_PL = CSize( tmp.Width(), tmp.Height()/2 );
        m_PairImg.GetWindowRect( &tmp );
        m_csMinWin_PI = CSize( tmp.Width(), tmp.Height()/2 );
    }

    if( m_csLastWin_zd )
    {
        ShowWindow( SW_SHOWMAXIMIZED );
    }
    else if( m_csLastWin_sz.cx != 0 )
    {
        SetWindowPos( nullptr, 0, 0, m_csLastWin_sz.cx, m_csLastWin_sz.cy
            , SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOREDRAW|SWP_NOSENDCHANGING );
    }

    Init();
    m_PairList.SetFocus();
	return FALSE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CDupImgFinder2Dlg::OnDestroy()
{
    if( IsZoomed() )
    {
        m_csLastWin_zd = true;
    }
    else
    {
        CRect rect;
        GetWindowRect( &rect );
        m_csLastWin_sz = rect.Size();
    }

    CDialogEx::OnDestroy();
}

void CDupImgFinder2Dlg::OnGetMinMaxInfo( MINMAXINFO* lpMMI )
{
    CDialogEx::OnGetMinMaxInfo( lpMMI );

    if( m_csMinWin.cx != 0 )
    {
        lpMMI->ptMinTrackSize.x = m_csMinWin.cx;
        lpMMI->ptMinTrackSize.y = m_csMinWin.cy;
    }
}

void CDupImgFinder2Dlg::OnBnClickedBtnSwapPair()
{
    CString a, b;
    GetDlgItemText( IDC_LEFT, a );
    GetDlgItemText( IDC_RIGHT, b );
    a.SetAt( 0, L'右' );
    b.SetAt( 0, L'左' );
    SetDlgItemTextW( IDC_LEFT, b );
    SetDlgItemTextW( IDC_RIGHT, a );
    Invalidate();

    m_PairList.Swap_AllPair();
}

void CDupImgFinder2Dlg::OnBnClickedBtnDeleteFile()
{
    m_PairList.Delete_Selected_Item_RHS();
}

BOOL CDupImgFinder2Dlg::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
    if( m_MouseL != CPoint() )
    {
        SetCursor( AfxGetApp()->LoadStandardCursor(IDC_SIZENS) );
        return TRUE;
    }

    if( pWnd==this && IsTopParentActive() && IsWindowEnabled() )
    {
        CRect a, b; CPoint pt;
        m_PairList.GetWindowRect( &a );
        m_PairImg.GetWindowRect( &b );
        CRect c( (std::max)(a.left,b.left), a.bottom, (std::min)(a.right,b.right), b.top );
        GetCursorPos( &pt );
        if( c.PtInRect(pt) )
        {
            SetCursor( AfxGetApp()->LoadStandardCursor(IDC_SIZENS) );
            return TRUE;
        }
    }

    return CDialogEx::OnSetCursor( pWnd, nHitTest, message );
}

void CDupImgFinder2Dlg::OnLButtonDown( UINT nFlags, CPoint point )
{
    if( IsTopParentActive() && IsWindowEnabled() )
    {
        CRect a, b; CPoint pt;
        m_PairList.GetWindowRect( &a );
        m_PairImg.GetWindowRect( &b );
        CRect c( (std::max)(a.left,b.left), a.bottom, (std::min)(a.right,b.right), b.top );
        GetCursorPos( &pt );
        if( c.PtInRect(pt) )
        {
            m_MouseL = pt;
            CRect d( c.left, a.top+m_csMinWin_PL.cy, c.right, b.bottom-m_csMinWin_PI.cy );
            ClipCursor( &d );
            m_PairList.EnableWindow( FALSE );
            m_PairImg.EnableWindow( FALSE );
        }
    }

    CDialogEx::OnLButtonDown( nFlags, point );
}

void CDupImgFinder2Dlg::OnLButtonUp( UINT nFlags, CPoint point )
{
    if( m_MouseL != CPoint() )
    {
        CRect a, b; CPoint pt;
        m_PairList.GetWindowRect( &a );
        m_PairImg.GetWindowRect( &b );
        GetCursorPos( &pt );
        if( pt.y != m_MouseL.y )
        {
            EnableDynamicLayout( FALSE );

            LONG delta = pt.y-m_MouseL.y;

            m_PairList.SetWindowPos( nullptr, 0, 0, a.Width(), a.Height()+delta
                , SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOREDRAW|SWP_NOSENDCHANGING );

            ScreenToClient( &b );
            m_PairImg.SetWindowPos( nullptr, b.left, b.top+delta, b.Width(), b.Height()-delta
                , SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOREDRAW|SWP_NOSENDCHANGING );

            HRSRC hRes = FindResource( NULL, MAKEINTRESOURCE(IDD_DUPIMGFINDER2_DIALOG), _T("AFX_DIALOG_LAYOUT") );
            if( hRes )
            {
                HGLOBAL hData = LoadResource(NULL, hRes);
                if( hData )
                {
                    LPVOID lpData = LockResource(hData);
                    DWORD dwSize = SizeofResource(NULL, hRes);
                    CMFCDynamicLayout::LoadResource(this, lpData, dwSize);
        
                    FreeResource(hData);
                }
            }

            Invalidate();
        }

        m_PairImg.EnableWindow( TRUE );
        m_PairList.EnableWindow( TRUE );
        ClipCursor( nullptr );
        m_MouseL = CPoint();
    }

    CDialogEx::OnLButtonUp( nFlags, point );
}