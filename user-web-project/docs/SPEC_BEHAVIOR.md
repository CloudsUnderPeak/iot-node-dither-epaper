# Dither Image Editor PM 行為 Spec

```text
Version: 0.1.0
Status: Draft
Last Updated: 2026-09-12
Split From: SPEC_INDEX.md
```

本文件用 PM / 產品驗收角度描述使用者可見行為、範圍、畫面互動與成功標準。實作架構、檔案結構、命名、state、pipeline、儲存與演算法細節請看 [SPEC_TECHNICAL.md](SPEC_TECHNICAL.md)。文件入口與閱讀導引請先看 [SPEC_INDEX.md](SPEC_INDEX.md)。

## 設計沿革與現行範圍

最初以 standalone 編輯器起步，目前已包含同源 ESP32 裝置管理、電子紙輸出與可攜式 PNG 專案。下列章節描述現行契約；早期逐日變更由 git history 保留。

- Classic scripts 與本地資產保留 `file://` 直接使用；Python/Make 發佈建置是選用的 HTTP 資產處理流程。
- 圖片與 workspace 不自動寫入 browser storage；切頁使用記憶體狀態，跨 session 由使用者明確匯出／匯入 `.dither.png`。
- 顯示色盤與 EPDIMG 協定色盤分離，校色不改變既有六色 code 或檔案格式。

## 產品目標

建立一個可離線使用、也能在支援裝置上直接驅動電子紙的瀏覽器圖片編輯器。使用者能在本機打開頁面，輸入圖片、調整圖片、設定 Dither 流程、預覽結果；standalone 時輸出 PNG，偵測到相容電子紙時改為更新固定圖片並繪製面板。

核心行為：

1. Standalone Mode 不需要後端服務；Device Mode 只連接同源 ESP32 REST API。
2. 使用者不需要下載遠端資源；所有圖片處理仍在瀏覽器完成。
3. 使用者可以用瀏覽器原生能力完成上傳、編輯、預覽與輸出。
4. App 以固定順序套用會影響圖片結果的效果，避免 palette / dither 語意混亂。
5. App 依 `/api/alive` 與 `/api/epaper` 自動選擇 Standalone 或 E-paper Device Mode，不提供手動偽裝裝置能力的選項。

## 使用者範圍

目前支援：

- 匯入本機圖片。
- 從專案內建 demo 開始操作。
- 透過 New Image 重新選擇本機圖片。
- 裁切、縮放與基礎影像調整。
- 選擇或自訂 palette。
- 選擇 Dither 效果。
- 依固定效果順序預覽結果。
- 預覽處理前後的結果。
- 匯出 PNG。
- 相容裝置上以 runtime capability E-paper profile 更新圖片並繪製。
- 電子紙空白、六色測試圖與 stored image refresh。
- 保留基本工作狀態與設定。

目前不包含：

- 雲端帳號系統（裝置管理員登入已提供）。
- 遠端圖片 URL 輸入。
- 雲端儲存。
- 圖層、annotation、文字工具。
- 電子紙 API 的管理員認證（現行 contract 為同網路公開操作）。
- Preset Manager 的完整管理介面。

## 主要使用流程

1. 使用者打開 `index.html`。
2. App 顯示 Dither Editor 頁面。
3. 第一次進入且尚未載入圖片時，`source` group 的 Image Input 自動展開；只有來源輸入可操作，其餘工具與動作反灰停用。
4. 使用者匯入本機圖片、選擇 demo，或建立新圖片。
5. App 解碼圖片後自動進入 `prepare` group 並只展開 Crop；此時 Image Input、Crop 與 edit tools 可選。
   若部署的 feature manifest 停用或移除 Crop，載圖後直接進入可用的 edit 流程；Crop 設定、控制項與 operation 不存在，其他效果、PNG／圖片專案匯出及 E-paper 的尺寸、六色 palette、encoder guards 仍可使用。
6. 使用者按下右下角顯眼的 OK 或自行收合 Crop 時，App 進入 `edit` group 並展開 Resize、Adjust、Palette、Dither；若使用者改點單一 edit tool，則只展開該 edit panel。
7. `edit` group 會依 Crop 範圍與 Resize、Adjust、Palette、Dither 等設定更新 Result。
8. App 依固定 Effects order 更新圖片處理結果。
9. 使用者可在圖片呈現區右下角切換 Original / Result / Expand。
10. 使用者確認結果後：Standalone Mode 匯出 PNG；E-paper Device Mode 重新跑正式 pipeline、必要時旋轉 portrait、編碼 EPDIMG 並繪製。
11. 使用者可切換到 Web Setting、Help 或 About，再返回編輯頁並保留目前工作狀態。

## User Stories

本節提供給測試 agent 作為操作導向的情境清單。每個 story 都應從使用者可見行為驗證，不以內部實作細節作為主要判斷。

### US-01 Open App Offline

As a user, I want to open `index.html` directly, so that I can use the editor without a backend or build step.

Acceptance:

- Given the project files are present locally, when the user opens `index.html`, then the Dither Editor page is shown.
- The app must not require `npm install`, `npm run build`, a dev server, CDN, or remote API.
- The header, menu button, editor area, and preview area are visible.

### US-02 Load Supported Local Image

As a user, I want to choose a local image file, so that I can edit it in the browser.

Acceptance:

- Given a supported `PNG`, `JPEG/JPG`, or `WebP` file, when the user selects it from Image Input, then the preview area shows the image.
- The editor enters the prepare flow after the image loads, with only Crop expanded and the OK button shown in the preview toolbar.
- While the prepare flow is active, Image Input, Crop, and edit tool rows can be selected.
- After the user presses OK or collapses Crop, the editor enters the edit flow, expands Resize / Adjust / Palette / Dither, and applies the configured pipeline to the cropped range.
- When the user selects one edit tool from the prepare flow, the editor enters the edit flow, collapses Crop, opens only the selected edit panel, and applies the configured pipeline.
- When the user manually expands Image Input or Crop after an image is loaded, all other tool panels collapse.
- The source image must not be uploaded to a server.

### US-03 Reject Unsupported Image Format

As a user, I want unsupported image formats to be rejected clearly, so that I know why the image cannot be edited.

Acceptance:

- Given an unsupported file such as `SVG`, `GIF`, `AVIF`, `HEIC/HEIF`, `RAW`, `PSD`, `TIFF`, or `BMP`, when the user selects it, then the app rejects it before the canvas / pipeline flow.
- The app shows a clear error message.
- The previous valid working image, if any, should not be silently replaced by the unsupported file.

### US-04 Start From Built-In Demo

As a user, I want to load a built-in demo, so that I can try the editor without preparing my own image.

Acceptance:

- Given the user clicks the demo action, then the app loads a project-bundled demo image.
- The demo must not come from a remote URL.
- After loading, the app enters the prepare flow; after OK or Crop collapse, Crop, Resize, Adjust, Palette, Dither, Effects Order, and Export can be tested against the demo.

### US-05 Choose New Image File

As a user, I want New Image to open the local image picker, so that I can replace the current workspace with another image.

Acceptance:

- Given the user opens Image Input and clicks New Image, then the browser file picker opens.
- After the user selects a supported image, the app loads it, resets algorithm settings to defaults, and enters the prepare flow.
- Image Input must not show a separate Choose Image row or a panel drop zone.

### US-06 Crop With Fixed Ratio

As a user, I want to crop with fixed aspect ratios, so that the output matches common display targets.

Acceptance:

- The user can choose one of the supported fixed ratios.
- The default crop ratio is 16:9.
- The crop overlay remains centered and represents the final output area.
- Dragging in the preview moves the image under the fixed crop frame, not the crop frame itself.
- Zoom, rotation, horizontal flip, and vertical flip affect the image transform without changing the selected crop ratio.
- The Crop panel uses a two-column layout: Ratio with Zoom, Rotate with Fill, and a full-width equal button row.
- The Crop panel keeps the same row structure in mobile layouts.
- Fill chooses the color for areas not covered by the source image after crop transform.
- Flip icon buttons should behave visually like the rotate buttons and must not show a persistent active highlight after being pressed.

### US-07 Resize Output

As a user, I want to set output width and height, so that the exported image matches my target display size.

Acceptance:

- The user can set output width and output height.
- Width and height stay locked to the same aspect ratio; changing either value updates the other immediately.
- When the user reopens Crop after editing and changes the crop ratio, Resize width / height must update to the new crop output ratio before the user returns to Resize.
- Width and height are shown on the same row.
- Width and height show a linked-ratio indicator between the two controls.
- Width and height use the same repeated-step unit-number input style as Crop zoom and rotation.
- Width and height values are constrained to the supported output size range.
- Resize does not expose a Fit selector in the MVP.
- Preview and export use the resize settings consistently.
- The exported PNG dimensions match the configured output dimensions.

### US-08 Adjust Image

As a user, I want to adjust brightness, contrast, and saturation, so that I can tune the source before dithering.

Acceptance:

- The user can change brightness, contrast, and saturation.
- Brightness, contrast, and saturation default to `0`.
- Each slider shows its current numeric value on the left.
- Each slider can be dragged to its minimum and maximum ends.
- Preview updates should match the final pipeline result; if live feedback would be misleading, final-result consistency wins.

### US-09 Edit Palette

As a user, I want to choose and edit palette colors, so that dithering uses the colors I intend.

Acceptance:

- Palette starts from `Original`.
- The user can select a preset palette or switch to `Custom` by adding, deleting, or editing swatches.
- Deleting all custom swatches returns the palette state to `Original`.
- The currently effective palette is visible and is used by Dither as its target color set.
- When Dither is active, Palette supplies target colors and does not pre-quantize pixels before dithering.
- When Dither is `None`, a fixed or custom Palette may directly map pixels to the nearest palette colors.

### US-10 Apply Dither

As a user, I want to choose a Dither algorithm, so that the preview and export show the selected dithering result.

Acceptance:

- Dither starts from Floyd-Steinberg.
- Serpentine starts disabled.
- Color Distance starts from `Euclidean RGB`.
- The shared strength slider starts from `100%` and is adjustable from `0%` to `150%` in `2%` steps.
- The strength slider is shown as `Error Strength` for Error Diffusion and Dot Diffusion algorithms, and applies to error diffusion coefficients.
- The same strength slider is shown as `Dither Strength` for Bayer and Blue Noise threshold algorithms, and applies to threshold strength across Palette Mapping modes.
- The same strength slider is shown as `Dot Density` for Dot Halftone, and changes clustered-dot density while keeping `100%` as the default density.
- Switching between algorithms resets the strength slider to `100%`.
- Choosing an algorithm updates the result preview.
- Choosing a Color Distance updates the result preview.
- Changing the strength slider updates the result preview when the selected algorithm supports strength adjustment.
- Returning to `None` disables dither output changes while leaving Palette behavior available.
- Dither uses the current effective palette as fixed output colors.

### US-11 Fixed Effects Order

As a user, I want effects to run in a predictable order, so that Palette and Dither results are easier to understand.

Acceptance:

- Effects run in the fixed edit order: Adjust, Palette, Dither.
- Tool rows do not show drag handles.
- Fixed non-effect steps remain outside the edit effects order.
- Export is not part of the image effects order.

### US-12 Export Result Or Image Project

As a user, I want to export the processed result as PNG, so that I can use it outside the editor.

Acceptance:

- Given a valid working image, when the user clicks Export, then the app produces a PNG.
- Export runs the formal pipeline instead of relying on a stale preview bitmap.
- If export fails, the app shows an understandable error state.
- In Standalone Mode the action remains `Export PNG`, and the image-project export action is hidden.
- In E-paper Device Mode `Draw to E-paper` uses the processed credit-card/edit icon. A separate primary `Download Image Project` action appears below it with the export/download icon.
- The image project uses the `.dither.png` suffix. A normal PC image viewer shows its final dithered PNG, while importing it restores the embedded original image and supported editor settings.
- Image-project export remains available while the device is offline or cooling down because it is a local browser operation.
- Every editor upload path automatically routes ordinary PNG/JPEG/WebP images to normal image loading and valid `.dither.png` files to project restore.
- Project routing is content-first: browser-renamed files such as `.dither(1).png` remain importable when valid project chunks are present. A `.dither.png` without project data, corrupt PNG/chunk data, an unsupported schema/feature version, or invalid embedded image/settings is rejected with an understandable error.

### US-13 Navigate Away And Return

As a user, I want to open Web Setting, Help, or About and return to Dither Editor, so that I do not lose my current editing session.

Acceptance:

- Menu navigation updates the current page.
- Browser back / forward switches pages consistently.
- Returning to Dither Editor restores the current working image, settings, effects order, and preview state for the current session.

### US-14 Use On Narrow Viewport

As a user on a small screen, I want the editor layout to remain usable, so that I can complete the main workflow on mobile-sized viewports.

Acceptance:

- Preview, tool dock, and open tool panels remain accessible.
- The Crop prepare step must not cause the preview stage or whole editor to grow beyond the viewport in a way that breaks operation.
- Text and controls must not overlap.
- The user can load an image, adjust settings, preview, and export from a narrow viewport.

### US-15 Learn From The Help Center

As a user, I want the Help page to behave like a small documentation space, so that I can learn the workflow and understand how algorithm settings affect output.

Acceptance:

- Help provides separate documents for the Help home, project introduction, quick start, Dithering Algorithms overview, Error Diffusion, Ordered / Blue Noise, Dot-based Algorithms, Palette Mapping, and Color Distance.
- Every document has a shareable nested route under `#/help` and browser back / forward restores the corresponding document.
- The left document tree, breadcrumb, previous / next links, and desktop on-page table of contents remain consistent with the active document.
- Desktop uses a documentation layout with a document tree, article, and optional on-page table of contents; narrow viewports collapse the document tree behind an explicit button and keep the article in one column.
- Help content follows the current English or Traditional Chinese language without losing the active document when language changes.
- Algorithm guides use the project-supported algorithms and controls, not options that only exist in external reference projects.
- The visible algorithm cards follow the runtime Dither Algorithm registry order. Removing an algorithm removes its card; a newly registered algorithm appears in its declared family even before long-form Help is authored.
- Missing long-form algorithm content uses a clear fallback and emits a development validation error without preventing Help from loading.
- Input working-size and configured output-size limits are rendered from the editor constants instead of duplicated numeric Help text.
- Visual comparisons identify their synthetic source size, palette, algorithm, mapping, distance, and strength settings.
- The comparison slider, algorithm/mapping/distance tabs, and Color Distance palette explorer remain keyboard accessible.
- Opening Help and returning to Dither Editor preserves the current editor session.

### US-16 Draw To A Detected E-paper Device

As a user with a compatible device, I want the editor to lock its output to the panel and draw it safely, so that the browser result matches the physical display without allowing conflicting operations.

Acceptance:

- `GET /api/alive` and a valid `GET /api/epaper` capability response switch the editor to E-paper Device Mode; an absent or unsupported endpoint leaves PNG export unchanged.
- Crop only offers the capability panel ratio W:H and its inverse H:W, reduced by gcd. Square panels have one option. Resize is read-only W×H or H×W, and Palette is the fixed E6 six-color set.
- A newly loaded image or demo initially selects the inverse panel ratio when its decoded original height exceeds its width; landscape and square images select the panel ratio. The user can still switch either ratio afterward, and background target/calibration synchronization does not override that valid choice.
- The fixed palette has separate display RGB A′ and protocol RGB A values. Both arrays and the six-color test image follow EPD code order `0,1,2,3,5,6` (black, white, yellow, red, blue, green). Swatches, the Result canvas, palette mapping, and dither error use A′; the encoder boundary maps the exact palette index back to A before producing EPDIMG.
- The formal pipeline decides orientation from the actual output dimensions. W×H stays unchanged; H×W rotates clockwise to W×H before EPDIMG encoding; any other size is rejected before upload.
- The e-paper action sends one gzip `POST /api/epaper/image`. The accepted upload already updates the stored image and queues draw, so the editor does not append a refresh request.
- Processing, upload and physical draw show a blocking spinner, phase copy and simulated percentage. Percentage is weighted by expected time, with `refreshing` using the largest interval; repeated polling of the same phase must not reset its timer, and an over-time phase keeps moving asymptotically without reaching 100% before server success/cooldown.
- While an operation is active, editing, Menu navigation and repeated action are blocked. Cooldown unlocks editing but keeps every e-paper action disabled and displays the interpolated `retry_after_seconds`. At local zero the client immediately rechecks status until the server restores `can_draw`.
- A disconnect after HTTP 202 never causes automatic upload retry. The client waits for reconnect and confirms `/api/epaper/status` before allowing another action.
- A separate local `Download Image Project` action remains enabled during disconnection and cooldown; it does not call the device API or change draw admission state.

## 頁面與導覽行為

App 由一個固定外殼承載多個頁面：

- `Dither Editor` 是預設主頁。
- `裝置資訊`、`網路`、`裝置設定` 組成 Menu 的「裝置管理」群組；確認 e-paper capability 後，同群組另顯示「面板測試」。
- `Web Setting` 用於一般網頁設定，例如主題。
- `Help` 用於使用說明。
- `About` 用於產品資訊。

導覽行為：

- 使用者第一次開啟頁面時，若 URL 沒有指定頁面，進入 Dither Editor。
- 使用者透過 Menu 切換頁面時，瀏覽器 URL 需要反映目前頁面。
- 使用者按瀏覽器上一頁或下一頁時，App 需要切換到對應頁面。
- 返回 Dither Editor 時，應恢復該 session 內的編輯狀態。
- Help 子文件使用 `#/help/...` 巢狀路徑；切換子文件時不需要卸載整個 Help page，但 URL、上一頁與下一頁必須同步。

### Help 文件中心

Help 不是單一 placeholder 文章，而是內建文件中心。文件樹固定包含：

```text
Help
├─ Help 首頁
├─ 開始使用
│  ├─ 專案介紹
│  └─ 快速操作
└─ 演算法指南
   ├─ 抖色演算法
   │  ├─ Error Diffusion
   │  ├─ Ordered / Blue Noise
   │  └─ Dot-based Algorithms
   ├─ 調色盤映射
   └─ 色彩距離
```

桌面版顯示左側文件樹、中間文章與空間足夠時的右側頁內目錄。窄版將文件樹收合為文章上方的明確按鈕，文章改為單欄。文章必須提供 Breadcrumb 與上一篇／下一篇，所有文件連結都要能複製或另開分頁。

演算法解說必須以目前產品實際註冊的 Dither Algorithm、Palette Mapping 與 Color Distance 為準。Dither Algorithm 卡片依 runtime registry 順序與 `helpFamily` 顯示：移除註冊後自動消失，新增但尚無雙語長文時仍顯示名稱、processor、支援控制與結構 fallback，同時讓開發驗證回報缺漏。每種已有解說的演算法以一致格式說明摘要、特性、適用情境、注意事項、控制項與結構。

Help 內的輸入工作圖長邊與可設定單邊輸出上限必須由 editor constants 暴露的 capability fact 取得，不可複製固定數字。完整句子或含單位文字透過 i18n placeholder 排版；同一 placeholder 在字串內出現多次時必須全部替換。比較圖必須由專案 dither render 流程產生，並在畫面標示重現設定。Help 可提供 Before / After 拖曳、範例切換、matrix/kernel 圖解與 Color Distance 互動色票選擇，但不得改變 Dither Editor 工作圖片或 settings。

## 裝置管理行為

本專案部署到 iot-node-bedrock `user-web/` 後，網頁由 ESP32 裝置提供；Menu 的「裝置管理」群組提供監看與設定入口。

裝置頁的視覺語言必須與 Dither Editor 對齊：卡片使用編輯器面板樣式（緊湊標題列＋內容區），表單為左標籤欄密度，輸入框、按鈕與字級沿用編輯器的尺寸階；頁內不放大標題，頁名由 app header 顯示。

直接開啟 source（`file://` 或本機 server）或 `make demo` 的輸出時，裝置頁使用假資料以便確認 UI：header 顯示常駐「PREVIEW」標示（滑入可見測試帳密與失敗模擬方式），登入、掃描、Wi-Fi 儲存與驗證成功／失敗、修改密碼與 hostname 皆可完整操作；網址加 `?mock=0` 可關閉。`make build` 的正式產物不含假資料程式；此時偵測不到裝置的環境顯示「未偵測到裝置」引導文案並反灰。Dither Editor 在任何情境都不受影響。

### 連線狀態指示與斷線反灰

- header 右上的狀態圓點是裝置連線的全站指示：綠＝裝置連線正常、紅＝裝置已斷線、灰＝未偵測到裝置；編輯器的短暫 busy 狀態仍可暫時覆蓋為藍色。tooltip 顯示目前語言的狀態文字。
- 前端每 5 秒以 alive API 偵測；連續 2 次失敗才判離線（避免單次逾時誤判），恢復 1 次成功即解除。分頁切到背景時暫停偵測，回到前景立即補查。
- 裝置斷線時：三個裝置頁全部反灰鎖定、無法送出任何設定，頁面頂部顯示常駐的「裝置連線中斷，正在自動重試…（最後成功連線 n 秒前）」。Menu 不另外標示連線狀態，避免與 header 圓點重複。
- 恢復連線後：自動解除反灰、重抓本頁資料，並顯示約 2.2 秒的「已恢復裝置連線」。
- Wi-Fi 設定套用期間（裝置可預期短暫斷線）不判離線，由套用進度訊息呈現狀態。

### 登入與登出

- 裝置資訊與網路狀態不需登入即可查看；只有進入設定（Wi-Fi 設定、裝置設定）才要求登入。
- 未登入時設定區顯示鎖定卡與「登入解鎖」；登入 dialog 只有標題、帳號（固定 admin、唯讀）與密碼兩個欄位，不再重複說明用途，初始焦點在密碼欄，密碼錯誤顯示目前語言的錯誤訊息且不清空帳號。
- 登入成功後原地解鎖目前頁面，不跳頁；Menu 底部出現「登出」。
- 裝置重啟或他人登入會使目前登入失效：下一次操作自動退回鎖定卡並提示重新登入。

### 裝置資訊頁（公開唯讀）

- 四張卡片：裝置（晶片型號、CPU 核心、Flash、記憶體使用、MAC、主機名稱、configured Wi-Fi 發射功率，單位為 dBm）、電源、韌體空間與檔案儲存空間。電源卡位於裝置卡正下方，呈現量測電壓、電壓曲線推估的電池電量與取樣時間；API 欄位為 null 時顯示 unavailable，不得由這些數值宣稱供電來源、電池存在或充電狀態。韌體空間與檔案儲存空間使用薄型分段長條圖＋分欄 stat：每分段一欄、標籤在上數值在下、欄間直線分隔，長標籤在欄內換行；滑入長條分段顯示「名稱：容量」。兩張容量卡在標題右側顯示總空間，數值等於各分段之和，也就是長條圖的 100% 基準。分段配色使用全站主色的深→淺階梯，可用空間為中性灰，light／dark 自動對應。
- 進入頁面即抓取一次資料，之後每 10 秒背景更新；不提供手動重新整理，也不顯示更新頻率或資料過期字樣。更新失敗時畫面沿用最後成功數值，離線提示只由頁面頂部的連線 banner 呈現。
- 「可用空間」代表當下單一新檔的上傳上限，是未來上傳圖檔到裝置的容量依據。

