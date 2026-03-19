#include "encoding.h"
#include "logger.h"
#include <fstream>
#include <algorithm>
#include <windows.h>

// 计算字节序列中不符合 UTF-8 规则的比例
static double calculateInvalidUtf8Ratio(const unsigned char* data, size_t length) {
    if (length == 0) return 1.0;
    
    size_t invalidCount = 0;
    size_t i = 0;
    
    while (i < length) {
        if (data[i] <= 0x7F) {
            i++;
        } else if ((data[i] & 0xE0) == 0xC0) {
            if (i + 1 >= length || (data[i + 1] & 0xC0) != 0x80) {
                invalidCount++;
            }
            i += 2;
        } else if ((data[i] & 0xF0) == 0xE0) {
            if (i + 2 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                invalidCount++;
            }
            i += 3;
        } else if ((data[i] & 0xF8) == 0xF0) {
            if (i + 3 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) {
                invalidCount++;
            }
            i += 4;
        } else {
            invalidCount++;
            i++;
        }
    }
    
    return static_cast<double>(invalidCount) / length;
}

TextEncoding detectEncoding(const std::vector<unsigned char>& buffer) {
    if (buffer.empty()) {
        return TextEncoding::UNKNOWN;
    }
    
    // 检查 BOM
    if (buffer.size() >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        return TextEncoding::UTF8_BOM;
    }
    if (buffer.size() >= 4 && buffer[0] == 0xFF && buffer[1] == 0xFE && buffer[2] == 0x00 && buffer[3] == 0x00) {
        return TextEncoding::UTF32_LE;
    }
    if (buffer.size() >= 4 && buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0xFE && buffer[3] == 0xFF) {
        return TextEncoding::UTF32_BE;
    }
    if (buffer.size() >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
        return TextEncoding::UTF16_LE;
    }
    if (buffer.size() >= 2 && buffer[0] == 0xFE && buffer[1] == 0xFF) {
        return TextEncoding::UTF16_BE;
    }
    
    // 检查是否包含 null 字节（可能是 UTF-16）
    size_t nullCount = 0;
    for (size_t i = 0; i < buffer.size() - 1; i += 2) {
        if (buffer[i] == 0 && buffer[i + 1] != 0) {
            nullCount++;
        } else if (buffer[i + 1] == 0 && buffer[i] != 0) {
            nullCount++;
        }
    }
    
    // 如果每几个字节就有一个 null 字节，可能是 UTF-16
    if (buffer.size() > 10 && nullCount > buffer.size() / 16) {
        // 进一步判断是 LE 还是 BE
        if (buffer.size() >= 2) {
            if (buffer[0] != 0 && buffer[1] == 0) {
                return TextEncoding::UTF16_LE;
            } else if (buffer[0] == 0 && buffer[1] != 0) {
                return TextEncoding::UTF16_BE;
            }
        }
    }
    
    // 检查 UTF-8 有效性
    double invalidRatio = calculateInvalidUtf8Ratio(buffer.data(), buffer.size());
    
    // 如果无效字节比例很低，认为是 UTF-8
    if (invalidRatio < 0.01) {
        return TextEncoding::UTF8;
    }
    
    // 否则假定为 ANSI/GBK（中文 Windows 常用）
    return TextEncoding::ANSI;
}

LineEnding detectLineEnding(const std::string& content) {
    size_t crlfCount = 0;
    size_t lfCount = 0;
    size_t crCount = 0;
    
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                crlfCount++;
                i++; // 跳过 \n
            } else {
                crCount++;
            }
        } else if (content[i] == '\n') {
            lfCount++;
        }
    }
    
    size_t total = crlfCount + lfCount + crCount;
    if (total == 0) {
        return LineEnding::LF; // 默认
    }
    
    if (crlfCount >= lfCount && crlfCount >= crCount) {
        return LineEnding::CRLF;
    } else if (lfCount >= crlfCount && lfCount >= crCount) {
        return LineEnding::LF;
    } else {
        return LineEnding::CR;
    }
}

