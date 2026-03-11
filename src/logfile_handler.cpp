#include "logfile_handler.h"
#include "config.h"
#include "logger.h"
#include "encoding.h"
#include "core.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <windows.h>

// 读取文件的前N行（使用编码检测）
std::string LogFileHandler::readFirstLines(const std::string& filepath, int lineCount) {
    EncodedFileContent fileContent = readFileWithEncoding(filepath);
    
    if (fileContent.content.empty()) {
        LOG_ERROR("Failed to read file for reading first lines: " + filepath);
        return "";
    }
    
    // 按行分割内容
    std::vector<std::string> lines = split(fileContent.content, '\n');
    
    std::ostringstream oss;
    int count = 0;
    
    for (const auto& line : lines) {
        if (count >= lineCount) break;
        oss << line << "\n";
        count++;
    }
    
    return oss.str();
}

// 检查是否包含特定字符串
bool LogFileHandler::containsString(const std::string& content, const std::string& pattern) {
    return content.find(pattern) != std::string::npos;
}

// 识别log文件类型
LogFileType LogFileHandler::identifyLogType(const std::string& filepath) {
    try {
        // 读取前5行
        std::string firstLines = readFirstLines(filepath, 5);
        
        if (firstLines.empty()) {
            LOG_WARNING("Failed to read first lines from: " + filepath);
            return LogFileType::OTHER;
        }
        
        // 转换为大写以便匹配（不区分大小写）
        std::string upperContent = firstLines;
        std::transform(upperContent.begin(), upperContent.end(), upperContent.begin(), ::toupper);
        
        // 检查ORCA标识
        std::string orcaPattern = "* O   R   C   A *";
        std::string orcaPatternUpper = orcaPattern;
        std::transform(orcaPatternUpper.begin(), orcaPatternUpper.end(), orcaPatternUpper.begin(), ::toupper);
        
        if (containsString(upperContent, orcaPatternUpper)) {
            LOG_INFO("Identified log file as ORCA: " + filepath);
            return LogFileType::ORCA;
        }
        
        // 检查Gaussian标识
        std::string gaussianPattern = "Entering Gaussian System";
        std::string gaussianPatternUpper = gaussianPattern;
        std::transform(gaussianPatternUpper.begin(), gaussianPatternUpper.end(), gaussianPatternUpper.begin(), ::toupper);
        
        if (containsString(upperContent, gaussianPatternUpper)) {
            LOG_INFO("Identified log file as Gaussian: " + filepath);
            return LogFileType::GAUSSIAN;
        }
        
        // 默认归类为其他
        LOG_INFO("Identified log file as OTHER: " + filepath);
        return LogFileType::OTHER;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception identifying log file type: " + std::string(e.what()));
        return LogFileType::OTHER;
    }
}

// 使用配置的程序打开log文件
bool LogFileHandler::openLogFile(const std::string& filepath, LogFileType type) {
    try {
        std::string viewerPath;
        std::string typeName;
        
        // 根据类型选择查看器
        switch (type) {
            case LogFileType::ORCA:
                viewerPath = g_config.orcaLogViewer;
                typeName = "ORCA";
                break;
            case LogFileType::GAUSSIAN:
                viewerPath = g_config.gaussianLogViewer;
                typeName = "Gaussian";
                break;
            case LogFileType::OTHER:
            default:
                viewerPath = g_config.otherLogViewer;
                typeName = "Other";
                break;
        }
        
        if (viewerPath.empty()) {
            LOG_ERROR("Viewer path is empty for " + typeName + " log files");
            return false;
        }

        // Support %VAR% and paths relative to config.ini
        std::string viewerExe = resolveConfigPathForExecutable(viewerPath);
        
        // 构建命令：viewerPath "filepath"
        std::string command = "\"" + viewerExe + "\" \"" + filepath + "\"";
        LOG_DEBUG("Executing command: " + command);
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        // CreateProcess 可能会修改命令行缓冲区，因此必须传入可写 buffer
        std::vector<char> cmdBuf(command.begin(), command.end());
        cmdBuf.push_back('\0');

        if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to launch " + typeName + " log viewer (Error: " + std::to_string(error) + ")");
            return false;
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        LOG_INFO("Successfully opened " + typeName + " log file with: " + viewerPath);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception opening log file: " + std::string(e.what()));
        return false;
    }
}


