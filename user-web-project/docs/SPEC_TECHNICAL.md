# Dither Image Editor 技術 Spec

```text
Version: 0.1.0
Status: Draft
Last Updated: 2026-09-12
Split From: SPEC_INDEX.md
```

本文件收斂實作與架構規格，給工程實作、review、測試設計使用。產品目標、使用者行為、畫面行為與驗收節奏請看 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md)。文件入口與閱讀導引請先看 [SPEC_INDEX.md](SPEC_INDEX.md)。

## 設計沿革與現行範圍

最初以 standalone 編輯器起步，目前已包含同源 ESP32 裝置管理、電子紙輸出與可攜式 PNG 專案。下列章節描述現行契約；早期逐日變更由 git history 保留。

- Classic scripts 與本地資產保留 `file://` 直接使用；Python/Make 發佈建置是選用的 HTTP 資產處理流程。
- 圖片與 workspace 不自動寫入 browser storage；切頁使用記憶體狀態，跨 session 由使用者明確匯出／匯入 `.dither.png`。
- 顯示色盤與 EPDIMG 協定色盤分離，校色不改變既有六色 code 或檔案格式。

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

- Standalone 不需要後端服務；Device Mode 使用既有同源 ESP32 REST API。
- 不使用 React、Vue、TypeScript 或其他前端框架。
- 不使用 CDN。
- 不在 runtime 下載外部資源。
- demo 圖片、圖示、字型、樣式都必須保留在專案內；demo 不可由 runtime 程式臨時產生。
- 使用者必須能直接雙擊 `index.html` 使用，不可要求另外執行 `python -m http.server` 或其他本機指令。
- 開發時可用 VS Code Live Server 預覽，但正式使用方式不能依賴 Live Server。
- 原始專案必須不依賴 build step；不可要求 npm install、npm run build 或 bundler 才能使用。可提供選用的 Python/Make 發佈 build，輸出到 ignored `build/` 底下的時間戳子資料夾，只做 server/device 靜態檔案複製、minify 與 gzip-only 輸出，不改變 runtime 載入架構。minify 與 gzip 預設都必須啟用，且必須能用 CLI 參數分別關閉；啟用 gzip 時，輸出資料夾內只保留 gzip 後的 `.gz` 檔，不保留同名未壓縮檔。同一秒內多次 build 不可覆蓋既有輸出。
- Standalone 與 capability 驅動的 E-paper Device Mode 均為現行功能。

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
- `fetch`，用於專案內同源 assets 與裝置 REST API，不載入第三方 API 或遠端圖片
- `HTMLCanvasElement`
- `CanvasRenderingContext2D`
- `ImageData`
- `DragEvent`
- `PointerEvent`
- `localStorage`
- `Web Worker`，HTTP 下用於 diffusion；file 模式或 runtime failure 使用 CPU fallback，取消除外

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
    sourceFile: null,
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
- 可攜式 workspace 只透過使用者明確匯出／匯入的 `.dither.png` 保存，不寫入 browser storage。已不存在或版本不支援的 feature settings 不可讓頁面 crash；目前 schema v1 採拒絕載入並顯示版本／功能不支援，日後加入 migration 後才可轉換。

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
- `palette-utils` 必須集中管理 palette 最近色判斷；預設使用未加權 Euclidean RGB distance，並支援由 Dither settings 指定其他 Color Distance。

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

`color-distance-metrics.js` 宣告使用者可選的距離公式。Dither feature 預設使用 `DEFAULT_COLOR_DISTANCE_ID`，目前為 `euclidean-rgb`。同一個 `colorDistance` 必須傳給 Error Diffusion、Ordered Dither、Pattern Dither，以及 Dither 關閉時 Palette operation 的直接最近色映射；在 `pair-mix` 與 `tri-mix` 下，`colorDistance` 必須用來評估哪一組 palette mix 的混合結果最接近輸入顏色。`euclidean-bt709` 是 RgbQuant-style BT.709 weighted euclidean distance；`euclidean-rgb` 是未加權 RGB squared distance；`manhattan-bt709` 是 BT.709 weighted Manhattan distance；`manhattan-rgb` 是未加權 Manhattan distance。舊 id `euclidean` / `bt709` 應正規化到 `euclidean-bt709`，舊 id `rgb` 應正規化到 `euclidean-rgb`，舊 id `manhattan` 應正規化到 `manhattan-rgb`。

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

可攜式 workspace migration 分兩層：

- 全域 migration 負責 `schemaVersion` 與 state shape。
- feature migration 負責該 feature 自己的 settings。

