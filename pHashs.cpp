#include "pch.h"
#include "pHashs.h"
#include "misc.h"

// https://github.com/libvips/build-win64-mxe/releases
// vips-dev-x64-web-8.18.2-static.zip
// vips-dev-x64-all-8.18.2.zip


#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <set>
#include <numbers>
#include <optional>
#include <algorithm>
#include <ranges>
#include <chrono>
#include <thread>

#include "vips/vips.h"
#pragma comment( lib, "vips-dev-8.18.2/lib/libvips.lib" )

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

template <class T>
static std::wstring convert_to_wstring( const T& src )
{
    return std::filesystem::path(src).wstring();
}

static std::vector<std::u8string> collect_load_formats()
{
    std::vector<std::u8string> result;
    const char* nickname = vips_nickname_find( VIPS_TYPE_FOREIGN_LOAD );
    if( nickname )
    {
        guint n_children;
        GType* children = g_type_children( VIPS_TYPE_FOREIGN_LOAD, &n_children );
        for( guint i=0; i!=n_children; ++i )
        {
            const char8_t* name = (const char8_t*)vips_nickname_find( children[i] );
            std::u8string_view subnickname = name ? name : u8"";

            if( subnickname.ends_with(u8"load_base") )
            {
                subnickname.remove_suffix(9);
                result.emplace_back( subnickname );
            }
        }
        g_free( children );
    }
    return result;
}

[[maybe_unused]] static std::string foo( VipsImage* img )
{
    int width = vips_image_get_width(img);
    int height = vips_image_get_height(img);
    int bands = vips_image_get_bands(img);
    VipsBandFormat format = vips_image_get_format(img);
    VipsInterpretation interpretation = vips_image_get_interpretation(img);
    gboolean has_alpha = vips_image_hasalpha(img);
    guint64 pel_size = VIPS_IMAGE_SIZEOF_PEL(img);

    auto s = std::format( "Before flatten:\n");
    s += std::format( "  Width: {}\n", width );
    s += std::format( "  Height: {}\n", height );
    s += std::format( "  Bands: {}\n", bands );
    s += std::format( "  Format: {}\n", vips_enum_nick(VIPS_TYPE_BAND_FORMAT,format) );
    s += std::format( "  Interpretation: {}\n", vips_enum_nick(VIPS_TYPE_INTERPRETATION,interpretation) );
    s += std::format( "  sizeof pixel: {}\n", VIPS_IMAGE_SIZEOF_PEL(img) );
    s += std::format( "  Has Alpha: {}\n", has_alpha?"Yes":"No" );
    s += std::format( "  sizeof pixel: {}\n", pel_size );
    return s;
}

static std::optional<uint64_t> process_single_image( const char8_t* path, std::u8string& img_fmt )
{
    vips_error_clear();
    img_fmt.clear();

    // 生成彩色缩略图
    VipsImage* thumb = nullptr;
    if( vips_thumbnail((const char*)path, &thumb, 32, "height", 32
        , "crop", VIPS_INTERESTING_CENTRE
        , nullptr) )
    {
        [[maybe_unused]] const char* error_msg = vips_error_buffer();
        vips_error_clear();
        return {};
    }

    // 获得图像格式
    {
        const char* fmt;
        if( vips_image_get_string(thumb,"vips-loader",&fmt) == 0 )
        {
            std::u8string_view f = (const char8_t*)fmt;
            if( f.ends_with(u8"load") )
                f.remove_suffix( 4 );
            img_fmt = f;
        }
    }

    // 转换为灰度
    VipsImage* gray = nullptr;
    if( vips_colourspace(thumb, &gray, VIPS_INTERPRETATION_B_W, nullptr) )
    {
        g_object_unref(thumb);
        return {};
    }
    g_object_unref( thumb );

    // 如果带有alpha通道
    if( vips_image_hasalpha(gray) )
    {
        VipsImage* tmp = nullptr;
        if( vips_extract_band(gray,&tmp,0,"n",1,nullptr) )
        {
            g_object_unref(gray);
            return {};
        }
        g_object_unref(gray);
        gray = tmp;
    }

    // 确保图像是内存中的连续块
    size_t mem_size = 0;
    void* mem_buf = vips_image_write_to_memory( gray, &mem_size );
    if( !mem_buf || mem_size!=32*32 )
    {
        if( mem_buf )
            g_free(mem_buf);
        g_object_unref( gray );
        return {};
    }

    // 计算 pHash
    uint64_t calculate_phash_32x32_fast( const unsigned char data[1024] );
    uint64_t pHash = calculate_phash_32x32_fast( (const unsigned char*)mem_buf );

    g_free( mem_buf );
    g_object_unref(gray);
    return pHash;
}

