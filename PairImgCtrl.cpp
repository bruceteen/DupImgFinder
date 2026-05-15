// PairImgCtrl.cpp: 实现文件
//

#include "pch.h"
#include "DupImgFinder.h"
#include "PairImgCtrl.h"
#include <chrono>

#include "vips/vips.h"

// CPairImgCtrl

IMPLEMENT_DYNAMIC(CPairImgCtrl, CStatic)

CPairImgCtrl::CPairImgCtrl()
{
}

CPairImgCtrl::~CPairImgCtrl()
{
    m_lhs.reset();
    m_rhs.reset();
}

void CPairImgCtrl::PreSubclassWindow()
{
    ModifyStyle(0, SS_NOTIFY); // 否则没有鼠标信息
    ModifyStyle(0, SS_OWNERDRAW); // 设为自绘，否则偶尔会跳到 void CStatic::DrawItem(LPDRAWITEMSTRUCT) { ASSERT(FALSE); }

    CStatic::PreSubclassWindow();
}

BEGIN_MESSAGE_MAP(CPairImgCtrl, CStatic)
    //ON_WM_PAINT()
    ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

std::unique_ptr<Gdiplus::Bitmap> CPairImgCtrl::Load_Image( const std::filesystem::path& imgpath, std::vector<BYTE>& buffer, ImageInfo& info ) const
{
    info = ImageInfo{};
    vips_error_clear();

    VipsImage* image = vips_image_new_from_file( (const char*)imgpath.u8string().c_str(), "access", VIPS_ACCESS_SEQUENTIAL, nullptr );
    if( !image )
        return nullptr;

    // 获得图像格式
    {
        const char* fmt;
        if( vips_image_get_string(image,"vips-loader",&fmt) == 0 )
        {
            std::u8string_view f = (const char8_t*)fmt;
            if( f.ends_with(u8"load") )
                f.remove_suffix( 4 );
            info.imgfmt = std::filesystem::path(f).wstring();
        }
    }

    // 去掉alpha通道
    if( vips_image_hasalpha(image) )
    {
        VipsImage* temp = nullptr;
        if( vips_extract_band(image, &temp, 0, "n", image->Bands-1, nullptr) )
        {
            g_object_unref(image);
            return nullptr;
        }
        g_object_unref(image);
        image = temp;
    }

    // 转换到 sRGB 色彩空间（结果自动为 RGB24，即 3 uchar 或 4 uchar ）
    {
        VipsImage* temp = nullptr;
        if( vips_colourspace(image, &temp, VIPS_INTERPRETATION_sRGB, nullptr) )
        {
            g_object_unref(image);
            return nullptr;
        }
        g_object_unref(image);
        image = temp;
    }

    const int width = vips_image_get_width(image);
    const int height = vips_image_get_height(image);
    const int bands = vips_image_get_bands(image); // 3 or 4
    [[maybe_unused]] const VipsBandFormat fmt = vips_image_get_format(image); // VIPS_FORMAT_UCHAR
    guint64 bytes_per_pixel = VIPS_IMAGE_SIZEOF_ELEMENT(image) * bands;
    [[maybe_unused]] guint64 row_bytes = width * bytes_per_pixel;
    int stride_gdi = (width * bytes_per_pixel + 3) & ~3; // 等价于 (width * 3 + 3) / 4 * 4
    buffer.resize( stride_gdi * height );

    VipsRegion* region = vips_region_new( image );
    for( int y=0; y<height; ++y )
    {
        VipsRect rect = {0, y, width, 1};
        if( vips_region_prepare(region,&rect) )
        {
            g_object_unref(region);
            g_object_unref(image);
            return nullptr;
        }

        unsigned char* row_data = (unsigned char*)VIPS_REGION_ADDR( region, rect.left, rect.top );
        BYTE* ptr = buffer.data() + y*stride_gdi;
        //memcpy( buffer.data()+y*stride_gdi, row_data, row_bytes );
        for( int i=0; i!=width; ++i )
        {
            ptr[3*i+0] = row_data[3*i+2];
            ptr[3*i+1] = row_data[3*i+1];
            ptr[3*i+2] = row_data[3*i+0];
        }
    }
    g_object_unref(region);
    g_object_unref(image);

    auto pBitmap = std::make_unique<Gdiplus::Bitmap>( width, height, stride_gdi, PixelFormat24bppRGB, buffer.data() );
    if( !pBitmap || pBitmap->GetLastStatus()!=Gdiplus::Ok )
        return nullptr;

    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(imgpath,ec);
     
    //auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(ftime); // 看起来有内存泄漏
    //auto sys_time_seconds = std::chrono::round<std::chrono::seconds>(sys_time);
    //auto local_time = std::chrono::zoned_time{ std::chrono::current_zone(), sys_time_seconds };

    //auto utc_time = std::chrono::file_clock::to_utc(ftime);
    //auto local_tp = std::chrono::zoned_time{ std::chrono::current_zone(), utc_time }; 编译失败

    //auto utc_time = std::chrono::file_clock::to_utc(ftime);
    //using Converter = std::chrono::clock_time_conversion<std::chrono::system_clock, std::chrono::utc_clock>;
    //auto sys_now = Converter{}(utc_time); // 看起来有内存泄漏
    //std::chrono::zoned_time zt{std::chrono::current_zone(), sys_now};

    auto time_since_epoch = std::chrono::time_point_cast<std::chrono::seconds>(ftime).time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();
    std::time_t tt = static_cast<std::time_t>(seconds - 11644473600LL);
    std::tm tm_local = {};
    localtime_s( &tm_local, &tt );

    info.width = width;
    info.height = height;
    info.filesize = std::filesystem::file_size(imgpath,ec);
    info.lastwritetime = tm_local;
    info.lastwritetime_s = std::format( L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}"
                , 1900+tm_local.tm_year, 1+tm_local.tm_mon, tm_local.tm_mday
                , tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec );
    return pBitmap;
}