每份文件同時帶全域 `schemaVersion`、`rendererVersion` 與各 feature 的 `version`。目前 schema／renderer 為 v1，各既有 feature 宣告 version 1；版本由各 feature persistence contract 決定，未知／不支援版本必須拒絕，不猜測欄位語意。舊資料含已停用或不存在的 feature settings 時，預設不套用到 UI，也不得讓 preview/export crash。

## 可攜式圖片設定檔

`src/core/storage/project-file.js` 負責容器、PNG chunk、CRC 與基礎內容驗證；`src/pages/dither-editor/project-workspace.js` 負責共同 manifest、feature set／version、pipeline 與 restore candidate；個別 settings 委派 feature.persistence。匯入必須先完整驗證並算出結果，成功後才一次替換目前 state，避免半套用。

匯出檔名固定為 `<safe-base>.dither.png`，但匯入不得要求檔名維持不變。容器是標準 PNG，IDAT 保存正式 pipeline 的最終結果，讓一般 PC 圖片檢視器直接看到 dither 後畫面；IEND 前加入三個私有 ancillary chunks：

- `diMF`: UTF-8 JSON manifest，保存 format id、schema/renderer/feature versions、來源 metadata、尺寸、pipeline、feature settings 與 render context。
- `diOR`: 使用者載入的原始 PNG/JPEG/WebP bytes，用於匯入後檢視未處理原圖。
- `diWK`: lossless PNG 工作圖，用於確定性地重建目前 pipeline。

所有 Browse、drop 與隱藏 file input 都必須進入 `controller.loadFile()`：一般 PNG/JPEG/WebP 走 image loader；任何帶有 project chunks 的 PNG 都走 project restore，包括 `.dither(1).png` 或其他重新命名檔案。不得只依 `File.type` 或檔名 suffix 判斷。

讀取限制為整檔 64 MiB、原圖 50 MiB、manifest 256 KiB；必須驗證 PNG signature/chunk 邊界/IHDR/IEND/CRC、必要 project chunks 唯一性、JSON 深度與安全 key、format/schema、來源 magic/MIME/byte length、工作圖與外層結果尺寸，以及 pipeline/feature id、版本和 setting 範圍。`.dither.png` 缺資料、未知版本或驗證失敗都拒絕，不回退成一般圖片；有效 project chunks 不因匯入檔名改變而拒絕。

匯出開始時須先建立 state snapshot，再由 snapshot 重跑 pipeline、建立 manifest 與工作圖，避免編碼期間的 UI 變更造成資料不一致。此流程只使用本機 Canvas/Blob/download，不查詢也不呼叫裝置，因此 E-paper target 已確認後，即使裝置暫時離線或在 cooldown 仍可使用。Standalone target 必須隱藏該 action，但仍能自動匯入有效 `.dither.png`。

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
- EPDIMG 先透過 CompressionStream gzip，使用已知大小 Blob 由 `device-api.resources` 以 binary body、`application/octet-stream`、`Content-Encoding: gzip` 上傳；頁面不可直接 fetch，也不可手動設 Content-Length。Compressed Content-Length 由瀏覽器設定，與 logical imageBytes 不同。

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
- `device-api` 在每次 setToken／清除時推進 session epoch；request 擷取 epoch／token identity。僅帶 token 且仍屬當前 epoch 的 401 才發布 unauthorized(epoch)，auth listener 再確認 epoch 才清除／通知。login 401 不觸發全域登出；同 token 再設定也形成新 epoch。
- ensureSession 成功與 logout cleanup 必須仍屬原 epoch；登入 dialog 的 generation／closed guard 阻止舊成功或失敗更新新 dialog／token。不因關閉 dialog 發出全域 logout。
- session 驗證失敗但屬 transport 錯誤時保留 token，待裝置恢復後重新驗證，避免離線時誤登出。
- login dialog 為全站共用 modal；username 由公開 `GET /api/auth` 取得（固定 `admin`、唯讀），成功後原地解鎖目前頁面，不跳頁。Menu 底部在持有 token 時顯示登出。
- 修改管理員密碼成功、AP 密碼保護開關變更（裝置重啟）時，前端主動 `invalidateSession()`。

### 裝置管理頁

