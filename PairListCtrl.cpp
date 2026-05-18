// MyListCtrl.cpp
#include "pch.h"
#include "PairListCtrl.h"
#include "DupImgFinder2Dlg.h"
#include <filesystem>
#include <format>
#include <source_location>
#include <set>
#include <memory>
#include "misc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CPairListCtrl, CListCtrl)

CPairListCtrl::CPairListCtrl( CPairImgCtrl& m_PairImg, bool OnlyFileName ) : m_OnlyFileName(OnlyFileName), m_PairImg(m_PairImg)
{
}

CPairListCtrl::~CPairListCtrl()
{
}

BEGIN_MESSAGE_MAP(CPairListCtrl, CListCtrl)
    ON_WM_SIZE()
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, &CPairListCtrl::OnGetdispinfo)
    ON_NOTIFY_REFLECT( LVN_ODFINDITEM, &CPairListCtrl::OnLvnOdfinditem )
    ON_NOTIFY_REFLECT( LVN_COLUMNCLICK, &CPairListCtrl::OnLvnColumnclick )
    ON_NOTIFY( HDN_DIVIDERDBLCLICKA, 0, &CPairListCtrl::OnHdnDividerdblclick )
    ON_NOTIFY( HDN_DIVIDERDBLCLICKW, 0, &CPairListCtrl::OnHdnDividerdblclick )
    ON_NOTIFY_REFLECT( NM_RCLICK, &CPairListCtrl::OnNMRClick )
    ON_NOTIFY_REFLECT( LVN_ITEMCHANGED, &CPairListCtrl::OnLvnItemchanged )
END_MESSAGE_MAP()

void CPairListCtrl::PreSubclassWindow()
{
    ASSERT( (GetStyle() & LVS_OWNERDATA) != 0 ); // LVS_OWNERDATA必须提前设

    {
        CRect rect;
        GetWindowRect( &rect );
        min_window_cx = rect.Width();
        last_window_cx = min_window_cx;
        GetClientRect( &rect );
        min_client_cx = rect.Width();
    }

    // 在子类化之前设置自绘样式
    if( m_OnlyFileName )
        ModifyStyle(0, LVS_REPORT&~LVS_OWNERDRAWFIXED);
    else
        ModifyStyle(0, LVS_REPORT|LVS_OWNERDRAWFIXED);
    SetExtendedStyle( GetExtendedStyle() | LVS_EX_LABELTIP );
    SetExtendedStyle( GetExtendedStyle() | LVS_EX_AUTOSIZECOLUMNS );
    SetExtendedStyle( GetExtendedStyle() | LVS_EX_FULLROWSELECT );
    CListCtrl::PreSubclassWindow();

    InsertColumn( 0, _T("原始项"), LVCFMT_LEFT );
    InsertColumn( 1, _T("相似度"), LVCFMT_RIGHT );
    InsertColumn( 2, _T("重复项"), LVCFMT_LEFT );
    
    SetColumnWidth( 1, LVSCW_AUTOSIZE_USEHEADER );
    int w1 = GetColumnWidth(1)+1;
    int w0 = max( min_header_width, (min_client_cx-w1)/2 );
    int w2 = max( min_header_width, (min_client_cx-w1-w0) );
    SetColumnWidth( 0, w0 );
    SetColumnWidth( 1, w1 );
    SetColumnWidth( 2, w2 );
    

    //const int columnCount = GetHeaderCtrl()->GetItemCount();
    //for( int i=0; i<columnCount; ++i )
    //{
    //    // 根据内容调整列宽
    //    SetColumnWidth( i, LVSCW_AUTOSIZE );
    //    int content_width = GetColumnWidth(i);

    //    // 根据标题调整列宽
    //    SetColumnWidth( i, LVSCW_AUTOSIZE_USEHEADER );
    //    int header_width = GetColumnWidth(i);

    //    // 设置为较大值
    //    SetColumnWidth( i, max(content_width,header_width) );
    //}

    SetItemCountEx( (int)m_pair.size() );
}

