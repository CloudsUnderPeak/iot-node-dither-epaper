# Frontend Technical Specification

本文件定義 `builtin-web/` 內建裝置控制台、可替換的 `user-web/` 與 frontend build 的工程限制。產品體驗以 [SPEC_FRONTEND_BEHAVIOR.md](SPEC_FRONTEND_BEHAVIOR.md) 為準，REST contract 以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準。

## Source of truth 與產物邊界

- 本專案內建前端的功能、結構、樣式、文案與修正一律實作在 `builtin-web/`；它是內建 console 的唯一 source of truth。
- `user-web/` 接受使用者直接提供、可部署的完整靜態輸出；專案不替它執行 npm、Vite、React 或其他 framework build。資料夾由 tracked `.gitignore` 保留，內容預設不納入 Git。
- `build/latest/web/` 與各時間戳快照內的 `web/` 都是 generated output，不是實作位置，也不得手動修改。每次只建立 web 或 demo 時會用 web-only 結構替換 `build/latest/`，因此既有 `binary/` 與 `firmware.img` 必須一併失效。
- 處理內建前端需求時，必須先在 `builtin-web/` 完成並確認 source change；產生 `build/`、檢查產物及執行 build verification 是其後獨立階段。`build/` 仍是舊內容只代表尚未重建，source preview 以 `builtin-web/index.html` 及其 `assets/` 為準。

## 檔案結構

```text
builtin-web/
├── index.html
└── assets/
    ├── favicon.ico
    ├── css/
    │   ├── tokens.css
    │   ├── app.css
    │   └── responsive.css
    └── js/
        ├── namespace.js
        ├── main.js
        ├── app/
        │   ├── router.js
        │   └── shell.js
        ├── core/
        │   ├── api/
        │   │   ├── client.js
        │   │   └── resources.js
        │   ├── dom.js
        │   ├── resource-store.js
        │   └── state.js
        ├── i18n/
        │   ├── en.js
        │   ├── runtime.js
        │   └── zh-Hant.js
        ├── pages/
        │   ├── hardware.js
        │   ├── network.js
        │   └── settings.js
        ├── modules/
        │   ├── auth.js
        │   ├── system.js
        │   └── wifi/
        │       ├── controller.js
        │       └── view.js
        ├── ui/
        │   ├── dialog.js
        │   └── icons.js
        └── preview/
            ├── config.js
            ├── mockApi.js
            └── styles.css
```

實際新增或刪除檔案時同步更新本節；不要為了維持圖表而保留無用途檔案。

## Layer responsibility

