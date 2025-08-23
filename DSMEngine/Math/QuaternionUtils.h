#pragma once
#ifndef __QUATERNION_UTILS_H__
#define __QUATERNION_UTILS_H__

#include "Quaternion.h"
#include "Vector.h"
#include <cmath>

namespace DSM {

    template<typename T> requires std::is_arithmetic_v<T>
    class QuaternionUtils {
    public:
        static constexpr T PI = static_cast<T>(3.14159265358979323846);
        static constexpr T DEG_TO_RAD = PI / static_cast<T>(180.0);
        static constexpr T RAD_TO_DEG = static_cast<T>(180.0) / PI;

        // 角度转弧度
        static T DegreesToRadians(T degrees) {
            return degrees * DEG_TO_RAD;
        }

        // 弧度转角度
        static T RadiansToDegrees(T radians) {
            return radians * RAD_TO_DEG;
        }

        // 绕X轴旋转
        static Quaternion<T> RotateX(T degrees) {
            T angle = DegreesToRadians(degrees);
            Vector<T, 3> axis = {1, 0, 0};
            return Quaternion<T>(axis, angle);
        }

        // 绕Y轴旋转
        static Quaternion<T> RotateY(T degrees) {
            T angle = DegreesToRadians(degrees);
            Vector<T, 3> axis = {0, 1, 0};
            return Quaternion<T>(axis, angle);
        }

        // 绕Z轴旋转
        static Quaternion<T> RotateZ(T degrees) {
            T angle = DegreesToRadians(degrees);
            Vector<T, 3> axis = {0, 0, 1};
            return Quaternion<T>(axis, angle);
        }

        // 绕任意轴旋转
        static Quaternion<T> RotateAroundAxis(const Vector<T, 3>& axis, T degrees) {
            T angle = DegreesToRadians(degrees);
            Vector<T, 3> normalizedAxis = axis.Normalized();
            return Quaternion<T>(normalizedAxis, angle);
        }

        // 从欧拉角创建四元数（度数）
        static Quaternion<T> FromEulerDegrees(T pitch, T yaw, T roll) {
            T pitchRad = DegreesToRadians(pitch);
            T yawRad = DegreesToRadians(yaw);
            T rollRad = DegreesToRadians(roll);
            return Quaternion<T>(pitchRad, yawRad, rollRad);
        }

        // 从欧拉角创建四元数（弧度）
        static Quaternion<T> FromEulerRadians(T pitch, T yaw, T roll) {
            return Quaternion<T>(pitch, yaw, roll);
        }

        // 从两个向量创建四元数（从from旋转到to）
        static Quaternion<T> FromToRotation(const Vector<T, 3>& from, const Vector<T, 3>& to) {
            Vector<T, 3> fromNorm = from.Normalized();
            Vector<T, 3> toNorm = to.Normalized();
            
            T dot = Vector<T, 3>::Dot(fromNorm, toNorm);
            
            // 如果向量平行
            if (std::abs(dot) > static_cast<T>(0.9999)) {
                if (dot > 0) {
                    return Quaternion<T>(); // 单位四元数
                } else {
                    // 向量相反，需要找到垂直轴
                    Vector<T, 3> axis = {1, 0, 0};
                    if (std::abs(Vector<T, 3>::Dot(fromNorm, axis)) > static_cast<T>(0.9)) {
                        axis = {0, 1, 0};
                    }
                    axis = Vector<T, 3>::Cross(fromNorm, axis).Normalized();
                    return Quaternion<T>(axis, PI);
                }
            }
            
            Vector<T, 3> axis = Vector<T, 3>::Cross(fromNorm, toNorm).Normalized();
            T angle = std::acos(dot);
            return Quaternion<T>(axis, angle);
        }