void CPairListCtrl::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct ) // 仅当 m_OnlyFileName==true 才进入
{
    //TRACE( "CPairListCtrl::DrawItem {%u}\n", lpDrawItemStruct->itemID );

    const int row = lpDrawItemStruct->itemID;
    if( row == -1 )
        return;

    std::wstring common, lhs_pr, rhs_pr;
    {
        std::wstring lhs = m_raw[std::get<0>(m_pair[row])].first;
        if( !lhs.empty() && lhs.back()!=L'\\' && lhs.back()!=L'/' )
            lhs += std::filesystem::path::preferred_separator;
        std::wstring rhs = m_raw[std::get<3>(m_pair[row])].first;
        if( !rhs.empty() && rhs.back()!=L'\\' && rhs.back()!=L'/' )
            rhs += std::filesystem::path::preferred_separator;
        const size_t len = (std::min)( lhs.size(), rhs.size() );
        size_t sp = 0;
        for( size_t i=0; i!=len && lhs[i]==rhs[i]; ++i )
            if( lhs[i]==L'\\' || lhs[i]==L'/' )
                sp = i+1;
        common = lhs.substr( 0, sp );
        lhs_pr = lhs.substr( sp );
        rhs_pr = rhs.substr( sp );
    }

    CRect rect = lpDrawItemStruct->rcItem;
    COLORREF bkColor = (lpDrawItemStruct->itemState & ODS_SELECTED) ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW);
    COLORREF textColor = (lpDrawItemStruct->itemState & ODS_SELECTED) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT);

    CDC dc;
    dc.Attach( lpDrawItemStruct->hDC );
    dc.FillSolidRect( &rect, bkColor );

    auto draw_single_item = [&dc,textColor]( CRect textRect, std::wstring_view s1, std::wstring_view s2, std::wstring_view s3 )
        {
            textRect.DeflateRect(2, 0);
            int nSavedDC = dc.SaveDC();
            dc.IntersectClipRect( &textRect );
            dc.SetBkMode(TRANSPARENT);

            if( textRect.Width()>0 && !s1.empty() )
            {
                dc.SetTextColor( RGB(0,0,255) );
                dc.DrawText( s1.data(), (int)s1.size(), textRect, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
                textRect.left += dc.GetTextExtent( s1.data(), (int)s1.size() ).cx;
            }
            if( textRect.Width()>0 && !s2.empty() )
            {
                dc.SetTextColor( RGB(255,0,0) );
                dc.DrawText( s2.data(), (int)s2.size(), textRect, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
                textRect.left += dc.GetTextExtent( s2.data(), (int)s2.size() ).cx;
            }
            if( textRect.Width()>0 && !s3.empty() )
            {
                dc.SetTextColor(textColor);
                dc.DrawText( s3.data(), (int)s3.size(), textRect, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
                textRect.left += dc.GetTextExtent( s3.data(), (int)s3.size() ).cx;
            }

            dc.RestoreDC(nSavedDC);
        };


    CRect rect_lhs( rect.left, rect.top, rect.left + GetColumnWidth(0), rect.bottom );
    draw_single_item( rect_lhs, common, lhs_pr, GetItem_lhs(row,true) );

    CRect rect_mhs( rect_lhs.right, rect.top, rect_lhs.right + GetColumnWidth(1), rect.bottom );
    draw_single_item( rect_mhs, {}, {}, GetItem_mhs(row) );

    CRect rect_rhs( rect_mhs.right, rect.top, rect_mhs.right + GetColumnWidth(2), rect.bottom );
    draw_single_item( rect_rhs, common, rhs_pr, GetItem_rhs(row,true) );

    // 绘制焦点矩形
    if( lpDrawItemStruct->itemState & ODS_FOCUS )
    {
        dc.DrawFocusRect(&rect);
    }

    dc.Detach();
}

void CPairListCtrl::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    LVITEM* pItem = &pDispInfo->item;

    if( pItem->mask & LVIF_TEXT )
    {
        int nItem = pItem->iItem;
        int nSubItem = pItem->iSubItem;
        std::wstring value = GetItem(nItem,nSubItem,m_OnlyFileName);
//{
//if( nItem < m_pair.size() )
//{
//    auto [a,b,c,d,e,f] = m_pair[nItem];
//    if( nSubItem == 0 )
//        value = std::format(L"[{},{}]", a, b) + value;
//    else if( nSubItem == 2 )
//        value = std::format(L"[{},{}]", d, e) + value;
//}
//}
        lstrcpynW( pItem->pszText, value.c_str(), pItem->cchTextMax );
    }

    *pResult = 0;
}

void CPairListCtrl::OnLvnOdfinditem( NMHDR* pNMHDR, LRESULT* pResult )
{
    [[maybe_unused]] LPNMLVFINDITEM pFindInfo = reinterpret_cast<LPNMLVFINDITEM>(pNMHDR);
    
    // 返回 -1，禁止掉这个SB功能
    *pResult = -1;
}

void CPairListCtrl::OnLvnItemchanged( NMHDR* pNMHDR, LRESULT* pResult )
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    //TRACE( "%s\n", __func__ );

    //if( (pNMLV->uOldState&LVIS_SELECTED)!=0 || (pNMLV->uNewState&LVIS_SELECTED)!=0 )
    //{
    //    TRACE( L"iItem=%s, UOldState->uNewState=%d->%d, uChanged=%d\n"
    //        , pNMLV->iItem==-1 ? L"所有项" : std::format(L"{}",pNMLV->iItem).c_str()
    //        , pNMLV->uOldState
    //        , pNMLV->uNewState
    //        , pNMLV->uChanged );
    //}

    if( m_ModifyStateSafeCount != 0 )
    {
        *pResult = 0;
        return;
    }

    bool 需要检查 = false;
    if( (pNMLV->uChanged&LVIF_STATE)!=0 && pNMLV->iItem==-1 && (pNMLV->uOldState&LVIS_SELECTED)!=0 && (pNMLV->uNewState&LVIS_SELECTED)==0 ) // 所有项取消了选择
    {
        m_PairImg.SetImagePath( {}, {} );
    }
    else if( (pNMLV->uChanged&LVIF_STATE)!=0 && pNMLV->iItem!=-1 && (pNMLV->uOldState&LVIS_SELECTED)!=0 && (pNMLV->uNewState&LVIS_SELECTED)==0 ) // 某项取消了选择
    {
        需要检查 = true;
    }
    else if( (pNMLV->uChanged&LVIF_STATE)!=0 && pNMLV->iItem!=-1 && (pNMLV->uOldState&LVIS_SELECTED)==0 && (pNMLV->uNewState&LVIS_SELECTED)!=0 ) // 某项增加了选择
    {
        需要检查 = true;
    }

    if( 需要检查 )
    {
        if( GetSelectedCount() == 1 )
        {
            POSITION pos = GetFirstSelectedItemPosition();
            int nItem = GetNextSelectedItem(pos);
            bool r = m_PairImg.SetImagePath( GetItem_lhs(nItem,false), GetItem_rhs(nItem,false) );
            if( !r )
                FileInvalidated(nItem);
        }
        else
        {
            m_PairImg.SetImagePath( {}, {} );
        }
    }

    *pResult = 0;
}