- `assets/favicon.ico`：內建網頁的本地多尺寸 favicon；不得依賴外部資源。
- `index.html`：最小 document bootstrap，只包含 `#app`、local stylesheet 與有順序的 classic script entries；不得包含 shell、page、dialog、SVG symbol、application logic 或 inline style/script。它本身必須可由 `file://` 直接啟動，不依賴 build-time include 或 module server。
- `css/tokens.css`：只保存跨元件共用的 color、radius、shadow 與 width token；目前只保留實際使用的 light theme 值。單一元件專用色彩不放入 `:root`，Hardware 容量分區 palette 直接由 `app.css` 的 `.capacity-color-1`～`.capacity-color-5` 規則管理；類名只表達色池位置，不綁定系統、韌體或檔案等資料語意。
- `css/app.css`：依閱讀順序保存 document defaults、shell、共用元件與 page-specific presentation。這些規則共同描述同一個小型控制台，維持單檔比為每個短區段建立 partial 更容易追蹤 cascade。
- `css/responsive.css`：只保存 viewport 造成的 layout override；base rule 留在 `app.css`，不依 page 或 breakpoint 再拆小檔。
- `namespace.js`：只建立 app composition 需要的 `window.DeviceConsole.app`、`pages`、`ui` namespace；core helper 維持直接 classic-script dependency，不為尚未使用的公開介面建立 wrapper。
- `main.js`：composition root；依序 mount icon、shell、dialog，綁定 router/auth，啟動 route 與初始 resource refresh，是正式 application script 的唯一入口。
- `app/shell.js`：擁有 sidebar、topbar、notice、mobile navigation 與唯一 `pageHost`；只處理全域 chrome，不含 Network、Hardware 或 Settings 內部 markup。
- `app/router.js`：從 `DeviceConsole.pages` 取得 page 公開介面，解析 hash、驗證 Settings session、切換 page lifecycle、同步 history/menu/heading；不得渲染頁面內部欄位。固定三頁不另建 registry abstraction。
- `core/state.js`：跨 view runtime state，以及 language/token 的 localStorage access。
- `core/api/client.js`：REST envelope、JSON request、Bearer token 與 transport error normalization；共用 background error reporter 只記錄 operation、code 與 status，不記錄 request body、token、password 或 server message。
- `core/api/resources.js`：依 device、storage、Wi-Fi、auth 與 system resource 集中 REST path、method 與 request shape。
- `core/resource-store.js`：管理 resource reader、request version 與 server snapshot commit；module 不得反向依賴 `main.js` 取得這些能力。
- `core/dom.js`：小型 DOM factory、notice 與 format helper；以 `createElement`、`textContent`、attribute 與 child node 組裝介面，不保存 HTML markup 字串。
- `i18n/runtime.js`：翻譯查找、插值與 DOM 套用；`en.js`、`zh-Hant.js` 各自保存固定與動態文案，不得合併 locale。
- `pages/network.js`、`pages/hardware.js`：各自以 DOM factory 擁有 page structure、read-only state rendering 與 mount/unmount 介面，不直接持有 REST path；Hardware 依 storage resource 的 fixed regions、flash partitions、app／frontend sizes、app available、32 KiB `user_nvs`、coredump、filesystem reserve 與 user upload quota組成兩條共用 palette 的分段長條圖。Image space 的系統使用空間不計 `user.limits.reserved_bytes`；File storage 將它顯示為檔案緩衝空間，而可用空間直接使用 `user.limits.max_upload_bytes`。圖例的色塊與 i18n 文字必須使用分離子節點，避免 `applyLanguage()` 的 `textContent` 更新刪除色塊；segment tooltip 與 ARIA label 在 render 時依目前語言重建。Flash partitions 只參與容量計算，不另外渲染 Partition layout 卡片。
- `pages/settings.js`：擁有 Settings tabs 與 Wi-Fi/Admin/System 的 page markup，並在 mount 時組裝對應 feature controller。三個子頁共用同一個授權與 lifecycle，現階段不再拆成只包一小段 markup 的 entry 檔。
- `ui/icons.js`：以 DOM descriptor 保存全站共用的本地 SVG symbol，啟動時掛到 document；轉換檔案位置時必須保留既有 symbol 的 viewBox、子節點順序、path data 與 fill/stroke attribute，不得順便簡化或重畫。SVG path data 可不受一般 JavaScript 行寬限制。
- `ui/dialog.js`：只建立共用 dialog host/backdrop 並管理 add/open/close lifecycle，不包含產品能力的 dialog structure 或事件。Open 時保存觸發控制、隔離背景並限制 Tab focus，close 時解除 inert 並還原焦點；Escape/backdrop 的產品路由決策仍由 router 負責。
- `modules/`：只保存有實際互動流程的產品能力；簡單能力維持單一 `modules/<name>.js`，不為一個檔案額外建立同名子目錄。
- `modules/auth.js`：session verify、login/logout、password change，並擁有 login dialog structure。
- `modules/system.js`：hostname update 與完整 factory-reset confirmation/request，並擁有 reset dialog structure；完整 reset 呼叫 `/api/system/reset`，文案必須包含使用者檔案也會清除。Reset request 期間鎖定確認按鈕，成功後清除 token、切回 Network 並保留 restart notice。
- `modules/wifi/view.js`：mode-aware Wi-Fi form state、scan dialog structure 與 result rendering；`controller.js` 負責完整設定儲存、status retry、scan/cooldown 與 SSID 選擇。
- `modules/wifi/controller.js` 透過 `PUT /api/wifi` 送出 AP／STA／AP + STA 的完整 replacement。一般 `200` 流程輪詢 `/api/wifi`；若 submitted AP password enabled 相對載入 snapshot 改變，則在成功 response 後清除 token、切回 Network 並顯示 restart notice，不再啟動一般 status retry。Response data 為 `state: connecting` 的 `202` safe transition 改輪詢 `GET /api/wifi/connect`，但不處理 firmware 內部 test id 或 commit。Scan 將 radio busy 轉為既有 connecting 文案與短暫重試 feedback。
- `preview/config.js`：source page 與 generated demo 共用的 runtime flag；預設啟用 mock，並允許 `?mock=1`／`?mock=0` 覆寫。
- `preview/mockApi.js`：可直接執行的 preview adapter；只有 runtime flag 為 `true` 時啟用，不再透過 build-time partial 組合；storage mock 必須提供 flash、app 與 user，且 app／partition 數字及 user capacity／limits 必須符合正式 API invariant。
- `preview/styles.css`：preview topbar badge 與獨立 dialog 專用樣式；不得以 JavaScript template string 注入 CSS。

