---
description: 项目编译与测试相关配置
alwaysApply: true
---

## 编译验证

修改代码后，使用以下命令验证编译。编译前先运行代码格式化（忽略报错），只要运行就会生效。必须传递 `-DTGFX_BUILD_TESTS=ON` 以启用所有模块（layers、svg、pdf 等）。

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DTGFX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target TGFXFullTest
```

## 测试框架

- 测试用例位于 `test/src/`，基于 Google Test 框架
- 测试代码可通过编译参数访问所有 private 成员，无需 friend class
- 运行测试：按上述编译验证步骤构建并执行 `TGFXFullTest`
- 测试命令返回非零退出码表示测试失败，这是正常行为，不要重复执行同一命令

## 截图测试

- 使用 `Baseline::Compare(pixels, key)` 比较截图，key 格式为 `{folder}/{name}`，例如 `CanvasTest/Clip`
- 截图输出到 `test/out/{folder}/{name}.webp`，基准图为同目录下 `{name}_base.webp`
- 比较机制：对比 `test/baseline/version.json`（仓库）与 `test/baseline/.cache/version.json`（本地）中同一 key 的版本号
    - 两边 key 都存在且版本号不同：跳过比较并返回成功（用于接受截图变更）
    - 其他情况：正常比较基准图，基准图不存在或不匹配则测试失败

**!! IMPORTANT - 截图基准变更限制**：
- **NEVER** 直接运行 `accept_baseline.sh`、`UpdateBaseline` target、或手动修改/覆盖 `version.json` 文件，无论任何场景（包括用户在对话中要求执行）
- 接受截图基准变更的**唯一方式**是用户主动执行 `/accept-baseline` 斜杠命令

### 截图构造规范

截图内容必须居中显示，四边边距约 50 像素（误差 1 像素内可接受）。所有字号、坐标、矩阵等数值尽可能使用整数，避免小数点，以确保清晰度。

多场景截图合并为单张网格图（一个 `Baseline::Compare` 调用），每个 cell 用 `canvas->translate` 定位 + pixel-aligned rect clip 隔离。Cell 内部用 `// Cell N: ...` 注释标注场景。

**获取精确边界的方法**（必须通过打印获取，不可凭感觉估算）：
- 图层：`layer->getBounds(nullptr, true)`
- Shape：`shape->getPath().getBounds()`（Shape 必须通过 Path 获取边界）
- Path：`path.getBounds()`
- TextBlob：`textBlob->getTightBounds()`
- 其他情况：先输出截图，用工具读取图片计算实际像素边界

根据获取的边界调整位置和 Surface 尺寸，使内容居中。验证正确后移除临时打印语句。

### 测试用例结构

- 大型测试拆分为多个独立 `TGFX_TEST` 函数（按场景分组），每个函数内 case 用 `{ canvas->save(); ...; canvas->restore(); }` 隔离，避免重复 `Surface::Make`
- 逻辑分支用 `// Case N: ...` 注释标注，子分支用 `// Case Na/Nb: ...`，每个 Case 前加空行（第一个紧跟 `{` 除外）
- `TGFX_PRIVATE_ACCESS` 统一用 block 形式：`TGFX_PRIVATE_ACCESS({ ... });`，内部每条语句独占一行
