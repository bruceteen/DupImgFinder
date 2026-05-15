#include "pch.h"
#include "misc.h"
//#include <Shobjidl.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>

namespace misc
{
    std::wstring get_process_path()
    {
        std::wstring filename;
        for( std::wstring proc_path(MAX_PATH,'\0'); ; )
        {
            DWORD n = GetModuleFileNameW( nullptr, proc_path.data(), (DWORD)proc_path.size() );
            if( n == 0 ) // 失败
            {
                TRACE( L"[失败] %s\n", L"获取进程目录失败!" );
                return {};
            }
            else if( n == proc_path.size() ) // 缓冲区过小
                proc_path.resize( 2*proc_path.size() );
            else // 成功
            {
                filename = std::filesystem::path( proc_path.data() ).replace_extension(L".cache");
                break;
            }
        }
        return filename;
    }

    bool set_file_attribute_normal( const std::wstring& file )
    {
        std::wstring fn = L"\\\\?\\" + file;
        DWORD attributes = GetFileAttributesW( fn.c_str() );
        if( attributes == INVALID_FILE_ATTRIBUTES ) // 文件不存在
            return true;

        if( (attributes&FILE_ATTRIBUTE_READONLY) != 0 )
        {
            attributes &= ~FILE_ATTRIBUTE_READONLY;
            if( !SetFileAttributesW(file.c_str(),attributes) )
                return false;
        }
        return true;
    }

    bool set_files_attribute_normal( const std::vector<std::wstring>& files )
    {
        bool result = true;
        for( const auto& file : files )
        {
            if( !set_file_attribute_normal(file) )
                result = false;
        }
        return true;
    }

    bool delete_files_by_IFileOperation( const std::vector<std::wstring>& files )
    {
        CComPtr<IFileOperation> pFileOp;
        HRESULT hr = pFileOp.CoCreateInstance( CLSID_FileOperation, NULL, CLSCTX_ALL );
        if( FAILED(hr) ) return false;
    
        hr = pFileOp->SetOperationFlags(FOF_ALLOWUNDO|FOF_NOCONFIRMATION);  // 允许撤销 = 移至回收站
        if( FAILED(hr) ) return false;

        bool result = true;
        for( size_t i=0; i!=files.size(); ++i )
        {
            CComPtr<IShellItem> pItem;
            hr = SHCreateItemFromParsingName( files[i].c_str(), nullptr, IID_PPV_ARGS(&pItem) );
            if( FAILED(hr) ) { result = false; continue; }

            hr = pFileOp->DeleteItem(pItem, nullptr);
            if( FAILED(hr) ) { result = false; continue; }
        }

        hr = pFileOp->PerformOperations(); // COPYENGINE_E_USER_CANCELLED
        if( FAILED(hr) ) { result = false; }

        BOOL aborted = FALSE;
        hr = pFileOp->GetAnyOperationsAborted( &aborted );
        if( FAILED(hr) || aborted ) { result = false; }

        return result;
    }

    CString ShowFolderBrowserDialog( HWND hwndOwner, LPCWSTR title, LPCWSTR initial_path )
    {
        CComPtr<IFileOpenDialog> pFileOpenDlg;
        if( SUCCEEDED(pFileOpenDlg.CoCreateInstance(__uuidof(FileOpenDialog))) )
        {
            if( title )
                pFileOpenDlg->SetTitle(title);

            FILEOPENDIALOGOPTIONS options;
            if( SUCCEEDED(pFileOpenDlg->GetOptions(&options)) )
            {
                options |= FOS_PATHMUSTEXIST | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM;
                if( SUCCEEDED(pFileOpenDlg->SetOptions(options)) )
                {
                    if( initial_path && *initial_path!=0 )
                    {
                        CComPtr<IShellItem> psiStartPath;
                        if( SUCCEEDED(SHCreateItemFromParsingName(initial_path, nullptr, IID_PPV_ARGS(&psiStartPath))) )
                        {
                            pFileOpenDlg->SetFolder(psiStartPath);
                        }
                    }
                    else
                    {
                        CComPtr<IShellItem> psiMyComputer;
                        HRESULT hr = SHCreateItemFromParsingName(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", 
                                                         nullptr, IID_PPV_ARGS(&psiMyComputer));
                        if( SUCCEEDED(hr) )
                        {
                            pFileOpenDlg->SetFolder(psiMyComputer);
                        }
                    }

                    if( SUCCEEDED(pFileOpenDlg->Show(hwndOwner)) )
                    {
                        CComPtr<IShellItem> pShellItemResult;
                        if( SUCCEEDED(pFileOpenDlg->GetResult(&pShellItemResult)) )
                        {
                            CComHeapPtr<wchar_t> pszSelectedItem;
                            if( SUCCEEDED(pShellItemResult->GetDisplayName(SIGDN_FILESYSPATH, &pszSelectedItem)) )
                            {
                                return pszSelectedItem.m_pData;
                            }
                        }
                    }
                }
            }
        }
        return {};
    }