新增頂層頁面時，由單一 `pages/<name>.js` 擁有 markup 與 page interface，再公開到 `DeviceConsole.pages`。只有頁面內已實際出現可獨立維護的互動流程時才建立 `modules/<name>.js`，同一能力確實出現多個 view/controller 責任時才展開子目錄；不得為轉傳函式、單段 markup 或空角色建立檔案。REST path、auth header 與 envelope handling 不得散落在 page 或 module view。

## Classic-script 載入與 namespace 邊界

- `builtin-web/index.html` 的 classic-script 順序是目前的 runtime contract；`namespace.js` 必須先建立 `window.DeviceConsole`，`main.js` 必須最後啟動 application。`make test-web` 直接讀取這份順序，並驗證 bootstrap 後的 page、router、dialog 與 DOM utility 公開介面。
- 明確的 composition interface 放在 `DeviceConsole`：`app.shell`、`app.router`、`pages.network`、`pages.hardware`、`pages.settings`、`ui.icons`、`ui.dialog` 與 `utils.dom`。跨 page 共用且語意相同的 DOM factory（例如 `detailRow`）也放在 `utils.dom`，不得在 owner 內各存一份。
- 現有 core direct globals 是載入順序的一部分：state 層的 `state`／`setToken`／`setLanguage`、i18n 的 `t`／`tf`／`applyLanguage`、API/resource 層的 `api`／`reportBackgroundError`／`resources`／resource snapshot functions，以及 DOM 的 `$`／`$$`／notice／format helpers。
- Auth、System 與 Wi-Fi feature 目前也有由 Settings、router 或 composition root 呼叫的 classic-script globals；其餘同檔 helper 雖未掛在 `window` property，仍位於 shared global lexical environment。新增 file-private 能力應優先放入 owner IIFE，新增跨檔介面應掛到對應 `DeviceConsole` owner，不得再擴張未命名的全域契約。
- 目前各檔 top-level lexical/function 名稱沒有重複宣告；原先兩份 `detailRow` 都在 owner IIFE 內，沒有 runtime collision，但已有實作漂移風險，因此合併。Network 的 runtime mode label 必須保留 `off`，Wi-Fi 設定表單只接受 `ap`／`sta`／`ap_sta`，兩者語意不同，不合併成同一 helper。
- 本階段不全面轉換為 IIFE／namespace。全面轉換會同時改變上述跨檔 binding 與載入契約，必須先在本文件重新定義 public/private interface、遷移順序與相容範圍，並以 actual-index browser tests 保護後才可實作；不得只為消除 global 數量做機械包裝。
- Application event binding 統一使用 `addEventListener`；HTML inline handler 與 `onclick`／`oninput` property assignment 不作為 source pattern。

## Navigation 與 state