### 網路頁

- 上半部連線狀態卡（公開）：Wi-Fi mode 標示、STA（SSID、狀態、IP）、AP（SSID、狀態、IP、密碼保護開關狀態）、mDNS 網址（STA 連線後才可點擊）；每欄開頭有對應的 feature icon（STA=Wi-Fi、AP=訊號、mDNS=地球，accent 底色方塊，比照 builtin-web Interface status）；桌面版三欄一列（mDNS 欄較寬，網址單行不折行）；每 10 秒自動更新，且不覆蓋下方表單草稿。
- 下半部 Wi-Fi 設定（需登入）：連線模式三選一（AP／STA／AP + STA，右上三角形標示裝置目前已儲存的模式）；切換模式只顯示適用欄位。STA 提供 SSID＋「選擇網路」掃描、安全性（WPA／開放）、密碼（SSID 未變時留空沿用既有密碼）與「進階設定」面板（內含連線失敗退回 AP 開關與 IP 設定 DHCP／靜態）；AP 提供 SSID、密碼保護開關（AP 密碼即管理員密碼，變更後裝置會重啟）與進階位址設定。Wi-Fi 密碼與 AP 密碼保護的說明使用與裝置設定頁一致的限制區塊，Wi-Fi 密碼提示以「Wi-Fi 密碼限制：…」開頭。STA 與 AP 兩段之間有可見分隔線與較大留白，「進階」為可展開的獨立面板（可見邊框、淡底色，展開後標題下有分隔線），不與一般欄位混成一串。
- 儲存列固定顯示三態文字：尚無變更／可儲存（提示連線可能短暫中斷）／請修正無效欄位；儲存中禁止重複送出，「還原」可放棄草稿。儲存列永遠是單列：桌機狀態文字過長時省略，手機寬度隱藏狀態文字並讓「還原」與「儲存並套用」平分整列，按鈕不得換行到左側。
- 開關類欄位（連線失敗退回 AP、AP 密碼保護）：標籤與開關相鄰成一組並靠左，樣式沿用 Dither Editor 的 toggle switch。只有開關本身可切換，點擊標籤文字不會改變狀態。
- 安全性選項固定用英文（WPA／Open），與掃描結果顯示的加密方式（大寫，如 WPA2／OPEN）一致，同一個概念不混用中英文。
- 進階 IP 設定切到靜態時，每個欄位以 placeholder 提供範例位址（如「例如 192.168.1.50」），只作示範、不預填也不影響驗證。
- 從 AP 切換到 STA／AP + STA 時顯示「正在驗證新的 Wi-Fi 設定（最多 25 秒）」；驗證成功依情境提示新 IP、AP 即將關閉或需依新 AP 重新連線；驗證失敗顯示「已還原原設定與 AP」並保留表單草稿；期限內未取得結果時提示改用裝置 AP 或 mDNS 網址確認，草稿保留。
- 掃描視窗依訊號強度排序、顯示四級訊號圖示與 RSSI／channel／加密方式；篩選欄與「再次掃描」並列在標題下方同一列，兩者各有圖示（漏斗、重新整理）。重新掃描有倒數冷卻，倒數期間按鈕顯示「等待 n 秒」且寬度固定不隨文字變化；裝置忙碌或掃描過於頻繁時顯示稍候重試。選用只回填 SSID 與安全性，不自動送出。

### 裝置設定頁（需登入）

- 裝置名稱卡：hostname 輸入與 mDNS 網址預覽；規則提示以「主機名稱限制：…」開頭，明確說明長度與可用字元是設定限制而非目前狀態。儲存成功提示網路服務與 mDNS 重新套用、連線可能短暫中斷。
- 管理員密碼卡：新密碼＋確認新密碼，顯示／隱藏切換以圖示疊在輸入框右側，不佔用額外按鈕欄位；規則提示以「密碼限制：…」開頭。規則不符或兩次不一致時無法送出。成功後目前登入立即失效並要求以新密碼重新登入；AP 密碼保護開啟時，訊息明確包含裝置正在重啟。卡片內不重複提示重新登入，該資訊由儲存後的訊息負責。
- 恢復原廠設定（danger zone）：整張卡片使用紅色邊框、淡紅底與警告徽章，與其他設定卡明顯區隔；說明必須寫明會清除所有 Wi-Fi、管理員設定與使用者檔案，且無法復原。
- 按下「重設裝置」不直接執行，先開紅色確認 dialog（警告徽章、紅色標題、實心紅色送出鍵、預設焦點在取消）；送出期間鎖住兩個按鈕且不可用 Esc／背景關閉。成功後裝置立即重啟，本地登入立即失效並顯示常駐訊息，指示改用出廠預設的裝置 AP 重新連線並重新登入；失敗顯示錯誤訊息且不改變登入狀態。

### 電子紙模式與測試頁

- 裝置 alive 後才查 `/api/epaper`；只有固定 panel/image/refresh/capabilities schema 可被前端支援時，E-paper target 才在本次 session 生效。曾確認的 target 遇到暫時斷線仍保持尺寸與色票鎖定，恢復後重新同步 status。
- Dither Editor 的 Crop ratio 只允許 capability 的 Landscape W:H 與 Portrait H:W（gcd 化簡；正方形不重複）。使用者仍可 pan、zoom、rotate、flip 與選 fill；Resize 不可手動輸入，Palette 不可選 preset 或編輯 swatch。
- E-paper Device Mode 載入新圖片或 demo 時，以瀏覽器解碼後、工作圖縮小前的原始尺寸判斷初始 Crop ratio：高大於寬用 H:W，寬大於或等於高用 W:H。圖片先載入而 capability 後確認時，首次切換裝置模式套用相同規則。這只設定該圖片的初始值；使用者可手動切換，後續輪詢、校色更新、斷線重連、旋轉、翻轉或重新繪製不可覆蓋目前 panel 合法的 H:W／W:H 選擇。
- 固定六色以實體面板肉眼呈現的校色值 A′ 供網頁 Result、最近色判斷與 dither 誤差擴散使用；正式繪製時依六色固定 index 轉回裝置認得的協定色 A。調整校色值應改變網頁預覽與抖動選色，但不得改變硬體 color code。
- 原 `Export PNG` 按鈕在此模式顯示「繪製到電子紙」，並使用處理過的 credit-card/edit SVG。其下方另顯示使用 export/download SVG 的主按鈕「下載圖片專案」；兩顆按鈕底色一致。圖片專案是本機操作，裝置離線或 cooldown 期間仍可使用，且不得呼叫裝置 API；建立期間文字保持不變、按鈕暫時停用，不顯示取消文案。Standalone Mode 隱藏此按鈕。繪製完成進入 cooldown 後，全頁操作鎖解除，但繪製 action 顯示實際剩餘秒數並維持 disabled；本地倒數歸零後透過共用的每 5 秒 status 更新確認，收到 `can_draw` 恢復後啟用，不另外增加高頻請求。
- 全域操作 overlay 使用 spinner、目前 phase、percentage 與 progress bar。Percentage 是前端依預估時間插值的進度提示，不是 panel telemetry；`refreshing` 是最大區間，同一 phase 的重複 polling 不可重設計時，phase 超時後仍漸近移動，server 進入 cooldown success 才顯示 100%。
- 「面板測試」頁只在 session 已確認 capability 後出現在 Menu；公開且不要求登入。頁名只由 app header 顯示；內容以「電子紙狀態」卡呈現 panel/status/cooldown，並以「面板診斷」卡提供：
  - 顯示空白：`POST /api/epaper/image/white`，不寫 stored image。
  - 顯示六色測試圖：`POST /api/epaper/image/palette`，不寫 stored image。
  - 重新繪製目前圖片：`POST /api/epaper/image/refresh`，stored image 不存在或無效時不可操作。