void CPairListCtrl::OnSize( UINT nType, int cx, int cy ) // 先放大后缩小有问题，因为滚动条
{
    CListCtrl::OnSize( nType, cx, cy );
    return; // 太难了，如果要继续，请去掉上面的 SetExtendedStyle( GetExtendedStyle() | LVS_EX_AUTOSIZECOLUMNS );
}

void CPairListCtrl::OnLvnColumnclick( NMHDR* pNMHDR, LRESULT* pResult ) // 排序
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    SetRedraw( FALSE );

    SaveState();
    {
        CHeaderCtrl* pHdrCtrl = GetHeaderCtrl();
        int nClickedCol = pNMLV->iSubItem;
        bool ascending = true;

        HDITEM hdi = { HDI_FORMAT };
        pHdrCtrl->GetItem( nClickedCol, &hdi );
        if( (hdi.fmt & HDF_SORTUP) != 0 )
            ascending = false;

        for( int i=0; i<pHdrCtrl->GetItemCount(); ++i )
        {
            pHdrCtrl->GetItem( i, &hdi );
            hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if( i == nClickedCol )
            {
                if( ascending )
                    hdi.fmt |= HDF_SORTUP;
                else
                    hdi.fmt |= HDF_SORTDOWN;
            }
            pHdrCtrl->SetItem(i, &hdi);
        }

        auto cmp_lhs_asc = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::pair(std::get<0>(as),std::get<1>(as)) < std::pair(std::get<0>(bs),std::get<1>(bs));
            };
        auto cmp_lhs_des = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::pair(std::get<0>(as),std::get<1>(as)) > std::pair(std::get<0>(bs),std::get<1>(bs));
            };
        auto cmp_rhs_asc = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::pair(std::get<3>(as),std::get<4>(as)) < std::pair(std::get<3>(bs),std::get<4>(bs));
            };
        auto cmp_rhs_des = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::pair(std::get<3>(as),std::get<4>(as)) > std::pair(std::get<3>(bs),std::get<4>(bs));
            };
        auto cmp_mhs_asc = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::get<2>(as) < std::get<2>(bs);
            };
        auto cmp_mhs_des = []( const decltype(m_pair)::value_type& as, const decltype(m_pair)::value_type& bs )
            {
                return std::get<2>(as) > std::get<2>(bs);
            };

        if( ascending )
        {
            if( nClickedCol == 0 )
                std::ranges::stable_sort( m_pair, cmp_lhs_asc );
            else if( nClickedCol == 1 )
                std::ranges::stable_sort( m_pair, cmp_mhs_asc );
            else if( nClickedCol == 2 )
                std::ranges::stable_sort( m_pair, cmp_rhs_asc );
        }
        else
        {
            if( nClickedCol == 0 )
                std::ranges::stable_sort( m_pair, cmp_lhs_des );
            else if( nClickedCol == 1 )
                std::ranges::stable_sort( m_pair, cmp_mhs_des );
            else if( nClickedCol == 2 )
                std::ranges::stable_sort( m_pair, cmp_rhs_des );
        }
    }
    RestorState();

    SetRedraw( TRUE );
    Invalidate();
    UpdateWindow();
    *pResult = 0;
}