        // 从旋转矩阵创建四元数（简化版本，假设是3x3旋转矩阵）
        static Quaternion<T> FromRotationMatrix(const T matrix[3][3]) {
            T trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
            
            if (trace > 0) {
                T s = std::sqrt(trace + 1.0) * 2;
                T w = 0.25 * s;
                T x = (matrix[2][1] - matrix[1][2]) / s;
                T y = (matrix[0][2] - matrix[2][0]) / s;
                T z = (matrix[1][0] - matrix[0][1]) / s;
                return Quaternion<T>(Vector<T, 4>{w, x, y, z});
            } else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2]) {
                T s = std::sqrt(1.0 + matrix[0][0] - matrix[1][1] - matrix[2][2]) * 2;
                T w = (matrix[2][1] - matrix[1][2]) / s;
                T x = 0.25 * s;
                T y = (matrix[0][1] + matrix[1][0]) / s;
                T z = (matrix[0][2] + matrix[2][0]) / s;
                return Quaternion<T>(Vector<T, 4>{w, x, y, z});
            } else if (matrix[1][1] > matrix[2][2]) {
                T s = std::sqrt(1.0 + matrix[1][1] - matrix[0][0] - matrix[2][2]) * 2;
                T w = (matrix[0][2] - matrix[2][0]) / s;
                T x = (matrix[0][1] + matrix[1][0]) / s;
                T y = 0.25 * s;
                T z = (matrix[1][2] + matrix[2][1]) / s;
                return Quaternion<T>(Vector<T, 4>{w, x, y, z});
            } else {
                T s = std::sqrt(1.0 + matrix[2][2] - matrix[0][0] - matrix[1][1]) * 2;
                T w = (matrix[1][0] - matrix[0][1]) / s;
                T x = (matrix[0][2] + matrix[2][0]) / s;
                T y = (matrix[1][2] + matrix[2][1]) / s;
                T z = 0.25 * s;
                return Quaternion<T>(Vector<T, 4>{w, x, y, z});
            }
        }

        // 检查四元数是否为单位四元数
        static bool IsUnitQuaternion(const Quaternion<T>& q) {
            T magnitude = q.Get(0)*q.Get(0) + q.Get(1)*q.Get(1) + 
                         q.Get(2)*q.Get(2) + q.Get(3)*q.Get(3);
            return std::abs(magnitude - 1.0) < static_cast<T>(1e-6);
        }

        // 获取四元数的角度（弧度）
        static T GetAngle(const Quaternion<T>& q) {
            T w = q.Get(0);
            if (w > 1.0) w = 1.0;
            if (w < -1.0) w = -1.0;
            return 2.0 * std::acos(w);
        }

        // 获取四元数的角度（度数）
        static T GetAngleDegrees(const Quaternion<T>& q) {
            return RadiansToDegrees(GetAngle(q));
        }

        // 获取四元数的旋转轴
        static Vector<T, 3> GetAxis(const Quaternion<T>& q) {
            T angle = GetAngle(q);
            if (std::abs(angle) < static_cast<T>(1e-6)) {
                return {1, 0, 0}; // 默认轴
            }
            T s = std::sin(angle / 2.0);
            return {q.Get(1) / s, q.Get(2) / s, q.Get(3) / s};
        }

        // 创建朝向目标点的四元数（Y轴朝上）
        static Quaternion<T> LookAt(const Vector<T, 3>& from, const Vector<T, 3>& to) {
            Vector<T, 3> forward = (to - from).Normalized();
            Vector<T, 3> up = {0, 1, 0};
            Vector<T, 3> right = Vector<T, 3>::Cross(forward, up).Normalized();
            up = Vector<T, 3>::Cross(right, forward);
            
            // 构建旋转矩阵
            T matrix[3][3] = {
                {right.Get(0), up.Get(0), -forward.Get(0)},
                {right.Get(1), up.Get(1), -forward.Get(1)},
                {right.Get(2), up.Get(2), -forward.Get(2)}
            };
            
            return FromRotationMatrix(matrix);
        }

        // 创建朝向目标点的四元数（指定上方向）
        static Quaternion<T> LookAt(const Vector<T, 3>& from, const Vector<T, 3>& to, const Vector<T, 3>& up) {
            Vector<T, 3> forward = (to - from).Normalized();
            Vector<T, 3> upNorm = up.Normalized();
            Vector<T, 3> right = Vector<T, 3>::Cross(forward, upNorm).Normalized();
            Vector<T, 3> upCorrected = Vector<T, 3>::Cross(right, forward);
            
            // 构建旋转矩阵
            T matrix[3][3] = {
                {right.Get(0), upCorrected.Get(0), -forward.Get(0)},
                {right.Get(1), upCorrected.Get(1), -forward.Get(1)},
                {right.Get(2), upCorrected.Get(2), -forward.Get(2)}
            };
            
            return FromRotationMatrix(matrix);
        }

        // 创建旋转到指定方向的四元数
        static Quaternion<T> RotateTowards(const Quaternion<T>& from, const Quaternion<T>& to, T maxDegreesDelta) {
            T maxRadiansDelta = DegreesToRadians(maxDegreesDelta);
            T dot = from.Get(0)*to.Get(0) + from.Get(1)*to.Get(1) + 
                   from.Get(2)*to.Get(2) + from.Get(3)*to.Get(3);
            
            if (dot > 0.9995) {
                return to;
            }
            
            T theta = std::acos(dot);
            if (theta <= maxRadiansDelta) {
                return to;
            }
            
            T t = maxRadiansDelta / theta;
            return Quaternion<T>::Slerp(from, to, t);
        }
    };

    using QuaternionUtilsf = QuaternionUtils<float>;
    using QuaternionUtilsd = QuaternionUtils<double>;

} // namespace DSM

#endif
