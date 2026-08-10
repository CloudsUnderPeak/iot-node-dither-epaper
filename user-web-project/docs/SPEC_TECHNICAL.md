# Dither Image Editor 技術 Spec

```text
Version: 0.1.0
Status: Draft
Last Updated: 2026-07-11
Split From: SPEC_INDEX.md
```

本文件收斂實作與架構規格，給工程實作、review、測試設計使用。產品目標、使用者行為、畫面行為與驗收節奏請看 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md)。文件入口與閱讀導引請先看 [SPEC_INDEX.md](SPEC_INDEX.md)。

## History

- 2026-08-10: 新增 `device-epaper-calibration.js` 作為六色色準 canonical service，處理公開 GET／PUT／reset、deep-copy snapshot、revision 與 stale GET suppression。面板測試保留本地 draft；`target-policy` 改為 revision-aware 動態 palette，editor 收到變更時清 stage/image cache 並重跑 preview。
- 2026-08-10: `target-policy.js` 的 `OUTPUT_COLORS`／`DISPLAY_COLORS` 改為共用 EPD code order `0,1,2,3,5,6`（黑、白、黃、紅、藍、綠）；韌體的 palette validation、capability `color_codes` 與六色測試 frame source 改讀同一份具名 code-order 常數。
- 2026-08-09: E-paper effective palette 改為 `DISPLAY_COLORS`（A′），讓所有 palette mapper／dither processor 使用校色後的實體參考色；`target-policy.outputImageData()` 在 encoder boundary 以 exact RGB lookup 保留 palette index 並轉回 `OUTPUT_COLORS`（A），拒絕任何非六色色素。
- 2026-08-09: `EpaperOperationOverlay` 共用 `.app-loading-content`、spinner、message、progress 與 progress-bar primitives，僅以 e-paper CSS 保留全畫面 blocking 層與獨立 percentage；避免啟動 loading 與操作 loading 的尺寸、動畫及 theme token 漂移。
- 2026-08-09: E-paper frontend follow-up：phase progress 改為不受相同 status polling 重設的 time estimate 與 overtime asymptotic motion；cooldown local timer 在歸零後持續短輪詢至 server 離開 cooldown。`target-policy.js` 分離 immutable OUTPUT_COLORS 與人工可校正 DISPLAY_COLORS，viewport/display swatch 只套 display mapping，encoder 仍驗證純六色。
- 2026-08-09: 實作 E-paper Device Mode。新增 e-paper capability/status/operation service、5:3/3:5 target policy、portrait clockwise normalization、EPDIMG encoder、global time-weighted progress overlay、cooldown admission 與 device e-paper test page；沿用既有 `/api/epaper*` contract。
- 2026-07-29: `assets/icons/device/` 圖示來源對齊：`filter.svg` 與 `warning.svg` 改用 iot-node-bedrock builtin-web icon set 的原始 path（filter-funnel／warning），`lock.svg` 換成使用者提供的 SVG Repo lock-01（正規化為 24 viewBox、`stroke="currentColor"`、stroke-width 2、round cap/join、`aria-hidden`）。目前該目錄除 lock 外全部取自 builtin-web icon set。
- 2026-07-29: 連線狀態卡新增 `.network-status-icon`（34px accent-soft 方塊＋accent filter icon），icon 以 `grid-row: 1 / span 2` 跨 label 與值兩列並垂直置中，說明列縮排在同一文字欄下，icon 下方不留空欄；新增 `assets/icons/device/signal.svg` 與 `globe.svg`（path 取自 iot-node-bedrock builtin-web icon set），STA 沿用 `wifi-strength-4.svg`；桌面與手機共用同一結構（手機單欄逐項）。`.network-status-grid` 桌面為三欄一列，欄寬加權（1 / 1.15 / 1.35）讓 mDNS 網址維持單行。移除 `.device-mono`：mDNS 網址與 hostname 預覽改用全站一般字體，等寬字只保留給 Help 的 matrix/kernel 格子。
- 2026-07-29: `deviceSystemTitle`／`menuDeviceSystem` 文案改為「裝置設定」／"Device Setting"，`mockBadge` 簡化為 "PREVIEW"；`.device-grid` 與 `.network-status-grid` 在 920px 手機模式改為逐項單欄（移除 700/480 的併欄規則），且每列改為 130px 左標籤欄＋右側值（`.device-field` 兩欄、`.network-status-column` 的 value/hint 落在第二欄），不留整塊空白。
- 2026-07-29: Web Setting 比照裝置頁改用 `panel-section` + `panel-body` 卡片並移除頁內 `h1` 與 `simple-page` 依賴；`components.css` 移除已無使用者的 `.setting-section`，新增 `.web-setting-page` 置中卡片堆疊版面（含 920px 自行捲動斷點）。
- 2026-07-29: select 自訂箭頭抽成共用 `src/ui/select-field.js`（`app.ui.selectField.create(select)` 回傳 `.select-field` 包裝；樣式移入 components.css）；編輯器 `panel-utils.selectInput` 改回傳 `{ node, select }` 並套用包裝，crop／dither／palette feature 與 `wifi-form` 全部改走共用元件，全站 select 外觀一致。
- 2026-07-29: 裝置頁卡片改用 components.css 的 `panel-section` + `panel-body`（`device-card-header` 承載含 badge／總空間的標題列變體），移除頁內 `h1` 與 `simple-page` 依賴；`assets/styles/device.css` 全面對齊編輯器尺寸階（輸入框 34px、字級 11–14px、間距 10–12px、圓角 6/8px），`device-form-row`／`device-toggle-row` 改為 130px 左標籤欄並在 560px 斷點收合，modal 圓角與內距同步縮小。容量圖例改為 builtin-web 式 `capacity-stats`／`capacity-stat`（`segments-N` 決定欄數、700px 以下兩欄），`wifi-form` 的 select 以 `.select-field` 包裝：`appearance:none` 隱藏原生箭頭、疊 `chevron-down.svg`，盒位使用全站統一的結尾圖示尺寸，讓各表單元件結尾的 svg 對齊。容量圖 `capacity-color-1..4` 改為圖表專用固定色階，色相取自 accent teal 與 status-busy 藍（依「深藍→深青綠→中藍→亮青綠」明度遞增排列，light 與 dark 節奏一致：light `#08569a`/`#058461`/`#5795cc`/`#49c6a7`，dark 以 `body[data-theme="dark"]` 覆寫 `#1f62a4`/`#04865d`/`#5483bb`/`#30a981`，恢復獨立 dark 覆寫、與語意 token 脫鉤但同色相），`capacity-color-5` 維持 `color-mix` 中性底色；色階經 OKLCH 明度帶、彩度下限與相鄰 CVD ΔE 驗證，`capacity-seg + capacity-seg` 加 1px `--color-surface` 分隔線。
- 2026-07-29: `wifi-form` 的 `toggleRow()` 改為 `<div>` 外層＋只包住開關的 `<label>`，文字用 `aria-labelledby` 關聯，因此點標籤不會切換；靜態 IP 欄位透過 `wifiIpExample` placeholder 帶範例值；`wifiSecurityOpen` 改為英文 `Open`，掃描列的 `encryption` 統一 `toUpperCase()`；`scan-dialog` 新增 `.scan-tools`（篩選欄內嵌 `filter.svg`、rescan 帶 `refresh.svg` 並固定 148px 寬），倒數只改內層 label 文字以保留圖示。
- 2026-07-29: `<details>` 的非 summary 子節點會被瀏覽器包進單一 `::details-content` 盒子，因此 `.device-advanced` 的 grid gap 只作用在 summary 之後；進階欄位間距改由明確的 `.device-advanced-body` wrapper 負責（`wifi-form` 的 `advancedPanel()`）。`.save-dock` 取消 `flex-wrap` 並在窄螢幕隱藏狀態文字讓按鈕平分；`.device-toggle-row` 改為 `justify-content: space-between`，視覺順序用 `order` 調整以維持 `input:checked + .toggle-switch-track` 的相鄰關係；`device-info` 的 `capacityCard()` 在 header 顯示分段總和。
- 2026-07-28: 啟用完整重設：`device-api` 只保留 `systemReset()`（`POST /api/system/reset`），`pages/device-system` 新增 danger zone 與確認 dialog，成功後 `invalidateSession()` 並留常駐 notice；新增 `assets/icons/device/warning.svg` 與 `.device-danger-icon`（紅底＋既有 svg-icon 反白 filter）；device-mock 的 heap 改為固定值，`api/system/reset` 改為精確路徑並把 in-memory 狀態重設回出廠預設 AP。
- 2026-07-28: 裝置頁樣式與資訊密度調整集中在 `assets/styles/device.css`（不動 `layout.css` / `components.css`），以 `.device-page .setting-section` 覆蓋側欄尺寸的預設卡片並新增 480px 單欄斷點；容量圖色票改由 `--color-accent-strong` / `--color-accent` / `--color-control-accent` 與 `color-mix` 推導，取消獨立的 dark 覆寫；`ui/password-field` 改為 `.password-field` 相對定位 + 疊在 input 內的 `.password-toggle`；`wifi-form` 的密碼與 AP 密碼保護說明改用 `.device-rule-hint`，`.wifi-section` / `.device-advanced` 邊框改用 `--color-border` 並讓進階區塊帶 `--color-surface-muted` 底色與 `[open] summary` 分隔線；`app-menu` 移除裝置連線小點；`device-info` 移除手動 refresh、stale badge 與 config_state 欄位；device-mock 初始 Wi-Fi mode 改為 `ap_sta`（STA 已連線）。
- 2026-07-28: 新增開發／demo 假資料機制：`index.html` PREVIEW 區塊載入 `src/device/device-mock.js`（載入即預設啟用、`?mock=0` 關閉）；`make build` 剝除區塊並排除 mock 檔，新增 `make demo` 保留 mock 的靜態展示輸出（不 gzip）。`tools/build` 每次執行先清空並重建 `build/latest` 鏡像，內容與該次時間戳資料夾相同。
- 2026-07-28: 新增 `src/device/` 裝置整合層（`device-api` REST client、`device-live` alive 監看、`device-gate` 離線反灰、`device-auth` 認證）與三個裝置頁（`device-info`、`device-network`、`device-system`），對接 iot-node-bedrock REST API；共用 `ui/modal`、`ui/notice`、`ui/password-field` 與 `assets/styles/device.css`。header `#app-status` 圓點改為合成裝置連線與頁面狀態。
- 2026-07-11: 新增 app-level `project-capabilities.js` 與 Help content model/validator；Dither config 註冊可用清單、constants 註冊限制 fact，i18n 支援可重複 named/positional placeholder，Help 演算法卡片改由 registry 與 ID 文案交集產生。
- 2026-07-11: Help page 改為 manifest 驅動的雙語文件中心；PageRouter 保存完整 route 並支援同頁 `onRouteChange`，Help 使用獨立 i18n bundle、文件 renderer、互動視覺模組與 `assets/help/` 引擎渲染比較圖。
- 2026-07-11: `main.js` 本地化 startup gate 時透過 `applyShellCopy()` 同步 header placeholder；AppShell mount 後仍重套 shell 文案與 status。
- 2026-07-11: Startup gate 新增單調遞增的 `setProgress()` 與動態 script resource 計數，`main.js` 依 mount、initial image settle 與 paint 更新階段進度。
- 2026-07-11: Startup overlay 改用 theme-specific 半透明 `--color-loading-overlay`，固定從 64px header 下方開始，避免遮住 App title；`inert` 與 pointer fallback 維持全 App 鎖定。
- 2026-07-10: `index.html` 新增 startup gate 與初始 `inert`；`main.js` 在 page entries、AppShell mount、initial image settle 與 paint 完成後解鎖，fatal load error 則保留 gate 並提供 reload。
- 2026-07-10: Serpentine label 直接建立單一 info tip；圖示使用 `assets/icons/editor/info-circle.svg`，tip 文案走 i18n，自訂 tooltip 使用實心 theme surface 與 accent 邊線。
- 2026-07-10: `renderActiveTool` 不得在每次 render 重新 append 已位於正確 host 的 panel，避免使 panel 內控制失焦；Resize unit number callback 只強制同步等比連動的另一欄，目前輸入欄位透過 active-element guard 保留 DOM value 與游標位置。
- 2026-07-10: `.unit-number-input` 不直接繪製會被複合欄位裁切的 outline；其 `:focus-visible` 焦點環必須轉嫁到 `.unit-number-field` 外框。
- 2026-07-09: i18n 新增 `zh-TW` 字典與 runtime language preference；Web Setting language 提供 `auto`、`zh-TW`、`en`，偏好與 theme 一起保存於 `settings-store.js`，切換語言會重新套用 shell/menu/目前頁面文字。
- 2026-07-09: 導入 dither Web Worker：擴散類演算法（error diffusion、adaptive error diffusion、dot diffusion）在 HTTP serving 下可於背景執行緒執行；`file://` 或 worker 載入失敗時永久 fallback 同步路徑。preview/export 走 `pipelineRunner.runAsync`，controller 用 run id 丟棄過期 preview 結果並在 destroy 時 terminate worker。
- 2026-07-09: feature 之間不可直接讀寫 `state.settings.<其他 feature>`；跨 feature 查詢一律透過 `featureRegistry.api(id)` 取得對方宣告的 `api` 物件，api 回 null（feature 停用）時呼叫端必須有明確降級路徑。依賴其他 feature 資料的 operation 必須用 `operation.cacheKey` 把該資料帶進 stage cache key。
- 2026-07-09: crop feature 拆為三支 script：`crop-geometry.js`（純幾何，零依賴）、`crop-auto-background.js`（背景 preset 與 Auto 取色）、`crop-feature.js`（註冊/panel/operation）。feature manifest entry 支援 `paths` 陣列宣告多支 script，依序載入。
- 2026-07-09: `page.js` 職責收斂：Expand 拖曳平移移至 `viewport/pixel-preview-drag.js`，preview toolbar 移至 `preview-toolbar.js`，preview timing label 可見性與定位由 `viewport/overlay-renderer.js` 管理，Empty 畫布 dropzone 改由 input feature 提供。
- 2026-07-09: 移除 legacy 單面板狀態欄位 `settingsPanelOpen` 與 `activeTool`；`openToolPanels` 是 tool panel 開關狀態的唯一來源。
- 2026-07-09: 面板滑桿統一走 `panelUtils.labeledRange`（數值標籤 + range + setValue/setDisabled + --range-progress），live preview hold 樣板統一走 `panelUtils.previewHoldHandlers(controller, id)`；hex 與 RGB 互轉統一由 `core.colorUtils.hexToRgb/rgbToHex` 提供。
- 2026-07-09: GPU processor 的 WebGL 樣板（context 建立、shader 編譯、全屏 quad、texture、drawQuad、readPixels 翻轉讀回）統一由 `gpu/gl-helpers.js` 提供；adjust 與 threshold processor 只保留各自 fragment shader 與 uniform 邏輯。
- 2026-07-07: 全域 `:focus-visible` 焦點環由 `components.css` 統一提供（`--color-accent-strong`）；隱藏或位於複合控制內的 input 可使用 `outline: none`，但焦點環必須轉嫁到可見控制外框。
- 2026-07-06: 色彩通道 clamp 統一由 `core/color/color-utils.js` 提供：`clampByte`（round + clamp，最終整數輸出用）與 `clampChannel`（僅 clamp 保留小數，誤差擴散工作緩衝與距離計算用）；dither 模組不可再自定義 clamp。GPU threshold palette uniform 必須與 CPU `normalizedPalette` 相同做 round，維持 CPU/GPU 最近色一致。
- 2026-07-06: RGB 色距權重的唯一來源是 `core/color/palette-utils.js` 的純量距離函式；`palette-mapping` 透過 `createRgbDistanceContext()` 取得逐像素距離 context，不可另寫距離公式。GPU shader 的 `distanceTo` 是唯一允許的重複實作，修改權重時必須同步。
- 2026-07-06: `core` 拋出的使用者可見錯誤必須帶 `error.code`（如 `unsupported-format`、`demo-load-failed`），`message` 僅作 fallback；顯示層（controller）以 code 對應 i18n 文字，`core` 不可直接輸出 UI 文案。
- 2026-07-06: 使用者可見 UI 字串（含 page `title`、header、tooltip、aria-label）必須走 `src/i18n/en.js`；`index.html` 內的 header 文字僅為 JS 啟動前 placeholder，由 `AppShell.applyShellText()` 以 i18n 蓋章。
- 2026-07-06: 新增 `tools/regression/`：以 dither-benchmark headless harness 產生固定矩陣（algorithm x mapping x palette x distance，CPU backend）的 checksum baseline（`baseline.json` 入 repo）；變更 dither/palette/色彩工具前後必須跑 `python3 tools/regression/run.py`，刻意的輸出變更需人工確認後以 `--update` 更新 baseline。dither-benchmark 支援 `--distance` 參數。
- 2026-05-16: 定調目前專案版本為 `0.1.0`；此版本號與 localStorage `schemaVersion` 分開管理。
- 2026-05-16: 新增 `editor-mode-state-machine.js` 與 editor `mode` 狀態，集中管理 `empty`、`crop`、`edit` 轉換；重新載入圖片或 demo 時 controller 必須重建 default editor state，避免沿用上一張圖的演算法設定。
- 2026-05-16: 明確規範 Crop mode 不執行正式 preview pipeline，非允許工具與 action 必須由 controller guard；preview toolbar 必須由 mode 決定顯示列，未啟用列要真正 hidden，且各模式 toolbar 按鈕尺寸一致。
- 2026-05-21: Empty 模式的 upload/drop affordance 由 `page.js` 掛在 preview stage 中央，支援 hidden file input 的 Browse File 與 drop event；Image Input panel 不應顯示 Choose/Drop controls，empty canvas placeholder 必須隱藏。
- 2026-05-21: Image Input panel 的 `New Image` 改由 hidden file input 觸發本機圖片選擇，取代舊 `Choose Image` row；目前 UI 不暴露 blank canvas 建立入口。
- 2026-06-07: Crop 面板新增左轉 90 / 右轉 90 圖示按鈕，Flip 改為圖示按鈕；旋轉按鈕只更新 `rotation`，Flip 仍使用同一次 settings update 同步鏡射 rotation / pan，且不套用持續 active 視覺狀態。
- 2026-06-07: 左側 tool panel 展開預設集中到 mode state machine：`empty` 只開 Image Input、載圖或展開 Crop 只開 Crop、離開 Crop 進入 `edit` 時開 Resize / Adjust / Palette / Dither，手動展開 Image Input 或 Crop 時其他面板收合。
- 2026-06-09: editor `mode` 改為以 feature group 為單位保存 `source`、`prepare`、`edit`；feature 必須明確宣告 `panelGroup` 才會進入左側 tool dock，未宣告時預設為 `none`。
- 2026-06-09: Crop 預設比例改為 16:9，prepare toolbar 的 crop zoom 以 `+` / `-` 顯示；Resize 固定等比且移除 Fit；Adjust 移除 Reset Default 並在 slider 左側顯示數值。
- 2026-06-09: `onSettingChanged` lifecycle 改為廣播給 enabled features，由 feature 依 `context.id` 判斷是否處理，支援跨 feature setting 同步且避免 controller 硬寫 feature id。
- 2026-06-09: Crop preview toolbar 的 `+` / `-` 使用 compact square button；Resize output 單邊尺寸上限集中到 `MAX_RESIZE_OUTPUT_SIZE`。
- 2026-06-09: `constants.js` 提前於 feature scripts 載入；Resize width / height controls 改用同列 `unitNumberInput(..., 'px', ...)` 並共用 Crop 數字輸入樣式。
- 2026-06-13: `prepare` mode 放行 edit tool rows；從 `prepare` 開啟指定 edit tool 時，state machine 只展開該 edit panel 並切入正式 preview 流程。
- 2026-06-13: Crop frame 與 edit preview display size 統一由有內距的 preview frame fit 計算，避免模式切換時 canvas 位置或尺寸跳動。
- 2026-06-13: `source` group 載入圖片後保留 preview toolbar 高度但隱藏所有 button rows，避免切到 `prepare` / `edit` 時 preview stage 高度改變。
- 2026-06-13: `viewport-renderer.js` 改為 buffer 組幀後提交可見 canvas；首次進入 `edit` 且 result 尚未完成時保留上一個 preview frame。
- 2026-06-13: preview frame fit 改用 preview stage content-box 尺寸，避免 border-box 導致 prepare / edit 之間出現微小對齊差。
- 2026-06-13: Demo data asset 改為按下 Load Demo 時才載入；動態 script loader 改為同批並行下載、依插入順序執行。
- 2026-06-20: Demo 圖以專案內 `assets/demo/` 圖片作為 server 模式來源；demo data asset 僅作為 `file://` fallback，並由工具產生。
- 2026-06-27: 新增選用的 Python/Make 發佈 build，輸出 ignored `build/` 底下的時間戳子資料夾，只做 server/device 靜態檔案複製、minify 與 gzip-only 輸出；minify / gzip 預設啟用並可用 CLI 參數關閉。build 產物不支援 `file://` fallback，必須排除 generated demo data fallback。原始專案仍不可依賴 build step、npm 或 bundler 才能使用。`tools/` 底下工具必須放在各自子資料夾，以 `run.py` 作為主要 CLI 入口。
- 2026-06-28: `tools/generate-demo-data/run.py` 改為自動偵測 `assets/demo/` 內唯一支援格式 demo 圖，產生固定入口 `assets/demo/demo-manifest.js` 與 `assets/demo/demo-data.js`；runtime 不可硬綁 demo 圖檔名或 16:9 比例。
- 2026-06-28: Resize feature 必須在 render 後同步 width / height controls，確保 Crop ratio 變更後隱藏過的 Resize panel 不顯示舊尺寸。
- 2026-06-28: Original palette 萃取來源改為 `prepare` group 輸出；Resize、Original palette 與 `preparedImageData` invalidation 必須延後到 prepare commit，不可在 Crop zoom/pan setting 熱路徑即時計算。
- 2026-06-28: Edit preview 新增 Expand 檢視，內部使用 `pixel` viewMode 與同一份 Result ImageData 以真實 canvas CSS 尺寸顯示；初始 scroll 對準 Result 中心點，並讓 preview stage 可捲動、可拖曳平移。
- 2026-07-01: Dither `errorStrength` 保持為演算法目前使用的百分比強度；Error Diffusion 將其套用到誤差擴散倍率，Bayer 演算法將其套用到 thresholdScale 並在 UI 顯示為 Dither Strength。
- 2026-07-01: Blue Noise 64 啟用 `supportsThresholdStrength`，Dot Halftone 啟用 `supportsDotDensity`，Dot Diffusion 8x8 啟用 `supportsErrorStrength`，讓除 None 外的 Dither 演算法都能使用強度百分比。
- 2026-07-02: Dither algorithm 切換時必須同時把 `settings.dither.errorStrength` 重設為 `DEFAULT_DITHER_ERROR_STRENGTH`，避免 Error Strength、Dither Strength 與 Dot Density 在不同 algorithm 間繼承數值。
- 2026-06-13: `edit` 的 Original preview 改為使用 `prepare` group operations 產生的 `preparedImageData`，不直接顯示 raw source。
- 2026-06-13: Palette 預設維持 Original；Dither 預設改為 Floyd-Steinberg error diffusion 且 Serpentine 關閉，Dither 啟用時 Palette 不先量化像素。
- 2026-06-13: Original palette 萃取改為使用明暗錨點、灰階錨點、高飽和色相分區與加權填補，避免純頻率排序漏掉視覺重要色。
- 2026-06-14: Dither settings 新增 `colorDistance`，由 `palette-utils` 統一提供 RGB、Manhattan、BT.709 與 CIEDE2000 最近色判斷。
- 2026-06-14: Original palette 萃取改為 RgbQuant-style：8x8 box histogram、hue retention、`initColors: 4096` 候選上限與 BT.709 euclidean palette reduction。
- 2026-06-14: Color Distance 將 RgbQuant-style BT.709 weighted distance 命名為 `euclidean`，`bt709` 舊 id 僅作為相容 alias。
- 2026-06-14: Color Distance 拆成 `euclidean-bt709`、`euclidean-rgb`、`manhattan-bt709`、`manhattan-rgb` 與 `ciede2000`；舊 id 只作為 alias。
- 2026-06-14: Dither settings 新增 `errorStrength`，Error Diffusion 以百分比控制誤差擴散倍率，預設 100%。
- 2026-06-14: Original palette 重新對齊 RgbQuant `method: 2` 的 `buildPal()` 行為，完整排序 2D histogram 後再 reduce，不使用 `initColors` 候選截斷。
- 2026-06-14: 將 RgbQuant 以 MIT vendored library 納入 `src/vendor/`，由 `rgbquant-adapter.js` 統一提供 Original palette 萃取與支援的 Error Diffusion。
- 2026-06-14: Error Strength 對齊 dithering-studio-main，統一以 `errorStrength / 100` 乘上 error diffusion 擴散係數，UI step 為 5%。
- 2026-06-14: Dither 預設改回 `none`，Error Strength UI step 改為 1%。
- 2026-06-14: Error Strength UI step 改為 2%。
- 2026-06-14: Palette color input 的 `input` 事件只同步 state，不排正式 preview；`change` 時才排 preview，避免調色盤互動被 render 打斷。
- 2026-06-14: Palette settings 新增 `originalPaletteSize`，Original palette 可用 Colors 在 2 到 32 色間重新萃取。
- 2026-06-14: Palette swatches 使用 8 欄 grid，限制每列最多 8 個色票。
- 2026-06-14: Dither 預設改回 `floyd-steinberg`。
- 2026-06-14: Dither Serpentine 改用 `panelUtils.toggleSwitchInput()`；Crop 手機版維持 2x2 grid；range input 套用主題 accent 色。
- 2026-06-14: Crop settings 新增 `backgroundPreset` / `backgroundColor`，preview renderer 與正式 crop operation 共用 transform fill color。
- 2026-06-14: Crop preview overlay 改以 canvas rect + `layout.frame` 對齊，修正窄版長條圖框選與正式 crop output 偏移。
- 2026-06-14: 將 edit effects 的 `operation.pipeline.draggable` 改為 `false`，保留 sortable 架構但關閉目前工具列拖曳排序。
- 2026-06-14: 新增控制元件 accent 色，讓 slider 與 Toggle Switch 使用較淡的控制元件狀態色。
- 2026-06-14: Edit preview 的 Original / Result 切換改用 `.setting-choice` radio 結構，對齊 Web Setting Theme 選項。
- 2026-06-15: Crop Fill 新增 `auto` preset，使用低解析 transformed crop frame 邊界取樣估算填色，並用小幅色差穩定化降低旋轉閃爍。
- 2026-06-15: Crop Fill 預設 `backgroundPreset` 改為 `auto`。
- 2026-06-15: Preview stage 透明區域背景 token 改為柔和灰白 5x5 分組細網格 pattern，不再使用 checker token。
- 2026-06-15: Preview stage 網格改為重用既有 surface / border 灰階 tokens，不新增 preview pattern 專用色彩 tokens。
- 2026-06-15: Dither Editor `entry.js` 將 feature scripts 與後續 page scripts 併入同一載入波次，降低 GitHub Pages 首次載入瀑布。
- 2026-06-15: `index.html` 的 classic scripts 改用 `defer`，讓共用基礎檔與 page entries 可並行下載並依序執行。
- 2026-06-15: 將 Crop overlay sizing / positioning 與 pointer mapping 從 `page.js` 拆到 `viewport/overlay-renderer.js` 與 `viewport/pointer-mapper.js`。
- 2026-06-15: 左側 feature dock 圖示改用 `assets/icons/editor/` 的本地 SVG 檔案，取代 ASCII placeholder。
- 2026-06-16: `src/ui/svg-icons.js` 新增共用 SVG icon helper，供 app shell、feature panel、preview toolbar 與 action button 以外部 SVG image 重用本地 SVG。
- 2026-06-16: Header Menu button 改用本地 SVG menu icon 顯示圖示，並繼承目前 theme color。
- 2026-06-16: Tool accordion chevrons 與 Web Setting theme sun / moon icons 改用外部 SVG image，直接引用本地 SVG asset。
- 2026-06-16: Light / Dark theme token 收斂為共享語意色階，移除單一元件專用的 drop、idle status、add 與 scroll hover 色票。
- 2026-06-16: Crop pointer mapper 新增雙 pointer pinch zoom，讓觸控螢幕可用雙指縮放調整 crop zoom。
- 2026-06-16: Palette 新增色票 button 使用圓形外框包住加號；Original Colors 的 unitless number field 必須以獨立 grid 欄保留 stepper 前緩衝，降低窄螢幕誤觸 input。
- 2026-06-18: 正式 preview pipeline 完成後記錄 `previewRenderDurationMs`，由 `page.js` 在 edit Result 圖片右下角顯示 preview 計時 label，並由 `SHOW_PREVIEW_TIMING_LABEL` 控制顯示。
- 2026-06-19: preview 計時 label 改由 `previewTimingLabel.phase` 管理 rendering/done/hidden，完成後由 controller 依設定延遲排程自動隱藏。
- 2026-06-19: preview 計時 label 自動隱藏時只更新 label DOM，不可觸發整頁 render，避免關閉正在操作的 panel form。
- 2026-06-22: Edit Result preview canvas 不使用 `image-rendering: pixelated` 縮小 dither 結果；縮小顯示交給瀏覽器正常重採樣，避免 preview alias 與 export PNG 觀感大幅落差。
- 2026-06-18: `pipeline-runner.js` 新增可選 Stage Cache；controller 在 preview、prepared original 與 live preview base 使用同一份 in-memory cache，換圖與銷毀時清空，Export 維持完整正式 pipeline 重跑。
- 2026-06-18: RgbQuant adapter 限縮為 Original palette 萃取入口；Error Diffusion 改由專案內建 processor 執行，並參考 dithering-studio-main 將 hot loop 改為 typed array、本地 nearest-index palette search 與 Floyd-Steinberg fast path。
- 2026-06-18: Error Diffusion 內建 processor 在擴散誤差寫回工作緩衝時必須 clamp 到 `0..255`，避免 Error Strength 增大時累積誤差爆掉造成破圖。
- 2026-06-18: Dither 演算法改為使用 `dither-algorithm-registry.js` 註冊；演算法 metadata 指向 processor id，Dither feature 不再硬寫 ordered / pattern / error diffusion 分派。
- 2026-06-18: Dither 演算法精簡為 Floyd-Steinberg、Atkinson、Jarvis-Judice-Ninke、Sierra Lite、Stevenson-Arce、Adaptive FS 3x3、Bayer 4x4、Bayer 8x8、Blue Noise 64、Dot Diffusion 8x8 與 Dot Halftone。
- 2026-06-19: Palette Mapping 改為 dither strategy 介面，processor 只呼叫 `mapColor()` / `mapThresholdColor()`，不再依 `pair-mix` / `tri-mix` id 分支。
- 2026-06-19: Dither CPU hot path 新增保守快取：Dot Diffusion 預算 class recipient offsets、generic Error Diffusion 快取 matrix offsets、Ordered / Pattern threshold lookup 使用預先攤平表，不改演算法 kernel 或 threshold 語意。
- 2026-06-19: 新增 `threshold-dither-processor.js` 作為 WebGL threshold dither fast path；Ordered / Dot Halftone 類在 Nearest Color 與 Pair Mix mapping 可走 GPU，Tri Mix 或 WebGL 不可用時保留 CPU fallback。
- 2026-06-19: Tri Mix CPU hot path 預先列出 top-6 candidate 的三色組合並攤平 barycentric loop；不得改 top candidate 數量、組合順序、權重 clamp/normalize 或 threshold 選色規則。
- 2026-06-19: 新增共用 Palette Mapping 層，支援 Nearest Color 與 Pair Mix；Algorithm metadata 不再註冊 Pair Mix 組合項。
- 2026-06-27: MVP 移除未完成的 IndexedDB workspace 持久化路徑；`settings-store.js` 只透過 localStorage 保存 app shell preference/theme，Dither Editor 工作圖片與 pipeline/settings 僅透過 page module in-memory cache 在同一次 SPA 頁面切換期間保留。