bool CPairImgCtrl::SetImagePath( const std::filesystem::path& lhs, const std::filesystem::path& rhs )
{
    if( lhs==m_lhs_info.filepath && rhs==m_rhs_info.filepath )
        return true;

    TRACE( L"SetImagePath( %s, %s )\n", lhs.wstring().c_str(), rhs.wstring().c_str() );

    if( lhs != m_lhs_info.filepath )
    {
        m_lhs.reset();
        m_lhs_info = {};
        if( !lhs.empty() )
        {
            std::unique_ptr<Gdiplus::Bitmap> lhs_img = Load_Image( lhs, m_lhs_buf, m_lhs_info );
            if( lhs_img )
            {
                m_lhs = std::move(lhs_img);
                m_lhs_info.filepath = lhs.wstring();
            }
        }
    }
    if( !lhs.empty() && m_lhs_info.filepath.empty() )
    {
        m_rhs.reset();
        m_rhs_info.filepath.clear();
        Invalidate(FALSE);
        return false;
    }

    if( rhs != m_rhs_info.filepath )
    {
        m_rhs.reset();
        m_rhs_info = {};
        if( !rhs.empty() )
        {
            std::unique_ptr<Gdiplus::Bitmap> rhs_img = Load_Image( rhs, m_rhs_buf, m_rhs_info );
            if( rhs_img )
            {
                m_rhs = std::move(rhs_img);
                m_rhs_info.filepath = rhs.wstring();
            }
        }
    }
    if( !rhs.empty() && m_rhs_info.filepath.empty() )
    {
        m_lhs.reset();
        m_lhs_info.filepath.clear();
        Invalidate(FALSE);
        return false;
    }

    Invalidate(FALSE);
    UpdateWindow();
    return true;
}

// CPairImgCtrl 消息处理程序