- 正式 routes 為 `/#/network`、`/#/hardware`、`/#/settings/wifi`、`/#/settings/admin`、`/#/settings/system`。
- Hash 只由 browser 處理，不增加 firmware route。
- History back/forward 重新套用公開頁，或在進入 Settings 前驗證 session。
- Language 與 token 透過 state layer 存取；module 不直接散落 localStorage key。
- 登出、invalid session 或 password change 後，由 auth/state layer 清除 token。
- Server snapshot 與尚未儲存的 form draft 分開處理；背景 refresh 或 status polling 不得覆寫 dirty form。
- Wi-Fi form draft 包含 mode、fallback、STA/AP 與適用的 IPv4 設定；dirty／valid 只受目前 mode 適用且啟用的欄位約束，payload 仍保留完整 replacement 所需分類。
- 初始載入可並行讀取公開資料；單一 resource 失敗不得阻止其他成功 snapshot render。
- 手動 Refresh 只讀取目前頁面所需 resource：Network 為 device/Wi-Fi、Hardware 為 device/storage，各 Settings 子頁只讀取自己的 resource。
- 一般 Wi-Fi 設定成功後以有限次 status retry 讀取 `/api/wifi`，直到設定 mode 的 STA/AP 進入 terminal state 或用盡次數。Safe transition 在 25 秒期限內輪詢 `/api/wifi/connect`，terminal connected 才更新 baseline，terminal failed 保留 draft；背景結果不得覆寫使用者後續 draft。

## API client 邊界

- 前端只依賴 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md)，不引用 firmware class 或內部 JSON helper。
- 所有 protected request 由 API client 統一附加 Bearer token。
- Envelope、HTTP error 與 JSON parse error 集中處理；view 不重複判斷 transport 細節。
- 使用者輸入及 API 動態字串只可透過 `textContent`、DOM property 或 attribute API 寫入，不得拼接成 HTML markup。
- Password、token 不寫入 console、notice 或 DOM debug attribute。
- Login、STA credential 與管理員修改密碼 form 均以 `autocomplete="off"` 表示不需瀏覽器保存或填入；不得用 `current-password`／`new-password` 主動註冊為網站 credential。瀏覽器可依使用者設定覆寫此提示，前端不得以 `type="text"` 或非標準遮罩偽裝 password input。

## Architecture guardrails

- `index.html` 只保存 `#app` 與 asset entries；shell、page、dialog、local SVG symbol 必須由其 owner 在 bootstrap 時 mount。Build guard 必須拒絕把 `appView`、`page-network` 或 `loginDialog` 回填到 source HTML。
- `index.html` 不得加入 build-time include、inline `<style>`、無 `src` 的 `<script>`、`style` attribute 或 `onclick` 等 inline event handler；直接雙擊時必須由 source modules 顯示可操作的 preview。
- App shell/router 只能透過 page 的 mount/render/route/unmount 介面互動，不得 query 或修改某一頁的內部欄位。
- Page 只在 `pageHost` 內建立自己的持續內容；feature module 擁有對應 dialog structure，`ui/dialog.js` 只管共用 host/backdrop lifecycle，icon sprite 由 `ui/icons.js` 擁有。動態 mount 後必須立即套用 i18n，unmount 後的非同步流程不得再操作已移除的 DOM。
- Application structure 使用 `core/dom.js` 的小型 DOM factory 與 owner-local factory function 表達；不得以 `innerHTML`、`outerHTML`、`insertAdjacentHTML` 或大型 template string 保存 markup。不要為此引入 template registry、virtual DOM 或 build-time template layer。
- CSS source 只保留 `tokens.css`、`app.css`、`responsive.css` 與 preview 專用 `preview/styles.css`；不要為短 selector 群建立 partial 或 build-time CSS include layer。只有新增一份具有獨立載入生命週期的樣式時才增加 stylesheet。
- CSS source 每個 declaration 獨立成行，不得以 release minified 格式維護，也不得使用 runtime `@import`。
- `preview/mockApi.js` 保持單一 closure 與直接可執行來源；不得加入 build-time JavaScript include layer。
- `modules/` 不得直接使用 `fetch`、`localStorage` 或硬編碼 `/api/` path；網路傳輸、持久化狀態與 REST resource 必須分別留在 `core/api/client.js`、`core/state.js` 與 `core/api/resources.js`。
- `main.js` 是 composition root，只組裝 mount、refresh、render、event binding 與 bootstrap；共享 resource snapshot lifecycle 屬於 `core/resource-store.js`，module 不得依賴入口層實作。
- `tools/web-build/build_web.py` 在建立 web／demo 前檢查上述邊界；違反時 build 必須失敗，不能只靠 code review 記憶規則。

