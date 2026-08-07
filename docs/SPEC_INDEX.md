# SPEC Index

`docs/` 只保存目前有效、預期提交版本控制的規格。規格依「行為／技術」及「firmware／前端」分開，避免產品需求、實作細節與驗證流水帳互相覆蓋。

## 規格文件

| 文件 | 對象 | 用途 |
| --- | --- | --- |
| [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md) | PM、韌體開發者 | 前端以外的產品定位、使用者故事、裝置行為、權限與待決需求。描述系統應該做什麼，不規定程式如何組織。 |
| [SPEC_FRONTEND_BEHAVIOR.md](SPEC_FRONTEND_BEHAVIOR.md) | PM、UI/前端開發者 | `builtin-web/` 內建網頁的使用者流程、資訊架構、畫面行為、文案、RWD、accessibility 與驗收條件。 |
| [SPEC_TECHNICAL.md](SPEC_TECHNICAL.md) | 韌體開發者 | `builtin-web/` 以外的程式組織、模組責任、REST/serial dispatcher 架構、coding conventions 與嵌入式限制。 |
| [SPEC_FRONTEND_TECHNICAL.md](SPEC_FRONTEND_TECHNICAL.md) | 前端開發者 | `builtin-web/`、`user-web/`、frontend build、REST client 邊界、preview、部署與資源限制。 |
| [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) | API 使用者、整合開發者 | 對外提供的 REST/serial API 使用方式、auth、request、response、status code 與範例。它是外部整合 contract，不放內部架構。 |
| [SPEC_CONSOLE_REFERENCE.md](SPEC_CONSOLE_REFERENCE.md) | 開發者、測試與維運人員 | Serial monitor 連線方式、human commands、`api ...` 語法、token 使用與操作範例。 |

## 使用順序

- 改 firmware 行為：先讀 `SPEC_BEHAVIOR.md`，再讀 `SPEC_TECHNICAL.md`；涉及 API 時同步檢查 `SPEC_API_REFERENCE.md`。
- 改 `builtin-web/`：先讀 `SPEC_FRONTEND_BEHAVIOR.md`，再讀 `SPEC_FRONTEND_TECHNICAL.md`；所有 request/response 以 `SPEC_API_REFERENCE.md` 為準。
- 新增或修改 API：行為決策放在對應 behavior SPEC，對外 contract 放在 `SPEC_API_REFERENCE.md`，內部責任放在 technical SPEC。
- 使用 serial console：先讀 `SPEC_CONSOLE_REFERENCE.md`；API payload 與 response contract 再查 `SPEC_API_REFERENCE.md`。

## 不進入 docs 的資料

- Build、flash、filesystem upload、browser check 與 board smoke test 等日期驗證紀錄放在 `tmp/verification/`；其上層 `tmp/` 已由 `.gitignore` 排除。
- 已被取代的規劃、mockup、需求問答歷史與快照放在 `tmp/`，不得覆蓋現行 SPEC。
- 本機板子事實可保留在 ignored `board-profile.local.md`，不屬於 clean clone 或可重用產品規格；tracked 文件不得連向它。