std::string normalizeLineEndings(const std::string& content, LineEnding target) {
    if (content.empty()) {
        return content;
    }
    
    std::string result;
    result.reserve(content.size());
    
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                // CRLF -> LF or CRLF
                if (target == LineEnding::LF) {
                    result += '\n';
                    i++; // 跳过 \n
                } else {
                    result += "\r\n";
                    i++;
                }
            } else {
                // CR -> LF or CRLF
                if (target == LineEnding::LF) {
                    result += '\n';
                } else {
                    result += "\r\n";
                }
            }
        } else if (content[i] == '\n') {
            // LF -> LF or CRLF
            if (target == LineEnding::CRLF) {
                result += "\r\n";
            } else {
                result += '\n';
            }
        } else {
            result += content[i];
        }
    }
    
    return result;
}

std::vector<unsigned char> readRawFile(const std::string& filepath) {
    std::vector<unsigned char> buffer;
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for reading: " + filepath);
        return buffer;
    }
    
    // 获取文件大小
    file.seekg(0, std::ios::end);
    std::streampos endPos = file.tellg();
    if (endPos <= 0) {
        LOG_WARNING("Failed to determine file size or file is empty: " + filepath);
        return buffer;
    }

    size_t fileSize = static_cast<size_t>(endPos);
    file.seekg(0, std::ios::beg);
    
    if (fileSize == 0) {
        LOG_WARNING("File is empty: " + filepath);
        return buffer;
    }
    
    // 限制读取大小（避免处理过大的文件）
    const size_t MAX_READ_SIZE = 100 * 1024 * 1024; // 100MB
    if (fileSize > MAX_READ_SIZE) {
        LOG_WARNING("File too large, limiting read size: " + filepath);
        fileSize = MAX_READ_SIZE;
    }
    
    buffer.resize(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    std::streamsize bytesReadSigned = file.gcount();
    file.close();

    if (bytesReadSigned <= 0) {
        LOG_WARNING("No data read from file: " + filepath);
        buffer.clear();
        return buffer;
    }

    size_t bytesRead = static_cast<size_t>(bytesReadSigned);
    if (bytesRead < fileSize) {
        buffer.resize(bytesRead);
    }
    
    return buffer;
}

std::string convertToUtf8(const std::vector<unsigned char>& buffer, TextEncoding encoding) {
    if (buffer.empty()) {
        return "";
    }
    
    // 如果已经是 UTF-8（无 BOM），直接转换
    if (encoding == TextEncoding::UTF8 || encoding == TextEncoding::UTF8_BOM) {
        // 跳过 BOM
        size_t offset = (encoding == TextEncoding::UTF8_BOM) ? 3 : 0;
        return std::string(reinterpret_cast<const char*>(buffer.data() + offset), buffer.size() - offset);
    }
    
    // 使用 Windows API 进行编码转换
    UINT srcCodePage = CP_ACP;
    
    switch (encoding) {
        case TextEncoding::GBK:
        case TextEncoding::GB2312:
            srcCodePage = 936; // GBK
            break;
        case TextEncoding::BIG5:
            srcCodePage = 950; // Big5
            break;
        case TextEncoding::SHIFT_JIS:
            srcCodePage = 932; // Shift-JIS
            break;
        case TextEncoding::EUC_KR:
            srcCodePage = 949; // EUC-KR
            break;
        case TextEncoding::UTF16_LE:
            srcCodePage = 1200; // UTF-16 LE
            break;
        case TextEncoding::UTF16_BE:
            srcCodePage = 1201; // UTF-16 BE
            break;
        case TextEncoding::UTF32_LE:
            srcCodePage = 12000; // UTF-32 LE
            break;
        case TextEncoding::UTF32_BE:
            srcCodePage = 12001; // UTF-32 BE
            break;
        default:
            srcCodePage = CP_ACP; // 系统默认 ANSI
            break;
    }
    
    // 计算目标缓冲区大小
    int requiredSize = MultiByteToWideChar(srcCodePage, 0, 
        reinterpret_cast<const char*>(buffer.data()), 
        static_cast<int>(buffer.size()), 
        NULL, 0);
    
    if (requiredSize == 0) {
        LOG_ERROR("Failed to calculate wide char size for encoding conversion");
        return "";
    }
    
    std::wstring wideBuffer(requiredSize, L'\0');
    int result = MultiByteToWideChar(srcCodePage, 0, 
        reinterpret_cast<const char*>(buffer.data()), 
        static_cast<int>(buffer.size()), 
        &wideBuffer[0], requiredSize);
    
    if (result == 0) {
        LOG_ERROR("Failed to convert to wide char");
        return "";
    }
    
    // 从宽字符转换到 UTF-8
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, 
        wideBuffer.c_str(), -1, 
        NULL, 0, NULL, NULL);
    
    if (utf8Size == 0) {
        LOG_ERROR("Failed to calculate UTF-8 size");
        return "";
    }
    
    std::string utf8Buffer(utf8Size, '\0');
    result = WideCharToMultiByte(CP_UTF8, 0, 
        wideBuffer.c_str(), -1, 
        &utf8Buffer[0], utf8Size, NULL, NULL);
    
    if (result == 0) {
        LOG_ERROR("Failed to convert to UTF-8");
        return "";
    }
    
    // 移除结尾的空字符
    if (!utf8Buffer.empty() && utf8Buffer.back() == '\0') {
        utf8Buffer.pop_back();
    }
    
    return utf8Buffer;
}

