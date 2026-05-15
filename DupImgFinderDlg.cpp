
// DupImgFinderDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "DupImgFinder.h"
#include "DupImgFinderDlg.h"
#include "afxdialogex.h"
#include "misc.h"
#include "pHashs.h"
#include <filesystem>
#include <fstream>
#include <print>
#include <cinttypes>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

bool CDupImgFinderDlg::SaveCache()
{
    std::wstring cache_filename;
    {
        cache_filename = misc::get_process_path();
        if( cache_filename.empty() )
            return false;

        cache_filename = std::filesystem::path(cache_filename).replace_extension(L".cache").wstring();
        if( !misc::set_file_attribute_normal(cache_filename) )
            return false;
    }

    std::ofstream cache( cache_filename, std::ios_base::binary );
    if( !cache )
    {
        TRACE( L"[失败] %s - %s\n", L"无法创建文件", cache_filename.c_str() );
        return false;
    }
    cache.exceptions( std::ifstream::failbit | std::ifstream::badbit );

    auto write = [&]<class... Args>( std::wformat_string<Args...> fmt, Args&&... args )
    {
        std::wstring s = std::format( std::move(fmt), std::forward<Args>(args)... );
        cache.write( (const char*)s.data(), s.size()*sizeof(decltype(s)::value_type) );
    };

    try
    {
        write( L"\uFEFF" );

        CComboBox& simctrl = *(CComboBox*)GetDlgItem(IDC_COMBO_SIMILARITY);
        int similarity_cursel = simctrl.GetCurSel();

        write( L"\r\n[SEARCH FOLDER] {} {}\r\n", m_Folders.GetCurSel(), similarity_cursel );
        {
            const int n = m_Folders.GetCount();
            for( int i=0; i!=n; ++i )
            {
                CString s;
                m_Folders.GetLBText( i, s );
                write( L"{}\r\n", (LPCTSTR)s );
            }
        }

        write( L"\r\n[FILE PHASH]\r\n" );
        for( const auto& [a,b] : m_PairList.m_raw )
        {
            write( L" : {}\r\n", a );
            for( const auto& [c,d] : b )
            {
                write( L"\t{:016X} : {}\r\n", d, c );
            }
        }

        write( L"\r\n[IMAGE PAIR]\r\n" );
        for( const auto& [a,b,c,d,e,f] : m_PairList.m_pair )
        {
            write( L"{} {} {} {} {} {}\r\n", a,b,c,d,e,f );
        }
    }
    catch( const std::exception& e )
    {
        TRACE( "[失败] %s\n", e.what() );
        cache.close();
        std::filesystem::remove( cache_filename );
        return false;
    }

    return true;
}

