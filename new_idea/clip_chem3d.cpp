#include <windows.h>
#include <iostream>
#include <fstream>
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
        std::cout << "ERROR: Cannot open clipboard" << std::endl;
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
        std::cout << "ERROR: No Chem3D format found in clipboard" << std::endl;
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
            
            std::cout << "DEBUG: Processing atom block (first 100 chars): " 
                      << atomBlock.substr(0, 100) << "..." << std::endl;
            std::cout << "DEBUG: Found symbol='" << symbol << "', coords='" << coords << "'" << std::endl;
            
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
                        std::cout << "SUCCESS: Added " << atom.symbol << " at (" 
                                  << atom.x << ", " << atom.y << ", " << atom.z << ")" << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "WARNING: Cannot parse coordinates: " << coords << std::endl;
                    }
                } else {
                    std::cout << "WARNING: Expected 3 coordinates, got " << coordTokens.size() << std::endl;
                }
            } else {
                std::cout << "WARNING: Missing symbol or coordinates" << std::endl;
            }
            
            pos = endPos;
        } else {
            break;
        }
    }
    
    return atoms;
}

bool saveXYZ(const std::vector<Atom>& atoms, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "ERROR: Cannot create file: " << filename << std::endl;
        return false;
    }
    
    file << atoms.size() << std::endl;
    
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    file << "Converted from Chem3D - " 
         << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    
    for (const auto& atom : atoms) {
        file << std::left << std::setw(2) << atom.symbol 
             << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.x
             << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.y  
             << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.z
             << std::endl;
    }
    
    file.close();
    return true;
}

int main() {
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "    Chem3D XML to XYZ Converter" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Reading clipboard..." << std::endl;
    
    std::string xmlData = getChem3DFromClipboard();
    
    if (xmlData.empty()) {
        std::cout << "ERROR: No Chem3D data found" << std::endl;
        std::cout << "Please copy a molecule from Chem3D first" << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
    
    std::cout << "SUCCESS: Found Chem3D XML data" << std::endl;
    std::cout << "Data size: " << xmlData.length() << " characters" << std::endl;
    
    std::vector<Atom> atoms = parseXML(xmlData);
    
    if (atoms.empty()) {
        std::cout << "ERROR: No atoms found in data" << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
    
    std::cout << "SUCCESS: Parsed " << atoms.size() << " atoms" << std::endl;
    
    std::string filename = "molecule_" + getCurrentTime() + ".xyz";
    
    if (saveXYZ(atoms, filename)) {
        std::cout << "SUCCESS: Saved as " << filename << std::endl;
        
        std::cout << "\nXYZ File Preview:" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        std::cout << atoms.size() << std::endl;
        std::cout << "Converted from Chem3D" << std::endl;
        
        for (size_t i = 0; i < std::min(atoms.size(), (size_t)10); ++i) {
            const auto& atom = atoms[i];
            std::cout << std::left << std::setw(2) << atom.symbol 
                     << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.x
                     << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.y
                     << " " << std::right << std::setw(12) << std::fixed << std::setprecision(6) << atom.z
                     << std::endl;
        }
        
        if (atoms.size() > 10) {
            std::cout << "... (" << (atoms.size() - 10) << " more atoms)" << std::endl;
        }
        
        std::cout << std::string(50, '-') << std::endl;
    } else {
        std::cout << "ERROR: Failed to save file" << std::endl;
    }
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}