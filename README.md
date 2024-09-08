# 📸 PhotoMapping 项目

## 🌟 项目简介

PhotoMapping项目是一个用于处理和可视化照片数据的强大工具。它利用OpenSceneGraph库创建3D场景,并通过多线程技术高效处理大量照片数据。

## 🚀 主要功能

- 📁 加载和解析照片索引
- 🖼️ 处理单张照片,生成包含射线和相机中心球体的3D场景
- 📏 计算与视锥体相交的边界框
- 🔄 多线程并行处理照片数据,提高效率
- 🌐 可视化照片数据在3D空间中的分布

## 📂 文件结构

- `src/main.cpp`: 主程序文件,包含核心逻辑和功能实现
- `include/dirent.h`: 头文件,包含目录操作相关的函数声明
- `data/mesh/metadata.xml`: 包含网格数据的元数据文件
- `images/Tile_0016_0019/`: 存放照片数据的目录
- `CMakeLists.txt`: CMake构建配置文件
- `LICENSE`: 项目许可证文件

## 代码示例

### 加载照片索引

## 使用方法

1. 克隆仓库到本地：
    ```bash
    git clone https://github.com/lidapengpeng/PhotoMapping-test.git
    ```

2. 进入项目目录：
    ```bash
    cd PhotoMapping-test
    ```

3. 编译项目：
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```

4. 运行程序：
    ```bash
    ./PhotoMapping
    ```

## 依赖项

- OpenSceneGraph
- CMake
- C++11或更高版本

## 贡献

欢迎提交问题和拉取请求。如果你有任何建议或改进，请随时与我们联系。

## 许可证

本项目采用MIT许可证。详情请参阅LICENSE文件。