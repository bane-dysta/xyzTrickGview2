#pragma once

#include "core.h"
#include <string>
#include <vector>

// 格式检测函数
bool isValidCoordinateLine(const std::string& line);
bool isSimplifiedXYZFormat(const std::vector<std::string>& lines);
bool isXYZFormat(const std::string& content);

// 优化信息解析函数
OptimizationInfo parseOptimizationInfo(const std::string& comment);
double parseScientificNumber(const std::string& str);

// XYZ读取函数
bool readXYZFrame(const std::vector<std::string>& lines, size_t startLine, Frame& frame, size_t& nextStart);
std::vector<Frame> readMultiXYZ(const std::string& content);

// Gaussian相关函数
std::vector<Atom> parseGaussianClipboard(const std::string& filename);
std::string createXYZString(const std::vector<Atom>& atoms);

// Gaussian LOG格式转换
std::string writeGaussianLogHeader();
// 修改：增加previousFrame参数，用于在当前帧缺少收敛信息时使用前一帧的数据
std::string writeGaussianLogGeometry(const Frame& frame, int frameNumber, const Frame* previousFrame = nullptr);
std::string writeGaussianLogFooter();
std::string convertToGaussianLog(const std::vector<Frame>& frames);