## Firmware serving

- `ApiServer` 從 app image 的 generated asset table 提供 `/`、`/index.html` 與 `/assets/...`。合法的無前端狀態不建立 asset；`/` 與 `/index.html` 回 404 `not_found`／`frontend not bundled`，其他靜態路徑維持一般 404。
- `WEB=auto|builtin|user|none` 選擇來源；`auto` 在 `user-web/index.html` 或 `index.html.gz` 存在時選 user，空的 `user-web/` 選 builtin，若已有其他檔案卻沒有有效 index 則失敗。`auto` 不會選 none；只有明確 `WEB=none` 才建立 API-only firmware。Raw 與 `.gz` 對應到同一 logical path 時必須拒絕。
- `WEB_PROCESS=auto|minify-gzip|none` 控制處理；`auto` 對 builtin 使用 `minify-gzip`，對 user／none 使用 `none`。Builtin 與 user 都可明確覆寫；none 只接受 `auto`／`none`。Precompressed user input 只可用 `none`，指定 `minify-gzip` 時必須提供可安全處理的 raw HTML/CSS/JavaScript。
- `tools/web-build/` 是唯一 frontend output tool。Production builtin 不論 processing mode 都移除 PREVIEW block 與 preview directory；demo 只接受 builtin 並保留 preview；none 只接受 production，並發布空的 `build/latest/web/`。完成驗證後以 private staging 原子替換 `build/latest/web/` 與 `web-manifest.json`。
- `tools/release-build/` 只消費 `build/latest/` 內既有、已驗證且 target 為 production 的 web。Builtin／user 產生 generated C++ asset table並編入 `firmware.bin`；none 產生 count／payload 為零的安全 sentinel 及 null runtime hash。Demo 不得成為 embedded input。
- `ApiServer` 依 logical URL 查找 raw 或 `.gz` storage entry並依 logical 副檔名回 MIME type。只有 gzip entry 回 `Content-Encoding: gzip` 與 `Vary: Accept-Encoding`；raw entry不加 encoding。兩者都回 `Cache-Control: no-store`，`/` 對應 `/index.html`。
- Builtin 所有 asset 使用相對 URL且不得依賴 CDN、remote font、remote icon、analytics 或其他外部資源。User frontend 允許外部資源；本地 asset 仍須存在，hash routing 可用但不提供 history fallback。
- 動態 mDNS URL 是連回本機裝置的功能連結，不視為外部 dependency。
- User frontend 若使用 bundler，必須先在專案外完成，放入 `user-web/` 的內容要能直接部署。

## Preview 與 mock adapter

`preview/config.js` 集中建立 `window.DEVICE_CONSOLE_FLAGS.mockApi`。Generated demo 預設為 `true`，host 可在 config script 前提供 boolean flag，URL query `?mock=1`／`?mock=0` 則作為最高優先的人工覆寫。相同規則適用於 `file://`、本機 HTTP server 及靜態網站，不得再依 protocol 決定 mock 是否啟用。