- 同頁的「六色色準調整」依 EPD code `0,1,2,3,5,6` 顯示黑、白、黃、紅、藍、綠。色票或 RGB 欄位輸入時六段預覽即時更新，但不發送 API；六組值全部為 `0..255` 整數且互異時才可儲存。
- 儲存是完整六色 replacement；成功後清除 dirty 並立即同步抖色編輯器。重載會在 dirty 時要求確認；恢復預設一定要求確認。若編輯期間 canonical 被其他頁面更新，不自動覆蓋草稿，必須先重載。
- 離線、載入或儲存中禁止操作；離線樣式不可改變六色預覽本身的 RGB。色準 API 公開，不顯示登入鎖定卡。
- 三個測試 action 不送 request body，並和 editor upload 共用 single-operation admission、overlay、status polling、錯誤處理與 180 秒 cooldown。

### 驗收重點

- 裝置斷電後 10 秒內圓點轉紅、裝置頁反灰且無法送出設定；恢復供電後自動回綠並刷新資料，全程不需重新整理頁面，且裝置資訊頁在恢復後自行更新數值。
- 於另一個瀏覽器登入後，原瀏覽器的下一次設定操作退回鎖定卡並提示重新登入。
- Wi-Fi 驗證失敗（例如密碼錯誤）時，原設定與 AP 不受影響、表單草稿保留。
- 修改密碼後舊登入立即失效；修改 hostname 後可用新的 mDNS 網址開啟頁面。
- 完整重設必須經過確認 dialog；確認後裝置回到出廠預設 AP，且需以預設密碼重新登入。

## 版面行為

Dither Editor 主畫面包含：

- 標題區：顯示產品名稱、狀態與 Menu。
- 編輯區：放置圖片輸入、效果順序、各工具設定與匯出入口。
- 圖片呈現區：顯示 preview canvas、crop overlay、zoom / pan 等互動結果。

響應式要求：

- 桌面與手機版都必須被 viewport 高度約束，不可讓 `prepare` 中的 Crop 流程或 preview canvas 把整個頁面撐高。
- 編輯區可以捲動，但圖片呈現區與標題區不應因控制項過多而被擠出主要視野。
- Tool Row 與 Tool Panel 的展開、收合、捲動都要保持穩定，不應因 scrollbar 或內容高度造成明顯跳動。
- 使用者在窄螢幕上仍應能完成匯入、裁切、調整、預覽與匯出。

## 編輯區行為

### Editor Flow Groups

Dither Editor 有三個使用者可見流程 group，另有一個不顯示在工具面板中的 feature 分類：

- `source`：來源輸入流程。沒有來源圖片時，Image Input 面板自動展開並保持可用；Crop、Resize、Adjust、Palette、Dither、Effects Order、Export 與 Original / Result 反灰或隱藏，不可設定；右下角 preview toolbar 不顯示任何按鈕。已有來源圖片時手動回到 `source`，其他 tool panel 必須收合，preview toolbar 不顯示按鈕但保留高度。
- `prepare`：正式編輯前準備流程。目前 Crop 是唯一的 `prepare` tool。Preview 顯示原圖與 crop transform，不套用 Resize、Adjust、Palette、Dither 或其他非 Crop 演算法。Image Input、Crop 與 edit tool rows 可選；右下角 preview toolbar 只顯示 `+`、`-`、OK 三個按鈕。
- `edit`：Crop 已確認或收合。App 以 Crop 範圍作為 pipeline 輸入，依目前演算法設定更新 Result；Original 顯示 prepare 後的原圖，不顯示未經 prepare 的 source image。Expand 顯示 Result 的真實輸出像素。從 Crop 進入 `edit` 時，Resize、Adjust、Palette、Dither 預設展開，右下角 preview toolbar 顯示 Original、Result 與 Expand。
- `none`：無工具面板流程歸屬。未宣告 `panelGroup` 的 feature 不顯示在左側工具面板，也不形成使用者可切換的流程。

流程轉換：

- 載入任何新圖片或 demo 後，App 必須重設演算法設定為 default，並進入 `prepare`；若沒有 enabled `prepare` tool，則直接進入 `edit`。
- 在 `prepare` 按下 OK 或自行收合 Crop 後，App 進入 `edit` 並開始計算正式 preview。
- 在 `prepare` 點選單一 edit tool 後，App 必須離開 `prepare`、收合 Crop、進入 `edit`，並只展開被點選的 edit panel。
- 在 `edit` 重新展開 Crop，視同回到 `prepare`，並收合其他面板；此時停止顯示演算法結果，回到原圖 crop transform preview。
- 在已載入圖片後手動展開 Image Input，Crop 與其他編輯面板必須收合；若原本在 `prepare`，視同離開 Crop 並回到正式 preview 流程。
- 不支援格式載入失敗時，不應清掉上一個有效工作區；若沒有上一張圖，維持 `source`。

### Image Input

使用者可以：

- 在無來源圖片時的畫布中央拖放圖片。
- 在無來源圖片時的畫布上傳區點擊 Browse File 按鈕選擇本機圖片。
- 透過 Image Input 選擇內建 demo。
- 透過 Image Input 的 New Image 重新選擇本機圖片。

限制：

- 匯入支援 `PNG`、`JPEG/JPG`、`WebP` 與內容有效的 `.dither.png` 專案。
- 不支援 `SVG`、`GIF`、`AVIF`、`HEIC/HEIF`、`RAW`、`PSD`、`TIFF`、`BMP` 等格式進入演算法流程。
- 不支援格式必須在進入 canvas / pipeline 前被拒絕，並顯示明確錯誤。
- demo 必須來自專案內 `assets/demo/*` 的單一支援格式圖片資源，不可依賴遠端 URL，也不可在 runtime 由程式臨時產生假 demo；demo 圖檔名稱與比例不應被固定為特定值。
- Image Input panel 在任何流程下都不應顯示獨立的 Choose Image row 或 panel Drop Zone；無來源圖片時的主要上傳入口必須集中在畫布中央。
- 不接受遠端圖片 URL 作為 MVP 輸入來源。

### Effects Order

使用者可以：

- 看到目前啟用的圖片處理效果。
- 展開單一效果的設定面板。
- 啟用或停用可選效果。
- 依固定順序套用 edit effects。

行為要求：

- 預覽與匯出結果都要依照固定順序重新計算。
- Effects Order 只在 `edit` 可操作；`source` 與 `prepare` 時必須反灰停用。
- 固定前置流程不應被使用者拖曳。
- Export 不應成為圖片效果順序的一部分。

### Crop

使用者可以：

- 從固定比例清單選擇裁切比例。
- 調整圖片 zoom。
- 旋轉原圖。
- 左右反轉原圖。
- 上下反轉原圖。
- 選擇 transform 後原圖未覆蓋區域的底色。
- 在 preview 區拖曳原圖位置。
- 在 crop overlay 上用滑鼠滾輪調整 zoom。
- 在 crop overlay 上用觸控螢幕雙指縮放調整 zoom。

限制：