## Plug-and-Play 架構要求

Dither Editor 的 feature 必須是 plug-and-play 架構。這裡的 plug-and-play 不是只把檔案拆開，而是讓每個 feature 擁有明確邊界與單一入口：

- 新增 feature 時，預設只新增一個 feature entry，並在 `feature-manifest.js` 啟用。
- 停用 feature 時，預設只把 `feature-manifest.js` 裡的該 feature 設為 `enabled: false`。
- 移除 feature 時，預設只移除 manifest entry 與該 feature entry；其他共用檔不應殘留該 feature 的硬編碼 id、panel builder、image-loaded hook、settings default 或 pipeline order。
- `entry.js` 只負責載入 registry 與 manifest 解析後的 feature scripts，不可直接列出某個 feature 的內部檔案。
- `page.js` 只根據 registry 產生工具列、action、panel，不可保存另一份 feature 清單。
- `controller.js` 只 dispatch lifecycle hook，不可為 `crop`、`resize`、`palette` 等單一 feature 補特殊流程。
- `state.js` 只由 enabled features 建立 settings 與 pipeline，不可手寫 feature id 清單。
- `pipeline-presets.js` 可以覆蓋順序或 enabled 狀態，但不可成為第二份 feature manifest。
- feature 之間不可直接讀寫 `state.settings.<其他 feature>`；跨 feature 查詢一律透過 `featureRegistry.api(id)` 取得對方宣告的 `api` 物件，api 回 null 時呼叫端必須降級。

驗收標準：

- 把 `Crop` 設成 `enabled: false` 後，工具列、settings、pipeline 都不應再出現 `crop`。
- 新增一個 draggable effect feature 後，不修改 `page.js` 或 `controller.js` 也應能顯示、設定、拖曳與參與 preview。
- 刪除某個已從 manifest 移除的 feature entry 後，專案內不應再有必須同步刪除的硬引用。

## 技術原則

新專案採用純瀏覽器架構：

```text
HTML + CSS + classic JavaScript scripts + Canvas API
```

運行限制：

- 不使用後端。
- 不使用 React、Vue、TypeScript 或其他前端框架。
- 不使用 CDN。
- 不在 runtime 下載外部資源。
- demo 圖片、圖示、字型、樣式都必須保留在專案內；demo 不可由 runtime 程式臨時產生。
- 使用者必須能直接雙擊 `index.html` 使用，不可要求另外執行 `python -m http.server` 或其他本機指令。
- 開發時可用 VS Code Live Server 預覽，但正式使用方式不能依賴 Live Server。
- 原始專案必須不依賴 build step；不可要求 npm install、npm run build 或 bundler 才能使用。可提供選用的 Python/Make 發佈 build，輸出到 ignored `build/` 底下的時間戳子資料夾，只做 server/device 靜態檔案複製、minify 與 gzip-only 輸出，不改變 runtime 載入架構。minify 與 gzip 預設都必須啟用，且必須能用 CLI 參數分別關閉；啟用 gzip 時，輸出資料夾內只保留 gzip 後的 `.gz` 檔，不保留同名未壓縮檔。同一秒內多次 build 不可覆蓋既有輸出。
- MVP 以 Standalone Mode 為主；下一版可加入 ESP32 Device Mode。

因為要支援直接雙擊 `index.html`，不要使用 JavaScript ES Modules 的 `import` / `export`。多檔案仍然可以拆分，但要用 classic `<script>` 依序載入，並透過單一 namespace 暴露模組。

建議 namespace：

```js
window.DitherApp = window.DitherApp || {};
window.DitherApp.core = window.DitherApp.core || {};
window.DitherApp.ui = window.DitherApp.ui || {};
window.DitherApp.pages = window.DitherApp.pages || {};
```

允許使用的瀏覽器 API：

- `FileReader`
- `Blob`
- `URL.createObjectURL`
- `createImageBitmap`
- `fetch`，只能用於讀取專案內同源 demo assets，不可用於第三方 API 或遠端圖片
- `HTMLCanvasElement`
- `CanvasRenderingContext2D`
- `ImageData`
- `DragEvent`
- `PointerEvent`
- `localStorage`
- `Web Worker`，第二階段後再加入

