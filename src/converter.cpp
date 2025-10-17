#include "converter.h"
#include "logger.h"
#include "config.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

// 解析科学计数法数字
double parseScientificNumber(const std::string& str) {
    try {
        // 处理科学计数法格式，如 1E-4, 1.5E-3, 2.34e+5 等
        std::string processedStr = str;
        
        // 将E替换为e以保证一致性
        std::transform(processedStr.begin(), processedStr.end(), processedStr.begin(), ::tolower);
        
        return std::stod(processedStr);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to parse number: " + str + ", error: " + std::string(e.what()));
        return -1.0;
    }
}

// 解析优化信息
OptimizationInfo parseOptimizationInfo(const std::string& comment) {
    OptimizationInfo info;
    
    try {
        // 定义正则表达式来匹配各种格式
        std::regex maxfRegex(R"(MaxF\s*=\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))");
        std::regex rmsfRegex(R"(RMSF\s*=\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))");
        std::regex maxdRegex(R"(MaxD\s*=\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))");
        std::regex rmsdRegex(R"(RMSD\s*=\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))");
        std::regex energyRegex(R"(E\s*=\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))");
        
        std::smatch match;
        
        // 匹配最大受力 MaxF=
        if (std::regex_search(comment, match, maxfRegex)) {
            info.maxForce = parseScientificNumber(match[1].str());
            info.hasData = true;
            LOG_DEBUG("Parsed MaxF: " + std::to_string(info.maxForce));
        }
        
        // 匹配方均根受力 RMSF=
        if (std::regex_search(comment, match, rmsfRegex)) {
            info.rmsForce = parseScientificNumber(match[1].str());
            info.hasData = true;
            LOG_DEBUG("Parsed RMSF: " + std::to_string(info.rmsForce));
        }
        
        // 匹配最大位移 MaxD=
        if (std::regex_search(comment, match, maxdRegex)) {
            info.maxDisp = parseScientificNumber(match[1].str());
            info.hasData = true;
            LOG_DEBUG("Parsed MaxD: " + std::to_string(info.maxDisp));
        }
        
        // 匹配方均根位移 RMSD=
        if (std::regex_search(comment, match, rmsdRegex)) {
            info.rmsDisp = parseScientificNumber(match[1].str());
            info.hasData = true;
            LOG_DEBUG("Parsed RMSD: " + std::to_string(info.rmsDisp));
        }
        
        // 匹配能量 E=
        if (std::regex_search(comment, match, energyRegex)) {
            info.energy = parseScientificNumber(match[1].str());
            info.hasData = true;
            LOG_DEBUG("Parsed E: " + std::to_string(info.energy));
        }
        
    } catch (const std::exception& e) {
        LOG_WARNING("Exception parsing optimization info: " + std::string(e.what()));
    }
    
    return info;
}

// 检查是否为有效的坐标行
bool isValidCoordinateLine(const std::string& line) {
    std::vector<std::string> parts = splitWhitespace(line);
    
    // 需要足够的列来包含element和xyz
    int maxCol = std::max({g_config.elementColumn, g_config.xColumn, g_config.yColumn, g_config.zColumn});
    if (parts.size() < static_cast<size_t>(maxCol)) return false;
    
    try {
        // 验证xyz列是否为有效的数字（索引从0开始，所以要减1）
        double x = std::stod(parts[g_config.xColumn - 1]);
        double y = std::stod(parts[g_config.yColumn - 1]);
        double z = std::stod(parts[g_config.zColumn - 1]);
        (void)x; (void)y; (void)z;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// 检查是否为简化XYZ格式
bool isSimplifiedXYZFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) return false;
    
    size_t maxCheck = std::min(static_cast<size_t>(5), lines.size());
    for (size_t i = 0; i < maxCheck; ++i) {
        if (!isValidCoordinateLine(lines[i])) {
            return false;
        }
    }
    
    return true;
}

