#pragma once

// 版本信息定义
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_BUILD 0

// 版本字符串
#define VERSION_STRING "2.0.0"

// 程序信息
#define APP_NAME "XYZ Monitor"
#define APP_DESCRIPTION "XYZ Monitor - Clipboard to GView Bridge"
#define APP_INTERNAL_NAME "xyz_monitor"
#define APP_EXE_NAME "xyz_monitor.exe"
#define APP_COMPANY "Bane Dysta"
#define APP_AUTHOR "Author: Bane Dysta"

// 反馈链接
#define FEEDBACK_GITHUB "https://github.com/bane-dysta/xyzTrickGview2"
#define FEEDBACK_FORUM "http://bbs.keinsci.com/forum.php?mod=viewthread&tid=55596&fromuid=63020"

// RC文件需要的宏定义（保持兼容性）
#define VER_FILEVERSION VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,VERSION_BUILD
#define VER_FILEVERSION_STR VERSION_STRING "\0"
#define VER_PRODUCTVERSION VER_FILEVERSION
#define VER_PRODUCTVERSION_STR VERSION_STRING "\0"
#define VER_COMPANYNAME_STR APP_COMPANY "\0"
#define VER_FILEDESCRIPTION_STR APP_DESCRIPTION "\0"
#define VER_INTERNALNAME_STR APP_INTERNAL_NAME "\0"
#define VER_LEGALCOPYRIGHT_STR APP_AUTHOR "\0"
#define VER_ORIGINALFILENAME_STR APP_EXE_NAME "\0"
#define VER_PRODUCTNAME_STR APP_NAME "\0"
