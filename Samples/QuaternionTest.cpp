#include "DSMEngine.h"
#include <iostream>
#include <iomanip>

using namespace DSM;

void PrintQuaternion(const Quaternionf& q, const std::string& name) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << name << ": w=" << q.Get(0) << ", x=" << q.Get(1) 
              << ", y=" << q.Get(2) << ", z=" << q.Get(3) << std::endl;
}

void PrintVector(const Vector<float, 3>& v, const std::string& name) {
    std::cout << name << ": (" << v.Get(0) << ", " << v.Get(1) << ", " << v.Get(2) << ")" << std::endl;
}

int main() {
    std::cout << "=== 四元数修复测试 ===" << std::endl << std::endl;
    
    const float PI = 3.14159265359f;
    
    // 测试轴角构造函数
    std::cout << "1. 测试轴角构造函数:" << std::endl;
    Vector<float, 3> xAxis = {1, 0, 0};
    Quaternionf qX(xAxis, 90.0f * PI / 180.0f);
    PrintQuaternion(qX, "绕X轴旋转90度");
    
    // 测试欧拉角构造函数
    std::cout << "\n2. 测试欧拉角构造函数:" << std::endl;
    Quaternionf qEuler(30.0f * PI / 180.0f, 45.0f * PI / 180.0f, 60.0f * PI / 180.0f);
    PrintQuaternion(qEuler, "欧拉角(30°,45°,60°)");
    
    // 测试向量旋转
    std::cout << "\n3. 测试向量旋转:" << std::endl;
    Vector<float, 3> originalVector = {1, 0, 0};
    PrintVector(originalVector, "原始向量");
    
    Vector<float, 3> rotatedVector = qX * originalVector;
    PrintVector(rotatedVector, "绕X轴旋转90度后");
    
    // 测试四元数归一化
    std::cout << "\n4. 测试四元数归一化:" << std::endl;
    Quaternionf qUnnormalized = qX;
    qUnnormalized.Set(0, qUnnormalized.Get(0) * 2.0f); // 故意破坏归一化
    PrintQuaternion(qUnnormalized, "未归一化四元数");
    
    Quaternionf qNormalized = qUnnormalized.Normalized();
    PrintQuaternion(qNormalized, "归一化后");
    
    // 测试double类型
    std::cout << "\n5. 测试double类型:" << std::endl;
    Vector<double, 3> yAxis = {0, 1, 0};
    Quaterniond qYDouble(yAxis, 45.0 * M_PI / 180.0);
    std::cout << "Double四元数: w=" << qYDouble.Get(0) << ", x=" << qYDouble.Get(1) 
              << ", y=" << qYDouble.Get(2) << ", z=" << qYDouble.Get(3) << std::endl;
    
    std::cout << "\n=== 测试完成 ===" << std::endl;
    
    return 0;
}