void CPairListCtrl::OnHdnDividerdblclick( NMHDR* pNMHDR, LRESULT* pResult ) // 自动宽度
{
    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    int nCol = phdr->iItem;

    // 让系统自动计算内容所需宽度
    SetColumnWidth(nCol, LVSCW_AUTOSIZE);
    int nContentWidth = GetColumnWidth(nCol);
    
    // 与表头宽度比较，取较大值
    SetColumnWidth(nCol, LVSCW_AUTOSIZE_USEHEADER);
    int nHeaderWidth = GetColumnWidth(nCol);
    
    int nNewWidth = max(nContentWidth, nHeaderWidth);
    SetColumnWidth(nCol, nNewWidth);
    
    *pResult = TRUE;  // 阻止默认行为
}

void CPairListCtrl::ClearSortedState()
{
    CHeaderCtrl* pHdrCtrl = GetHeaderCtrl();
    for( int i=0; i<pHdrCtrl->GetItemCount(); ++i )
    {
        HDITEM hdi = { HDI_FORMAT };
        pHdrCtrl->GetItem( i, &hdi );
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        pHdrCtrl->SetItem( i, &hdi );
    }
}

void CPairListCtrl::ClearPairData()
{
    ClearSortedState();

    m_PairImg.SetImagePath( {}, {} );

    m_pair.clear();
    SetItemCountEx( (int)m_pair.size() );
}

void CPairListCtrl::SetData( decltype(m_raw)&& raw, decltype(m_pair)&& pair )
{
    ClearPairData();

    m_raw = std::move(raw);
    m_pair = std::move(pair);
    SetItemCountEx( (int)m_pair.size() );
    Invalidate();
    UpdateWindow();
}