- MVP 不提供 Free 自由比例。
- 預設固定比例為 16:9。
- E-paper Device Mode 覆蓋 standalone 預設，只顯示目前面板 W:H 與 H:W；新圖初始方向依原始尺寸選擇，切換比例時同步切換 W×H／H×W target。Standalone 仍使用 16:9 預設。
- Crop 面板不顯示 X、Y、Width、Height 或 Lock ratio。
- 使用者拖曳的是原圖位置，不是裁切框。
- Crop overlay 固定代表最後輸出的裁切範圍。
- 左右/上下反轉必須作用在原圖 transform，preview 與正式輸出需一致。
- 底色選項提供 Black、White、Custom；選 Black / White 時 color picker 顯示對應顏色，手動調整 color picker 時選項自動切成 Custom。
- 使用原生 color picker 微調底色時，調色盤必須維持開啟直到使用者完成選色。
- 底色只填補旋轉、平移、縮放或翻轉後原圖未覆蓋的 crop transform 區域，不是頁面背景；prepare 預覽時只顯示在 crop frame 內，frame 外仍可透出沿用既有灰階 theme tokens 的 5x5 分組網格背景與原圖脈絡。
- 若原圖已有 rotation，點擊左右/上下反轉時 rotation 需同步取反，並鏡射對應 pan 軸，讓反轉以目前畫面座標為準。
- 只要 Crop 展開，App 就是 `prepare` 流程，且其他 tool panel 必須收合；使用者可按 OK 或再次收合 Crop 進入 `edit`。

### Resize

使用者可以設定：

- output width。
- output height。

行為要求：

- Width 與 Height 固定等比連動。
- E-paper Device Mode 中 Width/Height 改為 read-only，依 Crop orientation 固定為 capability W×H 或 H×W。
- 使用者調整任一尺寸時，另一個尺寸必須立即依目前比例更新。
- 使用者在 `edit` 重新展開 Crop 並改變 crop ratio 後，Resize 的 Width / Height 欄位必須同步更新到新的 crop output ratio。
- Width 與 Height 必須顯示在同一列。
- Width 與 Height 中間必須顯示等比連動提示圖示。
- Width 與 Height 必須使用和 Crop zoom / rotation 一致的數字輸入樣式；按住上下箭頭時數值必須連續增減。
- Width 與 Height 必須限制在 `1..4096px` 的合法輸出尺寸內；等比換算時若另一邊會超過上限，使用者正在調整的那一邊也必須被壓回可維持比例的最大值。
- Resize 不顯示 Fit 選單。
- 輸出的圖片尺寸要符合設定。

### Adjust

使用者可以調整：

- brightness。
- contrast。
- saturation。

行為要求：

- 預設值必須是不改變圖片的 identity 狀態。
- 預設值必須為 `0`。
- 每個 slider 左側必須顯示目前數值。
- slider 必須可拉到最小與最大端點。
- 若 live feedback 和正式 pipeline 結果會跳變，應以正式結果一致性優先。
- Gamma 不作為主要控制項。

### Palette

使用者可以：

- 使用 Original palette。
- 選擇固定 preset palette。
- 建立 Custom palette。
- 新增、刪除或修改色票。

行為要求：

- Palette 預設為 Original。
- Original palette 必須使用專案內 vendored RgbQuant 的代表色萃取流程，而不是手寫明暗錨點 heuristic。
- Original palette 應以 Crop/prepare 後的裁切範圍做區塊統計、hue retention 與 BT.709 euclidean 色距合併產生代表色；使用者重新調整 Crop 後，Original 色票必須在離開 prepare 進入 edit 時重新萃取。
- Original palette 的 Colors 預設為 8，可調範圍為 2 到 32；此控制位於 Preset 下方，且只在 Palette 為 Original 時顯示，並需保留上下調整按鈕前的緩衝區以降低窄螢幕誤點。
- Original 不主動改變圖片。
- 手動變更色票後，狀態切換為 Custom。
- 使用原生 color picker 微調色票時，調色盤必須維持開啟直到使用者完成選色。
- Palette 色票每列最多顯示 8 個，超過時換到下一列。
- 新增色票按鈕必須以圓形外框包住加號，讓新增動作和一般色票清楚區分。
- Custom 是目前工作區設定，不是固定 preset。
- 色票被刪到空時，回到 Original。
- Palette 當前有效色票必須同步給 Dither 使用。
- Dither 啟用時，Palette 不先量化像素；Dither 以目前有效色票產生固定色點陣結果。
- Dither 為 None 時，固定 preset 或 Custom Palette 可直接把像素映射到最近色。
- 最近色映射使用 Dither 面板目前選擇的 Color Distance；預設為 Euclidean RGB。
- E-paper Device Mode 強制使用 E6 六色；不顯示新增、刪除、preset、Original Colors 或 swatch 編輯控制。色票固定依 EPD code `0,1,2,3,5,6` 排成黑、白、黃、紅、藍、綠，與面板六色測試圖一致。色票、Result canvas、palette mapping 與 dither error 使用可校正的 DISPLAY RGB A′；encoder boundary 再依相同 code slot 精確轉成 OUTPUT RGB A，EPDIMG 不接受其他 RGB。

### Dither

使用者可以：

- 選擇不套用 Dither。
- 選擇支援的 Dither algorithm。
- 選擇 Palette Mapping。
- 調整目前 Dither algorithm 的強度百分比。
- 配合目前有效 palette 產生處理結果。

行為要求：

- Dither 預設為 Floyd-Steinberg。
- Algorithm 選單支援 Floyd-Steinberg、Atkinson、Jarvis-Judice-Ninke、Sierra Lite、Stevenson-Arce、Adaptive FS 3x3、Bayer 4x4、Bayer 8x8、Blue Noise 64、Dot Diffusion 8x8 與 Dot Halftone。
- Palette Mapping 選單支援 Nearest Color、Pair Mix 與 Tri Mix。
- Serpentine 預設為關閉。
- Serpentine 使用 Toggle Switch 呈現。
- Color Distance 預設為 Euclidean RGB，使用者可切換支援的距離公式；既有已儲存的有效選項維持原值。
- 強度百分比預設為 100%；Error Diffusion 與 Dot Diffusion 顯示為 Error Strength，Bayer 與 Blue Noise 顯示為 Dither Strength，Dot Halftone 顯示為 Dot Density。
- 選擇 None 時，強度控制仍可見但不可調整。
- None 不改變圖片。
- Dither 使用目前有效 Palette 作為固定輸出色。
- Palette 與 Dither 仍要留在固定 edit effects order 中。

### Export

使用者可以匯出目前 pipeline 結果為 PNG。

行為要求：

- 匯出必須使用完整輸出尺寸重新計算。
- Export 只在 `edit` 可操作；`source` 與 `prepare` 時必須反灰停用。
- 匯出失敗時要給出可理解的錯誤狀態。
- 大圖運算或 Worker 配置記憶體失敗時，顯示雙語可理解的記憶體訊息並保留目前工作區，讓使用者可重試、關閉其他分頁或選擇較小圖片。合法輸出尺寸上限仍為 4096；不自動縮圖或降低正式輸出品質。瀏覽器直接終止分頁時無法保證由網頁捕捉或恢復。
- Export 不應被當成效果順序的一部分拖曳。
- E-paper Device Mode 以「繪製到電子紙」取代 Export PNG；它重新跑完整 pipeline、依實際尺寸正規化方向、編碼 EPDIMG，並只送一次 gzip upload。
- E-paper Device Mode 在繪製按鈕下另提供同為主按鈕樣式的「下載圖片專案」；Standalone Mode 不顯示這個 action。
- 圖片專案使用 `<原檔名>.dither.png`，外層 PNG 是正式 pipeline 的最終 dither 結果，可直接由一般 PC 圖片檢視器預覽；內嵌原始檔、正規化工作圖與版本化設定供重新匯入還原，但圖片輸入面板不增加獨立原圖檢視操作。
- Browse、dropzone 與其他檔案輸入都走同一個自動 routing 與驗證流程；不能只依 MIME 或副檔名信任內容。

