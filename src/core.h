#pragma once

#include <string>
#include <vector>
#include <map>

// 原子结构体
struct Atom {
    std::string symbol;
    double x, y, z;
    double charge = 0.0;  // 电荷（从CHG格式读取，用于Mulliken电荷）
};

// 优化信息结构体
struct OptimizationInfo {
    double maxForce = -1.0;      // 最大受力
    double rmsForce = -1.0;      // 方均根受力
    double maxDisp = -1.0;       // 最大位移
    double rmsDisp = -1.0;       // 方均根位移
    double energy = 0.0;         // 能量
    bool hasData = false;        // 是否包含优化数据
};

// 帧结构体
struct Frame {
    std::vector<Atom> atoms;
    std::string comment;
    OptimizationInfo optInfo;    // 优化信息
};

// 全局原子序数映射
extern std::map<std::string, int> atomicNumbers;
extern std::map<int, std::string> atomicNumberToSymbol;

// 工具函数
std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delim);
std::vector<std::string> splitWhitespace(const std::string& str);
int getAtomicNumber(const std::string& symbol);
size_t calculateMaxChars(int memoryMB);