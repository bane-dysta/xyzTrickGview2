#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <process.h>
#include <commctrl.h>
#include <random>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "comctl32.lib")

class XTBOptimizer {
private:
    HWND hOutputWindow;
    HWND hOutputEdit;
    std::string tempDir;
    std::string workDir;
    
    // Get temporary directory
    std::string getTempDir() {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        return std::string(tempPath);
    }
    
    // Create/ensure working directory exists - Modified to use fixed name
    std::string createWorkingDirectory() {
        std::string baseDir = getTempDir();
        std::string dirName = "xtb_clipboard_work\\";  // Fixed directory name
        std::string fullPath = baseDir + dirName;
        
        // Create directory if it doesn't exist (will fail silently if it already exists)
        CreateDirectoryA(fullPath.c_str(), nullptr);
        
        // Verify directory exists
        DWORD attributes = GetFileAttributesA(fullPath.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            return fullPath;
        }
        
        return "";
    }
    
    // Clean up working directory contents - Modified to be more thorough
    void cleanupWorkingDirectory() {
        if (workDir.empty()) {
            return;
        }
        
        std::cout << "Cleaning working directory..." << std::endl;
        
        // Remove files in the directory
        WIN32_FIND_DATAA findFileData;
        HANDLE hFind = FindFirstFileA((workDir + "*").c_str(), &findFileData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(findFileData.cFileName, ".") != 0 && 
                    strcmp(findFileData.cFileName, "..") != 0) {
                    std::string filePath = workDir + findFileData.cFileName;
                    
                    // Set file attributes to normal to ensure deletion
                    SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
                    
                    if (!DeleteFileA(filePath.c_str())) {
                        std::cout << "Warning: Could not delete file: " << filePath << std::endl;
                    }
                }
            } while (FindNextFileA(hFind, &findFileData));
            FindClose(hFind);
        }
        
        std::cout << "Working directory cleaned." << std::endl;
    }
    
    // Extract pure XYZ and get charge/spin
    std::string extractPureXYZ(const std::string& content, int& charge, int& spin) {
        std::istringstream iss(content);
        std::string line;
        std::vector<std::string> lines;
        
        // Read all lines
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        if (lines.size() < 2) {
            std::cerr << "Error: Invalid XYZ format - too few lines!" << std::endl;
            return "";
        }
        
        // Check if second line contains charge and spin (GView style)
        if (isGViewStyle(lines[1])) {
            std::istringstream secondLine(lines[1]);
            secondLine >> charge >> spin;
            
            std::cout << "Detected charge: " << charge << ", spin: " << spin << std::endl;
            
            // Return pure XYZ (first line + coordinates)
            std::string result = lines[0] + "\n\n";  // Number of atoms + empty comment line
            for (size_t i = 2; i < lines.size(); i++) {
                result += lines[i] + "\n";
            }
            return result;
        } else {
            // Manual input required
            std::cout << "XYZ format detected without charge/spin information." << std::endl;
            if (!getChargeAndSpin(charge, spin)) {
                return "";
            }
            
            std::cout << "Using charge: " << charge << ", spin: " << spin << std::endl;
            
            // Return original content (assuming it's already pure XYZ)
            return content;
        }
    }
    
    // Read text from clipboard
    std::string getClipboardText() {
        if (!OpenClipboard(nullptr)) {
            return "";
        }
        
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == nullptr) {
            CloseClipboard();
            return "";
        }
        
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText == nullptr) {
            CloseClipboard();
            return "";
        }
        
        std::string text(pszText);
        GlobalUnlock(hData);
        CloseClipboard();
        
        return text;
    }
    
    // Write text to clipboard
    bool setClipboardText(const std::string& text) {
        if (!OpenClipboard(nullptr)) {
            return false;
        }
        
        EmptyClipboard();
        
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, text.size() + 1);
        if (!hClipboardData) {
            CloseClipboard();
            return false;
        }
        
        char* pchData = (char*)GlobalLock(hClipboardData);
        strcpy(pchData, text.c_str());
        GlobalUnlock(hClipboardData);
        
        SetClipboardData(CF_TEXT, hClipboardData);
        CloseClipboard();
        
        return true;
    }
    
    // Check if second line is gview style (charge spin)
    bool isGViewStyle(const std::string& secondLine) {
        std::istringstream iss(secondLine);
        std::string token1, token2;
        
        if (!(iss >> token1 >> token2)) {
            return false;
        }
        
        // Check if there are only two numbers
        try {
            std::stoi(token1); // charge
            std::stoi(token2); // spin
            
            // Check if there's any remaining content
            std::string remaining;
            std::getline(iss, remaining);
            
            // Remove whitespace
            remaining.erase(0, remaining.find_first_not_of(" \t"));
            remaining.erase(remaining.find_last_not_of(" \t") + 1);
            
            return remaining.empty();
        } catch (...) {
            return false;
        }
    }
    
    // Get charge and spin input
    bool getChargeAndSpin(int& charge, int& spin) {
        std::cout << "Please enter charge (default 0): ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) {
            charge = 0;
        } else {
            try {
                charge = std::stoi(input);
            } catch (...) {
                std::cerr << "Invalid charge value!" << std::endl;
                return false;
            }
        }
        
        std::cout << "Please enter spin multiplicity (default 1): ";
        std::getline(std::cin, input);
        
        if (input.empty()) {
            spin = 1;
        } else {
            try {
                spin = std::stoi(input);
            } catch (...) {
                std::cerr << "Invalid spin value!" << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    // Run XTB optimization
    bool runXTB(const std::string& xyzFile, int charge, int spin) {
        std::string logFile = workDir + "xtb.log";
        std::string command = "cd /d \"" + workDir + "\" && xtb temp_structure.xyz --gfn2 --uhf " + 
                             std::to_string(spin - 1) + " --chrg " + std::to_string(charge) + 
                             " --opt > xtb.log 2>&1";
        
        std::cout << "Running XTB optimization..." << std::endl;
        std::cout << "Command: xtb --gfn2 --uhf " << (spin - 1) << " --chrg " << charge << " --opt" << std::endl;
        std::cout << "Working directory: " << workDir << std::endl;
        
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {};
        
        char cmdLine[1024];
        strcpy(cmdLine, ("cmd /c " + command).c_str());
        
        if (!CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            std::cerr << "Error: Unable to start XTB process!" << std::endl;
            return false;
        }
        
        // Wait for process completion
        std::cout << "Processing";
        DWORD result;
        int dots = 0;
        do {
            result = WaitForSingleObject(pi.hProcess, 1000);  // Wait 1 second
            if (result == WAIT_TIMEOUT) {
                std::cout << ".";
                if (++dots % 10 == 0) std::cout << std::endl << "Still processing";
                std::cout.flush();
            }
        } while (result == WAIT_TIMEOUT);
        
        std::cout << std::endl;
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Show log file content
        std::ifstream logFileStream(logFile);
        if (logFileStream.is_open()) {
            std::cout << "\n=== XTB Output ===" << std::endl;
            std::string line;
            
            // Show full output
            while (std::getline(logFileStream, line)) {
                std::cout << line << std::endl;
            }
            
            logFileStream.close();
            std::cout << "==================" << std::endl << std::endl;
        }
        
        return exitCode == 0;
    }
    
    // Final cleanup (remove directory) - only called in destructor now
    void finalCleanup() {
        if (!workDir.empty()) {
            std::cout << "Removing temporary directory..." << std::endl;
            
            // Clean files first
            cleanupWorkingDirectory();
            
            // Remove directory
            if (!RemoveDirectoryA(workDir.c_str())) {
                std::cout << "Warning: Could not remove temporary directory: " << workDir << std::endl;
            }
        }
    }
    
public:
    XTBOptimizer() {
        workDir = createWorkingDirectory();
    }
    
    ~XTBOptimizer() {
        finalCleanup();
    }
    
    // Main processing function - Modified to clean before processing
    bool process() {
        if (workDir.empty()) {
            std::cerr << "Error: Cannot create working directory!" << std::endl;
            return false;
        }
        
        std::cout << "XTB Clipboard Optimizer" << std::endl;
        std::cout << "======================" << std::endl;
        std::cout << "Working directory: " << workDir << std::endl;
        
        // Clean working directory before starting
        cleanupWorkingDirectory();
        
        // Read clipboard content
        std::string clipboardContent = getClipboardText();
        if (clipboardContent.empty()) {
            std::cerr << "Error: Clipboard is empty or cannot be read!" << std::endl;
            return false;
        }
        
        std::cout << "Read XYZ data from clipboard." << std::endl;
        
        // Extract pure XYZ and get charge/spin
        int charge, spin;
        std::string pureXYZ = extractPureXYZ(clipboardContent, charge, spin);
        if (pureXYZ.empty()) {
            return false;
        }
        
        // Write to temporary XYZ file
        std::string xyzFile = workDir + "temp_structure.xyz";
        std::ofstream outFile(xyzFile);
        if (!outFile.is_open()) {
            std::cerr << "Error: Cannot create temporary XYZ file!" << std::endl;
            return false;
        }
        outFile << pureXYZ;
        outFile.close();
        
        std::cout << "Temporary XYZ file created." << std::endl;
        
        // Run XTB optimization
        if (!runXTB(xyzFile, charge, spin)) {
            std::cerr << "Error: XTB optimization failed!" << std::endl;
            return false;
        }
        
        // Look for optimized structure file
        std::vector<std::string> possibleFiles = {
            workDir + "xtbopt.xyz",
            workDir + "temp_structure_optimized.xyz"
        };
        
        std::string optimizedFile;
        for (const auto& file : possibleFiles) {
            if (GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
                optimizedFile = file;
                break;
            }
        }
        
        if (optimizedFile.empty()) {
            std::cerr << "Error: Cannot find optimized structure file!" << std::endl;
            return false;
        }
        
        // Read optimization results
        std::ifstream resultFile(optimizedFile);
        if (!resultFile.is_open()) {
            std::cerr << "Error: Cannot read optimization results!" << std::endl;
            return false;
        }
        
        std::string optimizedContent((std::istreambuf_iterator<char>(resultFile)),
                                   std::istreambuf_iterator<char>());
        resultFile.close();
        
        // Write results back to clipboard
        if (setClipboardText(optimizedContent)) {
            std::cout << "SUCCESS: Optimization completed!" << std::endl;
            std::cout << "Optimized structure has been copied to clipboard." << std::endl;
        } else {
            std::cout << "WARNING: Optimization completed, but cannot write to clipboard!" << std::endl;
            std::cout << "Optimized structure file: " << optimizedFile << std::endl;
        }
        
        return true;
    }
};

// Main function
int main() {
    // Set console to UTF-8 for better compatibility
    SetConsoleOutputCP(CP_UTF8);
    
    XTBOptimizer optimizer;
    
    bool success = optimizer.process();
    
    if (!success) {
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}

// Compilation command (using MinGW):
// x86_64-w64-mingw32-g++ clipxtb.cpp -o clipxtb.exe -luser32 -lkernel32 -lcomctl32 -lole32 -lgdi32 -static-libgcc -static-libstdc++ -std=c++17