bool CDupImgFinderDlg::LoadCache()
{
    std::wstring cache_filename;
    {
        cache_filename = misc::get_process_path();
        if( cache_filename.empty() )
            return false;

        cache_filename = std::filesystem::path(cache_filename).replace_extension(L".cache").wstring();
        if( !std::filesystem::exists(cache_filename) )
            return true;
    }

    std::ifstream cache( cache_filename, std::ios_base::binary );
    if( !cache )
    {
        TRACE( L"[失败] %s - %s\n", L"无法打开文件", cache_filename.c_str() );
        return false;
    }
    cache.exceptions( std::ifstream::badbit );

    auto getline = [&,s=std::wstring()]( std::wstring& line ) mutable
    {
        for( ; ; )
        {
            if( cache.eof() && s.empty() )
                return false;

            size_t idx = s.find( L"\r\n" );
            if( idx != std::wstring::npos )
            {
                line = s.substr(0,idx);
                s.erase( 0, idx+2 );
                return true;
            }
            else if( cache.eof() )
            {
                line = std::move(s);
                s.clear();
                return true;
            }
            else
            {
                std::wstring t( 1024, L'\0' );
                cache.read( (char*)t.data(), t.size()*sizeof(decltype(t)::value_type) );
                size_t n = cache.gcount();
                if( n % sizeof(decltype(t)::value_type) != 0 )
                    return false;
                t.resize( n / sizeof(decltype(t)::value_type) );
                s += t;
                continue;
            }
        }
        return false;
    };

    int folders_cursel = -1;
    int similarity_cursel = -1;
    try
    {
        wchar_t BOM;
        cache.read( (char*)&BOM, sizeof(BOM) );
        if( cache.gcount()!=sizeof(BOM) || BOM!=L'\uFEFF' )
            return false;

        int cur_section = 0;
        for( std::wstring line; getline(line); )
        {
            if( line.empty() ) continue;

            if( line.front() == L'[' )
            {
                if( line.starts_with(L"[SEARCH FOLDER]") )
                {
                    if( 2 != swscanf(line.c_str(),L"[SEARCH FOLDER]%d %d",&folders_cursel,&similarity_cursel) )
                        return false;
                    cur_section = 1;
                }
                else if( line.starts_with(L"[FILE PHASH]") )
                {
                    cur_section = 2;
                }
                else if( line.starts_with(L"[IMAGE PAIR]") )
                {
                    cur_section = 3;
                }
                else
                {
                    return false;
                }
            }
            else if( cur_section == 0 )
            {
                return false;
            }
            else if( cur_section == 1 )
            {
                m_Folders.AddString( line.c_str() );
                m_PairList.m_folders.push_back( line );
            }
            else if( cur_section == 2 )
            {
                if( line.front() != L'\t' )
                {
                    if( !line.starts_with(L" : ") )
                        return false;

                    m_PairList.m_raw.push_back( {} );
                    m_PairList.m_raw.back().first = line.substr(3);
                }
                else if( m_PairList.m_raw.empty() )
                    return false;
                else
                {
                    size_t idx = line.find( L" : " );
                    if( idx == std::wstring::npos )
                        return false;

                    uint64_t phash;
                    if( 1 != swscanf(line.c_str(), L"%" SCNx64, &phash) )
                        return false;

                    m_PairList.m_raw.back().second.emplace_back( line.substr(idx+3), phash );
                }
            }
            else if( cur_section == 3 )
            {
                size_t a; size_t b; int c; size_t d; size_t e; UINT f;
                if( 6 != swscanf(line.c_str(), L"%zu%zu%d%zu%zu%u", &a,&b,&c,&d,&e,&f) )
                    return false;
                m_PairList.m_pair.emplace_back(a,b,c,d,e,f);
            }
        }
    }
    catch( const std::exception& e )
    {
        TRACE( "[失败] %s\n", e.what() );
        cache.close();
        return false;
    }

    m_Folders.SetCurSel( folders_cursel );
    CComboBox& simctrl = *(CComboBox*)GetDlgItem(IDC_COMBO_SIMILARITY);
    simctrl.SetCurSel( similarity_cursel );

    return true;
}

// CDupImgFinderDlg 对话框

CDupImgFinderDlg::CDupImgFinderDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DUPIMGFINDER_DIALOG, pParent)
    , m_PairList(m_PairImg,false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDupImgFinderDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange( pDX );
    DDX_Control( pDX, IDC_FOLDERS, m_Folders );
    DDX_Control( pDX, IDC_LIST_PAIR, m_PairList );
    DDX_Control( pDX, IDC_IMG, m_PairImg );
}