void CPairListCtrl::OnNMRClick( NMHDR* pNMHDR, LRESULT* pResult ) // 右键单击
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

    int nRow = pNMItemActivate->iItem;
    int nCol = pNMItemActivate->iSubItem;
    if( nRow!=-1 && (nCol==0 || nCol==2) )
    {
        // 右键点击时，OnLvnItemchanged先被调用，OnLvnItemchanged中可能删除m_pair部分内容
        if( nRow >= m_pair.size() )
        {
            *pResult = 0;
            return;
        }

        CRect rect;
        GetItemRect( 0, &rect, LVIR_BOUNDS );
        if( pNMItemActivate->ptAction.x < rect.right ) // 否则，右边超出处，也被认为是0列
        {
            POINT pt = pNMItemActivate->ptAction;
            ClientToScreen( &pt );

            std::wstring fullpath = GetItem(nRow,nCol,false);

            // 当 m_OnlyFileName 时允许 在文件管理器中 打开一侧选择的所有文件，因为其父目录相同
            std::wstring folder_;
            std::vector<std::wstring> files_;
            if( m_OnlyFileName )
            {
                auto [a,b,c,d,e,f] = m_pair[nRow];
                if( nCol == 0 )
                    folder_ = a!=size_t(-1) ? m_raw[a].first : m_raw[1-d].first;
                else
                    folder_ = d!=size_t(-1) ? m_raw[d].first : m_raw[1-a].first;

                for( POSITION pos=GetFirstSelectedItemPosition(); pos; )
                {
                    int row = GetNextSelectedItem(pos);
                    size_t folder_idx = nCol==0 ? std::get<0>(m_pair[row]) : std::get<3>(m_pair[row]);
                    size_t file_idx = nCol==0 ? std::get<1>(m_pair[row]) : std::get<4>(m_pair[row]);
                    if( folder_idx != size_t(-1) )
                        files_.push_back( m_raw[folder_idx].second[file_idx].first );
                }
            }

            CMenu menu;
            menu.CreatePopupMenu();
            HBRUSH hbrBack = nullptr;
            {
                MENUINFO menuInfo = {0};
                menuInfo.cbSize = sizeof(MENUINFO);
                menuInfo.fMask = MIM_BACKGROUND;  // 指定要获取背景信息
                if( menu.GetMenuInfo(&menuInfo) )
                    hbrBack = menuInfo.hbrBack;
            }

            CBitmap bitmap2;
            CBitmap bitmap3;
            if( !fullpath.empty() )
            {
                menu.InsertMenu( -1, MF_BYPOSITION, 1, L"浏览图片(&W)" );
                menu.InsertMenu( -1, MF_BYPOSITION, 2, L"使用默认程序打开图片(&O)" );

                HBITMAP hBitmap2 = misc::GetFileTypeIcon_then_to_HBITMAP( fullpath, FILE_ATTRIBUTE_NORMAL, hbrBack );
                if( hBitmap2 )
                {
                    bitmap2.Attach( hBitmap2 );
                    menu.SetMenuItemBitmaps( 2, MF_BYCOMMAND, &bitmap2, &bitmap2 );
                }
            }
            if( !fullpath.empty() || !folder_.empty() )
            {
                if( !folder_.empty() )
                    menu.InsertMenu( -1, MF_BYPOSITION, 3, std::format(L"在文件管理器中打开(&X), 选择了{}个文件",files_.size()).c_str() );
                else
                    menu.InsertMenu( -1, MF_BYPOSITION, 3, L"在文件管理器中打开(&X)" );

                HBITMAP hBitmap3 = misc::GetFileTypeIcon_then_to_HBITMAP( std::filesystem::path(fullpath).parent_path().wstring(), FILE_ATTRIBUTE_DIRECTORY, hbrBack );
                if( hBitmap3 )
                {
                    bitmap3.Attach( hBitmap3 );
                    menu.SetMenuItemBitmaps( 3, MF_BYCOMMAND, &bitmap3, &bitmap3 );
                }
            }
            if( !m_OnlyFileName
                && std::get<0>(m_pair[nRow]) != std::get<3>(m_pair[nRow])
                && std::get<0>(m_pair[nRow]) != size_t(-1)
                && std::get<3>(m_pair[nRow]) != size_t(-1) )
            {
                menu.AppendMenu(MF_SEPARATOR);
                menu.InsertMenu( -1, MF_BYPOSITION, 4, L"进行两目录比对..." );
            }
            if( menu.GetMenuItemCount() != 0 )
            {
                BOOL sid = menu.TrackPopupMenu( TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD, pt.x, pt.y, this );
                if( sid == 1 )
                {
                    if( m_PairImg.SetImagePath( GetItem_lhs(nRow,false), GetItem_rhs(nRow,false) ) )
                    {
                        m_PairImg.ShowImagesFullScreen( nCol==0 );
                    }
                    else
                        FileInvalidated(nRow);
                }
                else if( sid == 2 )
                    ShellExecuteW( nullptr, L"OPEN", fullpath.c_str(), nullptr, nullptr, SW_SHOWMAXIMIZED );
                else if( sid == 3 )
                {
                    if( !m_OnlyFileName )
                        misc::explore_file_in_explorer( fullpath );
                    else
                        misc::explore_files_in_explorer( folder_, files_ );
                }
                else if( sid == 4 )
                {
                    auto a = m_raw[std::get<0>(m_pair[nRow])];
                    auto b = m_raw[std::get<3>(m_pair[nRow])];
                    CDupImgFinder2Dlg dlg( this, a, b );
                    dlg.DoModal();
                    PathMaybeValidated_( a.first );
                    PathMaybeValidated_( b.first );
                    Invalidate();
                    UpdateWindow();
                }
            }
        }
    }

    *pResult = 0;
}

