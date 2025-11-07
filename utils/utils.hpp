#pragma once

#include <algorithm>
#include <string>

namespace track_project
{
    namespace utils
    {

        /*****************************************************************************
         * @brief 字符串处理函数
         *
         * @param s
         * @version 0.1
         * @author xjl (xjl20011009@126.com)
         * @date 2025-11-07
         * @copyright Copyright (c) 2025
         *****************************************************************************/
        inline void trim(std::string &s)
        {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                            { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                                 { return !std::isspace(ch); })
                        .base(),
                    s.end());
        }
    }
}