    std::wstring lexically_normal_tolower( const std::filesystem::path& path )
    {
        auto s = path.lexically_normal().wstring();
        std::ranges::transform( s, s.begin(), [](wchar_t c){return std::towlower(c);} );
        return s;
    }
    bool is_path_containing( const wchar_t* a, const wchar_t* b )
    {
        std::filesystem::path norm_a = lexically_normal_tolower(a);
        std::filesystem::path norm_b = lexically_normal_tolower(b);

        auto [p,q] = std::ranges::mismatch( norm_a, norm_b );
        if( p == norm_a.end() ) return true;
        if( q == norm_b.end() ) return true;
        return false;
    }

    bool explore_file_in_explorer( const std::wstring& fullpath )
    {
        // Bug: 文件浏览器关闭后 文件浏览器进程 并不退出
        //std::wstring param = std::format( L"/select,\"{}\"", fullpath );
        //ShellExecuteW( nullptr, L"OPEN", L"explorer.exe", param.c_str(), nullptr, SW_SHOWDEFAULT );

        PIDLIST_ABSOLUTE pidl = nullptr;
        HRESULT hr = SHParseDisplayName( fullpath.c_str(), nullptr, &pidl, 0, nullptr );
        if( SUCCEEDED(hr) )
        {
            hr = SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 );
            hr = SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 ); // 偶尔打开文件夹但文件未必选择

            ILFree(pidl);
            return true;
        }
        return false;
    }

    bool explore_files_in_explorer( const std::wstring& folder, const std::vector<std::wstring>& files )
    {
        LPITEMIDLIST folderPIDL = ILCreateFromPathW( folder.c_str() );
        if( !folderPIDL ) return false;
        std::vector<LPITEMIDLIST> filePIDLs;

        auto guard_lambda = [&]( void* ) {
                for( LPITEMIDLIST filePIDL : filePIDLs )
                    ILFree( filePIDL );
                ILFree( folderPIDL );
            };
        auto guard_ = std::unique_ptr<void,decltype(guard_lambda)>( nullptr, guard_lambda );

        {
            filePIDLs.reserve( files.size() );
            std::filesystem::path folder_p = folder;
            for( const std::wstring& file : files )
            {
                LPITEMIDLIST filePIDL = ILCreateFromPathW( (folder_p/file).wstring().c_str() );
                if( !filePIDL )
                    return false;
                filePIDLs.push_back( filePIDL );
            }
        }
        if( filePIDLs.empty() ) // 否则只“打开父目录，选择此目录”，而非“打开此目录”
        {
            filePIDLs.push_back( nullptr );
        }
        HRESULT hr = SHOpenFolderAndSelectItems( folderPIDL, (UINT)filePIDLs.size(), (PCUITEMID_CHILD_ARRAY)filePIDLs.data(), 0 );
        return SUCCEEDED(hr);
    }

    HBITMAP GetFileTypeIcon_then_to_HBITMAP( const std::wstring& path, DWORD dwFileAttributes, HBRUSH hbrBack )
    {
        HBITMAP hBitmap=nullptr;

        std::wstring filename = L"_dummy_" + std::filesystem::path(path).extension().wstring();
        DWORD flags = SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES; // SHGFI_USEFILEATTRIBUTES 不需要实际去查找文件
        //DWORD dwFileAttributes = std::filesystem::is_directory(path) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

        SHFILEINFOW shfi = {0};
        if( SHGetFileInfoW(filename.c_str(), dwFileAttributes, &shfi, sizeof(shfi), flags) && shfi.hIcon )
        {
            ICONINFO iconInfo = {};
            if( GetIconInfo(shfi.hIcon, &iconInfo) )
            {
                HBITMAP hbm = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
                if( hbm )
                {
                    BITMAP bm;
                    GetObject( hbm, sizeof(bm), &bm);
                    CRect rt( 0, 0, bm.bmWidth, iconInfo.hbmColor?bm.bmHeight:bm.bmHeight/2 );
    
                    HDC hdc = GetDC(NULL);
                    HDC hdcMem = CreateCompatibleDC(hdc);
                    hBitmap = CreateCompatibleBitmap( hdc, rt.Width(), rt.Height() );

                    HBITMAP hOldBmp = (HBITMAP)SelectObject( hdcMem, hBitmap );
                    FillRect( hdcMem, &rt, hbrBack );
                    DrawIconEx( hdcMem, 0, 0, shfi.hIcon, rt.Width(), rt.Height(), 0, NULL, DI_NORMAL );
                    SelectObject( hdcMem, hOldBmp );
    
                    DeleteDC(hdcMem);
                    ReleaseDC(NULL, hdc);
                }
                if( iconInfo.hbmMask )
                    DeleteObject( iconInfo.hbmMask );
                if( iconInfo.hbmColor )
                    DeleteObject( iconInfo.hbmColor );
            }

            DestroyIcon( shfi.hIcon );
        }

        return hBitmap;
    }
};