void CPairListCtrl::SaveState()
{
    ASSERT( m_ModifyStateSafeCount == 0 );
    ++m_ModifyStateSafeCount;

    for( size_t nItem=0; nItem!=m_pair.size(); ++nItem )
        std::get<5>(m_pair[nItem]) = 0;
    for( int nItem=-1; nItem=GetNextItem(nItem,LVNI_SELECTED), nItem!=-1; )
        std::get<5>(m_pair[nItem]) = LVIS_SELECTED;
    for( int nItem=-1; nItem=GetNextItem(nItem,LVNI_FOCUSED), nItem!=-1; )
        std::get<5>(m_pair[nItem]) |= LVIS_FOCUSED;
    size_t iTop = std::ranges::clamp( (size_t)GetTopIndex(), 0zu, m_pair.size() );
    size_t iBottom = std::ranges::clamp( (size_t)(GetTopIndex()+GetCountPerPage()), 0zu, m_pair.size() );
    for( size_t nItem=iTop; nItem!=iBottom; ++nItem )
        std::get<5>(m_pair[nItem]) |= LVIS_CUT;

    SetItemState( -1, 0, LVIS_FOCUSED|LVIS_SELECTED );
    m_PairImg.SetImagePath( {}, {} );
}

void CPairListCtrl::RestorState()
{
    ASSERT( m_ModifyStateSafeCount == 1 );

    SetItemCount( (int)m_pair.size() );

    size_t iTop = m_pair.size();
    size_t iBottom = 0;
    for( size_t nItem=0; nItem!=m_pair.size(); ++nItem )
    {
        auto& state = std::get<5>(m_pair[nItem]);
        auto state_sf = state & (LVNI_SELECTED|LVNI_FOCUSED);
        if( state_sf != 0 )
            SetItemState( (int)nItem, state_sf, LVIS_FOCUSED|LVIS_SELECTED ); // 可能引发 OnLvnItemchanged，最终导致 m_raw/m_pair 被修改
        if( (state & LVIS_CUT) != 0 )
        {
            iTop = (std::min)( iTop, nItem );
            iBottom = (std::max)( iBottom, nItem );
        }
        state = 0;
    }
    if( iBottom < m_pair.size() )
        EnsureVisible( (int)iBottom, FALSE );
    if( iTop < m_pair.size() )
        EnsureVisible( (int)iTop, FALSE );

    if( GetSelectedCount() == 1 )
    {
        POSITION pos = GetFirstSelectedItemPosition();
        int row = GetNextSelectedItem(pos);
        m_PairImg.SetImagePath( GetItem_lhs(row,false), GetItem_rhs(row,false) );
    }

    --m_ModifyStateSafeCount;
}

void CPairListCtrl::Swap_AllPair()
{
    ASSERT( m_OnlyFileName == true );

    SetRedraw( FALSE );

    {
        CHeaderCtrl* pHdrCtrl = GetHeaderCtrl();
        HDITEM hdi = { HDI_FORMAT };
        pHdrCtrl->GetItem( 0, &hdi );
        int fmt0 = hdi.fmt; // & (HDF_SORTUP | HDF_SORTDOWN);
        pHdrCtrl->GetItem( 2, &hdi );
        int fmt2 = hdi.fmt; // & (HDF_SORTUP | HDF_SORTDOWN);
        int new_fmt0 = (fmt0 & ~(HDF_SORTUP|HDF_SORTDOWN)) | (fmt2 & (HDF_SORTUP|HDF_SORTDOWN));
        int new_fmt2 = (fmt2 & ~(HDF_SORTUP|HDF_SORTDOWN)) | (fmt0 & (HDF_SORTUP|HDF_SORTDOWN));

        if( new_fmt0 != fmt0 )
        {
            hdi.fmt = new_fmt0;
            pHdrCtrl->SetItem( 0, &hdi );
        }
        if( new_fmt2 != fmt2 )
        {
            hdi.fmt = new_fmt2;
            pHdrCtrl->SetItem( 2, &hdi );
        }
    }

    int aw = GetColumnWidth(0);
    int cw = GetColumnWidth(2);
    std::swap( aw, cw );
    SetColumnWidth(0, aw);
    SetColumnWidth(2, cw);

    for( auto& v : m_pair )
    {
        std::swap( std::get<0>(v), std::get<3>(v) );
        std::swap( std::get<1>(v), std::get<4>(v) );
    }

    if( GetSelectedCount() == 1 )
    {
        POSITION pos = GetFirstSelectedItemPosition();
        int row = GetNextSelectedItem(pos);
        m_PairImg.SetImagePath( GetItem_lhs(row,false), GetItem_rhs(row,false) );
    }
    
    SetRedraw( TRUE );
    Invalidate();
    UpdateWindow();
}