BEGIN_MESSAGE_MAP(CDupImgFinderDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_WM_GETMINMAXINFO()
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_BN_CLICKED( IDC_BTN_APPEND_FOLDER, &CDupImgFinderDlg::OnBnClickedBtnAppendFolder )
    ON_BN_CLICKED( IDC_BTN_REMOVE_FOLDER, &CDupImgFinderDlg::OnBnClickedBtnRemoveFolder )
    ON_BN_CLICKED( IDC_BTN_RUN, &CDupImgFinderDlg::OnBnClickedBtnRun )
    ON_WM_TIMER()
    ON_BN_CLICKED( IDC_BTN_SWAP_PAIR, &CDupImgFinderDlg::OnBnClickedBtnSwapPair )
    ON_BN_CLICKED( IDC_BTN_REMOVE_PAIR, &CDupImgFinderDlg::OnBnClickedBtnRemovePair )
    ON_BN_CLICKED( IDC_BTN_DELETE_FILE, &CDupImgFinderDlg::OnBnClickedBtnDeleteFile )
END_MESSAGE_MAP()

// CDupImgFinderDlg 消息处理程序

BOOL CDupImgFinderDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

    CComboBox& simctrl = *(CComboBox*)GetDlgItem(IDC_COMBO_SIMILARITY);
    for( int i=1; i<=64; ++i )
        simctrl.AddString( std::format(L"{}%", std::round(i*100/64.)).c_str() );
    simctrl.SetCurSel(62);

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

    ::SetWindowText( ::GetDlgItem(m_hWnd,IDC_CURRENT_TIP), L"" );
    UpdateBtnState();

    EnableToolTips(TRUE);
    if( m_ToolTip.Create(this) )
    {
        m_ToolTip.Activate(TRUE);
        m_ToolTip.SetMaxTipWidth(300);
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_APPEND_FOLDER), _T("添加待处理目录") );
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_REMOVE_FOLDER), _T("移除待处理目录") );
        m_ToolTip.AddTool( GetDlgItem(IDC_COMBO_SIMILARITY), _T("设定最低相似度") );
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_RUN), _T("开始处理\r\n(右键可获得更多功能)") );
        
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_SWAP_PAIR), _T("交换左右项目") );
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_REMOVE_PAIR), _T("从表格中移除此对文件(而非删除)") );
        m_ToolTip.AddTool( GetDlgItem(IDC_BTN_DELETE_FILE), _T("删除右侧文件(有可能进入回收站中)") );
    }

    if( LoadCache() )
    {
        m_PairList.m_ModifyStateSafeCount = 1;
        m_PairList.RestorState();
    }
    UpdateBtnState();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

BOOL CDupImgFinderDlg::DestroyWindow()
{
    if( m_Thread.joinable() )
    {
        m_Thread.request_stop();
        m_Thread.join();
    }
    TRACE( L"主界面退出\n" );

    m_PairList.SaveState();
    SaveCache();

    return CDialogEx::DestroyWindow();
}

BOOL CDupImgFinderDlg::PreTranslateMessage( MSG* pMsg )
{
    // TODO: 在此添加专用代码和/或调用基类
    if( m_ToolTip.GetSafeHwnd() )
    {
        m_ToolTip.RelayEvent(pMsg);
    }

    return CDialogEx::PreTranslateMessage( pMsg );
}

void CDupImgFinderDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CDupImgFinderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CDupImgFinderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CDupImgFinderDlg::OnGetMinMaxInfo( MINMAXINFO* lpMMI )
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值

    CDialogEx::OnGetMinMaxInfo( lpMMI );

    if( m_csMinWin.cx != 0 )
    {
        lpMMI->ptMinTrackSize.x = m_csMinWin.cx;
        lpMMI->ptMinTrackSize.y = m_csMinWin.cy;
    }
}

void CDupImgFinderDlg::UpdateBtnState( void )
{
    if( m_Folders.GetCount() == 0 )
    {
        ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_APPEND_FOLDER), TRUE );
        ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_REMOVE_FOLDER), FALSE );
        ::SetWindowText( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), L"▶" );
        ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), FALSE );
    }
    else
    {
        CString run;
        GetDlgItem(IDC_BTN_RUN)->GetWindowText( run );
        if( run == L"▶" )
        {
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_APPEND_FOLDER), TRUE );
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_REMOVE_FOLDER), TRUE );
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), TRUE );
        }
        else
        {
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_APPEND_FOLDER), FALSE );
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_REMOVE_FOLDER), FALSE );
            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), TRUE );
        }
    }

    //if( m_PairList.GetSelectedCount() == 1 )
    //{
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_SWAP_PAIR), TRUE );
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_REMOVE_PAIR), TRUE );
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_DELETE_FILE), TRUE );
    //}
    //else
    //{
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_SWAP_PAIR), FALSE );
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_REMOVE_PAIR), FALSE );
    //    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_DELETE_FILE), FALSE );
    //}
}