- `pages/device-info/`：公開唯讀。`GET /api/device` + `GET /api/storage`，掛載時抓一次並每 10 秒背景輪詢；裝置卡直接呈現 API 的 configured `wifi_tx_dbm` 並加上 dBm 單位，不把它解讀為實測功率。緊接裝置卡的電源卡直接呈現 `power.voltage_mv`、`power.estimated_percent` 與 `power.sample_age_ms`；null 或非 numeric 值顯示 unavailable，前端不推導 power source、battery presence 或 charging state。不提供手動 Refresh、不顯示 stale badge，也不呈現 `config_state`；容量分段長條圖推導與 bedrock builtin-web Hardware 頁一致，Available 直接使用 `user.limits.max_upload_bytes`。失敗沿用最後成功值，離線提示交給 `device-gate` banner。
- `pages/device-network/`：狀態卡公開（`GET /api/wifi` 每 10 秒輪詢，`if-clean` 不覆蓋表單草稿）；Wi-Fi 設定需登入。`PUT /api/wifi` 是完整 replacement，不適用分類由已載入 baseline 帶入；202 safe transition 以 1s 間隔輪詢 `GET /api/wifi/connect`、25s deadline、generation 序號丟棄過期回覆，期間 `suppress` 離線判定並容忍 transport 失敗；terminal `connected` 才把送出值設為 baseline 並清 password input，`failed` 保留草稿並顯示 rollback。掃描 dialog 過濾 hidden／空 SSID／RSSI ≤ -75、依 RSSI 排序、10 秒 cooldown 倒數，429/409 依 `retry_after_seconds` 提示。
- `pages/device-system/`：整頁需登入。hostname 走 `PUT /api/system`（前端套用相同 1–31 字元規則與 mDNS 預覽）；管理員密碼走 `PUT /api/auth/password`（allowlist 正則 + 兩次一致），成功後清 token 要求重新登入。完整重設走 `POST /api/system/reset`（API client 只保留這一支；settings/data 兩支未使用已移除），UI 必須先通過確認 dialog，送出期間 `setDismissible(false)` 並鎖住按鈕，成功後 `invalidateSession()` 並以 sticky notice 保留重新連線指示。
- 表單皆採 `busy || !dirty || !valid` 三態儲存鈕與文字狀態列；一般成功 notice 約 2.2 秒自動消失，錯誤與需保留脈絡（重啟、斷線、rollback）的 notice 常駐。
- 視覺語言與 Dither Editor 對齊：卡片一律使用 components.css 的 `panel-section`（h2 標題列）＋`panel-body device-card-body`；需要右側 badge／總空間的卡片改用 `device-card-header` 標題列變體。頁面不放 `h1`（頁名由 app header 顯示）。`device.css` 只保存裝置專屬樣式，顏色沿用 theme token，尺寸沿用編輯器的 34px 輸入框與 11–14px 字級階。

### E-paper Device Mode

- `device-live` 仍只擁有 connection truth；`device-epaper` 在 online 後 probe `GET /api/epaper`，透過 `core/encoders/epaper-target.js` 驗證 model、safe positive dimensions、even width、EPDIMG、40-byte header、由尺寸推導的 frame/image bytes、gzip 與必要 capabilities。正規化 frozen target 供 policy／encoder／upload 共用，公開 capability 亦遞迴 freeze。
- Target state 與 connection state 分離。Capability 一旦在 session 內確認，offline 不清除 target，只禁止 operation；reconnect 後重抓 capability/status；明確 unsupported capability 會撤銷舊 upload target，transport failure 才保留先前 target。
- E-paper editor action 區第一顆按鈕為「繪製到電子紙」，使用正規化為 24×24/currentColor/aria-hidden 的 `credit-card-edit-svgrepo-com.svg`；第二顆為相同 primary 樣式的純本機「下載圖片專案」，沿用 `export-download.svg`。後者不綁 `can_draw`、cooldown 或 online，但仍須有合法 edit state；執行時維持固定文案並暫時 disabled，不轉為 Cancel。
- Target policy 從同一 normalized profile 取得 W/H 與 gcd ratio id；originalSize 高大於寬預選 H:W，否則 W:H。Crop registry 重用既有比例或登記 deviceOnly ratio，standalone list 排除此類新項目，square 只一項。合法 user ratio 不被背景 sync 覆蓋，resize、readonly UI、pipeline normalization 共用 state.target.profile。
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

