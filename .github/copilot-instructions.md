# Copilot 使用说明（针对 TrackManager 仓库）

此文件描述对 AI 编码助手在本代码库中立即有用的上下文、约定和常见工作流 —— 便于快速实现补丁、修复与单元测试。

1. 项目整体（大局观）
- **模块划分**: 数据层 `LatestKBuffer`（`src/latestKBuffer.hpp`）→ 管理层 `TrackManager`（`src/trackManager.*`）→ 通信/可视化（`src/TrackComm.hpp`、可选插件）。
- **设计要点**: `LatestKBuffer` 是固定容量的环形缓冲、禁止拷贝、支持移动；`TrackerManager` 使用内存池（vector pool + free_slots）管理多个轨迹容器，track id 从 1 开始，自增，返回 `0` 表示申请失败。

2. 关键文件与位置（快速定位）
- `src/latestKBuffer.hpp`: 环形缓冲实现；注意 POD 优化（memcpy）与非POD 安全复制。
- `src/trackManager.hpp` / `src/trackManager.cpp`: 航迹生命周期（create/delete/merge/push/pack）与内存池策略。
- `include/defstruct.h`: 对外通信数据结构（TrackPoint / TrackerHeader）。
- `src/TrackComm.hpp`: UDP 发送实现，构造时会创建 socket，输出目前使用 `std::cout`，TODO 已标记需要改成日志。
- `utils/logger.cpp` 和 `utils/logger.hpp`: 使用 `spdlog` 的工厂单例，日志文件路径当前在代码中硬编码（修改或使用 env 更佳）。
- `CMakeLists.txt`: 项目构建入口，**关键**：`add_definitions(-DENABLE_SPDLOG)`（开启日志宏），并将 `src`、`utils`、`include` 加入编译路径。
- `tests/`: 单元与基准测试（Catch2），提供 `TrackerManagerDebugger` 的打印/一致性工具（可在调试补丁时直接复用）。

3. 项目特定约定与模式（对 AI 很重要）
- 非拷贝类型：`LatestKBuffer` 与 `TrackerManager` 禁止拷贝构造/赋值；修改时优先采用移动语义或在容器外部构造新对象再替换。
- 内存池语义：`TrackerManager::createTrack()` 返回 `0` 表示失败；`free_slots_` 存储空闲索引，删除后会 push_back 索引。修改池相关逻辑要同时更新 `track_id_to_pool_index_` 与 `free_slots_`。
- 失效信号：`push_track_point` 在 `track.header.state == 2` 时会触发 `deleteTrack` 并返回 false，流水线需响应该返回值进行级联删除。
- 打包数据：`pack_track(char* buffer, id)` 先 memcpy 头，再调用 `LatestKBuffer::copyTo`，注意传入 buffer 空间必须足够。
- 日志：CMake 已设置 `-DENABLE_SPDLOG`，代码采用 `spdlog`；如果日志未生效，检查 `utils/logger.cpp` 中的 `log_dir` 路径。

4. 构建 / 测试 / 调试 快速命令
- 首次构建（推荐）：
  ```bash
  mkdir -p build && cd build
  cmake ..
  cmake --build . -j$(nproc)
  ```
- 运行所有测试：
  ```bash
  cd build
  ctest --output-on-failure
  ```
- 运行单个测试（示例）：
  ```bash
  cd build
  ctest -R trackermanager --output-on-failure
  # 或直接运行测试可执行文件，例如 build/tests/test_trackermanager
  ```
- 编辑器/LSP：`build/compile_commands.json` 已存在，直接指向该文件可为 clangd/IDE 提供正确 include 和编译标志。

5. 常见 PR / 代码风格注意点
- 保持头文件（`include/`）为对外接口；实现文件放 `src/`。对外 API 改动需更新 `include/defstruct.h` 的二进制兼容性。
- 代码中的 `// TODO` 与 `std::cout` 输出通常意味着应替换为日志（`spdlog`）。如果修改日志相关逻辑，请保持 `-DENABLE_SPDLOG` 宏兼容性。
- 测试依赖 Catch2（tests/），测试 CMake 会在 `tests/CMakeLists.txt` 中注入 AddressSanitizer（见 build/compile_commands.json）；不要在相同 commit 中同时更改生产与测试构建开关，除非明确目的。

6. 调试与检查技巧（可直接写入 PR 注释或 patch）
- 使用 `tests/TrackerManagerDebugger.hpp` 输出内部状态：它封装了 `printFullState` / `printStatistics` / `validateConsistency`，是检查内存池与索引一致性的首选工具。
- 对内存池变更请加单元测试（`tests/test_trackerManager.cpp` 中有多种场景），并在 build 中用 `ctest` 复现失败 case。

7. 变更示例（可贴到 PR 描述，便于 AI 生成补丁）
- 修复日志路径：修改 `utils/logger.cpp` 中 `log_dir` 为相对路径或读取环境变量。
- 优化 pack：验证 `pack_track` 在 buffer 空间不足时返回 `0` 或抛出可捕获的错误，并为调用者提供所需大小提示。

反馈：如果有特定的工作流、CI、或外部依赖（例如具体 spdlog 版本或 OpenCV 的定位方式），请告知，我会把它们并入该文件。