## 頁面切換架構

為了未來更換主頁風格或切換成其他頁面，頁面 shell 與功能頁要分離。

建議概念：

```text
app-shell
  header
    title
    status
    menu-button
  page-host
    dither-editor-page
      dither-editor-panel
      dither-preview-panel
    web-setting-page
    help-page
    about-page
```

`app-shell` 只處理：

- 標題區。
- 右上選單。
- 頁面切換。
- browser history / back / forward 對應頁面切換。
- 全域主題。
- page mount / unmount。

`dither-editor-page` 才處理：

- 圖片狀態。
- 編輯區。
- 圖片呈現區。
- Dither pipeline。

頁面必須透過一致介面被外層呼叫：

```js
(function (app) {
    app.pages = app.pages || {};

    app.pages.ditherEditorPage = {
        id: 'dither-editor',
        title: 'Dither Image Editor',
        mount(container, appContext) {},
        unmount() {},
    };
})(window.DitherApp);
```

這樣未來要換主頁風格時，只要重寫 shell 或 CSS，不必改 Dither 頁面內部、演算法與圖片處理模組。

### Browser History 與 SPA Router

此專案是純前端 SPA，Menu 切頁不能只改記憶體狀態，否則瀏覽器上一頁/下一頁無法回到前一個頁面。

Router 必須負責同步 browser history：

- app start 時，router 讀取目前完整 URL hash route，例如 `#/dither-editor`、`#/web-setting`、`#/help`、`#/help/dithering/error-diffusion`、`#/about`。
- 如果 URL 沒有 hash，預設進入 `#/dither-editor`。
- route 第一段是 page id；其餘段落交給該 page 解讀。第一段對應不到已註冊頁面時回退到 `#/dither-editor`。
- 使用 Menu 切頁時，router 必須透過 `history.pushState()` 寫入新頁面狀態。
- 初次進入或需要校正 URL 時，router 應使用 `history.replaceState()`，避免多塞一筆無意義 history。
- 使用者按瀏覽器上一頁/下一頁時，router 必須監聽 `popstate`，並依目前 history state 或 hash 重新 mount 對應頁面。
- `popstate` 觸發的頁面切換不可再次 `pushState()`，避免 history 堆疊重複。
- 同一個 page 內切換子 route 時不可先 unmount / mount 整頁；若 page 提供 `onRouteChange(route, context)`，router 應更新 `context.route` 後呼叫它。
- app context 必須提供共用 `navigate(route, options)` 與 `currentRoute()`，讓子文件仍透過唯一 router 寫入 history，不自行註冊第二套全域路由。

建議 URL 格式：

```text
#/dither-editor
#/web-setting
#/help
#/help/introduction
#/help/quick-start
#/help/dithering
#/help/dithering/error-diffusion
#/help/dithering/ordered
#/help/dithering/dot
#/help/palette-mapping
#/help/color-distance
#/about
```

Router 保存目前 route、由第一段解析出的 page id 與 browser history 狀態，不保存 Dither Editor 的圖片、settings、pipeline 或 canvas 內容。Dither Editor 切頁後回來的工作區保留，仍由 `pages/dither-editor/page.js` 的 page-specific cache 負責。

### 跨頁共用原則

新增頁面時，必須共用外層能力，而不是把功能改寫到同一個 canvas 或同一個頁面 controller 裡。

依賴方向固定如下：

```text
main -> app -> pages -> ui
              pages -> core
              pages -> utils
```

禁止反向依賴：

```text
core -> pages
core -> app
ui -> pages
ui -> pages/dither-editor/dither
app -> pages/dither-editor internals
```

也就是說：

- `app` 可以載入頁面模組，但不能知道某頁內部有幾個 canvas、幾個 panel、幾個 operation。
- `pages/*` 可以使用 `core` 和 `ui` 組出自己的功能。
- `core` 只提供資料處理能力，不知道任何頁面存在。
- `ui` 只提供可重用互動元件，不知道 Dither、Web Setting、Help、About 或其他頁面的業務語意。
- 每個頁面自己擁有自己的 DOM 容器與 canvas lifecycle。
- `index.html` 只載入 app shell、共用基礎檔與每個 page 的 `entry.js`。頁面內部 script 載入順序由該 page 自己的 `entry.js` 管理。

### Page Entry 命名規則

每個頁面資料夾的入口檔一律命名為 `entry.js`，頁面主體一律命名為 `page.js`。

```text
src/pages/{page-id}/
  entry.js
  page.js
  state.js
  controller.js
  constants.js
```

規則：

- `entry.js` 負責載入該頁自己的 config、algorithms、feature manifest、feature registry、enabled feature scripts、viewport、controller、state 與 `page.js`；feature scripts 必須從 `feature-manifest.js` 經由 `feature-registry.js` 解析產生，不直接逐一硬寫 operation 或 panel 檔。
- `constants.js` 必須在 feature scripts 前載入，讓 feature 可安全讀取頁面級限制值與預設目標，例如 resize output size limit、default dither algorithm id。
- `entry.js` 最後負責讓頁面模組可被 app registry 註冊。
- `page.js` 只負責該頁的 mount / unmount 與頁面 DOM 組合。
- `index.html` 不可直接列出某頁內部的 operation、algorithm、panel、viewport 檔案。
- 新增頁面時，第一時間只需要找 `src/pages/{page-id}/entry.js`。
- 若 IDE 開啟多個 `entry.js`，以完整路徑區分頁面；不要為了 IDE tab 名稱破壞命名規則。

## 建議專案結構

這是 target blueprint，不代表目前 repository 已經實作所有列出的檔案。缺少的 future path 不應只為了符合本章節而建立；只有在實作對應功能時才新增。

```text
embedded-web-dithering/
  index.html
  assets/
    demo/                 # built-in demo image assets
    icons/
    styles/
      base.css
      layout.css
      components.css
      themes.css
      device.css
  src/
    vendor/
      rgbquant.js
      rgbquant.LICENSE.txt
    namespace.js
    main.js
    i18n/
      en.js
    app/
      app-shell.js
      app-menu.js
      app-state.js
      project-capabilities.js
      page-router.js
      page-registry.js
    pages/
      dither-editor/
        entry.js
        page.js
        state.js
        editor-mode-state-machine.js
        feature-manifest.js
        feature-registry.js
        actions.js
        events.js
        controller.js
        constants.js
        config/
          palette-presets.js
          dither-algorithms.js
          pipeline-presets.js
          display-profiles.js
        features/
          input-feature.js
          crop-feature.js
          resize-feature.js
          adjust-feature.js
          palette-feature.js
          dither-feature.js
          export-feature.js
        operations/
          operation-registry.js
          pipeline-runner.js
        dither/
          rgbquant-adapter.js
          dither-matrices.js
          error-diffusion.js
          ordered-dither.js
          pattern-dither.js
        gpu/
          adjust-processor.js
          threshold-dither-processor.js
        panel-utils.js
        viewport/
          viewport-controller.js
          viewport-renderer.js
          overlay-renderer.js
          pointer-mapper.js
      web-setting/
        entry.js
        page.js
      help/
        entry.js
        document-manifest.js
        content-model.js
        validation.js
        visuals.js
        page.js
        i18n/
          en.js
          zh-TW.js
      about/
        entry.js
        page.js
      device-info/
        entry.js
        page.js
      device-network/
        entry.js
        page.js
        wifi-form.js
        wifi-controller.js
        scan-dialog.js
      device-system/
        entry.js
        page.js
    core/
      storage/
        storage-keys.js
        settings-store.js
      image/
        image-loader.js
        image-document.js
        image-exporter.js
      canvas/
        canvas-utils.js
        image-data-utils.js
      color/
        color-utils.js
        palette-utils.js
      encoders/
        png-encoder.js
        device-output-encoder.js
    device/
      device-api.js
      device-live.js
      device-gate.js
      device-auth.js
    ui/
      button.js
      dropdown-menu.js
      dropzone.js
      slider.js
      select-field.js
      color-swatch.js
      svg-icons.js
      sortable-list.js
      modal.js
      notice.js
      password-field.js
      tooltip.js
      toggle.js
    utils/
      dom.js
      events.js
      math.js
      naming.js
```

### Dither Editor 檔案分類規則

`src/pages/dither-editor/` 根目錄只放頁面級協調檔：`entry.js`、`page.js`、`state.js`、`controller.js`、`constants.js`、`feature-manifest.js`、`feature-registry.js`、`editor-mode-state-machine.js` 與 `panel-utils.js`。如果檔案只服務單一 feature，預設不放在根目錄。

- `features/` 是 feature 的單一 ownership 邊界。每個 `*-feature.js` 應集中保存該 feature 的 panel builder、settings default、operation、feature hooks、dock metadata 與 `panelGroup`。不應另建平行的 `panels/` 目錄存放 feature panel，避免停用或移除 feature 時需要同步多個位置。
- `config/` 只放開發者可擴充設定，例如 palette preset、dither algorithm、pipeline preset、display profile；不放使用者目前工作區 state。
- `operations/` 只放跨 feature 的 operation registry 與 pipeline runner；單一 feature 的 operation implementation 預設留在該 feature script。
- `dither/` 放 dither 演算法核心與矩陣資料，不處理 DOM、feature registration 或 editor state。
- `gpu/` 放可選硬體加速 processor，例如 WebGL adjust processor 與 threshold dither processor。GPU processor 必須有 CPU fallback，且不應直接操作 tool panel 或 editor mode。
- `viewport/` 放 canvas render、overlay render、座標轉換與 preview viewport 相關邏輯；page 仍負責 DOM mount 與工具列組合。Crop overlay 尺寸/定位必須由 `viewport/overlay-renderer.js` 管理，overlay pointer / wheel 到 crop pan/zoom 的換算必須由 `viewport/pointer-mapper.js` 管理。
- `viewport/pointer-mapper.js` 必須把單一 pointer 拖曳轉成 crop pan、wheel 轉成 crop zoom、兩個 active pointers 的距離變化轉成 crop zoom。進入雙指縮放時應暫停單指 drag；縮放結束且仍剩一個 pointer 時可回到拖曳 pan。
- `src/ui/svg-icons.js` 是唯一 SVG icon loading helper；它只把本地 SVG path 設為外部 `<img src>`，不可保存完整 SVG path data、不可 runtime `fetch()` SVG。`index.html` 不應直接保存完整 SVG symbol/path 資料。
- `assets/icons/editor/` 放 Dither Editor 專用本地 SVG icon；feature dock 可用 `iconPath` 指向此目錄的 SVG 檔，page 透過 `src/ui/svg-icons.js` 顯示圖示並保留 `icon` 文字作為 fallback。SVG 應使用 24x24 viewBox，不硬寫顯示尺寸。
- `assets/icons/app/` 放 app shell 或全站設定使用的本地 SVG icon；執行時透過外部 SVG image 顯示，不從檔案 fetch。
- `src/vendor/` 只放第三方程式碼與對應授權檔。Feature 不應直接依賴 vendor 全域物件，必須透過頁面 adapter 或 core wrapper 存取。
- 空目錄不應保留作為未來分類提示；需要對應功能時再建立實際檔案與規格。

## 命名與 Coding Style

命名必須統一，避免同一專案內混用大小寫風格。

### 檔案命名

全部使用 kebab-case：

```text
editor-state.js
dither-feature.js
palette-presets.js
viewport-renderer.js
```

不要混用：

```text
EditorState.js
editorState.js
dither_operation.js
```

### 函式與變數命名

使用 camelCase：

```js
function applyDither(input) {}
const activeOperationId = 'crop';
let previewImageData = null;
```

### Class 命名

使用 PascalCase：

```js
class EditorController {}
class ViewportRenderer {}
```

### 常數命名

全域不可變常數使用 SCREAMING_SNAKE_CASE：

```js
const MAX_IMAGE_SIZE = 4096;
const DEFAULT_PIPELINE_ORDER = ['crop', 'resize', 'adjust', 'palette', 'dither'];
```

模組內一般設定可用 camelCase：

```js
const defaultDitherOptions = {
  mode: 'none',
  algorithm: 'none',
};
```

### CSS 命名

使用 kebab-case class name，並以區域作前綴：

```css
.app-header {}
.page-host {}
.dither-editor-page {}
.dither-editor-panel {}
.dither-preview-panel {}
.pipeline-list {}
```

### Theme CSS

全站 light / dark theme 必須透過 `assets/styles/themes.css` 的 CSS variables 管理。

規則：
- `themes.css` 定義 `:root` / `body[data-theme="light"]` / `body[data-theme="dark"]` 的顏色 token。
- theme token 應優先維持跨元件語意層級；單一元件若可由既有 text、surface、border、accent、danger 或 status token 表達，不應新增專用色票。
- `base.css` 只處理全域元素、字體、body 背景與基本表單繼承。
- `layout.css` 只處理 app shell、page layout、preview stage、scrollbar gutter 等結構。
- `components.css` 只處理 panel、button、tool row、dropzone、menu、setting choice 等可重用元件。
- component 不應直接硬寫大面積 `#ffffff`、`#172026`、固定 rgba 邊框或陰影；應使用 `--color-*` token。
- 切換黑暗模式只改 `body[data-theme]`，不應重建 Dither Editor page，也不應重新跑 pipeline。
- theme 只影響 UI chrome，不改 canvas 內的影像處理結果。

### 程式風格

採用目前專案 `.prettierrc` 的方向：

- 4 spaces indentation。
- single quote。
- semicolon。
- `printWidth` 100。
- function 小而明確。
- 不在演算法模組中讀取 DOM。
- 不在 UI 模組中直接改 `ImageData`。
- 不使用 `import` / `export`，避免雙擊 `index.html` 時被瀏覽器 module/CORS 限制擋住。
- 每個檔案只掛載到 `window.DitherApp` namespace 下，不建立其他全域變數。
- 生成程式時必須寫好必要註解，說明非直覺的狀態流、DOM 掛載契約、canvas lifecycle、pipeline 順序與演算法取捨。
- 註解應該解釋「為什麼」或「這段如何與架構契約互動」，不要重複描述語法本身。
- 若某個 `id`、class、`data-*` 屬性會被 JavaScript 查找或被 CSS responsive layout 依賴，應在建立處或鄰近處加註解標明用途。

### Script 載入順序

因為原始專案必須支援直接雙擊 `index.html`，`index.html` 必須用 deferred classic scripts 載入共用基礎檔與 page entries，讓瀏覽器可並行下載並仍依 HTML 順序執行。單一頁面內部需要的 scripts 不應攤平在 `index.html`，必須交給該頁的 `entry.js` 管理。選用的發佈 build 不可 bundling 或改寫此載入模型。

示意：

```html
<script defer src="src/namespace.js"></script>
<script defer src="src/i18n/en.js"></script>
<script defer src="src/i18n/zh-TW.js"></script>
<script defer src="src/i18n/index.js"></script>
<script defer src="src/utils/dom.js"></script>
<script defer src="src/ui/svg-icons.js"></script>
<script defer src="src/core/canvas/canvas-utils.js"></script>
<script defer src="src/ui/sortable-list.js"></script>
<script defer src="src/ui/modal.js"></script>
<script defer src="src/ui/notice.js"></script>
<script defer src="src/ui/password-field.js"></script>
<script defer src="src/ui/select-field.js"></script>
<script defer src="src/device/device-api.js"></script>
<script defer src="src/device/device-live.js"></script>
<script defer src="src/device/device-gate.js"></script>
<script defer src="src/device/device-auth.js"></script>
<script defer src="src/app/project-capabilities.js"></script>
<script defer src="src/app/page-registry.js"></script>
<script defer src="src/pages/dither-editor/entry.js"></script>
<script defer src="src/pages/device-info/entry.js"></script>
<script defer src="src/pages/device-network/entry.js"></script>
<script defer src="src/pages/device-system/entry.js"></script>
<script defer src="src/pages/web-setting/entry.js"></script>
<script defer src="src/pages/help/entry.js"></script>
<script defer src="src/pages/about/entry.js"></script>
<script defer src="src/app/app-shell.js"></script>
<script defer src="src/main.js"></script>
```

`entry.js` 可以透過動態插入 classic `<script>` 的方式載入該頁檔案；同批 scripts 可一次插入以便瀏覽器並行下載，但每支 script 必須設為 `async = false`，維持 classic script 依插入順序執行。Dither Editor 應先載入可解析 feature manifest 的 bootstrap scripts，再把 feature scripts 與後續 page scripts 併入同一批載入，避免 GitHub Pages 上出現不必要的序列化網路波次。不得使用 ES Modules `import` / `export`，也不得用會被 `file://` CORS 擋住的 template/script `fetch()` 作為唯一載入方式。

`index.html` 必須在外部 scripts 執行前建立 `DitherApp.startupGate`、loading overlay，並讓 `#app` 帶有 `inert` 與 `aria-hidden="true"`。Overlay 使用 `inset: 64px 0 0` 保留 header 與 App title，Light / Dark theme 分別由 `--color-loading-overlay` 提供半透明背景，讓已建立 UI 可辨識但不可操作。startup gate 同時以 `body.is-app-loading #app { pointer-events: none; }` 作為 pointer fallback；ready 前不可只靠 overlay 遮擋視覺而讓鍵盤仍能聚焦下層 controls。

`main.js` 先本地化 startup gate，並透過 `app.app.applyShellCopy()` 同步 header App title 與 Menu placeholder。其後 startup ready 順序固定為：`whenPageEntriesReady()` resolve → `AppShell.start()` 完成預設頁 mount → `#app` 內當下所有 `<img>` load/decode settled → 兩次 `requestAnimationFrame` → `startupGate.complete()`。Image decode failure 必須視為 settled，避免單一 icon 404 永久鎖住 App；Demo source、worker 與互動後才建立的資源不可加入 startup wait。

`startupGate.setProgress()` 只接受單調遞增的整數百分比。`script-loader.js` 對新建的動態 script 登記 total，並在每支 script `load` 後讓啟動前段增加一個百分點（上限 60%）；後續由 `main.js` 在 page entries ready、AppShell mount、initial image settle 與 paint 階段推進至 100%。此百分比代表啟動階段完成度，不代表下載 bytes。