// 检查是否为XYZ格式
bool isXYZFormat(const std::string& content) {
    try {
        if (content.empty()) {
            LOG_DEBUG("Content is empty");
            return false;
        }
        
        if (content.find('\0') != std::string::npos) {
            LOG_DEBUG("Content contains binary data");
            return false;
        }
        
        std::vector<std::string> lines = split(content, '\n');
        if (lines.empty()) {
            LOG_DEBUG("No lines found in content");
            return false;
        }
        
        // 检查是否是标准XYZ格式（第一行是原子数）
        try {
            int atomCount = std::stoi(lines[0]);
            if (atomCount > 0 && atomCount <= 10000) {
                if (lines.size() < static_cast<size_t>(atomCount + 2)) {
                    LOG_DEBUG("Not enough lines for atom count: " + std::to_string(atomCount));
                    return false;
                }
                
                size_t maxCheck = std::min(static_cast<size_t>(5), static_cast<size_t>(atomCount));
                for (size_t i = 0; i < maxCheck; ++i) {
                    if (i + 2 < lines.size()) {
                        if (!isValidCoordinateLine(lines[i + 2])) {
                            LOG_DEBUG("Invalid coordinate line at index: " + std::to_string(i + 2));
                            return false;
                        }
                    }
                }
                LOG_DEBUG("Detected standard XYZ format");
                return true;
            }
        } catch (const std::exception&) {
            LOG_DEBUG("First line is not atom count, checking simplified format");
        }
        
        bool isSimplified = isSimplifiedXYZFormat(lines);
        if (isSimplified) {
            LOG_DEBUG("Detected simplified XYZ format");
        } else {
            LOG_DEBUG("Not recognized as XYZ format");
        }
        return isSimplified;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in isXYZFormat: " + std::string(e.what()));
        return false;
    }
}

// 检查是否为CHG格式
bool isChgFormat(const std::string& content) {
    try {
        if (content.empty()) {
            LOG_DEBUG("Content is empty");
            return false;
        }
        
        if (content.find('\0') != std::string::npos) {
            LOG_DEBUG("Content contains binary data");
            return false;
        }
        
        std::vector<std::string> lines = split(content, '\n');
        if (lines.empty()) {
            LOG_DEBUG("No lines found in content");
            return false;
        }
        
        int validLines = 0;
        int totalNonEmptyLines = 0;
        
        for (const std::string& line : lines) {
            std::string trimmedLine = trim(line);
            // 跳过空行和注释行
            if (trimmedLine.empty() || trimmedLine[0] == '#') {
                continue;
            }
            
            totalNonEmptyLines++;
            
            std::vector<std::string> parts = splitWhitespace(trimmedLine);
            // CHG格式需要有5列：Element X Y Z Charge
            if (parts.size() >= 5) {
                try {
                    // 验证第一列是元素符号（应该是字母开头）
                    if (parts[0].empty() || !std::isalpha(parts[0][0])) {
                        continue;
                    }
                    
                    // 验证第2、3、4、5列是数字
                    std::stod(parts[1]);  // X
                    std::stod(parts[2]);  // Y
                    std::stod(parts[3]);  // Z
                    std::stod(parts[4]);  // Charge
                    
                    validLines++;
                } catch (const std::exception&) {
                    // 不是有效的数字
                    continue;
                }
            }
            
            // 只检查前几行
            if (totalNonEmptyLines >= 5) {
                break;
            }
        }
        
        // 至少有3行有效的CHG格式数据
        bool isChg = (validLines >= 3);
        if (isChg) {
            LOG_DEBUG("Detected CHG format");
        } else {
            LOG_DEBUG("Not recognized as CHG format");
        }
        return isChg;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in isChgFormat: " + std::string(e.what()));
        return false;
    }
}