void CDupImgFinderDlg::OnBnClickedBtnAppendFolder()
{
    std::wstring initial_path;
    if( int idx=m_Folders.GetCurSel(); idx!=CB_ERR )
    {
        CString s;
        m_Folders.GetLBText( idx, s );
        std::filesystem::path p = (LPCTSTR)s;
        if( p.has_parent_path() )
            initial_path = p.parent_path().wstring();
        if( initial_path == p )
            initial_path.clear();
    }

    CString folder = misc::ShowFolderBrowserDialog( GetSafeHwnd(), nullptr, initial_path.c_str() );
    if( !folder.IsEmpty() )
    {
        folder = misc::lexically_normal_tolower( (LPCTSTR)folder ).c_str();

        if( int idx=m_Folders.FindStringExact(-1,folder); idx!=CB_ERR )
            m_Folders.DeleteString( idx );

        int n = m_Folders.GetCount();
        for( int i=0; i!=n; ++i )
        {
            CString s;
            m_Folders.GetLBText( i, s );
            if( misc::is_path_containing(folder,s) )
            {
                CString tip;
                tip.Format( L"\"%s\"\n与\n\"%s\"\n为互包含关系.", (LPCTSTR)folder, (LPCTSTR)s );
                MessageBoxW( tip, L"添加新目录失败", MB_OK|MB_ICONERROR );
                return;
            }
        }

        m_Folders.AddString( folder );
        m_Folders.SetCurSel( m_Folders.GetCount() - 1 );
    }

    UpdateBtnState();
}

void CDupImgFinderDlg::OnBnClickedBtnRemoveFolder()
{
    if( int idx=m_Folders.GetCurSel(); idx!=CB_ERR )
    {
        m_Folders.DeleteString( idx );
        int n = m_Folders.GetCount();
        if( idx < n )
            m_Folders.SetCurSel( idx );
        else if( n != 0 )
            m_Folders.SetCurSel( n-1 );
        else
            m_Folders.SetCurSel( -1 );
    }

    UpdateBtnState();
}