Startup 期間的 direct script、stylesheet、unhandled runtime error 或 60 秒 timeout 必須呼叫 `startupGate.fail()`。Error state 保留 `#app` inert、停止 spinner、隱藏 progressbar、以 i18n 顯示通用載入失敗文字與 Reload button；startup ready 後的 runtime error 不可重新開啟 gate。

每個檔案使用 IIFE 或清楚的 namespace assignment：

```js
(function (app) {
    app.core = app.core || {};

    app.core.applyDither = function applyDither(imageData, options) {
        return imageData;
    };
})(window.DitherApp);
```

## Editor State

專案必須有單一 editor state。DOM 是 state 的呈現，不是主要資料來源。

資料形狀：

```js
const editorState = {
    schemaVersion: 1,
    status: 'empty',
    mode: 'source',
    sourceImage: null,
    sourceImageData: null,
    preparedImageData: null,
    previewImageData: null,
    previewRenderDurationMs: null,
    previewTimingLabel: {
        phase: 'hidden',
        durationMs: null,
    },
    outputImageData: null,
    openToolPanels: {
        input: true,
    },
    viewMode: 'result',
    viewport: {
        zoom: 1,
        panX: 0,
        panY: 0,
    },
    pipeline: {
        fixedBefore: buildPipelineFromEnabledFeatures('fixedBefore'),
        effectsOrder: buildPipelineFromEnabledFeatures('effectsOrder'),
        fixedAfter: buildPipelineFromEnabledFeatures('fixedAfter'),
        enabled: buildPipelineEnabledMapFromEnabledFeatures(),
    },
    settings: buildDefaultSettingsFromEnabledFeatures(),
};
```

`pipeline` 與 `settings` 不可在 `state.js` 手動列出 `crop`、`resize`、`adjust` 等 feature id。它們必須由 `feature-registry.js` 依照 enabled features 建立：

- feature 停用後，不產生該 feature 的 settings key、pipeline order 或工具列項目。
- feature 新增後，只要 manifest 啟用且 feature contract 合法，就自動建立 default settings。
- Tool Panel 開啟狀態必須以 feature id 儲存在 state，例如 `openToolPanels[featureId] = true`；不可只用單一 `activeTool` 表示所有面板開合。
- Dither Editor 頁面在 app menu 切到 `Web Setting`、`About`、`Help` 或其他頁面再切回來時，必須保留 editor state、圖片與目前 preview。此保留屬於頁面模組層級的 in-memory state cache，不是 IndexedDB 持久化。
- `page.js` 在 `unmount()` 時應保存目前 `controller.state`；下次 `mount()` 時應把保存的 state 以 `initialState` 傳回 controller。第一次進入 Dither Editor 且沒有 cached state 時，必須停在 `source` group 且沒有來源圖片，不可自動建立 `New Image`。
- 若切頁時狀態停在 `loading-image`、`processing-preview` 或 `exporting` 這類 transient status，回到 Dither Editor 時應正規化或重新排 preview，避免畫面卡在不可完成的中間狀態。
- 若未來重新加入 workspace 持久化並需要載入舊文件，已不存在的 feature settings 不可讓頁面 crash；應由 migration 忽略、保留到 unknown 區，或交給對應 feature 處理。

### Editor Mode State Machine

`mode` 是目前啟用的 feature group，`status` 是 transient 執行狀態。Group 轉換必須集中在 `editor-mode-state-machine.js` 與 controller 方法中，不應散落在 feature panel event handler。

Groups：

- `source`：來源輸入 group。沒有來源圖片時只展開 `panelGroup: 'source'` 的 tool，且只有 `source` tool 可操作；右下角 preview toolbar 不可顯示任何按鈕。Preview stage 必須顯示中央 upload dropzone，支援 drop 與 Browse File；Image Input panel 不應重複顯示 Choose/Drop controls。有來源圖片時手動回到 `source` group 必須收合其他 group，preview toolbar 不顯示按鈕但保留 toolbar 高度。
- `prepare`：正式編輯前準備 group。有來源圖且只展開 `panelGroup: 'prepare'` 的 tool；目前 Crop feature 是唯一的 `prepare` tool。`source`、`prepare` 與 `edit` tool rows 可操作；Preview 顯示 `sourceImageData` 加上 Crop transform，不跑完整 pipeline，也不套用 Resize、Adjust、Palette、Dither；右下角 preview toolbar 只能顯示 `+`、`-`、OK。
- `edit`：有來源圖且 Crop 收合。Preview / Export 使用正式 pipeline，右下角 preview toolbar 只能顯示 Original、Result、Expand。從 Crop 收合或 OK 進入 `edit` 時，應展開目前 enabled dock tools 中明確宣告 `panelGroup: 'edit'` 的 panel group。
- `none`：無面板流程歸屬。feature 未宣告 `panelGroup` 時預設屬於 `none`，不顯示在左側 tool dock，也不作為可切換的 editor mode。

轉換規則：

- 成功載入本機圖片或 demo 時，controller 必須重建 default editor state、清掉上一張圖的 settings/pipeline order/live preview 暫態，再寫入新 `sourceImageData` 並進入 `prepare`；若沒有 enabled `prepare` feature，則直接進入 `edit` 並排程正式 preview。
- 重新載圖成功後，Resize、Adjust、Palette、Dither 等演算法 settings 必須回到 enabled feature default；不可沿用上一張圖的值。
- 展開 `prepare` tool 必須進入 `prepare`；收合 prepare tool 或按 OK 必須進入 `edit` 並排程正式 preview。
- 成功載圖或手動展開 Crop 時，`editor-mode-state-machine.js` 必須透過 feature registry 查詢 `prepare` panel group，並將 `openToolPanels` 設為只包含該 group；不可保留 Image Input 或其他 edit panel 的展開狀態。
- 收合 Crop 或按 OK 進入 `edit` 時，`editor-mode-state-machine.js` 必須透過 feature registry 展開 enabled dock tools 中的 `edit` panel group，不可硬寫特定 feature id 清單。
- 在 `prepare` 點選 `panelGroup: 'edit'` 的 tool row 時，controller 必須透過 state machine 離開 `prepare`，收合 source/prepare panels，只展開被點選的 edit panel，並排程正式 preview。
- 已有圖片時手動展開 source tool 必須收合其他 tool panel；若當下是 `prepare`，controller 必須先透過 state machine 離開 prepare group，再排程正式 preview。
- `prepare` 中的 Crop setting 變更只能重畫 crop preview，不可排程完整 pipeline。離開 `prepare` 後才依目前 settings 跑正式 preview。
- `prepare` 中的 setting guard 必須依 feature 的 `panelGroup` 判斷可用 settings group，不可用 `group === 'crop'` 這類固定 id 比對。
- 沒有來源圖片或目前 mode 不允許的 tool/action event 必須被 controller guard 掉，即使 DOM disabled 被繞過也不可改 settings、reorder effects 或 export。
- `page.js` 必須只根據 `mode` 決定 preview toolbar 內容：`source` 隱藏所有 button rows，且已有來源圖片時保留空 toolbar 高度；`prepare` 只顯示 Crop 控制列，`edit` 只顯示 Original / Result / Expand 切換列。
- `page.js` 的 tool button handler 應只呼叫 controller 或 state machine 的語意入口（例如 open source panel、open prepare mode、close prepare mode），並透過 `panelGroup` 判斷流程入口；不應在一般 feature panel event handler 中分散實作模式切換規則。
- 非目前模式的 preview toolbar row 必須使用 `hidden` 真正移出 layout，不可只做 disabled 或透明處理。
- `edit` 的 Original / Result / Expand buttons 必須共用固定尺寸設定；`prepare` 的 Crop zoom `+` / `-` buttons 使用 compact square size，OK button 使用 primary action size。

`schemaVersion` 必須用於 `settings-store.js` 的 localStorage 資料。讀取儲存資料時：

- schemaVersion 相同：正常載入。
- schemaVersion 不存在、不相同或解析失敗：回退到 default app shell preference。
- Dither Editor 工作圖片與 pipeline/settings 不從 localStorage 或 IndexedDB 還原。

## Preset 與演算法擴充

Preset 與演算法不做成使用者可見的 `Preset Manager` 頁面。它們是 Dither Editor 的開發者擴充點，透過 `src/pages/dither-editor/config/*` 和 registry 管理。

目標：

- 新增 palette preset 時，不需要改 UI panel 邏輯。
- 新增 dither algorithm 時，不需要改 dither panel 的選項生成邏輯。
- 新增 effect feature 時，只新增對應 `features/*-feature.js` 並在 `feature-manifest.js` 啟用，再讓 effects stack 自動可用。
- 預設 pipeline preset 由 config 管理，方便未來加入 e-paper、GameBoy、monochrome 等工作流。

### Config 檔案

```text
src/pages/dither-editor/config/
  palette-presets.js
  dither-algorithms.js
  pipeline-presets.js
```

`palette-presets.js`：

```js
(function (app) {
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};

    app.pages.ditherEditor.config.palettePresets = [
        {
            id: 'monochrome',
            labelKey: 'paletteMonochrome',
            colors: [
                { r: 0, g: 0, b: 0 },
                { r: 255, g: 255, b: 255 },
            ],
        },
    ];
})(window.DitherApp);
```

Palette feature 必須把 preset 與使用者自訂色票分清楚：

- 固定 preset 只來自 `palette-presets.js`。
- MVP 內建 fixed presets 至少包含 `monochrome`、`game-boy`、`warm-ink`、`e6-color-epaper`；`e6-color-epaper` 使用黑、白、紅、黃、藍、綠六色色票。
- `Original` 是 Palette 的預設選項，色票從 `sourceImageData` 跑過 `prepare` group 後的 ImageData 萃取，也就是 Crop 後的裁切範圍；不可包含 Resize、Adjust、Palette、Dither 或其他 edit / export pipeline step。
- 離開 `prepare` 並進入 `edit` 時，Palette feature 必須重新跑 `prepare` group 取得最新裁切輸出再萃取 `originalPalette`；Crop zoom/pan 等 prepare setting 變更期間不可即時重算 Original palette。
- `Original` palette 萃取必須透過 `rgbquant-adapter.js` 呼叫 vendored RgbQuant，設定使用 `colors: settings.originalPaletteSize`、`method: 2`、`boxSize: [8, 8]`、`boxPxls: 2`、`minHueCols: 2000` 與 `colorDist: 'euclidean'`。`ditherit-v2` options 雖包含 `initColors: 4096`，但 RgbQuant `method: 2` 的 `buildPal()` 實際使用完整 2D histogram，不以 `initColors` 截斷候選。
- `originalPaletteSize` 預設為 `8`，允許範圍為 `2..32`，面板必須在 Preset row 下方用短寬度、靠左的 `unitNumberInput` 呈現 Colors；當 `presetId` 不是 `original` 時，此控制必須隱藏。此 unitless control 必須保留 input 與 stepper 之間的固定緩衝欄，避免窄螢幕調整上下值時誤點到數字輸入。
- `Original` 只負責顯示裁切範圍代表色並同步給 `Dither`，palette operation 不主動改變圖片。
- `Custom` 只代表目前 settings 中的色票陣列，不應被加入 `palette-presets.js`。
- 選擇固定 preset 時，feature 應複製 preset colors 到目前 settings，避免使用者後續編輯污染 config。
- `Palette` 不提供 `Quantize` 開關；Dither 啟用且 Dither operation 未被停用時，palette operation 不先量化像素，只同步有效色票給 Dither。
- Dither 為 `none` 或 Dither operation 被停用時，選擇固定 preset 或 `Custom` 後，palette operation 直接把像素映射到目前色票中最接近的顏色。
- 使用者新增、刪除或完成編輯色票後，feature 應把 `presetId` 設為 `custom`，排程 preview，並讓 `Dither` 使用同一份有效 palette。
- 原生 color picker 的 `input` 事件只能更新 palette state 與 Dither palette，不應排程 preview 或重建色票 DOM；`change` 事件才排正式 preview。
- `.palette-swatches` 必須用 8 欄 grid 排列，讓每列最多 8 個色票或新增按鈕；新增按鈕必須維持與色票同尺寸的圓形外框加號 affordance。
- 色票陣列為空時，feature 應回到 `presetId: 'original'`；`Original` 不主動改圖。
- `palette-utils` 必須集中管理 palette 最近色判斷；預設使用 RgbQuant-style Euclidean BT.709 distance，並支援由 Dither settings 指定其他 Color Distance。

`dither-algorithm-registry.js` / `dither-algorithms.js`：

```js
(function (app) {
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};

    app.pages.ditherEditor.ditherAlgorithmRegistry.register({
        id: 'floyd-steinberg',
        labelKey: 'algorithmFloydSteinberg',
        processorId: 'error-diffusion',
        matrixId: 'floydSteinberg',
        supportsSerpentine: true,
        supportsErrorStrength: true,
    });
})(window.DitherApp);
```

Dither algorithm 必須透過 `ditherAlgorithmRegistry.register()` 註冊 metadata，並指定已註冊的 `processorId`。Dither processor 必須透過 `ditherAlgorithmRegistry.registerProcessor({ id, apply })` 註冊共同介面；`apply(imageData, options, algorithm)` 必須回傳新的 `ImageData` 或原圖。新增 matrix-only 演算法時，通常只需要新增 matrix、演算法註冊 entry 與 i18n label；新增新型演算法時，新增 processor script 並註冊 processor，不應修改 `dither-feature.js` 的分派邏輯。

Dither feature 傳給 processor 的 `serpentine` 必須尊重 algorithm metadata；只有 `supportsSerpentine === true` 的演算法可收到 `serpentine: true`。非 serpentine 演算法即使 UI state 為 true，也必須以標準掃描方向執行。

Dither feature 預設使用 `DEFAULT_DITHER_ALGORITHM_ID`，目前為 `floyd-steinberg`，且 `serpentine` 預設為 `false`。`DEFAULT_PALETTE_MAPPING_ID` 目前為 `nearest-color`。`DEFAULT_DITHER_ERROR_STRENGTH` 目前為 `100`，代表目前演算法使用的強度百分比；Error Diffusion 與 Dot Diffusion 演算法以其作為誤差擴散倍率，Bayer 與 Blue Noise threshold 演算法以其換算 threshold strength，Dot Halftone 以其換算 clustered-dot density。Dither 啟用時 output 必須以目前有效 Palette 作為固定輸出色，不自行產生新的顏色。

Dither panel 只能顯示一個強度 slider，並使用 `settings.dither.errorStrength` 作為目前 algorithm 的百分比 state。選到 `supportsErrorStrength === true` 的演算法時，label 顯示 `Error Strength`；選到 `supportsThresholdStrength === true` 的 threshold 類演算法時，label 顯示 `Dither Strength`；選到 `supportsDotDensity === true` 的演算法時，label 顯示 `Dot Density`。切換 algorithm 時必須使用單次 settings 更新同時寫入新的 `algorithm` 與 `errorStrength: DEFAULT_DITHER_ERROR_STRENGTH`，並同步 slider 顯示為 100%，不可沿用前一個 algorithm 的強度值。

`serpentine` 的 panel control 必須使用 `panelUtils.toggleSwitchInput()`，避免和一般 checkbox 視覺混用。Serpentine label 後方必須顯示 `info-circle.svg`，tooltip 文案走 i18n，背景使用實心 `--color-surface`，邊線使用 `--color-accent`。Adjust 與 Dither 的 range input、Toggle Switch checked state 必須使用 `--color-control-accent`，focus 或強調輪廓可沿用 `--color-accent-strong`，不可落回瀏覽器預設藍色或直接吃主 action accent。

`palette-mapping-modes.js` 宣告使用者可選的 Palette Mapping。`nearest-color` 直接選目前 palette 中距離最近的單一色；`pair-mix` 先找最能近似輸入 RGB 的兩個 palette 色與混合比例，再由目前 Dither Algorithm 的掃描、誤差擴散或 threshold mask 決定輸出其中一色；`tri-mix` 先找最能近似輸入 RGB 的三個 palette 色與混合比例，再由目前 Dither Algorithm 決定輸出其中一色。Pair Mix 與 Tri Mix 不是獨立 Algorithm，不應用 `Palette Dot Halftone`、`Mix Ordered` 或 `Tri Mix Ordered` 這類組合項擴增 Algorithm 選單。

`palette-mapping.js` 必須提供 dither strategy 介面。Dither processor 應只透過 `paletteMapping.createMapper(options)` 取得 mapper，並呼叫 `mapColor(r, g, b)` 或 `mapThresholdColor(r, g, b, threshold, thresholdScale)`；processor 不應依 `nearest-color`、`pair-mix` 或 `tri-mix` id 寫分支。Ordered / Pattern / Blue Noise 類 threshold 演算法應把 mask threshold 交給 `mapThresholdColor()`，由 mapping strategy 自行決定 threshold 是要當亮度偏移或 palette mix cutoff。Bayer 與 Blue Noise 的 Dither Strength 對 `nearest-color` 應使用 `thresholdScale` 控制 RGB 亮度偏移；對 `pair-mix` / `tri-mix` 應使用 `thresholdStrength` 將 cutoff 套用 `0.5 + (threshold - 0.5) * thresholdStrength`，讓同一個 slider 在所有 Palette Mapping 下都有可見效果。Dot Halftone 的 Dot Density 應調整 clustered-dot mask 的取樣密度，再將取樣到的 threshold 交給同一個 Palette Mapping。

Dither hot-path optimization 只能改資料結構、查表與快取，不可改變演算法定義。Dot Diffusion 可預先計算每個 class 的 recipient relative offsets、並可將擴散誤差乘上 `errorStrength / 100`；邊界像素仍必須依實際圖片尺寸重新計算有效 recipient 數。Error Diffusion 可快取 matrix offsets，但不可改 kernel factor、serpentine 掃描方向或 error strength 語意；Ordered / Pattern Dither 可快取 normalized threshold map，但不可改 matrix ranking、thresholdScale、threshold cell scale 或 Palette Mapping 的選色結果。Bayer 與 Blue Noise algorithm metadata 的 `thresholdScale` 是 100% strength 的基準值；實際傳給 processor 的 `options.thresholdScale` 必須使用 `algorithm.thresholdScale * errorStrength / 100`，`options.thresholdStrength` 必須使用 `errorStrength / 100`，因此 `100%` 必須保留既有輸出。Dot Halftone 必須固定使用 algorithm metadata 的 `thresholdScale`，並以 `options.dotDensity = errorStrength / 100` 換算 threshold cell scale 後調整網點密度。