- 只有 `window.DEVICE_CONSOLE_FLAGS.mockApi === true` 時才攔截 request。
- Flag 未啟用時不得攔截 request；不論 `file:` 或 HTTP(S) 都由正式 API client 使用原生 `fetch`。
- Topbar badge 明確標示 PREVIEW／假資料，位置在 Refresh 左側；測試登入資訊與 `Reset demo` 位於 badge 開啟的獨立 dialog，公共 Menu 不加入 Preview 內容。Preview adapter 在 shell 與共用 dialog host mount 後動態加入，正式 build 不保留該 UI。
- `Reset demo` 動作先 disable 按鈕並顯示進行中狀態，再清除 preview token、移除 view shortcut、回到 `#/network` 並強制重新載入初始記憶體 state。使用 `sessionStorage` 跨 reload 顯示一次完成 notice。
- Mock router 使用和正式 API 相同的 method/path、Bearer policy、response envelope 與主要欄位驗證；包含 STA connect 的 connecting/connected flow，未知 route 回 `not_found`。
- Auth、Wi-Fi、hostname 與 reset 為同一個 preview page lifecycle 內的 stateful flow；logout、password change 與 reset 必須使既有 session 依正式 contract 失效。
- Scan 套用 firmware 的 `RSSI > -75 dBm`、RSSI 排序、top 20 與 10 秒 cooldown；raw mock data 仍涵蓋 hidden、弱訊號及 open network 以驗證 API/UI boundary。
- Wi-Fi connect/update/reconnect 在 preview 內模擬短暫 `connecting`／`starting` 後進入 terminal runtime state；AP→STA/AP+STA update 必須走和正式 API 相同的 `202`、connection status、delayed commit/AP finalization observable flow。Preview 不建立 test id 或 commit route。
- 假資料集中在 preview adapter，不寫入正式 module 或 API client。

`builtin-web/index.html` 以明確 PREVIEW START／END block 依序載入 preview stylesheet、config 與 adapter。Production build 移除整個 block並排除 preview directory；`build/latest/web/` 的 production builtin 不得包含 flag、mock adapter、preview stylesheet 或 Preview UI。

## Static demo 與 GitHub Pages

- `builtin-web/index.html` 可直接由 `file://` 開啟完整 preview；`make demo WEB_PROCESS=none` 建立等價的 `build/latest/web/`，供一般靜態伺服器、GitHub Pages 或獨立輸出使用，並保留 preview config 與 mock adapter。Demo 預設仍為 `minify-gzip`，需要一般 static server 時必須明確指定 `none`。
- `make verify-web` 依 `web-manifest.json` 確認 config、adapter、application script 順序、HTML asset reference、raw/gzip collision、gzip 完整性及無 symbolic link。
- Demo 與 production web 必須由同一個 `tools/web-build/build_web.py` 依 target 處理；兩者共用 `build/latest/web/`，後建者完整取代前者，不得建立第二個 demo output 或工具。
- GitHub Pages workflow 執行 `make demo WEB_PROCESS=none` 並只發布 `build/latest/web/`；不得發布 `builtin-web/`、repository root、firmware snapshot 或 `user-web/`。
- GitHub Pages 使用 repository subpath 時，HTML/CSS/JavaScript asset 必須維持相對 URL；`/api/...` request 由啟用的 mock adapter 攔截。

## CSS、theme 與 motion

- 跨元件 theme color 集中為 CSS custom properties；只有 Hardware 容量分區這類獨立資料 palette 可在對應 component CSS 直接定義色碼，不佔用 `:root` token。
- 視覺層級只保留 shell、page、panel 與 transient overlay。Panel 內的 tabs、hint 與 advanced fields 優先使用 spacing、divider 或 accent line，不再建立帶 shadow 的內層卡片。
- Shadow 用於需要浮在內容上方的 menu、notice、dialog，以及持續浮動的 Wi-Fi `save-dock`；sticky topbar、Settings tabs 與 panel 不用 shadow 製造額外層級。
- Wi-Fi `save-dock` 使用 viewport-fixed 外層，desktop 外層固定在 sidebar 右側並重用 `page-area` 的 content width／32 px inset 計算；`.save-dock-inner` 才擁有 border、background、shadow 與操作內容。Mobile 外層改用和 page area 相同的 18 px／14 px inset，並避開固定底部導覽，對應 page area 保留足夠 bottom padding。
- Root scroll container 使用 `scrollbar-gutter: stable` 固定預留垂直捲軸空間，避免不同內容高度的頁面切換時改變 topbar 與 page area 的置中基準。
- 現階段只輸出 light theme，不保留未啟用的 dark selector 或 override；未來啟用新 theme 時仍應共用 HTML structure 與 component CSS。
- Icon 優先使用可重用的小型 SVG symbol 或 CSS，不為每個 icon 複製大型 path。
- 動畫只用必要的 opacity/transform feedback，並尊重 `prefers-reduced-motion`。
- 不依賴 `backdrop-filter`、大型 blur 或持續動畫作為主要效果。

