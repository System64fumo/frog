#pragma once
#include <map>
#include <string>

inline std::map<std::string, std::string> xdg_dirs;
void get_xdg_user_dirs();