Tri Mix CPU optimization 可預先列出 top-6 candidate 內的 20 組三色組合，並可將 barycentric weight 計算攤平到 hot loop；但不可改變 top candidate 數量、candidate insertion tie-break、三色組合枚舉順序、`denom` epsilon、weight clamp/normalize 流程、Color Distance 評分或 threshold 選色比較。優化後必須用 benchmark checksum 確認輸出與優化前一致。

`threshold-dither-processor.js` 是 Ordered / Pattern threshold 類演算法的可選 WebGL fast path。它只能在 `nearest-color` 或 `pair-mix` Palette Mapping、已支援的 Color Distance、palette 長度不超過 shader 上限，且瀏覽器可建立 WebGL context 時啟用；`tri-mix` 必須走 CPU，避免 shader 組合量過高且難以維持 Palette Mapping 語意。GPU path 必須使用同一份 threshold rank、`thresholdScale`、`thresholdStrength`、`thresholdCellScale`、palette 與 Color Distance；`nearest-color` 應把 threshold 當亮度偏移，`pair-mix` 應先找最佳 palette pair 與混合比例，再把縮放後 threshold 當 cutoff 決定輸出 pair 的哪個顏色。`auto` backend 必須以 CPU fallback 保留功能可用性；forced `gpu` backend 在不支援目前 options 時必須報錯。benchmark 工具應用 checksum 驗證 CPU/GPU 輸出一致後才報告速度差異。

`color-distance-metrics.js` 宣告使用者可選的距離公式。Dither feature 預設使用 `DEFAULT_COLOR_DISTANCE_ID`，目前為 `euclidean-bt709`。同一個 `colorDistance` 必須傳給 Error Diffusion、Ordered Dither、Pattern Dither，以及 Dither 關閉時 Palette operation 的直接最近色映射；在 `pair-mix` 與 `tri-mix` 下，`colorDistance` 必須用來評估哪一組 palette mix 的混合結果最接近輸入顏色。`euclidean-bt709` 是 RgbQuant-style BT.709 weighted euclidean distance；`euclidean-rgb` 是未加權 RGB squared distance；`manhattan-bt709` 是 BT.709 weighted Manhattan distance；`manhattan-rgb` 是未加權 Manhattan distance。舊 id `euclidean` / `bt709` 應正規化到 `euclidean-bt709`，舊 id `rgb` 應正規化到 `euclidean-rgb`，舊 id `manhattan` 應正規化到 `manhattan-rgb`。

`rgbquant-adapter.js` 是 Dither Editor 使用 RgbQuant 的唯一入口，且只可用於 Original palette 萃取。Adapter 必須：

- 將 RgbQuant `[r, g, b]` tuple 轉成專案 `{ r, g, b }` 色彩格式。
- 不提供 Error Diffusion / dither reduce wrapper。
- 不讓 Dither operation 直接呼叫 RgbQuant。

`pipeline-presets.js`：

```js
(function (app) {
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};

    app.pages.ditherEditor.config.pipelinePresets = [
        {
            id: 'default',
            labelKey: 'pipelineDefault',
        },
    ];
})(window.DitherApp);
```

`pipeline-presets.js` 可以覆蓋某個 preset 的順序或停用狀態，例如 `enabled: { palette: false }`，但不應為了預設順序或預設啟用狀態重複列出所有 feature id。預設 `fixedBefore`、`effectsOrder`、`fixedAfter` 順序應由 enabled features 的 `pipelineStage` 與 `pipelineOrder` 產生。

### Registry 規則

- plug-and-play 是此區塊的主要目標；registry 必須讓 feature 的載入、註冊、初始化、停用、清理和遷移都有固定路徑。
- `pages/dither-editor/operations/operation-registry.js` 負責保存 feature 註冊進來的 operation implementation；單一 operation 不應再拆成獨立 `*-operation.js` 檔。
- `pages/dither-editor/feature-manifest.js` 負責宣告 Dither Editor 頁 enabled feature manifest，例如 `Image Input`、`Crop`、`Resize`、`Adjust`、`Palette`、`Dither`、`Export` 對應的 feature script path、`enabled`、`dependsOn` 與 `loadOrder`。
- `pages/dither-editor/feature-registry.js` 負責驗證 feature contract、解析 manifest dependency/load order、註冊 feature、產生工具列、state settings、pipeline order、panel group 與 lifecycle dispatch。
- 每個 `pages/dither-editor/features/*-feature.js` 必須是該 feature 的工具列定義、operation、panel builder、預設 settings、feature hook、工具圖示、labelKey、pipeline stage、pipeline order、`panelGroup` 與是否顯示在 dock 的單一來源；`page.js` 不可另寫一份固定工具清單或固定 panel builder map，`entry.js` 不可直接手寫每個 feature script。
- `Image Input`、`Export` 這類 UI action 不放進 pipeline operations，但仍必須以 feature script 管理，並由 `feature-manifest.js` 控制是否載入。
- 要停用某個 feature，例如 `Crop`，預設做法是只在 `feature-manifest.js` 將該 feature 設為 `enabled: false`；停用後該工具不應出現在工具列、state settings、pipeline order，也不應載入對應 feature script。
- `pages/dither-editor/operations/operation-registry.js` 的 operation metadata 負責定義該 operation 是否屬於可拖曳 pipeline effect，例如 `pipeline: { draggable: true }`。
- `pages/dither-editor/dither/dither-algorithm-registry.js` 負責保存 dither algorithm metadata 與 processor implementation；Dither feature 只能透過 registry 產生選項與執行演算法，不可硬寫 processor if/else。
- `pages/dither-editor/config/dither-algorithms.js` 負責註冊 dither panel 可選演算法 metadata，並以 `config.ditherAlgorithms` getter 保留舊讀取路徑。
- `pages/dither-editor/config/palette-presets.js` 負責定義 palette panel 可選固定調色盤。
- `Palette` 的 `Custom` 色票不寫入 `palette-presets.js`；它由 `palette-feature.js` 的 settings 管理，隨目前工作區保存。
- UI panel 只能讀 registry/config 產生選項，不可把演算法名稱硬寫在 HTML。
- UI 產生工具列時，必須透過 operation registry 判斷哪些項目可拖曳排序；未註冊為 draggable pipeline effect 的項目不可被拖曳。
- 新增演算法時，必須同時新增 labelKey 到 `src/i18n/en.js`。

### Feature Manifest Schema

`feature-manifest.js` 只描述 feature 是否載入與載入順序，不放 UI、operation 或 default settings：

```js
{
    id: 'adjust',
    enabled: true,
    path: 'src/pages/dither-editor/features/adjust-feature.js',
    dependsOn: [],
    loadOrder: 40,
}
```

規則：

- `id` 與 `path` 必填。
- `enabled: false` 時，該 feature script 不載入。
- manifest 是 plug-and-play 的唯一開關；除非是 migration 或 preset override，不應在其他檔案用 feature id 判斷功能是否存在。
- `dependsOn` 只用於真正不能獨立運作的 feature；例如 `resize` 不應因為順序在 `crop` 後面就依賴 `crop`。
- dependency 必須先載入；dependency missing 或 circular dependency 必須中止該頁載入並顯示錯誤。
- `loadOrder` 只處理沒有 dependency 關係時的穩定排序。
- 同一個 feature id 不可重複出現在 manifest。
- feature script 載入後必須以相同 `id` 呼叫 `featureRegistry.register(feature)`；載入完成後 registry 必須檢查 enabled manifest 是否全數註冊成功。

### Feature Contract

每個 feature script 必須註冊一個 feature object。外部只依賴這個 object，不直接依賴 feature 內部檔案：

```js
app.pages.ditherEditor.featureRegistry.register({
    id: 'adjust',
    icon: '~~',
    labelKey: 'toolAdjust',
    dock: true,
    dockOrder: 40,
    panelGroup: 'edit',
    pipelineStage: 'effectsOrder',
    pipelineOrder: 10,

    defaultSettings: function defaultSettings(context) {
        return {};
    },

    buildPanel: function buildPanel(context) {
        return document.createElement('div');
    },

    operation: {
        pipeline: { draggable: true },
        run: function run(imageData, settings, context) {
            return imageData;
        },
    },

    onMount: function onMount(context) {},
    onUnmount: function onUnmount(context) {},
    dispose: function dispose(context) {},
    onImageLoaded: function onImageLoaded(context) {},
    onSettingChanged: function onSettingChanged(context) {},
    onBeforePreview: function onBeforePreview(context) {},
    onAfterPreview: function onAfterPreview(context) {},
    onBeforeExport: function onBeforeExport(context) {},
    onAfterExport: function onAfterExport(context) {},
    migrateSettings: function migrateSettings(oldSettings, fromVersion, toVersion) {
        return oldSettings;
    },
});
```

規則：

- feature id 必須唯一。
- 顯示在 dock 的 feature 必須提供 `icon` 與 `labelKey`。
- 有 `pipelineStage` 且屬於圖片處理步驟的 feature 必須提供 `operation.run()`。
- `Image Input`、`Export` 可以是 action feature，不一定要提供 image operation。
- feature 的 UI、operation、default settings、hooks 和 pipeline metadata 必須從同一個 feature contract 暴露，避免移除功能時到多個共用檔同步刪除。
- 單一 feature 可以在自己的資料夾內分檔，例如 `features/palette/feature.js`、`panel.js`、`operation.js`；但外部只載入 manifest 指定的 feature entry。

### Feature Lifecycle

feature lifecycle 必須固定，避免每個 feature 自行在 `page.js` 或 `controller.js` 補特殊 case：

```text
register
  -> onMount
  -> onImageLoaded
  -> onSettingChanged
  -> onBeforePreview
  -> onAfterPreview
  -> onBeforeExport
  -> onAfterExport
  -> onUnmount
  -> dispose
```

`onSettingChanged(context)` 必須廣播給 enabled features。`context.id` 代表實際被修改的 settings group；feature 必須自行判斷是否處理該事件。需要監聽其他 feature 的同步邏輯，例如 Resize 跟隨 Crop 輸出比例，應留在監聽 feature 自己的 `*-feature.js`，不可寫成 controller special case。

`dispose` 用於清理 event listener、object URL、timer、worker、temporary canvas、cached ImageData 等資源。

### Feature State Builder

`state.js` 不可手寫每個 feature 的 default settings。必須由 enabled features 建立：

```js
featureRegistry.all().forEach(function (feature) {
    if (feature.defaultSettings) {
        settings[feature.id] = feature.defaultSettings(context);
    }
});
```

pipeline 順序也必須由 enabled features 的 `pipelineStage` 與 `pipelineOrder` 建立；`pipeline-presets.js` 只能覆蓋順序或啟用狀態，不可成為第二份 feature 清單。

### Feature Migration

若未來重新加入 workspace 持久化，migration 可以分兩層：

- 全域 migration 負責 `schemaVersion` 與 state shape。
- feature migration 負責該 feature 自己的 settings。

如果舊資料包含已停用或不存在的 feature settings，預設不應套用到 UI，也不應讓 preview/export crash。需要保留資料時，可以放進 unknown settings 區，等該 feature 重新啟用後再由 `migrateSettings()` 處理。

## ESP32 裝置整合

本專案會放進 iot-node-bedrock 的 `user-web/`，由 ESP32 韌體提供同一份靜態網站；裝置管理與 E-paper Device Mode 均由同源 REST API 驅動。圖片處理、orientation normalization 與 EPDIMG encoding 全部在瀏覽器完成。API contract 以 iot-node-bedrock `docs/SPEC_API_REFERENCE.md` 為準，本章只描述前端側整合。

### 裝置整合層（src/device/）

```text
src/device/
  device-api.js    REST client：envelope 解析、Bearer token store、AbortController timeout、resources 目錄
  device-live.js   alive 監看：GET /api/alive 輪詢、online/offline/standalone 狀態、subscribe
  device-epaper.js e-paper capability、cached status、single operation、phase progress、cooldown
  device-epaper-calibration.js 六色 canonical snapshot、revision、公開 GET／PUT／reset 與 stale response suppression
  device-gate.js   離線反灰：banner、fieldset disable、恢復後自動刷新
  device-auth.js   認證：login dialog、session 驗證、logout、全域 401 處理、鎖定卡
```

規則：

- 頁面一律透過 `app.device.api.resources` 取用 endpoint，不直接呼叫 `fetch`；path 使用相對路徑（部署後與裝置 API 同源）。
- REST envelope 固定 `{success, data, message}`；HTTP 200 但 `success !== true` 也視為錯誤。錯誤統一帶 `status`、`code`、`fields`；網路層失敗包成 `{code: 'transport_error', status: 0}`。
- 常見 error code 由 `app.device.errorText()` 翻成使用者語言；API `message` 只作 fallback。
- Raw EPDIMG upload 仍透過 `device-api.resources`，使用 `fetch` 的 binary body 與 `application/octet-stream`；頁面不可直接呼叫 fetch。Content-Length 由瀏覽器依固定 192,040-byte body 設定。

### 裝置連線監看（device-live）

- `main.js` 於 startup gate complete 後呼叫 `app.device.live.start()`。
- 每 5 秒打 `GET /api/alive`（timeout 2.5s）；連續 2 次失敗才轉 `offline`，1 次成功即轉 `online`；從未成功過為 `standalone`（開發環境）。
- 任何 API 成功都視為 alive；一般 API 網路層失敗會立即補查一次。分頁隱藏時暫停輪詢，恢復可見立即補查。
- `suppress(reason)` / `release(reason)` 供已知會短暫斷線的流程（Wi-Fi safe transition）暫停離線判定。
- header `#app-status` 圓點合成優先序：裝置離線（紅）> 頁面 error/busy > 裝置在線（綠）> standalone（灰）。
- 裝置頁以 `app.device.bindLiveGate(container, {onOnline})` 綁定：offline/standalone 時容器加 `.is-device-offline`、內部 `fieldset` 全部 disable、顯示常駐 banner；恢復後自動刷新資料並顯示 2.2 秒成功 banner。Dither Editor 為純前端頁，不受影響。

### 開發／demo 假資料（device-mock）

仿 iot-node-bedrock builtin-web 的 preview 機制：**有沒有 mock 是 build 產物的差異，不做任何執行期偵測**。

- `index.html` 以 `<!-- PREVIEW START -->` / `<!-- PREVIEW END -->` 註解包住 `src/device/device-mock.js` 的 script 標籤；source 工作副本永遠帶著這個區塊，因此 `file://` 直開或本機 server 開 source 都是假資料。
- `device-mock.js` 被載入即預設啟用（載入本身就代表非正式 build），`?mock=0`（或 `false`／`off`）可關閉、回到 standalone 呈現。啟用時攔截相對路徑 `api/...` 的 `window.fetch`（其餘 URL 照常放行），並於 header 顯示常駐「PREVIEW」badge。
- 正式 build（`make build`）由 `tools/build/run.py` 剝除 index.html 的 PREVIEW 區塊並排除 mock 檔；找不到 PREVIEW 區塊時 build 直接失敗，避免默默漏剝。裝置產物物理上不含任何 mock 程式。
- `make demo`（`run.py --demo`）保留 PREVIEW 區塊與 mock 檔，輸出時間戳資料夾加 `-demo` 後綴；demo 供一般靜態站（如 GitHub Pages）使用，固定不做 gzip-only 輸出，minify 照常。
- Mock 為 in-memory 狀態機，涵蓋 alive、device、storage、wifi（scan、202 safe transition 與 4 秒後 connected/failed）、auth（login/session/logout/password）與 system；登入帳密 `admin` / `password`；`heap_used_percent` 為固定值，不隨輪詢隨機浮動。完整重設會把 in-memory 狀態（密碼、hostname、Wi-Fi、token）全部還原成出廠預設。初始 Wi-Fi 狀態為 `ap_sta`（STA 已連線 `HomeWiFi-5G` / `192.168.1.50`，AP 同時啟用），因此 safe transition 需先存成 AP mode 再切回含 STA 的 mode；儲存 SSID 含 `fail`（掃描清單內建 `Mock-Fail`）模擬驗證失敗。重新整理即重設，不持久化。
- 注意：`tools/build` 的簡易 JS minifier 會把 regex 字面值內的 `//` 誤判為註解；demo 會 minify mock 檔，regex 內的斜線需寫成 `[/]` 類寫法。

### 認證與 session

- token 由 `POST /api/auth/login` 取得，存 localStorage（key 走 `storage-keys.js`）；裝置重開機或他人登入即失效，本地只是快取，有效性以 `GET /api/auth/session` 為準。
- 所有帶 token 的 request 收到 401 時，`device-api` 觸發全域 unauthorized 事件 → `device-auth` 清 token 並通知訂閱者，受保護頁面退回鎖定卡。login 本身的 401 是帳密錯誤，不觸發全域登出。
- session 驗證失敗但屬 transport 錯誤時保留 token，待裝置恢復後重新驗證，避免離線時誤登出。
- login dialog 為全站共用 modal；username 由公開 `GET /api/auth` 取得（固定 `admin`、唯讀），成功後原地解鎖目前頁面，不跳頁。Menu 底部在持有 token 時顯示登出。
- 修改管理員密碼成功、AP 密碼保護開關變更（裝置重啟）時，前端主動 `invalidateSession()`。

### 裝置管理頁

