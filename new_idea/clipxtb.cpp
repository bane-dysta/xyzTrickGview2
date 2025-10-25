// Compilation command (using MinGW):
// x86_64-w64-mingw32-g++ clipxtb.cpp -o clipxtb.exe -luser32 -lkernel32 -lcomctl32 -lole32 -lgdi32 -lshell32 -static-libgcc -static-libstdc++ -std=c++17

#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <process.h>
#include <commctrl.h>
#include <random>
#include <map>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Configuration structure
struct Config {
    std::string xtb_execpath;
    std::string tmp_path;
    int gfn_type;
    std::string extra_flag;
    
    Config() : gfn_type(2) {}  // Default values
};

// Simple INI parser
class INIParser {
public:
    static Config parseINI(const std::string& filename) {
        Config config;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open config file: " << filename << std::endl;
            std::cerr << "Using default settings." << std::endl;
            return config;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Remove leading/trailing whitespace
            line = trim(line);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Find '=' separator
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            
            // Remove quotes from value if present
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            // Parse key-value pairs
            if (key == "xtb_execpath") {
                config.xtb_execpath = value;
            } else if (key == "tmp_path") {
                config.tmp_path = value;
            } else if (key == "gfn_type") {
                try {
                    config.gfn_type = std::stoi(value);
                } catch (...) {
                    std::cerr << "Warning: Invalid gfn_type value, using default (2)" << std::endl;
                }
            } else if (key == "extra_flag") {
                config.extra_flag = value;
            }
        }
        
        file.close();
        return config;
    }
    
private:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }
};

class XTBOptimizer {
private:
    HWND hOutputWindow;
    HWND hOutputEdit;
    std::string tempDir;
    std::string workDir;
    Config config;
    
    // Get executable directory
    std::string getExecutableDir() {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string path(exePath);
        size_t pos = path.find_last_of("\\/");
        return (pos != std::string::npos) ? path.substr(0, pos + 1) : "";
    }
    
    // Load configuration from INI file
    void loadConfig() {
        std::string exeDir = getExecutableDir();
        std::string iniPath = exeDir + "xtbclip.ini";
        
        std::cout << "Looking for config file: " << iniPath << std::endl;
        config = INIParser::parseINI(iniPath);
        
        // Display loaded configuration
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  XTB executable: " << (config.xtb_execpath.empty() ? "xtb (from PATH)" : config.xtb_execpath) << std::endl;
        std::cout << "  Temp path: " << (config.tmp_path.empty() ? "System temp" : config.tmp_path) << std::endl;
        std::cout << "  GFN type: " << config.gfn_type << std::endl;
        std::cout << "  Extra flags: " << (config.extra_flag.empty() ? "(none)" : config.extra_flag) << std::endl;
        std::cout << std::endl;
    }
    
    // Verify XTB executable exists
    bool verifyXTBExecutable() {
        if (config.xtb_execpath.empty()) {
            // Will use xtb from PATH, cannot easily verify
            std::cout << "Note: Using 'xtb' from system PATH. Make sure it's installed and accessible." << std::endl;
            return true;
        }
        
        DWORD attributes = GetFileAttributesA(config.xtb_execpath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "\nERROR: XTB executable not found at: " << config.xtb_execpath << std::endl;
            std::cerr << "Please check the path in xtbclip.ini or leave xtb_execpath empty to use system PATH." << std::endl;
            return false;
        }
        
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::cerr << "\nERROR: xtb_execpath points to a directory, not a file: " << config.xtb_execpath << std::endl;
            return false;
        }
        