void CPairImgCtrl::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);
    CRect rect = lpDrawItemStruct->rcItem;
    dc.FillSolidRect( &rect, RGB(128,128,128) );
    if( !m_lhs && !m_rhs )
    {
        dc.Detach();
        return;
    }
    
    TEXTMETRIC tm{};
    dc.GetTextMetrics(&tm);

    if( rect.Width()%2 == 0 )
    {
        m_lhs_info.rect.SetRect( rect.left, rect.top, rect.left+rect.Width()/2-1, rect.bottom - 2*tm.tmHeight );
        m_rhs_info.rect.SetRect( rect.left+rect.Width()/2+1, rect.top, rect.right, rect.bottom - 2*tm.tmHeight );
    }
    else
    {
        m_lhs_info.rect.SetRect( rect.left, rect.top, rect.left+rect.Width()/2, rect.bottom - 2*tm.tmHeight );
        m_rhs_info.rect.SetRect( rect.left+rect.Width()/2+1, rect.top, rect.right, rect.bottom - 2*tm.tmHeight );
    }

    auto foo = []( Gdiplus::Bitmap* bmp, const CRect& rect ) -> CRect
        {
            if( !bmp )
                return {};

            UINT w = bmp->GetWidth();
            UINT h = bmp->GetHeight();
            double rate = (std::max)( w*1.0/rect.Width(), h*1.0/rect.Height() );
            w = (UINT) std::floor( w/rate );
            h = (UINT) std::floor( h/rate );

            return CRect( rect.left + (rect.Width()-w)/2
                        , rect.top + (rect.Height()-h)/2
                        , rect.left + (rect.Width()-w)/2 + w
                        , rect.top + (rect.Height()-h)/2 + h );
        };
    m_lhs_info.rect_img = foo( m_lhs.get(), m_lhs_info.rect );
    m_rhs_info.rect_img = foo( m_rhs.get(), m_rhs_info.rect );

    auto bar = []( Gdiplus::Bitmap* hs, const ImageInfo& hs_info, CDC& dc, COLORREF b_color, COLORREF c_color, LONG font_height )
        {
            if( !hs )
                return;
            {
                std::wstring a = std::format( L"[{}] ", hs_info.imgfmt );
                int aw = dc.GetTextExtent(a.c_str()).cx;
                std::wstring b = std::format( L"{}x{}  ", hs_info.width, hs_info.height );
                int bw = dc.GetTextExtent(b.c_str()).cx;
                std::wstring c = std::format( L"{}KB", std::ceil(hs_info.filesize/1024.0) );
                std::wstring d = hs_info.lastwritetime_s;
                int tw = (std::max)( dc.GetTextExtent((a+b+c).c_str()).cx, dc.GetTextExtent(d.c_str()).cx );
                int s = tw<hs_info.rect.Width() ? (hs_info.rect.Width()-tw)/2 : 0;
	            s += hs_info.rect.left;

                dc.SetTextColor( RGB(0,0,0) );
                dc.TextOut( s, hs_info.rect.bottom, a.c_str() );
                dc.SetTextColor( b_color );
                dc.TextOut( s+aw, hs_info.rect.bottom, b.c_str() );
                dc.SetTextColor( c_color );
                dc.TextOut( s+aw+bw, hs_info.rect.bottom, c.c_str() );
                dc.SetTextColor( RGB(0,0,0) );
                dc.TextOut( s, hs_info.rect.bottom+font_height, d.c_str() );

                Gdiplus::Graphics graphics( dc.GetSafeHdc() );
                graphics.DrawImage( hs, hs_info.rect_img.left, hs_info.rect_img.top, hs_info.rect_img.Width(), hs_info.rect_img.Height() );
            }
        };

    int oldbkmode = dc.SetBkMode( TRANSPARENT );
    bar( m_lhs.get(), m_lhs_info, dc
        , m_lhs_info.width*m_lhs_info.height>=m_rhs_info.width*m_rhs_info.height ? RGB(0,0,0) : RGB(255,255,255)
        , m_lhs_info.filesize>=m_rhs_info.filesize ? RGB(0,0,0) : RGB(255,255,255)
        , tm.tmHeight );
    bar( m_rhs.get(), m_rhs_info, dc
        , m_lhs_info.width*m_lhs_info.height<=m_rhs_info.width*m_rhs_info.height ? RGB(0,0,0) : RGB(255,255,255)
        , m_lhs_info.filesize<=m_rhs_info.filesize ? RGB(0,0,0) : RGB(255,255,255)
        , tm.tmHeight );
    dc.SetBkMode( oldbkmode );
}


void CPairImgCtrl::ShowImagesFullScreen( bool left_first )
{
    {
        m_ImgDlg.m_imgs[0] = m_lhs.get();
        m_ImgDlg.m_imgs[1] = m_rhs.get();
        if( left_first )
            m_ImgDlg.m_ImgIdx = 0;
        else
            m_ImgDlg.m_ImgIdx = 1;
        m_ImgDlg.DoModal();
    }
}

void CPairImgCtrl::OnLButtonDblClk( UINT nFlags, CPoint point )
{
    if( m_lhs.get() && m_lhs_info.rect.PtInRect(point) )
        ShowImagesFullScreen( true );
    else if( m_rhs.get() && m_rhs_info.rect.PtInRect(point) )
        ShowImagesFullScreen( false );

    CStatic::OnLButtonDblClk( nFlags, point );
}