- `pages/device-info/`：公開唯讀。`GET /api/device` + `GET /api/storage`，掛載時抓一次並每 10 秒背景輪詢，不提供手動 Refresh、不顯示 stale badge，也不呈現 `config_state`；容量分段長條圖推導與 bedrock builtin-web Hardware 頁一致，Available 直接使用 `user.limits.max_upload_bytes`。失敗沿用最後成功值，離線提示交給 `device-gate` banner。
- `pages/device-network/`：狀態卡公開（`GET /api/wifi` 每 10 秒輪詢，`if-clean` 不覆蓋表單草稿）；Wi-Fi 設定需登入。`PUT /api/wifi` 是完整 replacement，不適用分類由已載入 baseline 帶入；202 safe transition 以 1s 間隔輪詢 `GET /api/wifi/connect`、25s deadline、generation 序號丟棄過期回覆，期間 `suppress` 離線判定並容忍 transport 失敗；terminal `connected` 才把送出值設為 baseline 並清 password input，`failed` 保留草稿並顯示 rollback。掃描 dialog 過濾 hidden／空 SSID／RSSI ≤ -75、依 RSSI 排序、10 秒 cooldown 倒數，429/409 依 `retry_after_seconds` 提示。
- `pages/device-system/`：整頁需登入。hostname 走 `PUT /api/system`（前端套用相同 1–31 字元規則與 mDNS 預覽）；管理員密碼走 `PUT /api/auth/password`（allowlist 正則 + 兩次一致），成功後清 token 要求重新登入。完整重設走 `POST /api/system/reset`（API client 只保留這一支；settings/data 兩支未使用已移除），UI 必須先通過確認 dialog，送出期間 `setDismissible(false)` 並鎖住按鈕，成功後 `invalidateSession()` 並以 sticky notice 保留重新連線指示。
- 表單皆採 `busy || !dirty || !valid` 三態儲存鈕與文字狀態列；一般成功 notice 約 2.2 秒自動消失，錯誤與需保留脈絡（重啟、斷線、rollback）的 notice 常駐。
- 視覺語言與 Dither Editor 對齊：卡片一律使用 components.css 的 `panel-section`（h2 標題列）＋`panel-body device-card-body`；需要右側 badge／總空間的卡片改用 `device-card-header` 標題列變體。頁面不放 `h1`（頁名由 app header 顯示）。`device.css` 只保存裝置專屬樣式，顏色沿用 theme token，尺寸沿用編輯器的 34px 輸入框與 11–14px 字級階。

### E-paper Device Mode

- `device-live` 仍只擁有 connection truth；`device-epaper` 在 online 後 probe `GET /api/epaper`，驗證固定 800×480、EPDIMG、192,040 bytes、六個 color codes 與必要 capabilities。
- Target state 與 connection state 分離。Capability 一旦在 session 內確認，offline 不清除 target，只禁止 operation；reconnect 後重抓 capability/status。
- `device-epaper` 擁有 cached status、operation run id、polling、blocking state、phase progress與 cooldown deadline。Editor 與 test page 只能訂閱 snapshot及呼叫公開 operation method。
- `device-epaper-calibration` 擁有六色 canonical snapshot。所有對外 colors 都 deep copy；只有 response 通過固定 id/code、channel integer/range 與 duplicate RGB 驗證後才發布。`revision` 只在 RGB 真正改變時遞增；較舊 GET 不得覆蓋較新的 save/reset，斷線或 request failure 保留最後 canonical。
- 面板測試 draft 與 canonical 分離，高頻 e-paper status render 不得重建輸入 DOM 或覆蓋草稿。save/reset/reload 期間以 request generation 防 stale response；dirty 遇到外部 revision 只標記 conflict，不自動 rebase。
- `app.app.state.blockingOperation` 只保存 coarse global lock reason，不保存 editor image 或 API payload。Router/Menu 在 lock 期間拒絕 navigation。
- Static site 由 ESP32 提供；ESP32 不負責 crop、resize、palette、dither、portrait rotation 或 EPDIMG packing。

### Web Setting Page

`Web Setting` 是 app shell 層級頁面，不屬於 Dither Editor feature。它負責全站 UI 偏好，例如 light / dark theme 與 language。

```text
pages/web-setting/
  entry.js
  page.js
```

規則：
- Web Setting 頁面只修改 app shell state 和持久化 settings，不保存 Dither Editor 的 canvas、圖片、pipeline 或 feature settings。
- theme 選項必須由 `app-state.js` 統一提供，例如 `light`、`dark`。
- language 選項必須由 `i18n/index.js` 統一提供，固定為 `auto`、`zh-TW`、`en`；Auto 依瀏覽器語言解析目前支援語系，未匹配時 fallback 到 `en`。
- 切換 theme 時，必須立刻更新 `body[data-theme]`，讓 `assets/styles/themes.css` 內的 CSS variables 套用到全站。
- theme 與 language 必須透過 `settings-store.js` 寫入 localStorage；重新整理或下次重新打開瀏覽器頁面後仍保留。
- 切換 language 時，必須重新套用 app shell、menu 與目前頁面文字；Dither Editor 的圖片、pipeline 與 editor state 不應因此寫入 localStorage。
- 未來若加入後端登入或 session，再另外導入 cookie；現階段 Web Setting 不使用 cookie。
- 新增 Web Setting 頁面不應要求修改 Dither Editor 的 controller、page 或 feature registry。
- 版面與裝置頁一致：`.web-setting-page` 是置中卡片堆疊（gap 12、max-width 860、padding 16），卡片使用 `panel-section`（h2 標題列）＋`panel-body`，頁內不放 `h1`。

### Help Page

`Help` 是 app shell 層級的文件中心，不屬於 Dither Editor feature，也不可保存或修改 editor state。

```text
pages/help/
  entry.js
  document-manifest.js
  content-model.js
  validation.js
  visuals.js
  page.js
  i18n/
    en.js
    zh-TW.js
```

規則：

- `document-manifest.js` 是文件 id、route、parent 與導覽順序的唯一來源；文章 renderer、文件樹、Breadcrumb、上一篇／下一篇不可另寫平行文件清單。
- `i18n/en.js` 與 `i18n/zh-TW.js` 以 `helpBundle` 擴充既有 i18n dictionary，兩種語言必須維持相同文件 id 與 section 結構。
- `app/project-capabilities.js` 保存由實際 config/registry 發布的 collection 與 fact。它是 app-level 唯讀橋接，不反向依賴 Dither Editor；config 或 constants 在 capability registry 存在時才發布資料，讓 Worker 與 headless harness 可在未載入它時繼續執行。
- `content-model.js` 以 `dither-algorithms` capability collection 決定演算法卡片順序與可見性，並以 stable algorithm id 查找目前語言的長文。沒有長文時必須從 `labelKey`、processor、matrix 與 supports metadata 產生可用 fallback，不可讓 Help crash。
- `constants.js` 必須把 `MAX_INPUT_LONG_EDGE` 與 `MAX_RESIZE_OUTPUT_SIZE` 發布為 `maxInputLongEdge` / `maxResizeOutputSize` fact；Help table cell 只保存 i18n key 與 fact id，不複製數值。
- `i18n.t(key, replacements)` 支援 `{value}` named placeholder 與 `{0}` positional placeholder；replacement 使用 global match，同一 placeholder 重複出現時必須全部替換，缺值則保留原 placeholder 供驗證發現。
- `validation.js` 比對 runtime algorithms、`helpFamily`、英文／繁中 detail id、必要 fact 與 i18n template，並以模擬新增／移除確認 fallback 與隱藏行為。驗證錯誤只在 Help 載入時警告，不阻止使用者閱讀 fallback。
- `page.js` 負責文件 layout、通用 article block renderer、route 切換與 DOM lifecycle，不保存大段雙語文案。
- `visuals.js` 負責 Help 專用流程圖、Before / After、matrix/kernel 與 Color Distance explorer；它可以使用 `core.paletteUtils` 驗證實際色距選色，但不可呼叫或修改 Dither Editor controller/state。
- Help route 固定由 `#/help` 開始；未知 Help 子 route 顯示 Help 首頁，不可造成 startup failure。
- Help 文件連結必須保留實際 `href` 供複製與另開分頁；一般左鍵點擊則交給 app context `navigate()`，確保 history state 完整。
- 桌面 Help layout 由 `assets/styles/layout.css` 提供文件樹、文章與頁內目錄欄位；Help article/navigation/visual components 由 `assets/styles/components.css` 提供，顏色必須使用既有 theme tokens。
- Help i18n、manifest、visuals 與 page scripts 由 `pages/help/entry.js` 依序載入；`index.html` 不可展開列出內部檔案。
- `assets/help/*.png` 必須由 `tools/dither-render/run.py` 使用專案實際 dither scripts 產生，不手動畫假結果。比較圖使用固定 480x288 synthetic gradient、E6 palette 與畫面標示的 Algorithm / Mapping / Color Distance / Strength；輸出變更時必須重新產生對應資產並人工確認。
- `tools/help-validate/run.py` 必須透過 headless browser 載入實際 classic scripts，驗證 capability、雙語內容、限制模板與重複 placeholder；缺少任何已註冊演算法的雙語 detail 時以非零 exit code 結束，已移除演算法殘留的不可見 detail 只報 cleanup warning。

### E-paper Display Profile

顯示器必須透過 display profile registry 擴充，不可在 crop、resize、upload 邏輯中硬寫某一個尺寸。Profile 對使用者與程式都以像素尺寸為主，不使用 7.3 吋、13.3 吋這類實體尺寸作為主要識別。

```js
(function (app) {
    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.config = app.pages.ditherEditor.config || {};

    app.pages.ditherEditor.config.displayProfiles = [
        {
            id: 'epaper-color-800x480',
            labelKey: 'displayEpaperColor800x480',
            width: 800,
            height: 480,
            aspectRatio: 800 / 480,
            colorMode: 'color',
            refreshMode: 'direct',
            outputFormat: 'device-native',
        },
        {
            id: 'epaper-color-custom',
            labelKey: 'displayEpaperColorCustom',
            width: null,
            height: null,
            aspectRatio: null,
            colorMode: 'color',
            refreshMode: 'direct',
            outputFormat: 'device-native',
            note: 'Set exact pixel size before enabling this profile.',
        },
    ];
})(window.DitherApp);
```

Device Mode 啟用 device display target 時：

- 固定 panel profile 由 ESP32 `GET /api/epaper` 回傳；local registry 只描述前端支援形狀，不可單獨宣告真裝置存在。
- E-paper Crop ratio allowlist 固定為 landscape `5-3` 與 portrait `3-5`。
- Landscape output 固定 800×480；portrait output 固定 480×800。Resize UI 為 read-only。
- Effective palette 固定為 `e6-color-epaper`；Palette UI 不建立 add/remove control，color input、preset、Original size 均不可修改。`OUTPUT_COLORS` 與 calibration service 的動態 display colors 必須平行採用 EPD code order `0,1,2,3,5,6`，即黑、白、黃、紅、藍、綠；前者是 EPDIMG 協定色 A，後者是 pipeline 的實體參考色 A′，固定 swatch、Result viewport、palette mapping 與 dither error 都使用 A′。`target-policy.outputImageData()` 必須在 encoder boundary 以 exact DISPLAY/OUTPUT RGB lookup 取得固定 code slot，再轉成 OUTPUT RGB；非六色像素必須拒絕，不可用 nearest-color 猜測硬體色碼。
- `target-policy` 的 display/output `WeakMap` cache 必須帶 calibration revision。Editor 訂閱色準 service；revision 改變時重新 force target palette、清除 stage cache、live preview 與 output image，再依目前模式排程正式 preview，確保已開啟的工作區不沿用舊色盤。
- `target-policy.js` 同時提供 UI guard、controller setting guard 與 formal pipeline 前 normalization。DOM disabled 不是安全邊界。
- Capability 在 editor mount 後才確認時，policy 原子套用 target、增加 `uiRevision`、rebuild controls並重跑 preview。

### E-paper Operation Overlay

Image upload/draw 與 white/palette/refresh action 期間必須顯示 app-shell global blocking overlay：

```text
Uploading to device...
Refreshing display...
```

行為：

- 顯示 spinner。
- 禁止編輯。
- 禁止切換頁面。
- 禁止重複上傳。
- 成功後解除 blocking。
- 失敗後解除 blocking 並顯示錯誤。
- 不提供 cancel；HTTP 202 後沒有可安全取消 physical draw 的 API contract。
- Progress 是 time-weighted simulated percentage。每個 phase 依 expected duration 在自己的區間使用漸近曲線持續前進；相同 server phase 的輪詢不得重設 `stageStartedAt`。`refreshing` 使用最大區間 42–90%，只有 cooldown success 能到 100%。
- Operation 完成進入 cooldown 後解除全域 lock，但所有 e-paper action 依 server `retry_after_seconds` disabled；local countdown 每 250ms 更新 deadline、只在整秒變更時 render，歸零後至少每 500ms 重查 status，直到 server 離開 cooldown 並恢復 `can_upload && can_draw`。

### EPDIMG Output Encoding

目前裝置 contract 固定為 EPDIMG，不再保留未決 encoder format。Encoder 位於 pure core：

保留 encoder 位置：

```text
src/core/encoders/
  epdimg-encoder.js
```

規則：

- Formal pipeline output 只接受 800×480 或 480×800。Encoder 必須直接檢查 ImageData dimensions，不依賴 UI orientation flag。
- 800×480 原樣編碼；480×800 在六色 pipeline 完成後逐 pixel 順時針旋轉 90°，不重新 resize或 dither；其他尺寸在 API 前失敗。
- Normalized 800×480 pixel 必須精確匹配 E6 reference RGB，mapping 固定為 black=0、white=1、yellow=2、red=3、blue=5、green=6；不可用 palette array index。
- 每兩 pixel 打包一 byte（left high nibble、right low nibble），frame CRC32 後寫 40-byte little-endian header與 non-zero uint64 generation，總長固定 192,040 bytes。
- `POST /api/epaper/image` 成功已同時 atomic update stored image並 queue draw，client 不得自動追加 refresh。

## 圖片處理流程與固定效果堆疊

圖片處理流程順序會影響結果，但目前不提供使用者拖曳改變順序。為了避免 Palette / Dither 語意混亂，流程切成三段並以固定順序執行：

```text
fixed before: Crop -> Resize
edit effects: Adjust -> Palette -> Dither
fixed after: Export
```

規則：

- `crop` 和 `resize` 固定在效果演算法之前。
- `adjust`、`palette`、`dither` 固定在 edit effects order 中。
- `export` 固定在最後，不出現在圖片效果順序中。
- Tool dock 仍透過 operation registry metadata 判斷可排序項目；目前 edit effects 的 `draggable` 為 `false`，因此不顯示 drag handle。
- 未來若重新開放排序，必須重新定義 Original palette 是否跟隨前序 effects 重算。

例如：

```text
Crop -> Resize -> Adjust -> Palette -> Dither -> Export
```

### Effects Stack UI

編輯區不建立獨立可見的 `pipeline-panel`。edit effects 的工具列項目本身就是效果堆疊，但只提供展開/收合與設定，不提供拖曳排序。

```text
[~~] Adjust    enabled
[# ] Palette   enabled
[..] Dither    enabled
```

`Crop`、`Resize` 各自留在自己的固定工具列項目，不可拖曳。`Export` 不在 accordion 工具列內，而是固定外露動作。`Adjust`、`Palette`、`Dither` 依 `effectsOrder` 固定執行，且 `operation.pipeline.draggable` 必須為 `false`。

工具列順序、operation、panel builder、feature hook 與可見性必須由 feature script 產生，並由 `feature-manifest.js` 控制是否載入。若未來要移除 `Crop`，主要應只從 `feature-manifest.js` 停用或移除該 feature；`entry.js`、`pipeline-presets.js`、`page.js`、`controller.js` 不應還有另一份 `crop` 載入、順序、工具列、panel builder 或 image-loaded hook 定義需要同步刪除。

每個項目需要：

- 功能圖示。
- enabled / disabled toggle。
- 點選後顯示該步驟的參數面板。
- 多個 Tool Panel 可以同時展開。
- `panelGroup: 'source'` 與 `panelGroup: 'prepare'` 是流程入口，手動展開時採互斥收合規則；`panelGroup: 'edit'` 的 panels 在 `edit` 中可以多個同時展開。未宣告 `panelGroup` 的 feature 預設屬於 `none`，不顯示在左側 tool dock。
- 顯示是否有錯誤設定。

### Effects Drag Feel

effects stack 的拖曳手感屬於 UI tuning，不應散落在 feature 或 controller。可調參數應集中在 `src/ui/sortable-list.js` 與 `assets/styles/components.css`。

建議可調項：

```js
var DRAG_THRESHOLD = 5;
holdDelay: 260;
entry.node.style.transition = 'transform 105ms ease-out';
```

```css
body.is-sorting,
body.is-sorting * {
    cursor: grabbing !important;
}

.tool-accordion-item.is-dragging .tool-button {
    border-color: var(--color-accent);
    background: var(--color-accent-soft);
}
```

Sortable 機制保留在通用 UI 層；目前因 edit effects 的 `draggable` 為 `false`，tool dock 不會產生可排序項目。以下只保留為未來重新開放排序時的調整參考：

- `DRAG_THRESHOLD` 控制按下後要移動多少像素才開始排序；數值越小越靈敏，越大越不容易誤拖。
- `holdDelay` 控制從 Tool Row 非 drag handle 區域長按多久才進入拖曳；數值越小越容易誤拖，越大越接近純點擊展開。
- `transform ... ms ...` 控制其他 Tool Row 讓位的動畫速度；過短會生硬，過長會有過度滑動感。
- animation cleanup timeout 必須略大於 transition duration，例如 transition `105ms` 時 cleanup 可約 `120ms`。
- `.is-dragging` 只用來提示目前被拖曳的 Tool Row，不應製造另一個可見殘影。
- `body.is-sorting` 必須鎖定 cursor，避免滑過 icon、label、button 或其他元素時游標樣式跳動。
- 若重新顯示 `.tool-drag-handle`，它應是唯一平常顯示 `grab` 的區域；整個 `.tool-button` 不應預設顯示手握取游標。

### Pipeline 限制

Pipeline 需要明確規則。

MVP 規則：

- `crop` 固定在 `fixedBefore` 第一段。
- `resize` 固定在 `fixedBefore`，並在 `crop` 之後。
- `adjust`、`palette`、`dither` 屬於 `effectsOrder`，但 `operation.pipeline.draggable` 必須為 `false`，UI 不開放拖曳改變順序。
- `export` 不在 pipeline list 中，它永遠使用目前 pipeline 結果。
- 彩色電子紙預設流程中，`palette` 表示固定輸出色集合；`dither` 啟用時負責把像素落到該色集合並擴散誤差，避免 `palette` 先量化造成 dither 失去誤差。
- 如果某 operation 需要前置資料不存在，該 operation 應回傳明確錯誤並在 UI 顯示。

