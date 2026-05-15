#pragma once
#include <string>
#include <filesystem>

namespace misc
{
    std::wstring get_process_path();

    bool set_file_attribute_normal( const std::wstring& file );

    bool set_files_attribute_normal( const std::vector<std::wstring>& files );

    bool delete_files_by_IFileOperation( const std::vector<std::wstring>& files );

    CString ShowFolderBrowserDialog( HWND hwndOwner, LPCWSTR title=nullptr, LPCWSTR initial_path=nullptr );

    std::wstring lexically_normal_tolower( const std::filesystem::path& path );

    bool is_path_containing( const wchar_t* a, const wchar_t* b );

    bool explore_file_in_explorer( const std::wstring& fullpath );

    bool explore_files_in_explorer( const std::wstring& folder, const std::vector<std::wstring>& files );

    HBITMAP GetFileTypeIcon_then_to_HBITMAP( const std::wstring& path, DWORD dwFileAttributes, HBRUSH hbrBack );
};

