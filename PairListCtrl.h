// MyListCtrl.h
#pragma once
#include <tuple>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <algorithm>
#include <ranges>
#include <tuple>
#include <set>
#include "PairImgCtrl.h"

class CPairListCtrl : public CListCtrl
{
    DECLARE_DYNAMIC(CPairListCtrl)

    const bool m_OnlyFileName;
public:
    CPairListCtrl( CPairImgCtrl& m_PairImg, bool OnlyFileName );
    virtual ~CPairListCtrl();

    CPairImgCtrl& m_PairImg;

    virtual void PreSubclassWindow() override;
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) override;
    afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnOdfinditem( NMHDR* pNMHDR, LRESULT* pResult );
    afx_msg void OnLvnItemchanged( NMHDR* pNMHDR, LRESULT* pResult );
    afx_msg void OnSize( UINT nType, int cx, int cy );
    afx_msg void OnLvnColumnclick( NMHDR* pNMHDR, LRESULT* pResult );
    afx_msg void OnHdnDividerdblclick( NMHDR* pNMHDR, LRESULT* pResult );
    afx_msg void OnNMRClick( NMHDR* pNMHDR, LRESULT* pResult );
protected:
    DECLARE_MESSAGE_MAP()

protected:
    int min_window_cx, min_client_cx;
    int last_window_cx;
    const int min_header_width=10; int last_client_cx;

public:
    std::vector<std::wstring> m_folders;
    std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>> m_raw;
    std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>> m_pair;
    void ClearSortedState();
    void ClearPairData();
    void SetData( decltype(m_raw)&& raw, decltype(m_pair)&& pair );

    std::wstring GetItem_( std::pair<size_t,size_t> folder_file_idx, bool only_filename ) const
    {
        if( folder_file_idx.first == size_t(-1) ) return {};
        ASSERT( folder_file_idx.first < m_raw.size() );
        std::wstring folder = m_raw[folder_file_idx.first].first;
        ASSERT( folder_file_idx.second < m_raw[folder_file_idx.first].second.size() );
        std::wstring file = m_raw[folder_file_idx.first].second[folder_file_idx.second].first;
        if( only_filename )
            return file;
        if( !folder.empty() && folder.back()!=L'\\' && folder.back()!=L'/' )
            folder += std::filesystem::path::preferred_separator;
        return folder + file;
    }
    std::wstring GetItem( int row, int col, bool only_filename ) const
    {
        if( row >= m_pair.size() ) return {};

        if( col==0 || col==2 )
        {
            size_t a = col==0 ? std::get<0>(m_pair[row]) : std::get<3>(m_pair[row]);
            size_t b = col==0 ? std::get<1>(m_pair[row]) : std::get<4>(m_pair[row]);
            return GetItem_({a,b},only_filename);
        }
        else if( col == 1 )
        {
            int c = std::get<2>(m_pair[row]);
            if( c == (std::numeric_limits<decltype(c)>::max)() )
                return {};
            return std::format( L"{:>3}%", (c*100+32)/64 );
        }
        return {};
    }
    std::wstring GetItem_lhs( int row, bool only_filename ) const
    {
        return GetItem( row, 0, only_filename );
    }
    std::wstring GetItem_mhs( int row ) const
    {
        return GetItem( row, 1, false );
    }
    std::wstring GetItem_rhs( int row , bool only_filename) const
    {
        return GetItem( row, 2, only_filename );
    }

    int m_ModifyStateSafeCount = 0;
    void SaveState();
    void RestorState();

    void Swap_AllPair();
    void Swap_Selected_Item();

    void Remove_Selected_Item();

    void Delete_Selected_Item_RHS();
    std::set<std::pair<size_t,size_t>> IFileOperation_Delete_Files_( const std::set< std::pair<size_t,size_t> >& pending );

    void PathMaybeValidated_( const std::filesystem::path& path );
    void FileInvalidated( int nRow );
};