### Operation 介面

每個圖片處理 feature 必須透過 feature object 註冊 operation；外部不直接載入獨立 `*-operation.js`：

```js
const cropFeature = {
    id: 'crop',
    labelKey: 'toolCrop',
    pipelineStage: 'fixedBefore',
    pipelineOrder: 10,
    defaultSettings: function defaultSettings(context) {
        return {};
    },
    createLivePreviewBase: function createLivePreviewBase(context) {
        return null;
    },
    livePreviewFilter: function livePreviewFilter(context) {
        return '';
    },
    operation: {
        pipeline: { draggable: false },
        run: function run(imageData, settings, context) {
            return imageData;
        },
    },
};
```

`createLivePreviewBase()` 與 `livePreviewFilter()` 是可選契約，只能用於拖曳期間的輕量回饋。Feature 必須自行判斷 live preview 是否會誤導使用者；如果正式 pipeline 會因後續 `Palette`、`Dither` 或其他 effect 產生不同結果，應回傳 `null` 或空字串，讓 UI 放棄假的即時預覽。

### Crop Transform

Crop feature 的 transform settings 必須由 `crop-feature.js` 自己定義與 normalize，不能在 `page.js` 或 `controller.js` 寫死。MVP crop settings 至少包含：

```js
{
    aspectRatioId: '16-9',
    panX: 0,
    panY: 0,
    zoom: 1,
    rotation: 0,
    flipX: false,
    flipY: false,
    backgroundPreset: 'auto',
    backgroundColor: '#ffffff',
    autoBackgroundColor: '#ffffff',
}
```

Crop 預設 `aspectRatioId` 必須是 `16-9`。若舊 settings 或無效 settings 找不到對應 ratio，應 fallback 到 16:9。Crop transform fill 預設為 auto；`backgroundPreset` 僅允許 `auto`、`black`、`white`、`custom`，`backgroundColor` 與 `autoBackgroundColor` 必須正規化為 `#rrggbb`。

Preview renderer 與正式 crop operation 必須套用同一套 transform 規則：

1. 先用目前 crop transform fill color 填滿 target canvas，覆蓋旋轉、平移、縮放或翻轉後原圖未覆蓋的區域。
2. 將輸出中心移到 crop frame 中心，並加上 `panX` / `panY`。
3. 套用 `rotation`。
4. 套用 signed scale：`flipX` 時 X scale 為 `-zoom`，否則為 `zoom`；`flipY` 時 Y scale 為 `-zoom`，否則為 `zoom`。
5. 從原圖中心繪製來源圖片。

`viewport-renderer.renderTransformed()` 的 prepare preview canvas 可以大於 crop frame；此時 transform fill color 只能填在 `layout.frame` 內，frame 外必須保持透明，讓 preview stage 的 5x5 分組細網格透明背景與原圖調整脈絡可見。該網格必須重用既有 `--color-surface-muted`、`--color-border-faint`、`--color-border` 等灰階 theme tokens，不新增 preview pattern 專用色彩 tokens。正式 `cropToImageData()` 的 target canvas 本身就是 crop frame 尺寸，因此仍填滿整個 target。

`viewport-renderer.js` 的 transform cache key 必須包含 `flipX`、`flipY` 與 crop transform fill color，否則切換反轉或底色狀態可能不會重繪。Fill 設為 `auto` 時，crop feature 必須依目前 source image、ratio、pan、zoom、rotation、flip 與 crop output size 建立 cache key；任一 transform 調整後都要重新估算。

`auto` fill 必須以低解析 transformed crop frame 取樣估算，不可每次都掃完整輸出尺寸。演算法應找出來源圖覆蓋像素旁的透明邊界像素，使用簡單 trimmed average 產生代表色；若沒有邊界樣本，才 fallback 到 crop frame 外框樣本或目前保存的 fill color。Auto color 必須和上一個 auto color 比較，小幅差異沿用舊色，大幅差異才切換，避免旋轉連續調整時因樣本抖動造成閃爍。

Crop 面板的左轉 90 / 右轉 90 button 必須只更新 `rotation`，以目前 rotation 為基準加減 90 度，並將結果維持在 `-180..180` 範圍。

Crop 面板在桌面與手機版都必須維持同一個兩欄 row 結構：Ratio / Zoom 同列、Rotate / Fill 同列、左轉 90 / 右轉 90 / horizontal flip / vertical flip button row 跨滿整列。手機版只能縮小欄距、label 欄寬或兩欄比例，不可退回單欄堆疊。

Fill control 由 `select` 與 32x32 `input[type="color"]` 組成。選擇 `auto`、`black` 或 `white` 時，color input 必須同步顯示對應顏色；使用者手動改 color input 時，`backgroundPreset` 必須切成 `custom`。

Crop Fill color input 的 `input` 事件只能同步 crop state 與 select 顯示，不應呼叫 controller update 或重建面板；`change` 事件才排正式更新，避免原生 color picker 被 render 打斷。

Crop 面板的 horizontal flip / vertical flip icon button 必須用同一次 settings update 完成狀態切換：

- `Flip Horizontal`：切換 `flipX`、將 `rotation` 取反、將 `panX` 取反。
- `Flip Vertical`：切換 `flipY`、將 `rotation` 取反、將 `panY` 取反。

這個規則讓反轉以目前畫面座標為準；若只切換 `flipX` / `flipY` 而不處理 rotation，已旋轉圖片會呈現和使用者預期不同的鏡射方向。

Flip icon button 可以用 `aria-pressed` 表示狀態，但視覺上必須和左轉 90 / 右轉 90 button 一樣，不套用持續 active color / background。

### Resize Controls

Resize feature 固定維持等比 resize，不提供 Fit / Stretch / Contain / Cover 選單。

- `resize` settings 至少包含 `width`、`height` 與內部使用的 `aspectRatio`。
- Resize output 的合法尺寸範圍是 `1..MAX_RESIZE_OUTPUT_SIZE`，目前 `MAX_RESIZE_OUTPUT_SIZE = 4096`。
- 新圖片載入後，Resize 預設尺寸應跟目前 Crop 輸出尺寸同步；若 Crop feature 不存在，才 fallback 到 working image size。
- Resize width / height controls 必須顯示在同一列。
- 使用者調整 `width` 時，`height` 必須立即依 `aspectRatio` 更新。
- 使用者調整 `height` 時，`width` 必須立即依 `aspectRatio` 更新。
- Resize width / height controls 必須使用 `panelUtils.unitNumberInput(..., 'px', ...)`，以和 Crop zoom / rotation 共用數字輸入樣式與長按 stepper 行為。
- `panelUtils.unitNumberInput` 必須提供可由 feature 更新的 value 與 range，讓不重建 panel 的 render cycle 仍可同步最新 constraints。
- 手動輸入 Resize width / height 時，目前聚焦的欄位不可被 callback 強制重寫；只強制同步等比連動的另一欄，避免輸入游標或選取位置在每次按鍵後跳動。
- 等比換算若會讓另一邊超過 `MAX_RESIZE_OUTPUT_SIZE`，正在調整的尺寸也必須 clamp 到可維持比例的最大值。
- Crop ratio 改變後，Resize 應在 prepare commit 時以目前 resize width 為錨點更新 `aspectRatio` 與對應 height，避免 pipeline 把 crop 結果拉伸成不同輸出比例；Crop zoom/pan 熱路徑不可即時觸發 Resize 重算。
- Resize feature 必須在 `onRender` 將最新 `settings.resize.width` / `height` 與 ratio 對應的合法範圍同步回既有 DOM controls，避免 panel 被隱藏後重新打開時顯示舊值。
- Page render 同步 panel 開關狀態時，若 panel 已在對應的 tool panel host 內，不可再次呼叫 `appendChild`；重掛既有 panel 會讓其中的 active control blur，破壞 unit number input 的連續輸入。

### Adjust Controls

Adjust feature 只提供 brightness、contrast、saturation 三個 slider，不提供 Reset Default button。

- 三個設定預設值必須都是 `0`，代表 identity。
- 每個 slider 左側必須顯示目前數值。
- Range input 必須能拖曳到最小與最大端點；樣式不可用 padding 或 border 壓縮可拖曳範圍。

Pipeline 執行器只根據 fixed before、固定 effects order 與 fixed after 逐步套用：

```js
function runPipeline(sourceImageData, state) {
    let currentImageData = sourceImageData;
    const order = [
        ...state.pipeline.fixedBefore,
        ...state.pipeline.effectsOrder,
        ...state.pipeline.fixedAfter,
    ];

    for (const operationId of order) {
        if (operationId === 'export') continue;
        if (!state.pipeline.enabled[operationId]) continue;
        const operation = operationRegistry.get(operationId);
        if (!operation) {
            throw new Error('Missing operation: ' + operationId);
        }
        currentImageData = operation.run(currentImageData, state.settings[operationId] || {}, {
            id: operationId,
            state: state,
        });
    }

    return currentImageData;
}
```

錯誤策略：

- operation 設定無效時，應在 `run()` 內 throw 明確錯誤並停止 pipeline。
- operation 的第三個參數是 pipeline context，只能用於讀取目前 operation id、state、settings 或 pipeline enabled 狀態；operation 不可透過 context 讀寫 DOM。
- 不略過失敗 operation，避免輸出結果不可預期。
- preview 時由 controller catch error，更新 `state.status = 'error'` 與 `state.error`。
- export 時若 pipeline throw error，禁止輸出並要求使用者修正設定。

### Stage Cache

`pipeline-runner.js` 可接受可選的 Stage Cache，用於 preview 類 pipeline 加速。Cache key 必須包含：

- 輸入 stage identity。
- operation id。
- operation 自己的 settings stable serialization。
- operation 透過 `operation.cacheKey(settings, context)` 宣告的額外相依狀態。

規則：

- Stage Cache 只保存 in-memory `ImageData`，不可寫入 localStorage、IndexedDB 或其他 browser storage。
- controller 擁有單一 Stage Cache，`runPreview()`、`updatePreparedPreview()` 與 live preview base 可共用；換新圖、重建 state 或 `destroy()` 時必須清空。
- Stage Cache 必須有固定容量上限，避免大圖連續設定變更時無限制保留 `ImageData`。
- 預設 cache key 只包含 operation 自己的 settings；若 operation 讀取其他 feature state 或 pipeline enabled 狀態，該 operation 必須提供 `cacheKey()`。例如 `Palette` 會讀取 Dither 啟用狀態與 Dither settings，因此必須把這些值納入額外 cache key。
- Operation 不可修改 upstream `ImageData`；cache 會重用 operation 回傳的 `ImageData`。若 operation 為 no-op 並回傳原物件，下游 stage identity 應保持與輸入相同，讓後續 stage 可重用。
- Export 不使用 preview Stage Cache，也不直接使用暫存 preview bitmap；它仍從工作圖執行完整正式 pipeline，確保輸出與最新 settings 一致。

## 核心模組邊界

### app

負責主頁外殼，不處理圖片演算法。

包含：

- 初始化。
- header。
- 右上選單。
- 頁面切換。
- theme 切換。
- language 切換。
- 提供 `page-host` 讓功能頁掛載。
- 提供共用 `appContext`，例如 theme、language、目前頁面、全域訊息。

`app` 不可直接操作功能頁內部 DOM，也不可保存某個頁面的 canvas reference。頁面切換時，只能呼叫 page module 的 `mount()` / `unmount()`。

`app-state.js` 只保存 app shell 層級狀態，不保存任何 page-specific state。
theme 與 language 屬於 app shell 層級狀態，必須由 `app-state.js` 正規化，並透過 `settings-store.js` 持久化到 localStorage。theme 需套用到 `body[data-theme]`；language 需套用到 i18n runtime 與 document `lang`。

職責：

```js
const appState = {
    currentPageId: 'dither-editor',
    theme: 'light',
    language: 'en',
    globalStatus: '',
    appMode: 'standalone',
    deviceStatus: 'unavailable',
};
```

允許：

- 目前頁面 id。
- theme。
- language。
- header/status message。
- app mode。
- coarse device status。

禁止：

- 保存圖片資料。
- 保存 editor state。
- 保存 canvas reference。
- 保存 pipeline settings。
- 保存 device token。

### pages

負責不同頁面的組合。

MVP 至少：

- `pages/dither-editor/entry.js`
- `pages/dither-editor/page.js`
- `pages/web-setting/entry.js`
- `pages/web-setting/page.js`

裝置管理（詳見 ESP32 裝置整合章節）：

- `pages/device-info/`
- `pages/device-network/`
- `pages/device-system/`
- `pages/device-epaper-test/`（capability 確認後顯示；white/palette/refresh 共用 e-paper operation service）

預留：

- `pages/help/entry.js`
- `pages/help/page.js`
- `pages/about/entry.js`
- `pages/about/page.js`

每個頁面資料夾都要自成一組，不把頁面專屬 controller、state、panel 散在外層。

### dither-editor page

負責 Dither Image Editor 這一頁的狀態與協調。

包含：

- `entry.js`。
- `page.js`。
- `state.js`。
- `actions.js`。
- `controller.js`。
- `features/*-feature.js`。
- `viewport/*`。
- pipeline 重新運算。

此頁面可以使用 `core` 與 `ui`，但 `core` 不可反向依賴此頁面。

### core

負責純資料處理，不讀 DOM。

包含：

- image loading。
- image exporting。
- canvas helper。
- dither algorithm。
- palette。
- operation pipeline。

`core` 可回傳 `ImageData`、`Blob`、plain object、array、number、string，但不可回傳或保存 DOM element、canvas element、page controller、UI component。

`core/canvas` 只能放通用 canvas helper，例如：

- 建立暫存 canvas。
- 從 image 取得 `ImageData`。
- 將 `ImageData` 轉成 `Blob`。
- 尺寸換算。

`core/canvas` 不可放全域 preview canvas，也不可保存單一 app-wide canvas instance。

`core` 拋出的使用者可見錯誤必須是帶 `code` 屬性的 `Error`（例如 `unsupported-format`、`image-load-failed`、`image-processing-blocked`、`demo-load-failed`、`demo-manifest-missing`、`demo-data-missing`）；英文 `message` 只作為未知 code 的 fallback。顯示層以 code 對應 `i18n` 文字，`core` 本身不可依賴 `i18n`。

色彩通道 clamp 與 RGB 色距權重是 `core/color` 的單一來源：`colorUtils.clampByte`（round）、`colorUtils.clampChannel`（保留小數）、`paletteUtils.createRgbDistanceContext()`。頁面層與 dither processor 不可另寫同語意的 clamp 或距離公式；GPU shader 的距離實作是唯一例外，修改權重時必須同步。

### ui

負責可重用 UI 元件，不知道圖片處理細節。

包含：

- dropzone。
- slider。
- select field（select 的共用包裝：隱藏原生箭頭、疊主題化 chevron，盒位使用全站統一的結尾圖示尺寸，讓各表單元件結尾的 svg 對齊）。
- color swatch。
- sortable list。
- menu。
- tooltip。

`ui` 元件只能透過 options、callback、custom event 對外溝通。它們不可依賴 `pages/*`，也不可依賴 `pages/dither-editor/dither/*`。例如 `sortable-list.js` 只負責排序 UI，不知道排序的是 pipeline、menu item 或 preset list。

### Canvas Ownership

不要建立全域共用 canvas 讓所有頁面共用。每個需要 canvas 的頁面，都在自己的 page module 裡建立與銷毀 canvas。

正確：

```text
pages/dither-editor/viewport/viewport-renderer.js
  owns dither preview canvas rendering

pages/other-page/viewport/other-viewport-renderer.js
  owns its own canvas if needed

core/canvas/canvas-utils.js
  provides helper functions only
```

錯誤：

```text
app/app-shell.js
  owns one global canvas used by every page

core/canvas/canvas-utils.js
  stores shared previewCanvas variable

ui/canvas-view.js
  directly runs dither algorithm
```

頁面切換時，舊頁面的 canvas event listener、object URL、worker、timer 都必須在 `unmount()` 清掉。

## 圖片處理流程

完整流程：

```text
local file
  -> decode image
  -> normalize transparency with white background
  -> downscale if image exceeds input limit
  -> draw to hidden canvas
  -> source ImageData
  -> run fixed before operations
  -> run enabled effects by user-defined effects order
  -> preview ImageData
  -> render preview canvas
  -> export PNG Blob
```

Preview 和 Export 可以共用 pipeline，但執行設定要分開：

```text
preview: 使用完整 working image，可以 debounce，但不以降低解析度作為主要使用者可見結果
export: 使用完整輸出尺寸，永遠重新跑正式 pipeline
```

Preview 策略：

```js
const PREVIEW_DEBOUNCE_MS = 80;
const PREVIEW_SLOW_THRESHOLD_MS = 500;
const SHOW_PREVIEW_TIMING_LABEL = true;
const PREVIEW_TIMING_LABEL_HIDE_DELAY_MS = configuredDelayMs;
```

規則：