// 读取单帧XYZ数据
bool readXYZFrame(const std::vector<std::string>& lines, size_t startLine, Frame& frame, size_t& nextStart) {
    if (startLine >= lines.size()) return false;
    
    try {
        int numAtoms = std::stoi(lines[startLine]);
        if (numAtoms <= 0) return false;
        
        frame.comment = (startLine + 1 < lines.size()) ? lines[startLine + 1] : "";
        
        // 解析优化信息
        frame.optInfo = parseOptimizationInfo(frame.comment);
        
        frame.atoms.clear();
        
        for (int i = 0; i < numAtoms; ++i) {
            size_t lineIndex = startLine + 2 + static_cast<size_t>(i);
            if (lineIndex >= lines.size()) break;
            
            std::vector<std::string> parts = splitWhitespace(lines[lineIndex]);
            int maxCol = std::max({g_config.elementColumn, g_config.xColumn, g_config.yColumn, g_config.zColumn});
            if (parts.size() >= static_cast<size_t>(maxCol)) {
                try {
                    Atom atom;
                    atom.symbol = parts[g_config.elementColumn - 1];
                    atom.x = std::stod(parts[g_config.xColumn - 1]);
                    atom.y = std::stod(parts[g_config.yColumn - 1]);
                    atom.z = std::stod(parts[g_config.zColumn - 1]);
                    frame.atoms.push_back(atom);
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to parse atom at line " + std::to_string(lineIndex) + ": " + std::string(e.what()));
                    continue;
                }
            }
        }
        
        nextStart = startLine + static_cast<size_t>(numAtoms) + 2;
        return !frame.atoms.empty();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in readXYZFrame: " + std::string(e.what()));
        return false;
    }
}

// 读取多帧XYZ数据
std::vector<Frame> readMultiXYZ(const std::string& content) {
    std::vector<Frame> frames;
    
    try {
        std::vector<std::string> lines = split(content, '\n');
        
        if (lines.empty()) {
            LOG_DEBUG("No lines to process");
            return frames;
        }
        
        try {
            std::stoi(lines[0]);
            // 标准格式
            LOG_DEBUG("Processing standard XYZ format");
            size_t lineIndex = 0;
            while (lineIndex < lines.size()) {
                Frame frame;
                size_t nextStart;
                if (readXYZFrame(lines, lineIndex, frame, nextStart)) {
                    frames.push_back(frame);
                    lineIndex = nextStart;
                } else {
                    LOG_WARNING("Failed to read frame starting at line: " + std::to_string(lineIndex));
                    break;
                }
            }
        } catch (const std::exception&) {
            // 简化格式：直接处理坐标行
            LOG_DEBUG("Processing simplified XYZ format");
            Frame frame;
            frame.comment = "Simplified XYZ format";
            
            for (const std::string& line : lines) {
                std::vector<std::string> parts = splitWhitespace(line);
                int maxCol = std::max({g_config.elementColumn, g_config.xColumn, g_config.yColumn, g_config.zColumn});
                if (parts.size() >= static_cast<size_t>(maxCol)) {
                    try {
                        Atom atom;
                        atom.symbol = parts[g_config.elementColumn - 1];
                        atom.x = std::stod(parts[g_config.xColumn - 1]);
                        atom.y = std::stod(parts[g_config.yColumn - 1]);
                        atom.z = std::stod(parts[g_config.zColumn - 1]);
                        frame.atoms.push_back(atom);
                    } catch (const std::exception& e) {
                        LOG_WARNING("Failed to parse simplified format line: " + std::string(e.what()));
                        continue;
                    }
                }
            }
            
            if (!frame.atoms.empty()) {
                frames.push_back(frame);
            }
        }
        
        LOG_INFO("Processed " + std::to_string(frames.size()) + " frames");
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in readMultiXYZ: " + std::string(e.what()));
    }
    
    return frames;
}

