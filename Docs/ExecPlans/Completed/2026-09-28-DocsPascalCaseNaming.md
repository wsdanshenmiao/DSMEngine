# Docs 目录及文档 PascalCase 命名迁移

## 状态

- 状态：completed
- 日期：2026-09-28
- 基线：当前工作树；实施前旧文档目录共有 43 个文件、7 个子目录，`git status -- Docs` 为干净；保留其它未提交改动。

## 摘要 (Summary)

将仓库根目录改为 `Docs/`；文档分类目录使用 PascalCase（`Architecture`、`ExecPlans/Active`、`ExecPlans/Completed`、`Guides`、`Knowledge`、`Reviews`），Markdown/HTML/PDF 的说明性文件名使用日期前缀 + PascalCase 或 PascalCase。保留标准 `README.md`、`.gitkeep`，不改变二进制内容或文档的技术结论。同步更新仓库内链接、AGENTS/CLAUDE/CODEBUDDY/PLANS 路由和 `.gitattributes` LFS 匹配。

## 背景 (Context)

原文档目录在 Windows 不区分大小写的文件系统上不能直接进行仅大小写不同的同路径改名，需在已验证的仓库根目录内经唯一临时目录过渡。文档内有相对链接、HTML 到 Markdown 的本地链接和源文件路径引用；`.gitattributes` 为 `Docs/Knowledge/*.pdf` 设置 LFS，迁移后若不更新将改变二进制跟踪方式。已完成计划是历史记录，允许仅改路径引用，保留正文事实。

## 实施计划 (Implementation Plan)

1. 列出所有目录与文件并建立一一映射；检查目标在 Windows 大小写不敏感比较下无重名碰撞。记录 PDF 等二进制哈希。
2. 使用同一 PowerShell 会话、`Move-Item -LiteralPath` 在工程根目录内完成逐文件和逐目录迁移；根 `docs` 改名采用安全两步路径。
3. 逐一更新第一方文本中对旧路径的绝对、仓库相对及文档内部相对链接；更新 `.gitattributes`，不触碰 ThirdParty。
4. 扫描旧路径残留、检查迁移前后文件数量/二进制哈希、校验链接，运行 `git diff --check`、项目检查和最小构建验证，记录不能验证的原因。

## 验证 (Validation)

工作目录：`D:\Code\DSMEngine`。临时清单/哈希在 `.tmp/docs-pascal-case/`，不覆盖既有数据。

- 检查 `Docs/` 存在且旧目录名不在目录枚举中；文件数 44（包括本计划），PDF/HTML 数量与迁移前一致。
- `rg` 扫描第一方文本旧目录路径残留、旧文档文件名；明确历史文本/外部链接残留的例外。
- 检查 Markdown/HTML 相对链接从新路径解析后的存在性，与迁移前的已知断链作对照。
- `git check-attr filter -- Docs/Knowledge/<PDF>` 返回 `lfs`；二进制文件 SHA-256 不变。
- `git diff --check`、`Tools/dsm.ps1 check Projects/PBR/PBR.dsmproj`、`xmake build DSMEngine`。

## 进展 (Progress)

- [x] 读取项目规则、PLANS、Docs 文档地图并记录目录清单及引用入口。
- [x] 建立映射并完成目录/文件迁移。
- [x] 同步修正路径引用和 LFS 规则。
- [x] 完成静态检查和最小可用验证（构建失败原因已记录）。

## 意外与发现 (Surprises & Discoveries)

- `Docs` 与 `docs` 在 Windows 路径查找中等价，检查是否真正改名需枚举目录项的原始大小写，而非仅 `Test-Path Docs`。
- `Docs/Knowledge/README.md` 原本引用未存在的课程学习指南，迁移时只改引用风格，不把既有缺失误判为迁移引起。

## 决策记录 (Decision Log)

- 固定 `README.md`、`.gitkeep` 是行业惯例/占位文件，不改名；日期前缀保留，标题词改为 PascalCase。
- PDF/HTML 也是 Docs 中的文档，文件名同样统一；PDF 字节保持不变。
- 不改动供应商目录，且不改变项目运行/资产路径。

## 结果与复盘 (Outcomes & Retrospective)

已完成 44 个文档文件和 7 个分类目录的 PascalCase 迁移，保留 README/.gitkeep；8 个 PDF 的 SHA-256 保持一致，LFS 规则已更新为 `Docs/Knowledge/*.pdf`。25 个 Markdown/HTML 相对链接检查通过，旧路径引用扫描为 0；`git diff --check`、项目检查通过。`xmake build DSMEngine` 已尝试，但因已有 Xmake Assimp 包缓存缺失 `assimp/Importer.hpp` 而失败；该问题与本次文档改名无关，未修改构建/第三方代码。