        std::cout << "XTB executable verified: OK" << std::endl;
        return true;
    }
    
    // Get temporary directory
    std::string getTempDir() {
        // Use configured temp path if available
        if (!config.tmp_path.empty()) {
            // Ensure path ends with backslash
            std::string path = config.tmp_path;
            if (path.back() != '\\' && path.back() != '/') {
                path += "\\";
            }
            
            // Verify directory exists
            DWORD attributes = GetFileAttributesA(path.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return path;
            } else {
                std::cerr << "Warning: Configured tmp_path does not exist: " << path << std::endl;
                std::cerr << "Falling back to system temp directory." << std::endl;
            }
        }
        
        // Fallback to system temp
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
    
    // NEW: Read file content from disk
    std::string readFileContent(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << filepath << std::endl;
            return "";
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        return content;
    }
    
    // NEW: Check if clipboard contains files and read XYZ file
    std::string getClipboardFile() {
        if (!OpenClipboard(nullptr)) {
            return "";
        }
        
        std::string result;
        
        // Check if clipboard contains files (CF_HDROP format)
        if (IsClipboardFormatAvailable(CF_HDROP)) {
            HANDLE hDrop = GetClipboardData(CF_HDROP);
            if (hDrop != nullptr) {
                // Get the number of files
                UINT fileCount = DragQueryFileA((HDROP)hDrop, 0xFFFFFFFF, nullptr, 0);
                
                if (fileCount > 0) {
                    // Get the first file path
                    char filepath[MAX_PATH];
                    if (DragQueryFileA((HDROP)hDrop, 0, filepath, MAX_PATH) > 0) {
                        std::string filepathStr(filepath);
                        
                        // Check if it's an .xyz file
                        std::string extension;
                        size_t dotPos = filepathStr.find_last_of('.');
                        if (dotPos != std::string::npos) {
                            extension = filepathStr.substr(dotPos);
                            // Convert to lowercase for comparison
                            for (auto& c : extension) {
                                c = tolower(c);
                            }
                        }
                        
                        if (extension == ".xyz") {
                            std::cout << "Found XYZ file in clipboard: " << filepathStr << std::endl;
                            result = readFileContent(filepathStr);
                        } else if (fileCount > 1) {
                            std::cerr << "Warning: Multiple files detected. Only the first .xyz file will be used." << std::endl;
                        } else {
                            std::cerr << "Warning: File is not an XYZ file: " << filepathStr << std::endl;
                            std::cerr << "Please copy an .xyz file or XYZ text content." << std::endl;
                        }
                    }
                }
            }
        }
        
        CloseClipboard();
        return result;
    }
    
    // MODIFIED: Read text from clipboard (original function)
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
    
    // NEW: Smart clipboard reader - tries file first, then text
    std::string getClipboardContent() {
        // First, try to read as file
        std::string content = getClipboardFile();
        
        if (!content.empty()) {
            std::cout << "Read XYZ data from clipboard file." << std::endl;
            return content;
        }
        
        // If no file, try to read as text
        content = getClipboardText();
        
        if (!content.empty()) {
            std::cout << "Read XYZ data from clipboard text." << std::endl;
            return content;
        }
        
        return "";
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
    
    // Get charge and spin from user
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
    
    // Run XTB with specified parameters
    bool runXTB(const std::string& xyzFile, int charge, int spin) {
        // Build command
        std::string xtbExec = config.xtb_execpath.empty() ? "xtb" : config.xtb_execpath;
        std::string gfnFlag = "--gfn " + std::to_string(config.gfn_type);
        std::string chargeFlag = "--chrg " + std::to_string(charge);
        std::string spinFlag = "--uhf " + std::to_string(spin - 1);
        
        std::string command = "\"" + xtbExec + "\" \"" + xyzFile + "\" --opt " + 
                             gfnFlag + " " + chargeFlag + " " + spinFlag;
        
        if (!config.extra_flag.empty()) {
            command += " " + config.extra_flag;
        }
        
        std::cout << "\nRunning XTB optimization..." << std::endl;
        std::cout << "Command: " << command << std::endl;
        std::cout << "============================" << std::endl;
        
        // Create log file path
        std::string logFile = workDir + "xtb_output.log";
        std::ofstream logFileStream(logFile);
        
        // Setup process
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;
        
        // Create pipes for stdout and stderr
        HANDLE hStdOutRead, hStdOutWrite;
        HANDLE hStdErrRead, hStdErrWrite;
        
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
            !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
            std::cerr << "Error: Cannot create pipes!" << std::endl;
            return false;
        }
        
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
        
        // Setup startup info
        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdErrWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        // Process info
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));
        
        // Set working directory
        const char* workingDir = workDir.c_str();
        
        // Create process
        if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr,
                           TRUE, CREATE_NO_WINDOW, nullptr, workingDir, &si, &pi)) {
            std::cerr << "Error: Cannot start XTB process!" << std::endl;
            std::cerr << "Make sure XTB is installed and accessible." << std::endl;
            CloseHandle(hStdOutRead);
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdErrRead);
            CloseHandle(hStdErrWrite);
            return false;
        }
        
        // Close write ends
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);
        
        // Read output
        const int BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];
        DWORD bytesRead;
        
        bool processRunning = true;
        while (processRunning) {
            // Check if process is still running
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            processRunning = (exitCode == STILL_ACTIVE);
            
            // Read stdout
            while (PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &bytesRead, nullptr) && bytesRead > 0) {
                if (ReadFile(hStdOutRead, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << buffer;
                    std::cout.flush();
                    
                    // Write to log file
                    if (logFileStream.is_open()) {
                        logFileStream << buffer;
                        logFileStream.flush();
                    }
                }
            }
            
            // Read stderr
            while (PeekNamedPipe(hStdErrRead, nullptr, 0, nullptr, &bytesRead, nullptr) && bytesRead > 0) {
                if (ReadFile(hStdErrRead, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::cerr << buffer;
                    std::cerr.flush();
                    
                    // Write to log file
                    if (logFileStream.is_open()) {
                        logFileStream << buffer;
                        logFileStream.flush();
                    }
                }
            }
            
            // Small delay to prevent CPU spinning
            if (processRunning) {
                Sleep(50);
            }
        }
        
        // Read any remaining output
        while (ReadFile(hStdOutRead, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << buffer;
            if (logFileStream.is_open()) {
                logFileStream << buffer;
            }
        }
        
        while (ReadFile(hStdErrRead, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cerr << buffer;
            if (logFileStream.is_open()) {
                logFileStream << buffer;
            }
        }
        
        // Close log file
        if (logFileStream.is_open()) {
            logFileStream.close();
        }
        
        std::cout << "============================" << std::endl << std::endl;
        
        // Get exit code
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        // Cleanup
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return exitCode == 0;
    }
    
    // Add charge and spin to XYZ format (Gaussian style)
    std::string addChargeSpinToXYZ(const std::string& xyzContent, int charge, int spin) {
        std::istringstream iss(xyzContent);
        std::string line;
        std::vector<std::string> lines;
        
        // Read all lines
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        if (lines.size() < 2) {
            std::cerr << "Error: Invalid XYZ format!" << std::endl;
            return xyzContent;
        }
        
        // Construct new XYZ with charge and spin in second line
        std::ostringstream result;
        result << lines[0] << "\n";  // First line (number of atoms)
        result << charge << " " << spin << "\n";  // Second line (charge spin)
        
        // Add coordinate lines (skip original second line which is comment/empty)
        for (size_t i = 2; i < lines.size(); i++) {
            if (!lines[i].empty()) {  // Skip empty lines
                result << lines[i] << "\n";
            }
        }
        
        return result.str();
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
        loadConfig();  // Load configuration first
        workDir = createWorkingDirectory();
    }
    
    ~XTBOptimizer() {
        finalCleanup();
    }
    
    // Main processing function - Modified to use new clipboard reader
    bool process() {
        if (workDir.empty()) {
            std::cerr << "Error: Cannot create working directory!" << std::endl;
            return false;
        }
        
        // Verify XTB executable before proceeding
        if (!verifyXTBExecutable()) {
            return false;
        }
        
        std::cout << "XTB Clipboard Optimizer" << std::endl;
        std::cout << "======================" << std::endl;
        std::cout << "Working directory: " << workDir << std::endl;
        
        // Clean working directory before starting
        cleanupWorkingDirectory();
        
        // Read clipboard content (file or text) - MODIFIED LINE
        std::string clipboardContent = getClipboardContent();
        if (clipboardContent.empty()) {
            std::cerr << "Error: Clipboard is empty or cannot be read!" << std::endl;
            std::cerr << "Please copy either:" << std::endl;
            std::cerr << "  1. An .xyz file from Windows Explorer, or" << std::endl;
            std::cerr << "  2. XYZ text content from a text editor" << std::endl;
            return false;
        }
        
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
        
        // Add charge and spin information to second line (Gaussian style)
        std::string formattedContent = addChargeSpinToXYZ(optimizedContent, charge, spin);
        
        // Write results back to clipboard
        if (setClipboardText(formattedContent)) {
            std::cout << "SUCCESS: Optimization completed!" << std::endl;
            std::cout << "Optimized structure (with charge " << charge << " and spin " << spin << ") has been copied to clipboard." << std::endl;
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