## 圖片呈現區行為

圖片呈現區負責：

- 顯示目前圖片。
- 顯示處理後結果。
- 在 `edit` 支援 Original / Result / Expand 切換；Original 必須顯示 prepare 後、edit effects 前的原圖，Expand 必須顯示 Result 的真實輸出像素。
- 支援 zoom / pan。
- 在 `prepare` 顯示 crop overlay。
- 讓使用者拖曳原圖位置並看到即時位置變化。
- 在 `source` 右下角 preview toolbar 不顯示任何按鈕；已有來源圖片時仍需預留 toolbar 高度，避免切換到 `prepare` 或 `edit` 後圖片重新縮放。
- 在 `prepare` 右下角 preview toolbar 只能顯示 `+`、`-`、OK 三個按鈕。
- 在 `edit` 右下角 preview toolbar 只能顯示 Original、Result、Expand 三個按鈕。
- `edit` 的 Original / Result / Expand 切換必須採用 Theme choice 風格且尺寸一致；`prepare` 的 `+` / `-` buttons 必須是 compact square buttons，OK button 可維持較寬的 primary action 尺寸。

Crop preview 要求：

- crop overlay 代表輸出裁切範圍，必須和 preview canvas 內的 crop frame 對齊。
- prepare 的 crop frame 與 edit 的 preview image 在相同比例下必須維持同一個中央位置與顯示尺寸，且不可貼齊 preview stage 邊界。
- prepare 的 crop canvas 可以延伸到 crop frame 外並覆蓋 preview stage，讓 zoom / pan 時看得到原圖周邊脈絡；這個延伸不可改變 crop frame 本身的尺寸或位置。
- prepare 的 crop preview 即使 canvas 為了顯示周邊脈絡而延伸，也必須讓 overlay 對準 canvas 內的 crop frame，確保 OK 後的正式 crop output 與畫面框選一致。
- 從 prepare 進入 edit 且 result 尚未完成時，畫面應保留上一個可見 preview，不應短暫跳回 source fallback。
- prepare 與 edit 的 preview 對齊必須使用同一個 preview stage content-box，不可讓 border-box 差異造成微小位移。
- 旋轉、zoom、pan 都應作用在原圖 transform。
- 左右/上下反轉也應作用在原圖 transform，並且 preview 與正式輸出結果一致。
- 若使用者已旋轉原圖，點擊左右/上下反轉後，畫面應以目前可見座標鏡射，不應突然變成以未旋轉原圖座標鏡射。
- Fill 設為 Auto 時，ratio、zoom、rotation、pan、flip 調整後都應重新估算填色，且 preview 與 OK 後的正式 crop output 應一致。
- canvas 尺寸變化不應抵消使用者看到的 zoom / pan 效果。
- 桌面與手機版都不可因 `prepare` 中的 Crop 流程造成 preview stage 或整個 editor 高度被撐開。

## 非同步操作的有效性

- 載入本機圖片、demo 或圖片專案時，以最後一次選擇為準。較早開始但較晚完成的成功或失敗不得覆蓋目前工作區或新操作的提示。
- 圖片／專案先建立完整候選工作區；驗證或還原失敗時保留既有 source、settings 與 pipeline。離開頁面後的舊結果不更新畫面。
- 快速調整保留最新待計算 preview；過期工作不提交結果，也不因取消而重新在主執行緒計算。真正 Worker 故障仍可使用 CPU fallback。
- 匯出使用開始時的 settings／pipeline／target snapshot；同時只接受一份本地 heavy job，重複匯出不排入隱藏佇列。既有本地匯出取消不顯示錯誤，圖片專案不新增取消按鈕。
- 本地取消不表示已送出的裝置 upload／draw 已撤回，也不自動重送。
- 舊 session 的 401、session check 或 logout 回應不能作廢新登入；關閉或被取代的登入 dialog 不再提交 token。當前 session 401 仍回到鎖定狀態，transport failure 保留 token。

## 預覽與狀態行為

使用者調整 slider、select、color 或 effects order 時，App 應更新 preview，但可以短暫 debounce，避免每一次輸入都完整重算。
slider 控制的填色與 thumb、Toggle Switch 的啟用狀態必須使用較淡的 control accent 主題色，不使用瀏覽器預設藍色。

Edit Result preview 應在圖片右下角顯示正式 preview 狀態：繪製開始時顯示 Rendering，繪製完成後顯示最近一次正式 preview 完成耗時，並依設定延遲自動隱藏。空狀態、prepare crop preview、Original view，或全域顯示開關關閉時，不顯示此計時 label。此 label 只描述目前繪製在 preview canvas 上、使用者可見的 Result 圖片。計時 label 自動隱藏不可關閉、重置或打斷使用者正在操作的其他 panel form。

Edit Result preview 為了放入 preview stage 而縮小顯示時，應使用正常重採樣呈現 canvas，不使用 pixelated 硬縮放。Dither 結果包含大量單像素點陣，硬縮放會產生 alias / moire，使網頁預覽看起來比實際匯出的 PNG 更髒或顏色偏移。

Edit Expand preview 必須使用和 Result 相同的正式輸出結果，但以真實像素尺寸顯示，不為了畫面尺寸重新 resize 或重新 dither。Expand 初始視角必須對準 Result fit preview 的圖片中心點；當圖片尺寸大於 preview stage 時，使用者必須能透過較寬的水平/垂直捲軸或拖曳 preview stage 查看其他區域。

主要狀態：

- `empty`：尚未有工作圖片。
- `loading-image`：圖片載入中。
- `ready`：可操作。
- `processing-preview`：預覽計算中。
- `preview-ready`：預覽已更新。
- `exporting`：匯出中。
- `exported`：匯出完成。
- `error`：發生錯誤。

頁面切換狀態：

- app start。
- mount page。
- unmount page。
- mount next page。

## 設定與保存行為

MVP 應保存：

- Web Setting theme，重新整理或下次重新打開瀏覽器頁面後仍保留。
- Web Setting language，提供 Auto、繁中、English，重新整理或下次重新打開瀏覽器頁面後仍保留。
- 同一次 SPA session 內，從 Dither Editor 切到 Web Setting / Help / About 再返回時所需的 editor state、工作圖片與目前 preview。

MVP 不要求：

- 重新整理頁面或關閉瀏覽器後還原 Dither Editor 工作圖片。
- 重新整理頁面或關閉瀏覽器後還原 crop / resize / adjust / palette / dither / export settings。
- 跨 browser session 保存 pipeline effects order 或 operation enabled 狀態。
- 完整 undo / redo history。
- 每一次 preview 的歷史版本。

## App 啟動載入

- HTML 顯示後立即在 64px header 下方出現半透明 loading 遮罩、64px spinner、18px 百分比文字與加寬進度條；App 標題必須保持可見，已顯示的 App 內容可透過遮罩辨識，但在啟動完成前不可接受滑鼠、觸控或鍵盤操作。
- 百分比依動態 scripts、預設頁 mount、初始 images settle 與 paint 等已完成階段單調前進，不得倒退，也不宣稱代表精確下載 bytes；成功完成時到達 100%。
- i18n 可用並本地化 loading 文案時，header 的 App 標題與 Menu placeholder 必須同步切換語言，不得等待預設頁 mount 才更新。
- loading 必須持續到所有 page entry scripts 完成、預設 Dither Editor 掛載、初始頁面的 SVG images settled，且掛載內容完成一次畫面繪製。
- 初始 SVG image 單獨載入失敗不應讓 App 永久停在 loading；Demo 圖、Web Worker 與使用者操作後才需要的資源不屬於啟動等待範圍。
- 啟動成功後移除 loading 畫面並一次解除操作鎖定，不設定額外最低顯示時間。
- 啟動 script、stylesheet 或初始化流程失敗時，停止 spinner、隱藏進度條、保持 App 鎖定，並顯示可重新整理頁面的錯誤狀態。