void CPairListCtrl::Swap_Selected_Item()
{
    ASSERT( m_OnlyFileName == false );
    SetRedraw( FALSE );

    size_t bChanged_count = 0;
    int bUnique = -1;
    for( POSITION pos=GetFirstSelectedItemPosition(); pos; )
    {
        int row = GetNextSelectedItem(pos);
        std::swap( std::get<0>(m_pair[row]), std::get<3>(m_pair[row]) );
        std::swap( std::get<1>(m_pair[row]), std::get<4>(m_pair[row]) );
        ++bChanged_count;

        bUnique = row;
    }
    if( bChanged_count != 0 )
    {
        ClearSortedState();
    }

    if( bChanged_count == 1 )
        m_PairImg.SetImagePath( GetItem_lhs(bUnique,false), GetItem_rhs(bUnique,false) );

    SetRedraw( TRUE );
    if( bChanged_count != 0 )
    {
        Invalidate();
        UpdateWindow();
    }
}

void CPairListCtrl::Remove_Selected_Item()
{
    ASSERT( m_OnlyFileName == false );
    if( GetSelectedCount() == 0 ) return;
    SetRedraw( FALSE );

    SaveState();
    for( size_t i=m_pair.size(); i!=0; --i )
    {
        if( (std::get<5>(m_pair[i-1]) & LVIS_SELECTED) != 0 )
            m_pair.erase( m_pair.begin()+i-1 );
    }
    RestorState();

    SetRedraw( TRUE );
    Invalidate();
    UpdateWindow();
}

void CPairListCtrl::Delete_Selected_Item_RHS()
{
    if( GetSelectedCount() == 0 ) return;

    {
        SetRedraw( FALSE );
        SaveState();
        // 缺少 std::scope_exit 的苦恼
        auto guard = std::unique_ptr<CPairListCtrl,void(*)(CPairListCtrl*)>( this, [](CPairListCtrl* the){
                the->RestorState();
                the->SetRedraw( TRUE );
            } );

        std::set< std::pair<size_t,size_t> > pending;
        for( size_t i=0; i!=m_pair.size(); ++i )
        {
            auto [a,b,c,d,e,f] = m_pair[i];
            if( (f&LVIS_SELECTED) == 0 ) continue;

            if( d!=size_t(-1) && !pending.contains({a,b}) && !pending.contains({d,e}) )
			    pending.insert( {d,e} );
        }
        if( pending.size() == 0 )
            return;
        if( pending.size() == 1 )
        {
            auto s = std::format( L"删除文件: {}", GetItem_(*pending.begin(),false) );
            if( IDOK != MessageBoxW( s.c_str(), NULL, MB_OKCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2 ) )
                return;
        }
        else
        {
            auto s = std::format( L"删除这 {} 个文件", pending.size() );
            if( IDOK != MessageBoxW( s.c_str(), NULL, MB_OKCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2 ) )
                return;
        }

        // 返回已删除的文件
        std::set<std::pair<size_t,size_t>> foo = IFileOperation_Delete_Files_( pending );
        for( size_t i=m_pair.size(); i!=0; --i )
        {
            size_t j = i-1;
            auto& [a,b,c,d,e,f] = m_pair[j];
            if( m_OnlyFileName == false )
            {
                if( foo.contains({a,b}) || foo.contains({d,e}) )
                {
                    m_pair.erase( m_pair.begin()+j );
                }
            }
            else if( m_OnlyFileName == true )
            {
                if( foo.contains({d,e}) )
                {
                    if( a == size_t(-1) )
                        m_pair.erase( m_pair.begin()+j );
                    else
                        c=INT_MAX, d=size_t(-1), e=0;
                }
            }
        }
    }

    Invalidate();
    UpdateWindow();
}

std::set<std::pair<size_t,size_t>> CPairListCtrl::IFileOperation_Delete_Files_( const std::set< std::pair<size_t,size_t> >& pending )
{
    std::vector<std::wstring> files;
    files.reserve( pending.size() );
    for( const auto& v : pending )
    {
        std::wstring fn = GetItem_(v,false);
        if( !misc::set_file_attribute_normal(fn) )
        {
            MessageBox( std::format(L"无法去除文件只读属性 - {}",fn).c_str(), nullptr, MB_OK|MB_ICONWARNING );
            return {};
        }
        files.push_back( std::move(fn) );
    }

    misc::delete_files_by_IFileOperation( files );

    std::set<std::pair<size_t,size_t>> result;
    for( const auto& v : pending )
    {
        std::wstring fn = L"\\\\?\\" + GetItem_(v,false);
        DWORD attrib = GetFileAttributesW( fn.c_str() );
        if( attrib == INVALID_FILE_ATTRIBUTES )
            result.insert( v );
    }

    for( auto [a,b] : result )
    {
        std::wstring& filename = m_raw[a].second[b].first;
        filename.clear();
    }

    return result;
}

