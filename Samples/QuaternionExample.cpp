#include "DSMEngine.h"
#include "Math/QuaternionUtils.h"
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
    std::cout << "=== 四元数角度计算示例 ===" << std::endl << std::endl;
    
    // 1. 使用工具类的轴角表示法示例
    std::cout << "1. 轴角表示法 (Axis-Angle):" << std::endl;
    
    // 绕X轴旋转90度
    Quaternionf qX = QuaternionUtilsf::RotateX(90.0f);
    PrintQuaternion(qX, "绕X轴旋转90度");
    
    // 绕Y轴旋转45度
    Quaternionf qY = QuaternionUtilsf::RotateY(45.0f);
    PrintQuaternion(qY, "绕Y轴旋转45度");
    
    // 绕Z轴旋转30度
    Quaternionf qZ = QuaternionUtilsf::RotateZ(30.0f);
    PrintQuaternion(qZ, "绕Z轴旋转30度");
    
    // 绕任意轴旋转
    Vector<float, 3> arbitraryAxis = {1, 1, 1};
    Quaternionf qArbitrary = QuaternionUtilsf::RotateAroundAxis(arbitraryAxis, 60.0f);
    PrintQuaternion(qArbitrary, "绕(1,1,1)轴旋转60度");
    
    std::cout << std::endl;
    
    // 2. 欧拉角表示法示例
    std::cout << "2. 欧拉角表示法 (Euler Angles):" << std::endl;
    
    // 俯仰角30度，偏航角45度，滚动角60度
    Quaternionf qEuler = QuaternionUtilsf::FromEulerDegrees(30.0f, 45.0f, 60.0f);
    PrintQuaternion(qEuler, "欧拉角(30°,45°,60°)");
    
    // 只有偏航角（Y轴旋转）
    Quaternionf qYawOnly = QuaternionUtilsf::FromEulerDegrees(0.0f, 90.0f, 0.0f);
    PrintQuaternion(qYawOnly, "只有偏航角90度");
    
    std::cout << std::endl;
    
    // 3. 四元数组合示例
    std::cout << "3. 四元数组合:" << std::endl;
    
    // 先绕X轴旋转90度，再绕Y轴旋转45度
    Quaternionf qCombined = qX * qY;
    PrintQuaternion(qCombined, "X轴90° × Y轴45°");
    
    // 注意：四元数乘法不满足交换律
    Quaternionf qCombinedReverse = qY * qX;
    PrintQuaternion(qCombinedReverse, "Y轴45° × X轴90°");
    
    std::cout << std::endl;
    
    // 4. 向量旋转示例
    std::cout << "4. 向量旋转:" << std::endl;
    
    Vector<float, 3> originalVector = {1, 0, 0}; // 指向X轴正方向的向量
    PrintVector(originalVector, "原始向量");
    
    // 用绕Y轴旋转45度的四元数旋转向量
    Vector<float, 3> rotatedVector = qY * originalVector;
    PrintVector(rotatedVector, "绕Y轴旋转45度后");
    
    // 用绕Z轴旋转30度的四元数旋转向量
    Vector<float, 3> rotatedVectorZ = qZ * originalVector;
    PrintVector(rotatedVectorZ, "绕Z轴旋转30度后");
    
    std::cout << std::endl;
    
    // 5. 四元数插值示例
    std::cout << "5. 四元数插值 (Slerp):" << std::endl;
    
    Quaternionf qStart = QuaternionUtilsf::RotateX(0.0f); // 无旋转
    Quaternionf qEnd = QuaternionUtilsf::RotateX(90.0f); // 绕X轴旋转90度
    
    for (float t = 0.0f; t <= 1.0f; t += 0.25f) {
        Quaternionf qInterpolated = Quaternionf::Slerp(qStart, qEnd, t);
        std::cout << "t=" << t << ": ";
        PrintQuaternion(qInterpolated, "插值结果");
    }
    
    std::cout << std::endl;
    
    // 6. 从向量到向量的旋转
    std::cout << "6. 从向量到向量的旋转:" << std::endl;
    
    Vector<float, 3> fromVector = {1, 0, 0};
    Vector<float, 3> toVector = {0, 1, 0};
    Quaternionf qFromTo = QuaternionUtilsf::FromToRotation(fromVector, toVector);
    PrintQuaternion(qFromTo, "从(1,0,0)到(0,1,0)的旋转");
    
    // 验证旋转结果
    Vector<float, 3> rotatedFromTo = qFromTo * fromVector;
    PrintVector(rotatedFromTo, "旋转后的向量");
    
    std::cout << std::endl;
    
    // 7. LookAt功能示例
    std::cout << "7. LookAt功能:" << std::endl;
    
    Vector<float, 3> from = {0, 0, 0};
    Vector<float, 3> to = {1, 1, 0};
    Quaternionf qLookAt = QuaternionUtilsf::LookAt(from, to);
    PrintQuaternion(qLookAt, "朝向(1,1,0)的旋转");
    
    // 验证LookAt结果
    Vector<float, 3> forward = {0, 0, -1}; // 默认前方向
    Vector<float, 3> rotatedForward = qLookAt * forward;
    PrintVector(rotatedForward, "旋转后的前方向");
    
    std::cout << std::endl;
    
    // 8. 四元数属性查询
    std::cout << "8. 四元数属性查询:" << std::endl;
    
    Quaternionf testQuat = QuaternionUtilsf::RotateAroundAxis({1, 1, 1}, 45.0f);
    PrintQuaternion(testQuat, "测试四元数");
    
    float angle = QuaternionUtilsf::GetAngleDegrees(testQuat);
    Vector<float, 3> axis = QuaternionUtilsf::GetAxis(testQuat);
    bool isUnit = QuaternionUtilsf::IsUnitQuaternion(testQuat);
    
    std::cout << "角度: " << angle << "度" << std::endl;
    PrintVector(axis, "旋转轴");
    std::cout << "是否为单位四元数: " << (isUnit ? "是" : "否") << std::endl;
    
    std::cout << std::endl;
    
    // 9. 角度转换示例
    std::cout << "9. 角度转换:" << std::endl;
    
    float degrees = 90.0f;
    float radians = QuaternionUtilsf::DegreesToRadians(degrees);
    float backToDegrees = QuaternionUtilsf::RadiansToDegrees(radians);
    
    std::cout << degrees << "度 = " << radians << "弧度" << std::endl;
    std::cout << radians << "弧度 = " << backToDegrees << "度" << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== 示例结束 ===" << std::endl;
    
    return 0;
}
