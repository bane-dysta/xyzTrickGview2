#pragma once

#include <string>
#include <vector>

// 支持的编码类型
enum class TextEncoding {
    UTF8,           // UTF-8 (with or without BOM)
    UTF8_BOM,       // UTF-8 with BOM
    UTF16_LE,       // UTF-16 Little Endian
    UTF16_BE,       // UTF-16 Big Endian
    UTF32_LE,       // UTF-32 Little Endian
    UTF32_BE,       // UTF-32 Big Endian
    GBK,            // GBK (Chinese simplified)
    GB2312,         // GB2312 (Chinese simplified)
    BIG5,           // Big5 (Chinese traditional)
    SHIFT_JIS,     // Shift-JIS (Japanese)
    EUC_KR,         // EUC-KR (Korean)
    ANSI,           // System default ANSI encoding
    UNKNOWN         // Unknown encoding
};

// 换行符类型
enum class LineEnding {
    LF,     // Unix: \n
    CRLF,   // Windows: \r\n
    CR,     // Old Mac: \r
    UNKNOWN
};

// 文件读取结果结构体
struct EncodedFileContent {
    std::string content;        // 转换后的 UTF-8 内容
    TextEncoding encoding;      // 检测到的编码
    LineEnding lineEnding;     // 换行符类型
    bool hasBOM;                // 是否有 BOM
};

// 编码检测和转换函数

// 检测文件编码（基于 BOM 和字节序列分析）
TextEncoding detectEncoding(const std::vector<unsigned char>& buffer);

// 检测换行符类型
LineEnding detectLineEnding(const std::string& content);

// 读取文件并自动检测编码，转换为 UTF-8
// 返回空字符串表示失败
EncodedFileContent readFileWithEncoding(const std::string& filepath);

// 读取原始文件内容（不进行编码转换）
// 返回空 vector 表示失败
std::vector<unsigned char> readRawFile(const std::string& filepath);

// 将指定编码的字节 buffer 转换为 UTF-8 字符串
std::string convertToUtf8(const std::vector<unsigned char>& buffer, TextEncoding encoding);

// 统一换行符：将内容中的换行符统一为 LF
std::string normalizeLineEndings(const std::string& content, LineEnding target = LineEnding::LF);

// 获取编码名称（用于日志）
std::string encodingToString(TextEncoding encoding);

// 获取换行符名称（用于日志）
std::string lineEndingToString(LineEnding ending);