static std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>>
GetAllPHash( std::stop_token& stopToken
    , const std::vector<std::wstring>& folders
    , std::mutex& m_mutex
    , std::wstring& m_progress
    , std::wstring& info
    , size_t& images_count )
{
    std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>> result;
    size_t num_folders=0, num_files=0, num_images=0;
    std::map<std::u8string,std::map<std::u8string,std::wstring>> extnames;
    std::map<std::u8string,std::wstring> failed_extnames;

    auto foo = [&]( const std::filesystem::path& folder )
        {
            std::wstring cur_folder = folder.wstring();
            std::wstring t_cur_folder = cur_folder;
            if( m_mutex.try_lock() )
            {
                m_progress = std::move(t_cur_folder);
                m_mutex.unlock();
            }

            std::vector<std::wstring> sub_folders;
            std::vector<std::pair<std::wstring,uint64_t>> tmp;
            std::error_code ec{};
            for( const auto& entry : std::filesystem::directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied,ec) )
            {
                if( stopToken.stop_requested() )
                    break;

                if( entry.is_directory() )
                {
                    sub_folders.push_back( entry.path().wstring() );
                }
                else if( entry.is_regular_file() )
                {
                    ++num_files;
                    std::u8string img_fmt;
                    auto pHash = process_single_image( entry.path().u8string().c_str(), img_fmt );
                    auto ext = entry.path().extension().u8string();
                    std::ranges::transform( ext, ext.begin(), [](char8_t ch){return (char8_t)std::tolower(ch);} );
                    if( pHash.has_value() )
                    {
                        ++num_images;
                        auto& q = extnames[img_fmt];
                        if( !q.contains(ext) )
                            q.insert( {ext, entry.path().wstring()} );
                        tmp.emplace_back( entry.path().filename().wstring(), pHash.value() );
                    }
                    else if( entry.path().filename() != L".nomedia" )
                    {
                        if( !failed_extnames.contains(ext) )
                            failed_extnames.insert( {ext, entry.path().wstring()} );
                    }
                }
            }
            if( !tmp.empty() )
            {
                result.emplace_back( folder.wstring(), std::move(tmp) );
            }
            return sub_folders;
        };

    auto folders_ = folders;
    for( auto& v : folders_ )
        v = misc::lexically_normal_tolower(v);
    std::ranges::reverse( folders_ );
    for( ; !folders_.empty(); )
    {
        if( stopToken.stop_requested() )
            break;
        ++num_folders;
        auto sub_folders = foo( folders_.back() );
        std::ranges::reverse( sub_folders );
        folders_.pop_back();
        folders_.insert_range( folders_.end(), std::move(sub_folders) );
    }
    if( stopToken.stop_requested() )
        return {};

    std::ranges::sort( result, {}, &decltype(result)::value_type::first );
    for( auto& v : std::views::elements<1>(result) )
        std::ranges::sort( v, {}, &std::remove_cvref_t<decltype(v)>::value_type::first );
        
    info += std::format( L"\n目录数量 {}, 文件数量 {}, 图像文件数量 {}.\n", num_folders, num_files, num_images );
    info += std::format( L"\n图像格式 与 存在的扩展名:\n" );
    for( const auto& [fmt,exts] : extnames )
    {
        info += std::format( L"    [{}]:\n", convert_to_wstring(fmt));
        for( const auto& [ext,filename] : exts )
        {
            if( !ext.empty() )
                info += std::format( L"        {} (其一: {})\n", convert_to_wstring(ext), filename );
            else
                info += std::format(L"        . (其一: {})\n", filename );
        }
    }
    if( !failed_extnames.empty() )
    {
        info += L"    [处理失败]:\n";
        for( const auto& [ext,filename] : failed_extnames )
        {
            if( !ext.empty() )
                info += std::format(L"        {} (其一: {})\n", convert_to_wstring(ext), filename );
            else
                info += std::format(L"        . (其一: {})\n", filename );
        }
    }
    info += L'\n';

    images_count = num_images;
    return result;
}