void CPairListCtrl::PathMaybeValidated_( const std::filesystem::path& path )
{
    //struct _DEBUG_SOME_
    //{
    //    const std::filesystem::path& path;
    //    const char* fn;
    //    _DEBUG_SOME_( const std::filesystem::path& path, const char* fn ) : path(path), fn(fn)
    //    { TRACE( "+%s( %s )\n", fn, path.string().c_str() ); }
    //    ~_DEBUG_SOME_()
    //    { TRACE( "~%s( %s )\n", fn, path.string().c_str() ); }
    //} _DEBUG_SOME_( path, __func__ );

    if( path.empty() )
        return;

    auto GetFirstSubPath = []( const std::filesystem::path& ancestor, const std::filesystem::path& descendant )
        {
            std::filesystem::path relative = descendant.lexically_relative(ancestor);
            if( relative.empty() || relative.u8string()==u8"." || relative.u8string().starts_with(u8"..") )
                return std::filesystem::path{};
            return ancestor / *relative.begin();
        };
    
    auto GetExistsAncestor = []( const std::filesystem::path& path )
        {
            std::filesystem::path p = path;
            while( !std::filesystem::exists(p) )
            {
                if( !p.has_parent_path() )
                    break;
                auto t = p.parent_path();
                if( t == p )
                    break;
                p = std::move(t);
            }
            return p;
        };

    std::set<size_t> 待移除的目录s;
    std::set<std::pair<size_t,size_t>> 待移除的文件s;
    {
        auto a = GetExistsAncestor(path);

        for( size_t i=0; i!=m_raw.size(); ++i )
        {
            std::filesystem::path folder = m_raw[i].first;
            auto& files = m_raw[i].second;
            if( !files.empty() )
            {
                if( a == folder ) // 检查所有的files
                {
                    for( size_t j=0; j!=files.size(); ++j )
                    {
                        if( !std::filesystem::exists(folder / files[j].first) )
                            待移除的文件s.insert( {i,j} );
                    }
                }
            }
            if( auto sub=GetFirstSubPath(a,folder); !sub.empty() )
            {
                if( !std::filesystem::exists(sub) )
                    待移除的目录s.insert( i );
            }
        }
    }

    if( 待移除的目录s.empty() && 待移除的文件s.empty() )
        return;

    for( auto [v,w] : 待移除的文件s )
    {
        m_raw[v].second[w].first.clear();
    }
    for( auto v : 待移除的目录s )
    {
        m_raw[v].first.clear();
        m_raw[v].second.clear();
    }

    SaveState();
    for( size_t i=m_pair.size(); i!=0; --i )
    {
        size_t j = i-1;
        auto& [a,b,c,d,e,f] = m_pair[j];
        if( m_OnlyFileName == false )
        {
            if( 待移除的目录s.contains(a) || 待移除的目录s.contains({d})
                || 待移除的文件s.contains({a,b}) || 待移除的文件s.contains({d,e}) )
            {
                m_pair.erase( m_pair.begin()+j );
            }
        }
        else if( m_OnlyFileName == true )
        {
            if( 待移除的目录s.contains(a) || 待移除的文件s.contains({a,b}) )
                a=size_t(-1), c=INT_MAX;
            if( 待移除的目录s.contains(d) || 待移除的文件s.contains({d,e}) )
                d=size_t(-1), c=INT_MAX;
            if( a==size_t(-1) && d==size_t(-1) )
                m_pair.erase( m_pair.begin()+j );
        }
    }
    RestorState();
}

void CPairListCtrl::FileInvalidated( int nRow )
{
    SetRedraw( FALSE );

    if( std::filesystem::path lhs=GetItem_lhs(nRow,false); !lhs.empty() && !std::filesystem::exists(lhs) )
    {
        PathMaybeValidated_( m_raw[std::get<0>(m_pair[nRow])].first );
    }
    if( std::filesystem::path rhs=GetItem_rhs(nRow,false); !rhs.empty() && !std::filesystem::exists(rhs) )
    {
        PathMaybeValidated_( m_raw[std::get<3>(m_pair[nRow])].first );
    }

    SetRedraw( TRUE );
    Invalidate();
    UpdateWindow();
}