下列 config 是明確的 standalone editor default 範例，不能用來判定實體装置相容性。Device Mode 使用 capability normalized target。

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
- E-paper Crop ratio allowlist 為 capability normalized profile 的 landscapeRatioId 與 portraitRatioId；正方形只一項。
- Landscape output 為 capability W×H；portrait output 為 H×W。Resize UI 為 read-only。
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
- Operation 完成進入 cooldown 後解除全域 lock，但所有 e-paper action 依 server admission disabled；local countdown 每秒更新，只負責顯示。Cooldown 到期與 marker cleanup 恢復共用既有 5 秒 status polling，不新增 timer-driven request；同時請求共用 in-flight promise。重連錯過 cooldown 時，成功 idle 亦須結束已接受操作。

### EPDIMG Output Encoding

目前裝置 contract 固定為 EPDIMG，不再保留未決 encoder format。Encoder 位於 pure core：

保留 encoder 位置：

```text
src/core/encoders/
  epdimg-encoder.js
```

規則：

- Formal pipeline output 只接受 target W×H 或 H×W。Encoder 必須直接檢查 ImageData dimensions，不依賴 UI orientation flag。
- W×H 原樣編碼；H×W 在六色 pipeline 完成後逐 pixel 順時針旋轉 90°，不重新 resize或 dither；其他尺寸在 API 前失敗。
- Normalized W×H pixel 必須精確匹配 E6 reference RGB，mapping 固定為 black=0、white=1、yellow=2、red=3、blue=5、green=6；不可用 palette array index。
- 每兩 pixel 打包一 byte（left high nibble、right low nibble），frame CRC32 後寫 40-byte little-endian header與 non-zero uint64 generation，logical 總長為 40 + W×H/2 bytes（目前 profile example 192,040）。
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

Crop 幾何／背景／zoom 公開能力只由 `featureRegistry.api('crop')` 取得；沒有註冊 Crop 時 preview overlay、pointer 與 toolbar 不進入 Crop 分支，mode state machine 直接進入 edit。Resize、Palette 的裝置模式鎖定由 feature `targetLocked` metadata 與 `target-policy.featureLocked()` 共用同一份 policy。Action feature 可以沒有 operation；宣告 pipeline stage 且不是 action 的 feature 必須提供有效 `operation.run()`，pipeline runner 依 operation registry 能力略過 action，不依賴 `export` 等固定 ID。Core image loader 的 demo script 載入函式由 controller 明確傳入，維持 classic script 與 `file://` fallback。

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
- Cache owner 預設限制 32 MiB retained pixel buffers 與 32 entries，以 LRU 淘汰至兩者都成立。以實際 backing ArrayBuffer identity／byteLength 計數，partial view 計整個 buffer，alias 使用引用數不重複計算。零 budget 關閉 cache；超大單筆不入 cache但仍回傳結果，不清空其他可用小 entry。
- clear 同時清 entries、buffer references 與 retainedBytes，並遞增 generation；舊 async 結果不得回填。同步／async runner 共用 accounting。此限制不包含 source、Canvas、Worker 複本、暫存陣列或 GPU texture，不能解讀為整頁 RAM 上限。
- 預設 cache key 只包含 operation 自己的 settings；若 operation 讀取其他 feature state 或 pipeline enabled 狀態，該 operation 必須提供 `cacheKey()`。例如 `Palette` 會讀取 Dither 啟用狀態與 Dither settings，因此必須把這些值納入額外 cache key。
- Operation 不可修改 upstream `ImageData`；cache 會重用 operation 回傳的 `ImageData`。若 operation 為 no-op 並回傳原物件，下游 stage identity 應保持與輸入相同，讓後續 stage 可重用。
- Export 不使用 preview Stage Cache，也不直接使用暫存 preview bitmap；它仍從工作圖執行完整正式 pipeline，確保輸出與最新 settings 一致。

Error diffusion 的 Floyd–Steinberg、各 matrix kernel 與 adaptive FS 使用最多 `max(matrix.dy)+1` 列 Float32 RGBA 工作緩衝與一份完整 Uint8Clamped RGBA output；adaptive FS 另保留 Float32 integral image 與 local mean map。Dot diffusion 按 class 跨全圖遍歷，使用完整 Float32 RGB 工作緩衝（不保存無需擴散的 alpha）與一份完整 RGBA output。以 4096² 計，單純掃描式工作緩衝從約 256 MiB 降為 kernel 列數乘 4096×16 bytes，output 約 64 MiB；dot RGB 約 192 MiB 加 output 64 MiB。這些是配置模型，不含 source、Worker 傳遞、canvas、cache、export 與 browser overhead，不能當作實測 peak 或低記憶體裝置的保證。Worker 傳遞仍複製一次 input 以維持 workspace owner；RangeError allocation failure 傳 `image_memory_exhausted`，Controller 保留 state 並顯示雙語訊息。直接 OOM 殺掉分頁不在 try/catch 可恢復範圍內。

