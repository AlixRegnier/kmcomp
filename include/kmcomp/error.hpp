#ifndef KMCOMP_ERROR_H
#define KMCOMP_ERROR_H

#include <stdexcept>
#include <string>

namespace kmcomp
{
    inline std::string error_str(const std::string& class_name, const std::string& function_name, const std::string& msg)
    {
        return "[ERROR] " + class_name + "::" + function_name + " : " + msg;
    }

    inline std::string warning_str(const std::string& class_name, const std::string& function_name, const std::string& msg)
    {
        return "[WARNING] " + class_name + "::" + function_name + " : " + msg;
    }

    class kmcomp_error : public std::runtime_error
    {
    public:
        explicit kmcomp_error(const std::string& class_name, const std::string& function_name, const std::string& msg)
            : std::runtime_error(error_str(class_name, function_name, msg)) {}
    };
}

#endif