## Dependency 與輸出

- 不使用前端 framework、外部 font、裝飾性 raster image 或大型視覺 dependency。
- 內建正式資源預設使用可重現的 minify 與 deterministic gzip pipeline；可明確指定 `WEB_PROCESS=none`。User frontend 預設保留 raw／precompressed輸入，只有使用者明確指定時才執行本專案的 minify/gzip。專案持久壓縮格式只允許 gzip，不加入 `.br`、zstd、xz、bzip2 或 ZIP；firmware package 的 TAR 僅是多檔案 container。
- Source formatting 不受 release payload 大小驅動；source 以直接可執行與 code review 可讀性優先。
- 每次主要改版量測正式資源、preview 與總大小，結果寫入 `tmp/verification/` 驗證紀錄，不寫入本 SPEC。

## 工程驗證要求

- 先完成並檢查 `builtin-web/` source，再進入 build／verification 階段；不得把 `build/` 是否同步當作 source change 是否完成的判準。
- 修改內建前端後至少執行 `make web WEB=builtin` 與 `make verify-web`；需產生可燒錄 firmware 時執行 `make esp`，完整流程使用 `make build WEB=builtin`。不得以 source directory 直接建立正式 app image。
- Production builtin 檢查 `build/latest/web/` 不含 `assets/js/preview/`、preview script entry 或 PREVIEW marker；`minify-gzip` output 必須只有有效 `.gz`，`none` output 可為 raw。
- User fixture 必須各驗證 default `none` 與 opt-in `minify-gzip`；不完整的 user tree、raw/gzip logical collision、無有效 index、unsafe path 或無法處理的 compressed input必須安全失敗。
- `make verify-web` 獨立確認 processed web 與 web manifest；`make verify` 同時確認 latest production web、binary manifest、所有 image hash、flat tar.gz `firmware.img` 與 web/firmware hash 關係。歷史快照使用 `make verify IMAGE=build/<timestamp>/firmware.img`。
- 以 `file://builtin-web/index.html`、`build/latest/web/` 的 unprocessed demo 及本機 HTTP server在 mock flag 開啟時檢查 script load/runtime error、主要 view 與 mock state，並以 `?mock=0` 確認 adapter 可明確停用。
- 執行 `make demo WEB_PROCESS=none` 與 `make verify-web`，確認靜態 demo保留 mock flag 與 adapter；GitHub Pages workflow 變更時檢查 YAML syntax 及官方 action contract。
- 至少檢查 360、390、768、1280 px viewport 的整頁 overflow。
- 檢查 keyboard navigation、focus、live region 與動態字串 escape。
- `make test-web` 必須以 actual `builtin-web/index.html` classic-script 順序，分別透過 `file://` 與本機 HTTP 執行 headless browser contracts，涵蓋 ResourceStore stale response、formal/mock API client 邊界、login/session/password、AP protection restart、save-dock/form width alignment、dialog focus lifecycle 與 Wi-Fi safe-transition stale protection；暫存 harness/profile 只寫入 ignored `tmp/web-tests/`。
- 檢查正式資源沒有 remote dependency。
- 最終整合以真實裝置 HTTP 驗證公開狀態、session、scan、save、password、hostname 與 reset。
- 所有日期結果與未通過項目記在 `tmp/verification/`，不得把執行狀態混入規格。