// 读取CHG格式数据
Frame readChgFrame(const std::string& content) {
    Frame frame;
    frame.comment = "CHG Format (Element X Y Z Charge)";
    
    try {
        std::vector<std::string> lines = split(content, '\n');
        
        if (lines.empty()) {
            LOG_DEBUG("No lines to process");
            return frame;
        }
        
        LOG_DEBUG("Processing CHG format");
        
        for (const std::string& line : lines) {
            std::string trimmedLine = trim(line);
            
            // 跳过空行和注释行
            if (trimmedLine.empty() || trimmedLine[0] == '#') {
                continue;
            }
            
            std::vector<std::string> parts = splitWhitespace(trimmedLine);
            // CHG格式：Element X Y Z Charge (至少5列)
            if (parts.size() >= 5) {
                try {
                    // 验证第一列是元素符号
                    if (parts[0].empty() || !std::isalpha(parts[0][0])) {
                        LOG_WARNING("Invalid element symbol in CHG line: " + trimmedLine);
                        continue;
                    }
                    
                    Atom atom;
                    atom.symbol = parts[0];
                    atom.x = std::stod(parts[1]);
                    atom.y = std::stod(parts[2]);
                    atom.z = std::stod(parts[3]);
                    atom.charge = std::stod(parts[4]);  // 第5列是电荷
                    
                    frame.atoms.push_back(atom);
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to parse CHG format line: " + trimmedLine + ", error: " + std::string(e.what()));
                    continue;
                }
            } else {
                LOG_WARNING("CHG line has insufficient columns: " + trimmedLine);
            }
        }
        
        if (frame.atoms.empty()) {
            LOG_WARNING("No valid atoms found in CHG format");
        } else {
            LOG_INFO("Parsed " + std::to_string(frame.atoms.size()) + " atoms from CHG format");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in readChgFrame: " + std::string(e.what()));
    }
    
    return frame;
}

// 解析Gaussian clipboard文件
std::vector<Atom> parseGaussianClipboard(const std::string& filename) {
    std::vector<Atom> atoms;
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open Gaussian clipboard file: " + filename);
            return atoms;
        }
        
        std::string line;
        
        // 跳过第一行（头部）
        if (!std::getline(file, line)) {
            LOG_ERROR("Empty file or cannot read header");
            return atoms;
        }
        
        // 读取原子数量
        if (!std::getline(file, line)) {
            LOG_ERROR("Cannot read number of atoms");
            return atoms;
        }
        
        int numAtoms;
        try {
            numAtoms = std::stoi(line);
            LOG_DEBUG("Expected number of atoms: " + std::to_string(numAtoms));
        } catch (const std::exception& e) {
            LOG_ERROR("Cannot parse number of atoms: " + line);
            return atoms;
        }
        
        // 读取原子数据
        for (int i = 0; i < numAtoms; i++) {
            if (!std::getline(file, line)) {
                LOG_WARNING("Expected " + std::to_string(numAtoms) + " atoms, but only found " + std::to_string(i));
                break;
            }
            
            std::istringstream iss(line);
            int atomicNumber;
            double x, y, z;
            std::string label;
            
            if (iss >> atomicNumber >> x >> y >> z) {
                // 可选的标签
                iss >> label;
                
                auto it = atomicNumberToSymbol.find(atomicNumber);
                if (it != atomicNumberToSymbol.end()) {
                    Atom atom;
                    atom.symbol = it->second;
                    atom.x = x;
                    atom.y = y;
                    atom.z = z;
                    atoms.push_back(atom);
                    
                    LOG_DEBUG("Added atom " + std::to_string(i + 1) + ": " + atom.symbol + 
                             " (" + std::to_string(atomicNumber) + ") at (" + 
                             std::to_string(atom.x) + ", " + std::to_string(atom.y) + ", " + std::to_string(atom.z) + ")");
                } else {
                    LOG_WARNING("Unknown atomic number " + std::to_string(atomicNumber) + " in line: " + line);
                }
            } else {
                LOG_WARNING("Cannot parse atom data in line: " + line);
            }
        }
        
        file.close();
        LOG_INFO("Parsed " + std::to_string(atoms.size()) + " atoms from Gaussian clipboard");
    } catch (const std::exception& e) {
        LOG_ERROR("Exception parsing Gaussian clipboard: " + std::string(e.what()));
    }
    
    return atoms;
}

// 创建XYZ字符串
std::string createXYZString(const std::vector<Atom>& atoms) {
    try {
        std::ostringstream oss;
        
        oss << atoms.size() << std::endl;
        oss << "Converted from Gaussian clipboard" << std::endl;
        
        for (const auto& atom : atoms) {
            oss << std::left << std::setw(2) << atom.symbol 
                << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.x
                << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.y
                << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.z
                << std::endl;
        }
        
        return oss.str();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception creating XYZ string: " + std::string(e.what()));
        return "";
    }
}

