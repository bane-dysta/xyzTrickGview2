#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

// 原子数据结构
struct Atom {
    string element;
    double x, y, z;
    double charge;
};

// 读取chg文件
vector<Atom> readChgFile(const string& filename) {
    vector<Atom> atoms;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "错误: 无法打开文件 " << filename << endl;
        return atoms;
    }
    
    string line;
    while (getline(file, line)) {
        // 跳过空行
        if (line.empty() || line.find_first_not_of(" \t\n\r") == string::npos) {
            continue;
        }
        
        Atom atom;
        istringstream iss(line);
        
        if (iss >> atom.element >> atom.x >> atom.y >> atom.z >> atom.charge) {
            atoms.push_back(atom);
        }
    }
    
    file.close();
    return atoms;
}

// 写入chg文件
bool writeChgFile(const string& filename, const vector<Atom>& atoms) {
    ofstream file(filename);
    
    if (!file.is_open()) {
        cerr << "错误: 无法创建文件 " << filename << endl;
        return false;
    }
    
    // 设置输出格式
    file << fixed << setprecision(6);
    
    for (const auto& atom : atoms) {
        file << left << setw(2) << atom.element << " "
             << right << setw(12) << atom.x << " "
             << setw(12) << atom.y << " "
             << setw(12) << atom.z << " "
             << setw(14) << setprecision(10) << atom.charge << endl;
    }
    
    file.close();
    return true;
}

int main(int argc, char* argv[]) {
    string file1, file2, outputFile;
    
    // 如果没有命令行参数，交互式输入
    if (argc < 4) {
        cout << "=== CHG文件电荷差值计算程序 ===" << endl;
        cout << "请输入第一个chg文件名: ";
        getline(cin, file1);
        
        cout << "请输入第二个chg文件名: ";
        getline(cin, file2);
        
        cout << "请输入输出文件名: ";
        getline(cin, outputFile);
    } else {
        file1 = argv[1];
        file2 = argv[2];
        outputFile = argv[3];
    }
    
    // 读取两个文件
    cout << "正在读取 " << file1 << "..." << endl;
    vector<Atom> atoms1 = readChgFile(file1);
    
    cout << "正在读取 " << file2 << "..." << endl;
    vector<Atom> atoms2 = readChgFile(file2);
    
    // 检查原子数量是否相同
    if (atoms1.size() != atoms2.size()) {
        cerr << "错误: 两个文件的原子数量不同!" << endl;
        cerr << "文件1: " << atoms1.size() << " 个原子" << endl;
        cerr << "文件2: " << atoms2.size() << " 个原子" << endl;
        return 1;
    }
    
    if (atoms1.empty()) {
        cerr << "错误: 文件为空或读取失败!" << endl;
        return 1;
    }
    
    // 计算电荷差值 (chg1 - chg2)
    vector<Atom> resultAtoms = atoms1;
    for (size_t i = 0; i < atoms1.size(); i++) {
        resultAtoms[i].charge = atoms1[i].charge - atoms2[i].charge;
    }
    
    // 写入输出文件
    cout << "正在写入 " << outputFile << "..." << endl;
    if (writeChgFile(outputFile, resultAtoms)) {
        cout << "成功! 已生成 " << outputFile << endl;
        cout << "处理了 " << resultAtoms.size() << " 个原子" << endl;
        
        // 显示电荷差值统计
        double totalDiff = 0.0;
        for (const auto& atom : resultAtoms) {
            totalDiff += atom.charge;
        }
        cout << "电荷差值总和: " << fixed << setprecision(10) << totalDiff << endl;
    } else {
        return 1;
    }
    
    return 0;
}