static void CompareAllPHash( std::stop_token& stopToken
    , const std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>>& files
    , std::mutex& m_mutex
    , std::wstring& m_progress
    , int min_similarity
    , std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>>& result2
    , std::wstring& info
    , size_t& images_count )
{
    if( files.empty() ) return;

    images_count = 0;
    for( const auto& t : files )
        images_count += t.second.size();
    const size_t n = images_count*(images_count-1)/2;
    images_count = 0;
    size_t cur = 0;

    // 要求 files 非空，以及files元素.second非空
    size_t idx1 = 0;
    size_t idx2 = 0;
    auto next = [&]( size_t& idx1, size_t& idx2 ) {
            ++idx2;
            if( idx2 == files[idx1].second.size() )
            {
                ++idx1;
                idx2 = 0;
            }
        };
    for( ; idx1!=files.size(); next(idx1,idx2) )
    {
        auto idx3 = idx1;
        auto idx4 = idx2;
        for( next(idx3,idx4); idx3!=files.size(); next(idx3,idx4) )
        {
            if( stopToken.stop_requested() )
                return;

            ++cur;
            if( cur*100/n != (cur-1)*100/n )
            {
                auto tmp = std::format(L"{}%",cur*100/n);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_progress = std::move(tmp);
                }
            }

            auto cmp_result = sizeof(uint64_t)*CHAR_BIT - std::popcount( files[idx1].second[idx2].second ^ files[idx3].second[idx4].second );
            if( cmp_result >= min_similarity )
            {
                result2.emplace_back( idx1, idx2, (int)cmp_result, idx3, idx4, 0 );
                ++images_count;
            }
        }
    }

    return;
};

void thread_cal_pHashs( std::stop_token stopToken, std::vector<std::wstring> folders
    , std::mutex& m_mutex
    , std::wstring& m_progress
    , std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>>& m_result1
    , int min_similarity
    , std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>>& m_result2
    , std::wstring& m_summary )
{
    std::wstring info;
    // 输出支持的图像类型
    {
        std::vector<std::u8string> formats = collect_load_formats();
        info += L"libvips 支持的读取格式:\n  ";
        for( const auto& fmt : formats )
        {
            info += convert_to_wstring(fmt), info += L", ";
        }
        if( info.ends_with(L", ") )
            info.erase( info.size() - 2 );
        info += L'\n';
    }

    // 能快1%不到，在 DupImgFinder.cpp 中已经设过了
    //vips_cache_set_max(0);
    //vips_cache_set_max_mem(0);

    // g_setenv("VIPS_BLOCK_UNTRUSTED", "1", TRUE); // 问问有什么用？

    // 使用线程池反而更慢
    // g_thread_pool_new( foo, nullptr, g_get_num_processors(), true, nullptr );

    std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>> result1;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        result1 = m_result1;
    }
    {
        // 移除空目录
        auto itor = std::ranges::remove_if( result1, [](const auto& v){return v.second.empty();} );
        result1.erase( itor.begin(), itor.end() );
        // 移除空文件
        for( auto& v : std::views::elements<1>(result1) )
        {
            auto itor = std::ranges::remove_if( v, [](const auto& w){return w.first.empty();} );
            v.erase( itor.begin(), itor.end() );
        }
    }
    if( result1.empty() )
    {
        auto t_start = std::chrono::high_resolution_clock::now();

        size_t images_count;
        result1 = GetAllPHash( stopToken, folders, m_mutex, m_progress, info, images_count );
        auto result1_tmp = result1;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_result1 = std::move(result1_tmp);
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
        info += std::format( L"elapsed time: {:02}:{:02}:{:02} ({}秒)\n"
            , sec/3600
            , sec%3600/60
            , sec%60
            , sec );
    }
    else
        info.clear();

    if( !result1.empty() )
    {
        auto t_start = std::chrono::high_resolution_clock::now();

        std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>> t_result2;
        size_t images_count;
        CompareAllPHash( stopToken, result1, m_mutex, m_progress, min_similarity, t_result2, info, images_count );
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_result2 = std::move(t_result2);
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
        info += std::format( L"\n共找到 {} 对相似图像\n", images_count );
        info += std::format( L"elapsed time: {:02}:{:02}:{:02} ({}秒)\n"
            , sec/3600
            , sec%3600/60
            , sec%60
            , sec );
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_summary = std::move(info);
    }
}