## Editor job 與 source ownership

`controller.js` 的窄 job owner 管理 generation、active job、最新 pending preview 與 dispose。最多一份 active computation 和一份可被取代的 pending preview；heavy export/import 不接受重複 admission。開始 heavy job 時作廢 preview並在必要時終止自己的 Worker。匯出 snapshot 深拷貝可變 settings／pipeline／target，EPDIMG 邊界也使用同次擷取的校色色盤，保留 immutable ImageData／Blob reference；不對整個 state 做 JSON round-trip。

每個 stage 前、async 結果後、fallback 前與 state／DOM commit 前檢查 job。取消使用 `job_cancelled`，reason 為 superseded／disposed／explicit；Worker failure 使用獨立 code，只有有效 job 能 fallback。同步 CPU loop 無法被外部事件搶占，此契約保證取消後不再開始下一份重算。destroy 作廢 job／load generation、丟掉 pending、終止自己的 client 並清 cache。

Worker client factory 提供 mount-local ownership；request id 與 worker epoch 排除舊事件。Admission 在複製 buffer 前，最多一份 in-flight；成功、失敗、postMessage throw、messageerror 與 terminate 均只 settle 一次並移除 pending。transfer 使用 pixels 複本，不能 detach cache／畫面資料。工具的 classic-script adapter 保留。

本機圖、demo、project route 共用 loadGeneration；載入前取消舊 local job，decode／restore／prepare 在候選資料完成，僅當前 generation 可提交。Decoder 自己關閉 bitmap／撤銷 object URL，不讀取 controller global state。已送到裝置的 action 仍由 device-epaper 管理，本地取消不撤回、不重送。

## Core color identity 與 feature persistence

`core/color/palette-utils.js` 擁有五種 colorDistanceIds、預設 euclidean-rgb 與 normalization，不反查 pages。Aliases 維持 bt709／euclidean → euclidean-bt709、rgb → euclidean-rgb、manhattan → manhattan-rgb。UI config 保留 label／顯示順序並校驗與 core 一致；主執行緒、Worker 與工具共用同一份核心公式與 ID。

有 defaultSettings 的 feature 必須註冊完整 `persistence: {version, serializeSettings(settings, context), restoreSettings(raw, version, context)}`；沒有 settings 的 action 可不提供。serialize 產生獨立 plain data，restore 驗證 type／enum／finite number／array／範圍後回傳新物件，不能讀 DOM、發 request、修改 live state 或計算圖片。

Workspace 管理 feature set／版本、pipeline stage／member／enabled 與候選 state，沒有 feature ID schema switch。委派前保留 depth 12、字串 4096、array/object 64 項與禁止 __proto__／prototype／constructor 等一般防護。新增 feature 只需提供契約；既有 schema、renderer、feature version 與 diMF／diOR／diWK chunks 不變。

## Correctness 與產物驗證入口

Browser regression runner 以獨立 HTML/profile 執行 legacy、Crop disabled/removed、Wi-Fi、e-paper、history 與共用樣式案例，各自回報 passed／failed 與已完成 assertion；單例失敗繼續執行後續案例，任一失敗或逾時仍使 runner 非零退出。主 en／zh-TW 字典的頂層鍵集合必須一致；缺鍵回報語言與 key。

- `make test`：build/helper Python tests、雙語 Help、PNG container／來源 startup、core／persistence／lifecycle regression、file demo／本地 PNG。
- `make test-production`：在 ignored tmp/verification 下建立新 production gzip 與 demo，臨時 loopback HTTP 驗證 startup、真實 Worker／CPU pixels、PNG、relative API、tooltip／Help styles 與 mock boundary；不部署，也不操作硬體。
- `tools/shared/browser.py` 共用 browser discovery、WSL path conversion、獨立 profile、DOM completion、bounded execution 與 cleanup。Native 使用標準庫 DevTools transport；WSL 的 Windows Chrome 使用內建 PowerShell／.NET bridge。一般 suite 預設 90 秒，可用 `--timeout` 調整；benchmark/render 預設 180 秒。錯誤／逾時未完成必須非零退出，不自動 retry。
- Browser 可用 `--chrome`、共用 `DITHER_BROWSER` 或既有 DITHER_RENDER_BROWSER／DITHER_BENCHMARK_BROWSER 指定。來源／純模組工具不用 server；只有 production integration 自動啟停 loopback server。
- CSS minifier 保留必要空白、字串、escape 與註解 token 邊界；minify／gzip 預設維持啟用。Tests／profiles／log 不進入 release copier。
- Benchmark 與手機／實板驗收獨立於一般 correctness；執行紀錄與未驗項目放 ignored tmp/verification，不放 SPEC。

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

