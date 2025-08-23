# 四元数角度计算数学原理

## 概述

四元数是一种用于表示3D旋转的数学工具，它避免了欧拉角的万向锁问题，并且比旋转矩阵更高效。四元数由四个分量组成：`q = [w, x, y, z]`，其中 `w` 是标量部分，`[x, y, z]` 是向量部分。

## 1. 轴角表示法 (Axis-Angle)

### 数学公式

给定旋转轴 `v = [vx, vy, vz]`（单位向量）和旋转角度 `θ`，四元数可以通过以下公式计算：

```
q = [cos(θ/2), vx·sin(θ/2), vy·sin(θ/2), vz·sin(θ/2)]
```

### 代码实现

```cpp
Quaternion(const Vector<T, 3>& axis, T angle) noexcept 
{
    T s = std::sin(angle/2);  // 半角的正弦
    T c = std::cos(angle/2);  // 半角的余弦
    m_Vector.Set(0, c);                     // w = cos(θ/2)
    m_Vector.Set(1, axis.Get(0) * s);       // x = vx * sin(θ/2)
    m_Vector.Set(2, axis.Get(1) * s);       // y = vy * sin(θ/2)
    m_Vector.Set(3, axis.Get(2) * s);       // z = vz * sin(θ/2)
}
```

### 使用示例

```cpp
// 绕X轴旋转90度
Vector<float, 3> xAxis = {1, 0, 0};
float angle = 90.0f * M_PI / 180.0f; // 转换为弧度
Quaternionf q(xAxis, angle);

// 结果：q = [0.7071, 0.7071, 0, 0]
```

## 2. 欧拉角表示法 (Euler Angles)

### 数学公式

欧拉角使用三个角度：俯仰角（pitch）、偏航角（yaw）、滚动角（roll）。四元数可以通过以下公式计算：

```
// 分别计算每个轴的旋转
cr = cos(roll/2), sr = sin(roll/2)
cp = cos(pitch/2), sp = sin(pitch/2)
cy = cos(yaw/2), sy = sin(yaw/2)

// 组合旋转（ZYX顺序）
w = cr*cp*cy + sr*sp*sy
x = sr*cp*cy - cr*sp*sy
y = cr*sp*cy + sr*cp*sy
z = cr*cp*sy - sr*sp*cy
```

### 代码实现

```cpp
Quaternion(T pitch, T yaw, T roll) noexcept 
{
    T cr = std::cos(roll/2), sr = std::sin(roll/2);
    T cp = std::cos(pitch/2), sp = std::sin(pitch/2);
    T cy = std::cos(yaw/2), sy = std::sin(yaw/2);
    
    m_Vector.Set(0, cr*cp*cy + sr*sp*sy);   // w
    m_Vector.Set(1, sr*cp*cy - cr*sp*sy);   // x
    m_Vector.Set(2, cr*sp*cy + sr*cp*sy);   // y
    m_Vector.Set(3, cr*cp*sy - sr*sp*cy);   // z
}
```

## 3. 常用旋转示例

### 绕坐标轴旋转

```cpp
// 绕X轴旋转θ度
Quaternionf RotateX(float theta) {
    float angle = theta * M_PI / 180.0f;
    Vector<float, 3> axis = {1, 0, 0};
    return Quaternionf(axis, angle);
}

// 绕Y轴旋转θ度
Quaternionf RotateY(float theta) {
    float angle = theta * M_PI / 180.0f;
    Vector<float, 3> axis = {0, 1, 0};
    return Quaternionf(axis, angle);
}

// 绕Z轴旋转θ度
Quaternionf RotateZ(float theta) {
    float angle = theta * M_PI / 180.0f;
    Vector<float, 3> axis = {0, 0, 1};
    return Quaternionf(axis, angle);
}
```

### 绕任意轴旋转

```cpp
// 绕任意轴旋转
Quaternionf RotateAroundAxis(const Vector<float, 3>& axis, float theta) {
    float angle = theta * M_PI / 180.0f;
    Vector<float, 3> normalizedAxis = axis.Normalized();
    return Quaternionf(normalizedAxis, angle);
}
```

## 4. 四元数运算

### 乘法（组合旋转）

```cpp
// 四元数乘法：q1 * q2 表示先应用q2，再应用q1
Quaternionf q1 = RotateX(90);  // 绕X轴旋转90度
Quaternionf q2 = RotateY(45);  // 绕Y轴旋转45度
Quaternionf combined = q1 * q2; // 先Y轴45度，再X轴90度
```

### 共轭（逆旋转）

```cpp
Quaternionf q = RotateX(90);
Quaternionf qConjugate = ~q; // 逆旋转，相当于绕X轴旋转-90度
```

### 向量旋转

```cpp
Vector<float, 3> v = {1, 0, 0};
Quaternionf q = RotateY(90);
Vector<float, 3> rotated = q * v; // 旋转向量v
```

## 5. 插值

### 球面线性插值 (Slerp)

```cpp
// 在两个四元数之间进行平滑插值
Quaternionf q1 = RotateX(0);
Quaternionf q2 = RotateX(90);
Quaternionf interpolated = Quaternionf::Slerp(q1, q2, 0.5f); // 插值到45度
```

## 6. 注意事项

1. **角度单位**：确保角度转换为弧度（乘以 π/180）
2. **轴向量归一化**：使用轴角表示法时，确保旋转轴是单位向量
3. **四元数归一化**：计算完成后，四元数应该是单位四元数
4. **旋转顺序**：欧拉角旋转顺序很重要，通常使用ZYX顺序
5. **万向锁**：虽然四元数避免了万向锁，但在某些情况下仍需注意

## 7. 性能优化

- 对于频繁使用的旋转，可以预计算四元数
- 使用SIMD指令（如DirectX Math）可以提高性能
- 避免不必要的四元数归一化操作

## 8. 调试技巧

```cpp
// 检查四元数是否为单位四元数
bool IsUnitQuaternion(const Quaternionf& q) {
    float magnitude = q.Get(0)*q.Get(0) + q.Get(1)*q.Get(1) + 
                     q.Get(2)*q.Get(2) + q.Get(3)*q.Get(3);
    return std::abs(magnitude - 1.0f) < 1e-6f;
}

// 打印四元数信息
void PrintQuaternion(const Quaternionf& q, const std::string& name) {
    std::cout << name << ": w=" << q.Get(0) << ", x=" << q.Get(1) 
              << ", y=" << q.Get(2) << ", z=" << q.Get(3) << std::endl;
}
```
