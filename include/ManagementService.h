/*****************************************************************************
 * @file ManagementService.h
 * @author xjl (xjl20011009@126.com)
 * @brief 虚函数用于提供服务接口
 * @version 0.1
 * @date 2025-12-04
 *
 * @copyright Copyright (c) 2025
 *****************************************************************************/

#include <string>

class ManagementService
{
public:
    virtual void merge(const std::string &name, const std::string &password) = 0;
    virtual void deleteUser(const std::string &name) = 0;
    virtual void modifyUser(const std::string &name, const std::string &newPassword) = 0;
    virtual void queryUser(const std::string &name) = 0;
};