// 写入Gaussian LOG头部
std::string writeGaussianLogHeader() {
    return " ! This file was generated by XYZ Monitor\n"
           " \n"
           " 0 basis functions\n"
           " 0 alpha electrons\n"
           " 0 beta electrons\n"
           "GradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGrad\n";
}

// 写入Gaussian LOG几何结构部分
std::string writeGaussianLogGeometry(const Frame& frame, int frameNumber, const Frame* previousFrame) {
    std::ostringstream oss;
    
    oss << "GradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGrad\n";
    oss << " \n";
    oss << "                         Standard orientation:\n";
    oss << " ---------------------------------------------------------------------\n";
    oss << " Center     Atomic      Atomic             Coordinates (Angstroms)\n";
    oss << " Number     Number       Type             X           Y           Z\n";
    oss << " ---------------------------------------------------------------------\n";
    
    for (size_t i = 0; i < frame.atoms.size(); ++i) {
        const Atom& atom = frame.atoms[i];
        int atomicNum = getAtomicNumber(atom.symbol);
        
        oss << "      " << (i + 1) << "          " << atomicNum 
            << "           0        " << std::fixed << std::setprecision(6)
            << std::setw(10) << atom.x << "    "
            << std::setw(10) << atom.y << "    "
            << std::setw(10) << atom.z << "\n";
    }
    
    oss << " ---------------------------------------------------------------------\n";
    oss << " \n";
    
    // 写入能量
    if (frame.optInfo.hasData && frame.optInfo.energy != 0.0) {
        oss << " SCF Done:  " << std::fixed << std::setprecision(9) << frame.optInfo.energy << "\n";
    } else if (previousFrame && previousFrame->optInfo.hasData && previousFrame->optInfo.energy != 0.0) {
        oss << " SCF Done:  " << std::fixed << std::setprecision(9) << previousFrame->optInfo.energy << "\n";
    } else {
        oss << " SCF Done:      -100.000000000\n";
    }
    
    oss << " \n";
    oss << "GradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGrad\n";
    oss << " Step number   " << frameNumber << "\n";
    oss << "         Item               Value     Threshold  Converged?\n";
    
    // 阈值定义
    const double MAX_FORCE_THRESHOLD = 0.00045;
    const double RMS_FORCE_THRESHOLD = 0.00030;
    const double MAX_DISP_THRESHOLD = 0.00180;
    const double RMS_DISP_THRESHOLD = 0.00120;
    
    // 获取有效的优化信息（当前帧优先，如果没有则使用前一帧）
    OptimizationInfo effectiveOptInfo = frame.optInfo;
    if (!effectiveOptInfo.hasData && previousFrame && previousFrame->optInfo.hasData) {
        effectiveOptInfo = previousFrame->optInfo;
        LOG_DEBUG("Using previous frame optimization info for frame " + std::to_string(frameNumber));
    }
    
    // 写入最大受力
    if (effectiveOptInfo.hasData && effectiveOptInfo.maxForce >= 0.0) {
        bool converged = effectiveOptInfo.maxForce <= MAX_FORCE_THRESHOLD;
        oss << " Maximum Force            " << std::fixed << std::setprecision(6) 
            << std::setw(8) << effectiveOptInfo.maxForce 
            << "     " << std::setw(8) << MAX_FORCE_THRESHOLD 
            << "     " << (converged ? "YES" : " NO") << "\n";
    } else {
        oss << " Maximum Force            1.000000     " << std::setw(8) << MAX_FORCE_THRESHOLD << "     NO\n";
    }
    
    // 写入方均根受力
    if (effectiveOptInfo.hasData && effectiveOptInfo.rmsForce >= 0.0) {
        bool converged = effectiveOptInfo.rmsForce <= RMS_FORCE_THRESHOLD;
        oss << " RMS     Force            " << std::fixed << std::setprecision(6) 
            << std::setw(8) << effectiveOptInfo.rmsForce 
            << "     " << std::setw(8) << RMS_FORCE_THRESHOLD 
            << "     " << (converged ? "YES" : " NO") << "\n";
    } else {
        oss << " RMS     Force            1.000000     " << std::setw(8) << RMS_FORCE_THRESHOLD << "     NO\n";
    }
    
    // 写入最大位移
    if (effectiveOptInfo.hasData && effectiveOptInfo.maxDisp >= 0.0) {
        bool converged = effectiveOptInfo.maxDisp <= MAX_DISP_THRESHOLD;
        oss << " Maximum Displacement     " << std::fixed << std::setprecision(6) 
            << std::setw(8) << effectiveOptInfo.maxDisp 
            << "     " << std::setw(8) << MAX_DISP_THRESHOLD 
            << "     " << (converged ? "YES" : " NO") << "\n";
    } else {
        oss << " Maximum Displacement     1.000000     " << std::setw(8) << MAX_DISP_THRESHOLD << "     NO\n";
    }
    
    // 写入方均根位移
    if (effectiveOptInfo.hasData && effectiveOptInfo.rmsDisp >= 0.0) {
        bool converged = effectiveOptInfo.rmsDisp <= RMS_DISP_THRESHOLD;
        oss << " RMS     Displacement     " << std::fixed << std::setprecision(6) 
            << std::setw(8) << effectiveOptInfo.rmsDisp 
            << "     " << std::setw(8) << RMS_DISP_THRESHOLD 
            << "     " << (converged ? "YES" : " NO") << "\n";
    } else {
        oss << " RMS     Displacement     1.000000     " << std::setw(8) << RMS_DISP_THRESHOLD << "     NO\n";
    }
    oss << "GradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGradGrad\n";
    // 检查是否有电荷数据（任意一个原子的charge不为0）
    bool hasChargeData = false;
    for (const auto& atom : frame.atoms) {
        if (atom.charge != 0.0) {
            hasChargeData = true;
            break;
        }
    }
    
    // 如果有电荷数据，写入Mulliken charges部分
    if (hasChargeData) {
        
        oss << " \n";
        oss << "          Condensed to atoms (all electrons):\n";
        oss << " Mulliken charges and spin densities:\n";
        oss << "               1          2\n";
        
        for (size_t i = 0; i < frame.atoms.size(); ++i) {
            const Atom& atom = frame.atoms[i];
            oss << "     " << std::setw(2) << (i + 1) << "  " 
                << std::setw(2) << std::left << atom.symbol << std::right << "   "
                << std::fixed << std::setprecision(6) << std::setw(8) << atom.charge 
                << "  " << std::setw(8) << 0.0 << "\n";  // spin density设为0
        }
        
        // 计算电荷总和
        double totalCharge = 0.0;
        for (const auto& atom : frame.atoms) {
            totalCharge += atom.charge;
        }
        
        oss << "\n Sum of Mulliken charges =  " << std::fixed << std::setprecision(5) 
            << std::setw(8) << totalCharge << "   " << std::setw(8) << 0.0 << "\n";
    }
    
    return oss.str();
}

// 写入Gaussian LOG尾部
std::string writeGaussianLogFooter() {
    return ""
           " Normal termination of Gaussian\n";
}

// 转换为Gaussian LOG格式
std::string convertToGaussianLog(const std::vector<Frame>& frames) {
    if (frames.empty()) {
        LOG_ERROR("No frames to convert");
        return "";
    }
    
    try {
        std::ostringstream oss;
        
        oss << writeGaussianLogHeader();
        
        for (size_t i = 0; i < frames.size(); ++i) {
            const Frame* previousFrame = (i > 0) ? &frames[i - 1] : nullptr;
            oss << writeGaussianLogGeometry(frames[i], static_cast<int>(i + 1), previousFrame);
        }
        
        oss << writeGaussianLogFooter();
        
        LOG_DEBUG("Converted " + std::to_string(frames.size()) + " frames to Gaussian log format");
        return oss.str();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in convertToGaussianLog: " + std::string(e.what()));
        return "";
    }
}