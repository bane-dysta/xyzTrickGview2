// x86_64-w64-mingw32-g++ -o clip_chem3d.exe clipchem3d_silent.cpp -mwindows -Wl,--subsystem,windows:6.0 -Wl,--entry,WinMainCRTStartup -static -static-libgcc -static-libstdc++ -O2 -s

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>

struct Atom {
    std::string symbol;
    double x, y, z;
};

std::string getCurrentTime() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string getChem3DFromClipboard() {
    if (!OpenClipboard(NULL)) {
        return "";
    }
    
    std::string xmlData;
    UINT format = 0;
    UINT chem3dFormatId = 0;
    
    while ((format = EnumClipboardFormats(format)) != 0) {
        char formatName[256] = {0};
        if (GetClipboardFormatNameA(format, formatName, sizeof(formatName))) {
            if (std::string(formatName) == "Chem3D") {
                chem3dFormatId = format;
                break;
            }
        }
    }
    
    if (chem3dFormatId == 0) {
        CloseClipboard();
        return "";
    }
    
    if (IsClipboardFormatAvailable(chem3dFormatId)) {
        HANDLE hData = GetClipboardData(chem3dFormatId);
        if (hData != NULL) {
            void* pData = GlobalLock(hData);
            if (pData != NULL) {
                SIZE_T dataSize = GlobalSize(hData);
                xmlData = std::string(static_cast<const char*>(pData), dataSize);
                GlobalUnlock(hData);
            }
        }
    }
    
    CloseClipboard();
    return xmlData;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string extractAttribute(const std::string& line, const std::string& attribute) {
    size_t pos = line.find(attribute + "=\"");
    if (pos == std::string::npos) return "";
    
    pos += attribute.length() + 2;
    size_t end = line.find("\"", pos);
    if (end == std::string::npos) return "";
    
    return line.substr(pos, end - pos);
}

std::vector<Atom> parseXML(const std::string& xmlData) {
    std::vector<Atom> atoms;
    std::string cleanXml = xmlData;
    
    // Remove line breaks within tags to make parsing easier
    size_t pos = 0;
    while ((pos = cleanXml.find('\n', pos)) != std::string::npos) {
        cleanXml[pos] = ' ';
    }
    while ((pos = cleanXml.find('\r', pos)) != std::string::npos) {
        cleanXml[pos] = ' ';
    }
    
    // Find all atom tags
    pos = 0;
    while ((pos = cleanXml.find("<atom", pos)) != std::string::npos) {
        size_t endPos = cleanXml.find("/>", pos);
        if (endPos == std::string::npos) {
            endPos = cleanXml.find("</atom>", pos);
            if (endPos != std::string::npos) {
                endPos += 7; // Include </atom>
            }
        } else {
            endPos += 2; // Include />
        }
        
        if (endPos != std::string::npos) {
            std::string atomBlock = cleanXml.substr(pos, endPos - pos);
            
            // Extract attributes
            std::string symbol = extractAttribute(atomBlock, "symbol");
            std::string coords = extractAttribute(atomBlock, "cartCoords");
            
            if (!symbol.empty() && !coords.empty()) {
                std::vector<std::string> coordTokens = split(coords, ' ');
                if (coordTokens.size() >= 3) {
                    try {
                        Atom atom;
                        atom.symbol = symbol;
                        atom.x = std::stod(coordTokens[0]);
                        atom.y = std::stod(coordTokens[1]);
                        atom.z = std::stod(coordTokens[2]);
                        atoms.push_back(atom);
                    } catch (const std::exception& e) {
                        // Silently skip invalid coordinates
                    }
                }
            }
            
            pos = endPos;
        } else {
            break;
        }
    }
    
    return atoms;
}

std::string generateXYZ(const std::vector<Atom>& atoms) {
    std::ostringstream oss;
    
    oss << atoms.size() << std::endl;
    
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    oss << "Converted from Chem3D - " 
        << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    
    for (const auto& atom : atoms) {
        oss << std::left << std::setw(2) << atom.symbol 
            << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.x
            << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.y  
            << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.z
            << std::endl;
    }
    
    return oss.str();
}

bool copyTextToClipboard(const std::string& text) {
    if (!OpenClipboard(NULL)) {
        return false;
    }
    
    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
    if (hMem == NULL) {
        CloseClipboard();
        return false;
    }
    
    void* pMem = GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }
    
    memcpy(pMem, text.c_str(), text.length() + 1);
    GlobalUnlock(hMem);
    
    if (SetClipboardData(CF_TEXT, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }
    
    CloseClipboard();
    return true;
}

// 使用 WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string xmlData = getChem3DFromClipboard();
    
    if (xmlData.empty()) {
        return 1;
    }
    
    std::vector<Atom> atoms = parseXML(xmlData);
    
    if (atoms.empty()) {
        return 1;
    }
    
    std::string xyzText = generateXYZ(atoms);
    
    if (!copyTextToClipboard(xyzText)) {
        return 1;
    }
    
    return 0;
}