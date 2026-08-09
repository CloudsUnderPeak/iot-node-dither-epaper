# Dither Image Editor Spec Index

```text
Version: 0.1.0
Status: Draft
```

本文件只作為 spec 文件入口與閱讀導引。詳細規格內容請看對應的 Behavior 或 Technical spec。

命名分工：

- `Embedded Web Dithering` 是 repository / project 對外名稱。
- `embedded-web-dithering` 是 repository slug、資料夾名稱與建議 GitHub repository 名稱。
- `Dither Image Editor` 是目前 app、主要 editor 頁面與 spec 主體名稱。

## 文件入口

- [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md)：PM / 產品角度的行為規格，描述使用者流程、畫面行為、功能範圍、里程碑與驗收標準。
- [SPEC_TECHNICAL.md](SPEC_TECHNICAL.md)：工程角度的技術規格，描述架構限制、模組邊界、state、pipeline、儲存、Dither、ESP32 預留與實作規則。

## 閱讀順序

1. 先判斷變更屬於使用者可見行為、技術實作限制，或兩者皆有。
2. 產品流程、UI 行為與驗收標準看 `SPEC_BEHAVIOR.md`。
3. 架構、狀態、pipeline、模組邊界與實作限制看 `SPEC_TECHNICAL.md`。
4. 若同一項變更同時影響行為與技術，兩份 spec 都要同步更新。

## 拆分原則

- 行為 spec 說明「使用者看見什麼、能做什麼、什麼算完成」。
- 技術 spec 說明「系統如何組織、程式如何實作、哪些限制不能破壞」。
- 本入口文件只保留導覽，不承載完整需求內容。
