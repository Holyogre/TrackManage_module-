# Copilot 使用说明（针对 TrackManager 仓库）

此文件为 AI 辅助开发在本仓库内的快速参考，包含：当前模块边界、重要约定、构建/测试命令与对常见改动的建议。

1) 大局观
- 模块：数据层 `LatestKBuffer`（`src/LatestKBuffer.hpp`） → 管理层 `TrackerManager`（`src/TrackerManager.*`） → 可视化 `TrackerVisualizer.hpp` / 通信 `TrackerComm.hpp`。
- 近期重构要点：`pack_track` 已从管理层剥离；可视化与通信使用管理层的只读 API（`getActiveTrackIds`、`getTrackSnapshot`）或受控的 const-reference 访问以避免拷贝。

2) 关键文件
- `src/LatestKBuffer.hpp`: 环形缓冲，POD 优先 memcpy，非POD 逐项赋值。
- `src/TrackerManager.hpp/.cpp`: 内存池（`buffer_pool_`）、id->pool 索引映射、生命周期操作（create/delete/merge/push）。提供只读访问器并尽量避免 friend。
- `src/TrackerVisualizer.hpp`: 现在通过 `TrackerManager` 的只读接口获取数据（不直接访问私有成员）。
- `src/TrackerComm.hpp`: 负责网络打包/发送，调用管理层只读接口获取数据并负责二进制打包。
- `utils/logger.*`: `spdlog` 单例，日志宏为流式 `LOG_INFO << ...`；CMake 在配置阶段注入 `TRACKMANAGER_DEFAULT_LOG_DIR`，若目录不存在会报错中止（便于调试）。

3) 项目约定（对 AI 很重要）
- 非拷贝语义：`LatestKBuffer` 与 `TrackerManager` 禁止拷贝；优先移动或引用访问。
- 内存池语义：`createTrack()` 返回 `0` 表示失败；`free_slots_` 为回收列表，删除后 push_back。
- 日志：使用 `LOG_DEBUG/LOG_INFO/LOG_ERROR` 流式宏；不要混用 `std::cout`。

4) 构建 / 测试
- 本地构建：
  ```bash
  mkdir -p build && cd build
  cmake ..
  cmake --build . -j$(nproc)
  ```
- 运行测试：
  ```bash
  cd build
  ctest --output-on-failure
  ```

5) 常见改动建议（可直接用作 PR body）
- 去友元：把外层模块改为使用 `getActiveTrackIds` + `getTrackSnapshot` 或暴露只读引用接口，避免大量 friend。
- 日志：CMake 注入 `TRACKMANAGER_DEFAULT_LOG_DIR`，运行前请确保该目录存在（脚本中提前创建），或者在 CI/部署里保持一致。
- 接口稳定：`include/defstruct.h` 是外部 ABI 约定，改动需谨慎并在测试里增加兼容性检测。

6) 设计/实现注意点（快速提醒）
- 若想避免拷贝但不放弃封装：提供 `const LatestKBuffer<TrackPoint>& getDataRef(track_id) const` + `const TrackerHeader& getHeaderRef(track_id) const`，并在文档中明确引用的生命周期（在 `TrackerManager` 修改或删除该航迹前引用有效）。
- 若担心未来修改破坏外部依赖，考虑返回只读 view/iterator 或提供 `begin()`/`end()` const 访问器于 `LatestKBuffer`。
- 对于生产并发场景（目前流水线单线程），若未来要并发访问，应在 API 上增加同步约定或返回不可变快照。

7) 其他资源
- 调试工具：`tests/TrackerManagerDebugger.hpp` （内存池一致性检查、打印）。
- 若需要我可以：生成 `.clang-format`（风格一致），或把 `TrackerComm` 的打包逻辑从管理层彻底移出（小 PR）。

若你要我把这些说明扩充为 PR 描述或生成 `.clang-format`，回复我想要的风格/细节即可。