E-paper operation overlay 位於 `src/app/`，由 app shell 建立並訂閱 e-paper service；銷毀時解除訂閱與移除 DOM。一般 modal 規則屬 `assets/styles/components.css`（含窄螢幕覆寫），裝置專屬 dialog 與 scan 規則仍屬 `device.css`。Light/dark 共用 `themes.css` 的 `--accent-icon-filter`，元件只引用 token。

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
- 效能改善以實際支援尺寸與最慢演算法量測為依據；現有 Worker／GPU 不保證同步 CPU loop 可被中途搶占。
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

Standalone 最大輸入長邊預設：

```js
const MAX_INPUT_LONG_EDGE = 800;
```

規則：

- 使用者丟入圖片後，先檢查寬高。
- Standalone 超過 `MAX_INPUT_LONG_EDGE` 時按比例縮小。Device Mode 使用 `constants.inputLongEdge()` = min(MAX_RESIZE_OUTPUT_SIZE, max(MAX_INPUT_LONG_EDGE, panel.width, panel.height))，涵蓋 demo/file/project import；Help maxInputLongEdge fact 同步此值。
- 編輯器後續使用縮小後的圖片作為工作圖。
- UI 需提示使用者圖片已被縮小，顯示原始尺寸與工作尺寸。
- 輸入縮小、cache retained-byte budget 與有界 job admission 共同控制資源。Diffusion 在 HTTP 可用時使用 controller-owned Worker。
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
    colorDistance: 'euclidean-rgb',
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

Browser storage 保存 app shell preferences 與裝置 token；Dither Editor 圖片／settings 不自動跨重新整理保存，可由使用者明確匯出／匯入 PNG 專案。

這一節處理 browser storage 邊界；Menu 切頁後回到 Dither Editor 的短期保留，應由 Dither Editor page module 的 in-memory state cache 處理，不依賴 localStorage 或 IndexedDB。

儲存方式：

- `localStorage`：保存 Web Setting／app shell theme、language，以及由 storage-keys 管理的裝置 token。
- `cookie`：現階段不使用；裝置登入使用 Bearer token。
- `IndexedDB`：不使用；目前可攜式 workspace restore 透過 `.dither.png`。

現階段決策：
- Web Setting 與 app shell preference 只使用 `localStorage`，不使用 cookie。
- Dither Editor 的圖片、workspace、canvas、pipeline settings 與 feature settings 不寫入 localStorage、IndexedDB 或 cookie。
- 同一次 SPA session 的 Dither Editor 狀態保留只靠 `pages/dither-editor/page.js` 的 module-level in-memory `cachedState`。
- cookie 不作為設定 fallback，避免同一份設定有兩個來源造成維護混亂。
- 裝置 session 與 app shell preferences 分開管理，不把圖片資料存入 token／preference storage。

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

## Gzip 與尺寸參數化的實作邊界

- `epaper-target.js` 的 geometry helper 驗證每邊最多 4096、even packed width，建立同一組 frameBytes、imageBytes、ratio ids；fromCapabilities 再驗證 server identity、palette、actions、encoding 與 size 一致性。`display-profiles.js` 與 DEFAULT_NEW_IMAGE_SIZE 只是 standalone defaults；mock 尺寸只在 MOCK_PANEL 定義一次，derived sizes 不作 production truth。
- `epdimgEncoder.encode(imageData, target)` 顯式接收該 target；portrait source index 為 `(y, W-1-x)`，沒有 production 尺寸 magic numbers；square 視為 landscape。Controller 在 operation 開始取得 target 與 palette snapshot，同次 encoder／submitUpload 共用。
- `submitUpload` 驗證 target logical size，gzip 成 Blob，再檢查 job/run generation 與 compressed limit 後送 resources API；compression 失敗、取消或 stale run 不送 request。CompressionStream 缺少回 `gzip_unavailable`。不提供 raw fallback 或 remote runtime dependency。
- EPDIMG version／header layout／CRC／generation／palette codes 不變；gzip-only upload 是 transport breaking change。Metadata 同時提供 logical size 與 stored compressed size，mock 與 firmware 同步。下載仍是 raw logical EPDIMG，這與 gzip upload body 不同。