void CDupImgFinderDlg::OnBnClickedBtnRun()
{
    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_COMBO_SIMILARITY), FALSE );

    CString run;
    GetDlgItem(IDC_BTN_RUN)->GetWindowText( run );
    ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), FALSE );
    if( run == L"▶" )
    {
        SetDlgItemTextW( IDC_BTN_RUN, L"🛇" );

        std::vector<std::wstring> t_folders;
        const int n = m_Folders.GetCount();
        for( int i=0; i!=n; ++i )
        {
            CString s;
            m_Folders.GetLBText( i, s );
            t_folders.emplace_back( (LPCTSTR)s );
        }

        bool 可以利用上次结果 = (t_folders==m_PairList.m_folders && !m_PairList.m_raw.empty());
        if( 可以利用上次结果 )
        {
            int r = MessageBoxW( L"是否利用上次搜索的缓存？\n\nYes: 利用上次的搜索缓存\nNo: 进行全新搜索\nCancel: 取消本次操作", nullptr, MB_ICONQUESTION|MB_YESNOCANCEL|MB_DEFBUTTON3 );
            if( IDCANCEL == r )
            {
                ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_COMBO_SIMILARITY), TRUE );
                SetDlgItemTextW( IDC_BTN_RUN, L"▶" );
                ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), TRUE );
                return;
            }
            else if( IDNO == r )
                可以利用上次结果 = false;
        }
        else if( !m_PairList.m_pair.empty() )
        {
            int r = MessageBoxW( L"是否进行本次搜索？\n\nYes: 进行搜索\nNo: 取消本次操作", nullptr, MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 );
            if( IDNO == r )
            {
                ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_COMBO_SIMILARITY), TRUE );
                SetDlgItemTextW( IDC_BTN_RUN, L"▶" );
                ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), TRUE );
                return;
            }
        }

        m_PairList.ClearPairData();
        m_Thread_Result1 = std::move( m_PairList.m_raw );
        m_PairList.m_raw.clear();

        m_PairList.m_folders = std::move( t_folders );
        m_Thread_Progress.clear();
        if( !可以利用上次结果 )
            m_Thread_Result1.clear();
        m_Thread_Result2.clear();
        m_Thread_Summary.clear();

        int min_similarity = ((CComboBox*)GetDlgItem(IDC_COMBO_SIMILARITY))->GetCurSel()+1;

        m_Thread = std::jthread( thread_cal_pHashs
            , m_PairList.m_folders
            , std::ref(m_Thread_Mutex)
            , std::ref(m_Thread_Progress)
            , std::ref(m_Thread_Result1)
            , min_similarity
            , std::ref(m_Thread_Result2)
            , std::ref(m_Thread_Summary) );
        SetTimer( 1, 100, nullptr );

        ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_BTN_RUN), TRUE );
    }
    else
    {
        if( m_Thread.joinable() )
            m_Thread.request_stop();
    }

    UpdateBtnState();
}

void CDupImgFinderDlg::OnTimer( UINT_PTR nIDEvent )
{
    if( m_Thread_Mutex.try_lock() )
    {
        std::wstring t_Thread_Progress = m_Thread_Progress;
        std::wstring t_Thread_Summary = m_Thread_Summary;
        m_Thread_Mutex.unlock();

        if( !t_Thread_Summary.empty() ) // 线程已结束
        {
            KillTimer( 1 );

            m_Thread.join();

            SetDlgItemTextW( IDC_CURRENT_TIP, L"" );
            m_PairList.SetData( std::move(m_Thread_Result1), std::move(m_Thread_Result2) );

            SaveCache();
            
            MessageBoxW( t_Thread_Summary.c_str(), 0, MB_OK );

            SetDlgItemTextW( IDC_BTN_RUN, L"▶" );
            GetDlgItem(IDC_BTN_RUN)->EnableWindow( TRUE );
            UpdateBtnState();

            ::EnableWindow( ::GetDlgItem(m_hWnd,IDC_COMBO_SIMILARITY), TRUE );
        }
        else
        {
            const wchar_t* prefix = clock()/(CLOCKS_PER_SEC/4)%2==0 ? L"⏳" : L"⌛";
            const std::wstring new1 = prefix + t_Thread_Progress;
            CString old1;
            GetDlgItemTextW( IDC_CURRENT_TIP, old1 );
            if( old1 != new1.c_str() )
                SetDlgItemTextW( IDC_CURRENT_TIP, new1.c_str() );
        }
    }

    CDialogEx::OnTimer( nIDEvent );
}

void CDupImgFinderDlg::OnBnClickedBtnSwapPair()
{
    m_PairList.Swap_Selected_Item();
}

void CDupImgFinderDlg::OnBnClickedBtnRemovePair()
{
    m_PairList.Remove_Selected_Item();
}

void CDupImgFinderDlg::OnBnClickedBtnDeleteFile()
{
    m_PairList.Delete_Selected_Item_RHS();
}

BOOL CDupImgFinderDlg::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
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

void CDupImgFinderDlg::OnLButtonDown( UINT nFlags, CPoint point )
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

void CDupImgFinderDlg::OnLButtonUp( UINT nFlags, CPoint point )
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

            HRSRC hRes = FindResource( NULL, MAKEINTRESOURCE(IDD_DUPIMGFINDER_DIALOG), _T("AFX_DIALOG_LAYOUT") );
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