## UI 文字與語言

MVP UI 文字支援 English 與繁體中文，並集中管理。Web Setting 的 Language 選項提供 Auto、繁中、English；Auto 依瀏覽器語言選擇目前支援語系，未匹配時 fallback 到 English。使用者可見文字應保持一致，例如：

- Dither Editor。
- Web Setting。
- Help。
- About。
- New Image。
- Export。
- Original。
- Custom。
- None。

產品行為重點：

- 按鈕、狀態、錯誤訊息、選單與設定 label 應有清楚文字。
- Tool icon 若不易理解，應提供 title 或 aria-label。
- Dither 的 Serpentine label 後方應提供 info tip，簡要說明蛇行掃描的名詞與用途。
- UI 文字不應分散硬寫在各處，避免後續維護困難。

## 驗收與測試重點

MVP 驗收重點：

- 任意寬高圖片都能進入 Dither 流程。
- 改變 effects order 時，輸出會重新計算。
- disabled operation 不會被執行。
- crop 固定比例、拖曳原圖、滾輪 zoom、rotation、左右/上下反轉不會破壞裁切結果。
- resize 輸出尺寸符合設定。
- 透明像素會以白色背景合成。
- 超過最大尺寸的圖片會先縮小再進入編輯流程。
- Web Setting theme / language 可保存並在重新整理後套用。
- Dither Editor 工作圖片與設定在同一次 SPA 頁面切換返回時保留。
- localStorage schemaVersion 不符時會 fallback，不會造成 runtime crash。
- export PNG 可以產生 Blob。

## 歷史里程碑

以下為初期 standalone 開發階段的拆分，不限制現行裝置管理與電子紙功能。

### Milestone 1: Static App Shell

完成無後端、無外部依賴的主頁骨架。

驗收：

- 使用者可直接開啟 `index.html`。
- 看到 app shell、header、page host。
- Dither Editor 頁面可載入。
- Menu 可切換 Web Setting、Help、About。

### Milestone 2: Image Input and Viewport

完成圖片輸入與基本 preview。

驗收：

- 使用者可匯入本機圖片。
- 可選內建 demo。
- New Image 會開啟本機圖片選擇器。
- preview canvas 能顯示圖片。
- 不依賴遠端 URL。

### Milestone 3: Pipeline System

完成固定效果堆疊。

驗收：

- 可看到 effects stack。
- 可啟用、停用 operation。
- effects order 依固定順序執行。
- pipeline error 會停止流程並顯示錯誤。

### Milestone 4: Crop and Resize

完成裁切與縮放。

驗收：

- Crop 固定比例清單可用。
- crop overlay 行為穩定。
- zoom / rotation / pan / flip 不破壞輸出。
- resize 輸出尺寸符合設定。

### Milestone 5: Dither Engine

完成 Dither 與 palette 處理。

驗收：

- 支援 error diffusion。
- 支援 ordered dither。
- 支援 pattern dither。
- 支援 palette。
- 輸出結果可預覽。

### Milestone 6: Export

完成 PNG 匯出。

驗收：

- 使用者可匯出 PNG。
- 匯出使用正式 pipeline。
- 匯出失敗會顯示錯誤。

### Milestone 7: UI Replacement Readiness

確保功能邏輯和 UI 外觀能分離。

驗收：

- Dither editor 功能集中在自己的頁面模組。
- core/color 純運算不依賴 DOM；core/canvas、image IO 是瀏覽器 adapter。
- app shell 不直接持有 canvas 細節。
- UI 文字集中管理。

## 建議不做的事

現行仍應避免：

- 加入遠端 runtime 依賴。
- 增加 standalone 使用時必須啟動的後端服務。
- 要求來源必須先 build 才能使用；選用 release minify／gzip 仍保留。
- 把所有邏輯塞進單一 `main.js`。
- 繞過既有 device API adapter、能力限制或裝置 action admission。
- 把 canvas controller 和 UI 緊耦合。
- 把 DOM control value 當成唯一狀態來源。
- 在 MVP 就導入大型第三方圖片編輯器。

## 歷史起始任務清單

1. 建立 `index.html`、styles 與 app shell。
2. 建立 namespace 與 classic script 載入順序。
3. 建立集中 UI 文字檔。
4. 建立 Dither Editor 設定檔與 registry。
5. 建立 app shell、page router 與 page registry。
6. 建立 Web Setting、Help、About 頁面。
7. 建立 Dither Editor feature manifest、feature registry 與 feature entries。
8. 建立 editor state 與 controller。
9. 建立 storage、image loader、viewport renderer、pipeline runner。
10. 實作 crop、resize、adjust、palette、dither 與 export。
11. 建立本機測試頁與基本測試案例。

## 成功標準

現行產品應符合：

- 使用者可離線打開頁面並完成主要圖片流程。
- 使用者可切換 Web Setting 並保留設定。
- 使用者可匯入圖片、調整效果、拖曳順序、預覽並匯出 PNG。
- UI 能被未來替換，而不重寫核心圖片處理邏輯。
- 新增 Dither algorithm、palette preset 或 effect feature 時，不需要大範圍改動現有流程。
- ESP32 Device Mode 由 capability 決定，standalone 本地編輯與 PNG 匯出保持可用。

## Reviewer Checklist

Reviewer 應確認：

- 來源沒有遠端 runtime 依賴或必要 build／後端需求；正式 HTTP 資產保留 minify／gzip。
- 使用者可直接打開 `index.html`。
- 頁面切換、上一頁、下一頁行為符合預期。
- Crop、Resize、Adjust、Palette、Dither、Export 行為符合本 spec。
- Effects order 改變後會重新產生結果。
- Crop overlay 在桌面與手機版都穩定。
- Pipeline 失敗時會停止並呈現錯誤。
- Web Setting theme / language 與 Dither Editor 頁面切換 session 狀態保存符合預期。
- UI 文字集中管理。
- 裝置 action 與本地匯出遵守各自的能力／連線限制。

## 面板 capability 與 gzip 傳輸

- `/api/epaper` 是 Device Mode 的面板尺寸來源。目前範例為 800×480，1600×1200 等相同六色 packed 格式亦可通過一致性驗證；瀏覽器尺寸上限每邊 4096、width 必須為偶數。
- Discovery 驗證非空 model、six color codes、40-byte header、W×H/2 frame、40+frame logical bytes、gzip upload/storage 與 upload/refresh actions；不符合時禁止繪製，不偷偷使用 raw upload。Snapshot 的 capability 與 target 不可由 caller 修改。
- 產生 EPDIMG 後用本地瀏覽器 `CompressionStream('gzip')` 壓縮，透過既有 resources API 上傳已知大小 Blob；沒有 CompressionStream 時顯示明確錯誤，不送 request、不加入遠端 library。上傳成功即已 queue draw，不追加 refresh。
- Firmware mounting flip 在 draw-time 執行；瀏覽器不依 capability flip flags 改寫 pixels，避免套用兩次。使用者原有 Crop flip 維持自己的編輯語意。
- Device Mode 載入 demo、檔案與圖片專案時，working long-edge limit 為 max(standalone default, panel long edge)，且不超過 resize 安全上限；不把較大面板的新輸入先縮為 standalone default。已在線但 discovery 未完成時，載入先等待 discovery。Help 的 input limit fact 與同一函式同步。已儲存圖片專案的 working image 本身解析度不足時不憑空還原來源細節。
