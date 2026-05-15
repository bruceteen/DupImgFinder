#pragma once
#include <stop_token>
#include <vector>
#include <string>
#include <mutex>

//void thread_cal_pHashs( std::stop_token stopToken, std::vector<std::wstring> folders
//    , std::mutex& m_mutex
//    , std::wstring& m_Thread_Data1
//    , std::vector<std::tuple<std::wstring,int,std::wstring>>& m_Thread_Data2
//    , std::wstring& m_Thread_Data3 );

void thread_cal_pHashs( std::stop_token stopToken, std::vector<std::wstring> folders
    , std::mutex& m_mutex
    , std::wstring& m_progress
    , std::vector<std::pair<std::wstring,std::vector<std::pair<std::wstring,uint64_t>>>>& m_result1
    , int min_similarity
    , std::vector<std::tuple<size_t,size_t,int,size_t,size_t,UINT>>& m_result2
    , std::wstring& m_summary );