EncodedFileContent readFileWithEncoding(const std::string& filepath) {
    EncodedFileContent result;
    result.encoding = TextEncoding::UNKNOWN;
    result.lineEnding = LineEnding::UNKNOWN;
    result.hasBOM = false;
    result.content = "";
    
    // 读取原始文件
    std::vector<unsigned char> rawBuffer = readRawFile(filepath);
    
    if (rawBuffer.empty()) {
        LOG_ERROR("Failed to read file: " + filepath);
        return result;
    }
    
    // 检测编码
    result.encoding = detectEncoding(rawBuffer);
    result.hasBOM = (result.encoding == TextEncoding::UTF8_BOM);
    
    LOG_DEBUG("Detected encoding: " + encodingToString(result.encoding) + " for file: " + filepath);
    
    // 转换为 UTF-8
    result.content = convertToUtf8(rawBuffer, result.encoding);
    
    if (result.content.empty()) {
        LOG_ERROR("Failed to convert content to UTF-8: " + filepath);
        return result;
    }
    
    // 检测换行符
    result.lineEnding = detectLineEnding(result.content);
    LOG_DEBUG("Detected line ending: " + lineEndingToString(result.lineEnding) + " for file: " + filepath);
    
    // 统一换行符为 LF（便于处理）
    result.content = normalizeLineEndings(result.content, LineEnding::LF);
    
    return result;
}

std::string encodingToString(TextEncoding encoding) {
    switch (encoding) {
        case TextEncoding::UTF8: return "UTF-8";
        case TextEncoding::UTF8_BOM: return "UTF-8 with BOM";
        case TextEncoding::UTF16_LE: return "UTF-16 LE";
        case TextEncoding::UTF16_BE: return "UTF-16 BE";
        case TextEncoding::UTF32_LE: return "UTF-32 LE";
        case TextEncoding::UTF32_BE: return "UTF-32 BE";
        case TextEncoding::GBK: return "GBK";
        case TextEncoding::GB2312: return "GB2312";
        case TextEncoding::BIG5: return "Big5";
        case TextEncoding::SHIFT_JIS: return "Shift-JIS";
        case TextEncoding::EUC_KR: return "EUC-KR";
        case TextEncoding::ANSI: return "ANSI";
        default: return "Unknown";
    }
}

std::string lineEndingToString(LineEnding ending) {
    switch (ending) {
        case LineEnding::LF: return "LF (\\n)";
        case LineEnding::CRLF: return "CRLF (\\r\\n)";
        case LineEnding::CR: return "CR (\\r)";
        default: return "Unknown";
    }
}