- `prepare` group 不執行正式 preview pipeline；page 只用 `viewport-renderer.renderTransformed()` 顯示來源圖與 Crop transform。按 OK 或收合 prepare tool 進入 `edit` 後，才從 `sourceImageData` 跑正式 pipeline。
- `prepare` 的 crop frame scale 必須用 crop frame fit preview stage 內的固定內距區域計算，不可用 source image 盲目 fit 整個 stage；`edit` preview canvas 必須使用同一個 fit rule，讓相同比例的 crop frame 與 result image 保持相同顯示位置與尺寸。
- `prepare` 的 transformed canvas layout 可以在 crop frame 外延伸到完整 preview stage，用於顯示 zoom / pan 的原圖周邊脈絡；延伸 layout 時只能調整 `layout.width` / `layout.height` 與 `layout.frame.x` / `layout.frame.y`，不可改變 crop frame scale。
- `assets/styles/layout.css` 在 `.preview-stage.is-crop-preview` 中必須讓 canvas 可由 `page.js` 明確定位，避免瀏覽器 grid overflow alignment 影響長條圖 crop preview。
- `assets/styles/layout.css` 在 `.preview-stage.is-pixel-preview` 中必須讓 preview stage 成為水平與垂直可捲動容器，canvas 不得套用 fit 模式的 `max-width` / `max-height` 限制；捲軸寬度不可使用 thin，避免大圖檢查時難以操作。
- `viewport/overlay-renderer.js` 的 crop overlay 必須以實際 canvas rect 加上 `layout.frame` offset 定位；不可只用 preview stage 中央公式，否則手機或平板上 canvas 溢出 stage 時，畫面框選與正式 crop output 會產生垂直或水平偏移。
- `viewport-renderer.renderTransformed()` 必須以 `layout.frame` center 作為 transform origin，而不是 layout canvas center，讓預覽 transform 與正式 crop operation 的裁切框中心一致。
- `viewport-renderer.js` 必須先把一般 preview 與 crop transformed preview 畫到 buffer canvas，再提交到可見 canvas，避免 resize canvas 時露出清空畫面。
- `page.js` 在 `edit` result 第一次算完前不可用 `sourceImageData` 當 result fallback 畫面；應保留上一個可見 preview frame，直到 pipeline 結果完成。
- `page.js` 在 `edit` 的 Original view 必須使用 `preparedImageData`；`preparedImageData` 由 `pipeline-runner.runPanelGroup(..., 'prepare')` 產生，代表 prepare group operations 後、edit effects 前的 source。
- prepare setting 變更期間不可即時清掉 `preparedImageData`；離開 `prepare` 並進入 `edit` 時才 invalidated，讓後續 Original view 或正式 preview 使用最新 prepare output。
- `page.js` 的 crop frame fit 與 edit preview fit 必須使用同一個 preview stage content-box 尺寸；若 stage 有 border，需排除 border 厚度再計算置中與縮放。
- 使用者調整 slider、select、color、effects order 時，不立即每次重算，先 debounce `PREVIEW_DEBOUNCE_MS`。
- 正式 preview 使用 working image 的完整尺寸，不使用降低解析度的 `ImageData` 當成使用者可見的最終預覽，避免拖曳中與放開後出現不可信的跳變。
- Edit Result preview canvas 的 backing `ImageData` 必須保留完整 pipeline output；CSS 縮小顯示時不得套用 `image-rendering: pixelated`，避免 dither 單像素點陣在非整數縮放下產生 alias / moire，導致 preview 和實際 export PNG 觀感不同。
- Edit Expand preview 必須重用 Result 的正式 `ImageData`，只改 canvas CSS 顯示尺寸為 backing pixel 尺寸；不可為了 Expand 檢視重新 resize source、重新跑 dither，或產生不同於 export 的像素資料。`viewport/overlay-renderer.js` 必須依 canvas 真實尺寸與 preview stage content box 切換 overflow class，並在進入 Expand 或 preview stage 尺寸改變時將 scroll 初始化到圖片中心點，使 Expand 初始視角對準 Result fit preview 的中心；初始中心基準不可使用扣除 scrollbar 後的 `clientWidth` / `clientHeight`。`page.js` 必須讓 Expand preview 在產生捲軸時支援 pointer drag 平移 scroll 位置。
- controller 必須在正式 preview 排程進入處理時把 `state.previewTimingLabel.phase` 設為 `rendering`，完成正式 preview pipeline 後更新 `state.previewRenderDurationMs` 並把 `state.previewTimingLabel` 設為 `done`；`page.js` 只負責在 `SHOW_PREVIEW_TIMING_LABEL === true` 時依 phase 顯示 Rendering 或格式化耗時文字，並貼齊目前 result canvas 右下角，不應在 DOM 層自行量測 pipeline。
- `state.previewTimingLabel.phase === 'done'` 後，controller 必須依 `PREVIEW_TIMING_LABEL_HIDE_DELAY_MS` 排程切回 `hidden`，並只更新 timing label DOM，不可觸發整頁 render 或重建 feature panel；載入圖片、preview error、destroy 或重新開始正式 preview 時需清掉舊 hide timer，避免舊 timer 關掉新的 label。
- export 永遠從工作圖和完整 pipeline 重新計算，不使用暫存 preview 結果。
- slider 拖曳期間以手感優先，不在每個 `input` event 跑完整 pipeline；可用 `requestAnimationFrame` 更新輕量 live feedback。
- live feedback 只能在「拖曳中看到的結果」與「放開後正式 pipeline 結果」足夠一致時啟用；不一致時寧可不顯示假的即時效果。
- `Adjust` 的 live feedback 僅允許 brightness、contrast、saturation，且只在 `Adjust` 是唯一啟用的 draggable effect 時使用。若 `Palette`、`Dither` 或其他 effect 會參與結果，拖曳中不套假的後處理濾鏡，放開後再更新正式 preview。
- live feedback 應使用 feature 提供的 `createLivePreviewBase()` 與 `livePreviewFilter()`，由 page 只更新 canvas filter，不重跑整頁 render。
- WebGL/GPU 可用於 operation 內部加速，但若需要同步 `readPixels()` 回到 `ImageData`，不可作為拖曳中即時 preview 的主要路徑。Dither GPU 化應優先從 Ordered / Pattern / Palette Mapping 這類逐 pixel 獨立演算法開始；Error Diffusion 類演算法因相鄰像素依賴，不應作為第一批 GPU 化目標。
- 若 1600px 以內工作圖的正式 preview 經常超過 `PREVIEW_SLOW_THRESHOLD_MS`，第二階段優先導入 Web Worker 或真正的 WebGL preview canvas。
- UI 不硬性承諾每次 300ms 內完成，但必須避免使用者連續調整時主畫面長時間卡住。

### Image Input Format Gate

所有進入演算法 pipeline 的使用者圖片都必須先被解碼成 origin-clean RGBA `ImageData`。MVP 只支援：

- `image/png`
- `image/jpeg`
- `image/webp`

檔案選擇器必須使用：

```html
accept="image/png,image/jpeg,image/webp"
```

拖放不能只依賴 `<input accept>`；`loadImageFromFile()` 必須再次檢查 `file.type` 與副檔名，只允許 `.png`、`.jpg`、`.jpeg`、`.webp`。不支援格式必須在 `drawImage()` / `getImageData()` 前被拒絕。

禁止讓 `SVG`、遠端圖片 URL 或可引用外部資源的圖片進入 canvas 後再呼叫 `getImageData()`，因為它們可能造成 canvas taint。若 `getImageData()` 仍遇到 `SecurityError`，必須轉成使用者可理解的錯誤訊息，不能讓瀏覽器原始例外直接漏到 UI。

內建 demo 的來源圖片由 `tools/generate-demo-data/run.py` 掃描 `assets/demo/` 決定。該目錄根層必須剛好有一張支援格式圖片作為 demo source；支援 `.png`、`.jpg`、`.jpeg`、`.webp`，副檔名大小寫不敏感，且不要求固定檔名或 16:9 比例。若找不到候選圖或找到多張候選圖，工具必須報錯並要求使用者保留剛好一張 demo source，避免靜默選錯圖。

`tools/generate-demo-data/run.py` 必須產生固定入口 `assets/demo/demo-manifest.js` 與 `assets/demo/demo-data.js`。Manifest 記錄實際 demo 檔名、同源 URL 與 fallback data script；runtime Load Demo 必須先載入 manifest，再於 Server/GitHub Pages 情境以 `fetch()` 取得同源圖片 blob，轉成 `Blob -> createImageBitmap -> ImageData`。Standalone `file://` 模式若因瀏覽器 origin 規則無法讀取 source image pixels，才 fallback 到 `assets/demo/demo-data.js` 的 data URL。Generated manifest/data 不應手動編輯；替換、重新命名或改變 demo source 後，應重新執行 `python3 tools/generate-demo-data/run.py`。Server/device build 產物不支援 `file://`，必須排除 generated demo data fallback，但保留 manifest 與 source image，讓 server/device runtime 仍可讀取 demo。

## 圖片尺寸與效能策略

MVP 建議最大輸入尺寸：

```js
const MAX_INPUT_LONG_EDGE = 800;
```

規則：

- 使用者丟入圖片後，先檢查寬高。
- 如果圖片長邊超過 `MAX_INPUT_LONG_EDGE`，依比例縮小到長邊 800px。
- 編輯器後續使用縮小後的圖片作為工作圖。
- UI 需提示使用者圖片已被縮小，顯示原始尺寸與工作尺寸。
- MVP 不要求 Web Worker；大型圖片先靠輸入縮小策略控制效能。
- Web Worker 可留到第二階段，當 error diffusion、palette color mapping 或正式 preview 更新開始造成 UI 卡頓時再加入。
- GPU/WebGL 可用於 brightness、contrast、saturation 這類可平行化 operation；error diffusion 類演算法因相鄰像素依賴，不列為第一優先 GPU 化目標。

這個限制能讓純 JS、無 build step、可雙擊執行的版本維持可接受速度，也避免使用者丟入手機高解析照片後讓瀏覽器長時間無回應。

## Dither 規格

MVP 支援：

- Error Diffusion。
- Ordered Dither。
- Pattern Dither。

Error Diffusion algorithms：

- Floyd-Steinberg。
- Atkinson。
- Jarvis-Judice-Ninke。
- Sierra Lite。
- Stevenson-Arce。
- Adaptive FS 3x3。

Ordered Dither algorithms：

- Bayer 4x4。
- Bayer 8x8。
- Blue Noise 64。

Pattern Dither algorithms：

- Dot Halftone。

Other Dither algorithms：

- Dot Diffusion 8x8。

Error Diffusion 與 Dot Diffusion processor 必須接受 `options.errorStrength` 百分比，將誤差擴散量乘上 `errorStrength / 100`。允許範圍為 `0` 到 `150`，UI step 為 `2`；缺值或無效值必須退回標準倍率 `1`。Error Diffusion 不可委派給 RgbQuant reduce；必須由專案內建 processor 執行，並支援目前的 Color Distance。Threshold 類演算法不直接套用 Error Strength 語意，而是把同一份百分比當作 Dither Strength 或 Dot Density。

Error Diffusion hot path 應避免每像素建立暫時 object、`forEach` callback 或跨模組 nearest-color callback。常用 Floyd-Steinberg path 應使用預先計算的擴散係數、typed array 工作緩衝與本地 nearest-index palette search；其他 matrix path 可共用預先編譯的 offset/factor 陣列。擴散誤差寫回工作緩衝時必須把每個 RGB channel clamp 到 `0..255`，維持高 Error Strength 下的穩定性。

具名 Error Diffusion 演算法必須優先保留常見公開實作的 matrix 相對權重與 divisor，避免同名演算法和其他工具輸出大幅偏離。必要轉換只可發生在本專案 offset/factor processor 的資料格式邊界，例如移除會落在當前像素而無法正確輸出 palette 色的權重。

Adaptive FS 3x3 必須先以 integral image 計算局部平均亮度 map，半徑為 `1`。因本專案 Dither 以 palette 為固定輸出色，Adaptive FS 不走灰階-only threshold fallback；它應使用局部平均亮度對 nearest palette color input 做亮度 bias，再以 Floyd-Steinberg 權重擴散原始 RGB 誤差。

Bayer ordered matrices 可由 `buildBayer(size)` 產生，避免手寫大型 16x16 / 32x32 matrix。Blue Noise 64 必須使用 deterministic seed 建立固定 ranking mask，且應 lazy 初始化後重用，避免未選用時增加初始載入成本。Blue Noise 正統常見作法是使用預先或離線產生的 blue-noise threshold texture；本專案目前使用 procedural void-and-cluster-style ranking mask，結果應視為 blue-noise-like，不承諾和特定外部 mask 完全一致。Blue Noise mask 不應出現穩定橫向或直向條紋。Blue Noise 的 ordered threshold strength 可低於 Bayer，減少彩色 palette 下的高頻錯色噪點。

Dot Diffusion 8x8 必須使用 8x8 class matrix 決定 tile 內處理順序；每個像素先透過目前 Palette Mapping 量化到 palette 色，再把 RGB 誤差平均分配給 3x3 鄰域內 class 較高、尚未處理的像素。它不可先把像素 threshold 成黑白亮度，避免多色 palette 輸出退化成黑白。它套用 Error Strength 以調整分配給鄰近像素的誤差量，但不需要 serpentine。

Dot Halftone 是 clustered-dot ordered halftone，不是 dot diffusion。Processor 應使用固定 cell matrix 由中心向外成長網點，並透過目前 Palette Mapping 輸出 palette 色，不可先轉成單一灰階亮度再映射。Dot Halftone 套用 Dot Density 以調整 clustered-dot mask 密度，而不是偏移 threshold cutoff；公式為 `thresholdCellScale = 2 ^ ((dotDensity - 1) * 2)`，因此 `50%` 約為半密度、`100%` 保留既有密度、`150%` 約為兩倍密度。CPU 與 GPU 都必須使用 `floor(pixel * thresholdCellScale)` 取樣 threshold cell。

Palette Mapping 必須透過共用 `paletteMapping` 進入各 Dither processor。Pair Mix 與 Tri Mix 在 Error Diffusion 類沒有 threshold mask 時應輸出混色比例中權重最高的 palette 色，再用實際輸出色計算誤差；Ordered Dither 與 Dot Halftone 類應把 matrix threshold 傳給 Pair Mix / Tri Mix，讓 mask 依照兩色或三色比例決定 palette 色落點。所有 Palette Mapping 都不可輸出 palette 外顏色。

Dither function 不可讀 DOM，不可硬編碼寬高：

```js
function applyDither(imageData, options) {
    const width = imageData.width;
    const height = imageData.height;
    const pixels = new Uint8ClampedArray(imageData.data);

    // process pixels

    return new ImageData(pixels, width, height);
}
```

設定範例：

```js
const ditherSettings = {
    mode: 'error-diffusion',
    algorithm: 'none',
    paletteMapping: 'nearest-color',
    serpentine: false,
    colorDistance: 'euclidean-bt709',
    errorStrength: 100,
    palette: [
        { r: 0, g: 0, b: 0 },
        { r: 255, g: 255, b: 255 },
    ],
};
```

## 透明背景策略

MVP 不處理透明輸出的完整問題。所有透明背景先以白色合成。

固定常數：

```js
const DEFAULT_TRANSPARENT_BACKGROUND = {
    r: 255,
    g: 255,
    b: 255,
    a: 255,
};
```

規則：

- 圖片 decode 後，若像素 alpha 小於 255，先與 `DEFAULT_TRANSPARENT_BACKGROUND` 合成。
- pipeline 後續處理不需要保留透明度。
- PNG export 輸出不保留透明背景。
- 未來若要支援透明輸出，必須新增明確設定，不在 MVP 隱含處理。

## 儲存策略

MVP 只持久化 app shell preference。Dither Editor 工作圖片、pipeline 與 feature settings 不做跨重新整理或關閉瀏覽器後的持久化。

這一節處理 browser storage 邊界；Menu 切頁後回到 Dither Editor 的短期保留，應由 Dither Editor page module 的 in-memory state cache 處理，不依賴 localStorage 或 IndexedDB。

儲存方式：

- `localStorage`：保存 Web Setting / app shell preference，目前包含 theme 與 language。
- `cookie`：現階段不使用。未來若加入後端登入、session 或伺服器需要讀取的狀態，再另行導入。
- `IndexedDB`：MVP 不使用。未來若重新加入 workspace restore，必須先補完整 source image persistence 與 load flow，再更新本 spec。

現階段決策：
- Web Setting 與 app shell preference 只使用 `localStorage`，不使用 cookie。
- Dither Editor 的圖片、workspace、canvas、pipeline settings 與 feature settings 不寫入 localStorage、IndexedDB 或 cookie。
- 同一次 SPA session 的 Dither Editor 狀態保留只靠 `pages/dither-editor/page.js` 的 module-level in-memory `cachedState`。
- cookie 不作為設定 fallback，避免同一份設定有兩個來源造成維護混亂。
- 未來加入後端登入時，cookie 只處理登入/session/server-readable state，不接管目前的 local app settings。

### localStorage schema

```js
const SETTINGS_STORAGE_KEY = 'dither-app:settings:v1';

const settingsValue = {
    schemaVersion: 1,
    theme: 'light',
    language: 'auto',
};
```

儲存規則：

- 使用者圖片不做 browser storage 持久化。
- 不保存 preview `ImageData`。
- 不保存每一步 operation 的中間結果。
- 不保存縮小後的工作圖來源。
- localStorage load 時必須檢查 `schemaVersion`。
- localStorage 開啟或寫入失敗時，功能仍可繼續編輯，但 theme / language 可能無法跨重新整理保留。

需要保存：

- Web Setting theme。
- Web Setting language。
- 同一次 SPA 頁面切換返回 Dither Editor 所需的 in-memory editor state、工作圖片與目前 preview。

不需要保存：

- pipeline effects order、operation enabled 狀態或 feature settings 的跨重新整理持久化。
- 使用者目前工作圖片的跨重新整理持久化。
- 最近使用的 demo preset。
- 最近一次輸出相關設定。
- undo / redo history，MVP 可不保存。
- 每次 preview 的中間結果。

New Image 規則：

- 編輯區提供 `New Image`。
- `New Image` 必須觸發隱藏的 file input，讓使用者選擇本機圖片。
- `Image Input` panel 不可顯示獨立的 `Choose Image` row 或 panel drop zone。
- 成功選擇圖片後，必須走與一般 upload 相同的 `controller.loadFile()` 流程，重建 default editor state 並進入 `prepare`；若沒有 enabled `prepare` feature，則直接進入 `edit`。
- 目前版本不在 UI 暴露空白 canvas 建立入口。

## 離線資源策略

所有 runtime 資源都必須存在專案內。

```text
assets/
  demo/      # built-in demo image assets
  icons/     # optional until custom icons are added
  styles/
```

禁止：

- 遠端 demo image URL。
- CDN script。
- CDN CSS。
- Google Fonts。
- runtime fetch 第三方 API。

允許：

- 讀取本機使用者上傳圖片。
- 讀取專案內 `assets/demo/*`。
- 使用瀏覽器內建字型。
