
// DupImgFinder.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "DupImgFinder.h"
#include "DupImgFinderDlg.h"

#include "vips/vips.h"
#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CDupImgFinderApp

BEGIN_MESSAGE_MAP(CDupImgFinderApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CDupImgFinderApp 构造

CDupImgFinderApp::CDupImgFinderApp()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CDupImgFinderApp 对象

CDupImgFinderApp theApp;

// CDupImgFinderApp 初始化

BOOL CDupImgFinderApp::InitInstance()
{
    auto my_log_handler = []( const gchar* log_domain
                   , GLogLevelFlags log_level
                   , const gchar* message
                   , gpointer user_data ) {};
    [[maybe_unused]] guint handler_id = g_log_set_handler( "VIPS", G_LOG_LEVEL_MASK, +my_log_handler, nullptr );
    // On startup, you need to call VIPS_INIT() single-threaded.
    // After that, you can freely create images in any thread and read them in any other thread. 
    // Note that results can also be shared between threads for you by the libvips operation cache.
    if( VIPS_INIT("DuplicateImageFinder") )
    {
        vips_error_exit( nullptr );
        MessageBoxW( nullptr, L"unable to start VIPS", L"DuplicateImageFinder", MB_OK|MB_ICONSTOP );
        return FALSE;
    }
    vips_cache_set_max(0); // 必须要有，否则文件的引用计数不为0，导致文件句柄不被释放。G_OBJECT(image)->ref_count
    vips_cache_set_max_mem(0);
    // 可以禁，但没法知道哪些没被禁
    //g_setenv("VIPS_BLOCK_UNTRUSTED", "1", TRUE);
    //vips_block_untrusted_set()
    //vips_operation_block_set("VipsForeignLoadCsv", TRUE);
    //vips_operation_block_set("VipsForeignLoadMatrix", TRUE);
    //vips_operation_block_set("VipsForeignLoad", TRUE);

    //AfxInitRichEdit2();

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::Status status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    if( status != Gdiplus::Ok )
    {
        MessageBoxW( nullptr, L"unable to start GDI+", L"DuplicateImageFinder", MB_OK|MB_ICONSTOP );
        return false;
    }

	// 如果应用程序存在以下情况，Windows XP 上需要 InitCommonControlsEx()
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

    {
	    CDupImgFinderDlg dlg;
	    m_pMainWnd = &dlg;
	    INT_PTR nResponse = dlg.DoModal();
	    if (nResponse == IDOK)
	    {
		    // TODO: 在此放置处理何时用
		    //  “确定”来关闭对话框的代码
	    }
	    else if (nResponse == IDCANCEL)
	    {
		    // TODO: 在此放置处理何时用
		    //  “取消”来关闭对话框的代码
	    }
	    else if (nResponse == -1)
	    {
		    TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		    TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	    }
    }

	// 删除上面创建的 shell 管理器。
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

    Gdiplus::GdiplusShutdown(gdiplusToken);
    // Call this to drop caches, close plugins, terminate background threads, and finalize any internal library testing.
    // vips_shutdown() is optional. If you don’t call it, your platform will clean up for you. The only negative consequences are that the leak checker and the profiler will not work.
    // you may call VIPS_INIT() many times and vips_shutdown() many times, but you must not call VIPS_INIT() after vips_shutdown(). In other words, you cannot stop and restart libvips.
    vips_shutdown();

#ifdef _MSC_VER // 否则一切涉及到 tzdb 的都会看起来内存泄漏，比如 std::chrono::current_zone()
    if( auto p=std::chrono::_Global_tzdb_list.exchange(nullptr); p )
    {
        std::destroy_at( p );
        ::__std_free_crt( p );
    }
#endif

    // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

