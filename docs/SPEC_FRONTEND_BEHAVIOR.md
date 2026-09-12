# Frontend Behavior Specification

本文件定義 `builtin-web/` 內建網頁的使用者故事與可觀察體驗，類似 PM SPEC。Firmware 行為以 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md) 為準，request/response 以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準，前端檔案與程式組織以 [SPEC_FRONTEND_TECHNICAL.md](SPEC_FRONTEND_TECHNICAL.md) 為準。

## 目標

內建網頁是 ESP32 Wi-Fi foundation 的本地 REST client：

1. 使用者不登入即可判斷裝置、儲存空間與網路狀態。
2. 只有進入設定流程才要求登入。
3. 新手可完成基本 Wi-Fi 設定，進階使用者仍能辨識 STA、AP 等術語。
4. 完全沒有網際網路的 AP 環境仍可完整操作。
5. 內建頁可被其他產品替換，不讓 firmware 依賴特定 HTML 或品牌流程。

## 使用者故事

### 第一次使用

使用者連上預設 AP 並開啟裝置頁時，先看到公開 Network 頁，不被登入畫面阻擋。進入 Settings 後才登入、掃描 Wi-Fi、填寫設定並儲存。套用期間清楚提示動作與可能的短暫斷線。

### 日常查看

使用者透過 AP IP、LAN IP 或 mDNS 開啟頁面，不登入即可讀取 Network 與 Hardware。公開頁不得顯示 Wi-Fi password、admin password 或 token。

### 管理設定

使用者主動進入 Settings 時才驗證既有 token。Token 有效則進入設定；無 token、無效或裝置重啟後顯示 login dialog。登出後清除瀏覽器 token 並返回公開 Network。

## 資訊架構

```text
Device Console
├── Network（公開、預設入口）
├── Hardware（公開）
└── Settings（需要有效 session）
    ├── Wi-Fi
    ├── Admin
    └── System
```

- Login 是 Settings 的權限閘門，不是網站首頁。
- Desktop 使用左側主導覽，mobile 使用 bottom navigation；mobile 三個導覽項目維持等距排列，但實際按鈕使用置中的有限寬度與可見 active feedback，項目之間保留不觸發導覽的間隔。
- Desktop topbar 以上下兩層顯示頁面資訊：與主導覽一致的簡短頁名使用上方小字，頁面內容說明作為下方主標題；mobile 只保留簡短頁名。
- 右上 `Menu` 不是頁面；未登入也能切換 Language，已登入 Settings 時可顯示 Log out。
- 頂層與 Settings 子頁使用 `/#/...` hash route；瀏覽器上一頁／下一頁可回復先前頁面。
- Page title、Refresh 與 Menu 所在 topbar sticky 置頂。

## 初始載入與導覽

1. 套用語言並綁定互動。
2. 顯示 Network 導覽與公開網路頁。
3. 載入 device、storage、Wi-Fi 公開狀態。
4. 呈現 loading、成功或錯誤狀態。
5. 初始載入不得主動顯示 login。

正式 routes：

- `/#/network`
- `/#/hardware`
- `/#/settings/wifi`
- `/#/settings/admin`
- `/#/settings/system`

### 進入 Settings

1. 使用者點擊 Settings。
2. 無 token 時顯示 login dialog。
3. 有 token 時呼叫 session API。
4. Session 有效才顯示 Settings；無效時清除 token 並顯示 login。
5. 登入成功後進入原本要前往的 Settings，不跳回 Network。

### Public Menu

- Desktop 與 mobile 右上角皆提供 Menu。
- Language 是第二層 submenu，支援 hover、focus、click 開啟；選項從主選單側邊展開，兩層選單之間必須有連續的滑鼠命中區域。
- 第一階段提供 `English` 與 `繁體中文`，以 check mark 標示目前語言。
- 切換語言立即套用，不重新載入，也不清除表單內容。

## Network 公開頁

Network 優先回答「目前連到哪個 Wi-Fi」及「如何直接連到這台裝置」：

- STA Wi-Fi 名稱與 IP、裝置 AP 名稱與 IP、mDNS URL；不另外顯示 hostname 欄位。
- 實際 Wi-Fi mode；需明確標示為 Wi-Fi mode，不另外顯示 configured mode。
- STA SSID、security/IP 與 connection state。
- AP SSID、state/IP，以及 password enabled/disabled；不顯示密碼。

呈現規則：

- mDNS URL 格式為 `http://<hostname>.local/`，下方顯示 `Available on STA LAN／可從 STA 區域網路存取`；不可用時仍區分 hostname 與 link 狀態。
- 位址卡回到第一版的三欄結構，依序顯示 STA、AP 與 mDNS URL；卡片標題上方不加 eyebrow 小字，標題列右側保留目前的 `Wi-Fi mode` 膠囊標示。
- STA 與 AP 欄以 SSID 作為藍色主要資訊，IP 置於欄位最下方並使用低調的次要文字；位址卡不顯示 hostname 欄位。
- STA／AP 欄不加入 runtime state 或存取說明，避免在主要 SSID 與次要 IP 之外重複資訊。
- Runtime state 必須轉為目前語言後呈現；介面名稱、badge 與 detail 不得混用 API 原始英文值。
- Interface status 的 STA／AP SVG 使用固定尺寸的淡主色圓角背景容器，不直接裸露在卡片底色上。
- Read-only runtime 若為 Off 可顯示 `Off`，Settings mode picker 不提供 Off。
- 空值使用一致 unavailable 表示，不顯示 `undefined` 或 `null`。
- Online、warning、error 不只靠顏色區分。
- 提供手動 Refresh；不顯示「最後更新」欄位。

## Hardware 公開頁

- Device：chip model、chip revision、CPU cores、flash capacity、heap usage、MAC address。
- Image space：使用薄型分段長條圖顯示系統使用空間、韌體程式、內含前端及可供燒錄空間。系統使用空間由 API 回報的 bootloader／partition table 固定區域、`nvs` 與 `otadata` 相加，不包含 `user.limits.reserved_bytes`。韌體程式為整包韌體映像扣除內含前端，兩者相加才是 `firmware_image_bytes`。
- File storage：使用另一條薄型分段長條圖，依序顯示 32 KiB `user_nvs` 使用者設定空間、使用者檔案、`user.limits.reserved_bytes` 檔案緩衝空間、64 KiB coredump 系統 Log 空間及可用空間；Available 必須直接使用 API 的 `limits.max_upload_bytes`，也就是當下單一新檔的上限。
- Device、Image space 與 File storage 各自使用卡片，依序分成上下列，不顯示 Partition layout 卡片，也不在 desktop 強制並排。
- Image space 與 File storage 都使用不高於 12 px 的比例分段長條圖，並共用同一組 palette；此 palette 在容量圖元件 CSS 內直接定義，不受全站 theme token 色相限制。第 1 色為 `#166534`，第 2 色為 `#0284C7`，第 3 色為 `#16A34A`，第 4 色為 `#7C3AED`，第 5 色為中性灰 `#D8DDE5`。Image space 依序使用第 1、2、3、5 色；File storage 依序使用第 1～5 色。
- 每個容量數值標籤前都要持續顯示對應色池的可見色塊，不得被 i18n 文字更新移除。滑鼠移入長條分段時，native tooltip 與 accessibility label 使用目前語言顯示「項目名稱：容量」，切換語言後立即同步。
- Image space 與 File storage 標題下方不顯示說明小字；「系統使用空間」圖例下方也不顯示組成說明。
- Desktop、tablet 與 mobile 都使用同一套分段長條圖，不使用圓餅圖，也不另外顯示抽象的 used percentage；tablet 與 mobile 的 Image space 與 File storage 統計項目皆固定每列兩個，奇數個項目時最後一列保留單一項目。
- UI 使用「File storage／檔案儲存空間」與「Available space／可用空間」等使用者語言；不顯示額外的可使用狀態，也不顯示 `LittleFS`、`mounted` 等實作名詞。
- Generic file endpoint 已由 firmware 提供，但目前內建頁面的 Hardware 卡片仍維持容量 read-only，不提供上傳、瀏覽或刪檔按鈕；`available_bytes` 仍直接表示當下單一新檔上傳上限。
- Hardware 不重複 Network 資料；read-only hardware 資訊不放入 Settings。

## Settings / Wi-Fi

Settings 只有有效 session 才可見，所有寫入或控制 request 都帶 Bearer token。

### Mode

| UI | API value | 使用者意義 |
| --- | --- | --- |
| `AP` | `ap` | 只開啟裝置自己的 Wi-Fi。 |
| `STA` | `sta` | 連接既有 Wi-Fi，成功後關閉裝置 AP。 |
| `AP + STA` | `ap_sta` | 連接既有 Wi-Fi 並保留裝置 AP。 |

- UI 保留 AP、STA 術語，但以一般使用者能理解的情境解釋。
- AP：只顯示 AP 設定。
- STA：只顯示 STA 設定與 fallback 開關；fallback 啟用時沿用既有 AP 設定，不顯示另一份 AP 表單。
- AP + STA：同時顯示 STA 與 AP。
- 切換 mode 後不留下不相關欄位，也不讓隱藏欄位阻止表單送出。
- Mode picker 以卡片右上角貼齊邊框的小型主色三角形指出裝置原本的 mode，不加入文字標記；三角形預設使用直角，並保留獨立 radius 參數供視覺微調。標記必須裁切在卡片圓角內，不得突出或蓋掉外側邊界。使用者另選尚未儲存的 mode 時，原設定三角形仍保留，當下選擇則以主色淡底區分。

### STA 基本設定

- 第一屏包含 Choose network／SSID、Security、Password 與 save/apply。
- Security 第一階段提供 WPA 與 Open；Open 必須明確選擇，不由空 password 推斷。
- WPA password 為 8–63 個 printable ASCII 字元（ASCII `0x20`–`0x7E`）；前端送出前必須套用相同限制。
- Password input 載入時保持空值；目前 SSID 等於已儲存 WPA SSID 時，以 `********` 表示沿用既有密碼。欄位取得 focus 時隱藏此 placeholder；失去 focus 且仍符合沿用條件時恢復。SSID 改動後立即移除 placeholder，新的 WPA SSID 必須輸入密碼。
- Password 提供 eye/eye-off 顯示／隱藏按鈕，切換時不得清除輸入；Open 或 STA 欄位不適用時，按鈕隨輸入框停用。
- STA password 是裝置要連線的 Wi-Fi credential，不是目前網站帳號密碼；form 與欄位使用 `autocomplete="off"`，不得標示為 `current-password` 或 `new-password`。

### AP 基本設定

- AP SSID。
- AP password enabled toggle；只切換 switch 本身，不顯示實際 AP password。
- 儲存 AP password enabled/disabled 變更成功後顯示裝置正在重啟、清除本機 token 並回到 Network；不得繼續把舊 runtime session 或原連線視為可用。

### 進階設定

- STA 與 AP 各自在所屬卡片內以 Accordion 提供進階設定；基本欄位保持可見。
- `fallback_to_ap` 只在 STA mode 顯示，並沿用已載入的 AP 設定。
- STA 支援 DHCP/static IPv4；static 欄位依序為 address、netmask、gateway 與最多兩筆 DNS。
- AP 支援 default/static IPv4；手動模式顯示 address 與 netmask，不把 AP 自身位址模式描述成 DHCP。
- 儲存使用 `PUT /api/wifi` 完整 replacement；不適用但 API 必需的分類由已載入表單 baseline 保留。

## Wi-Fi 掃描

- `Choose network` 位於 STA SSID 區塊。
- UI 隱藏 hidden、空 SSID 與 RSSI 小於 `-75 dBm` 的結果，並依 RSSI 由強到弱排序。
- 每筆顯示 SSID、RSSI、channel、security 與選用動作；訊號強度另以 Wi-Fi SVG 四級圖示呈現，依序使用 `RSSI >= -50`、`>= -60`、`>= -68`、其餘可見訊號四個等級，且不可取代 RSSI 數字。
- 選用後只填入 SSID 與可推斷的 security，不自動送出。
- 無符合結果時顯示「找不到符合條件的可見網路」，不可誤顯示成尚未掃描。
- Desktop 使用 dialog，mobile 使用適合窄螢幕的全高 sheet/dialog。
- 結果區支援數量、重新掃描、文字篩選與內部捲動；SSID 文字篩選欄使用 funnel 圖示。畫面只顯示 `Found n results／偵測到 n 個結果`，n 為目前實際列出的結果數，不另外顯示原始偵測數、隱藏／弱訊號排除規則或捲動提示。
- 掃描期間 disable 重複操作並顯示進行中狀態；cooldown 使用簡短秒數倒數，按鈕寬度固定為正常「Scan again／再次掃描」狀態的寬度，不隨倒數文字變動。錯誤必須轉成可理解 feedback，最終恢復可操作。
- 持久化 STA 尚在連線而暫時占用 radio 時，顯示可理解的稍候重試提示與短暫倒數，不顯示泛化的 firmware scan failure。

## Settings / Admin

- Username 固定為 `admin`，不提供修改欄位。
- REST API 仍接受 8–63 個 printable ASCII 字元；內建網頁的管理員新密碼採更嚴格 allowlist：英文字母 `A–Z`／`a–z`、數字 `0–9`，以及 `! @ # $ % ^ & * ( ) - _ = + . , : ?`。不得輸入空格、反斜線、引號、斜線、反引號或未列出的其他符號。
- Allowlist 與長度限制需顯示在欄位附近，HTML constraint 與送出前 JavaScript validation 必須使用相同規則；不符合時不得送出 request，並顯示目前語言的修正提示。
- 使用者輸入及確認兩次新密碼。
- 兩個 password 欄位都提供 Show/Hide 按鈕，切換時不得清除輸入；隱藏狀態使用 eye 圖示，顯示狀態切換為 eye-off 圖示。
- 新密碼規則提示使用完整細邊框與淡主色背景，不使用左側 accent border。
- 內建裝置不要求瀏覽器保存管理員密碼；修改密碼 form 與兩個 password 欄位使用 `autocomplete="off"`。
- 成功修改後清除 token，要求以新密碼登入。
- AP password 已啟用時，成功修改管理員密碼後的裝置重啟仍沿用相同的清除 token／重新登入流程，成功訊息需明確包含裝置正在重啟。
- 不顯示額外的 Local authentication 說明卡。

## Settings / System

- System form 標題為 `System settings／系統設定`，Device name label 與 hostname input 同列。
- Hostname 位於 System，不出現在 Wi-Fi form；使用 system API 更新。
- 更新成功後提示網路服務與 mDNS 會重新套用，連線可能短暫中斷。
- Factory reset 送出前需要二次確認，並明確告知 Wi-Fi、管理員設定與所有使用者檔案都會被清除。
- Reset 成功後提示裝置將重啟且目前連線可能中斷，不假設原 IP 仍可用。
- Reset request 送出後 confirmation action 保持 disabled，直到成功進入 restart flow 或失敗關閉 dialog，避免重複 request；成功時立即清除本機 token 並回到 Network。
- System 只放真正操作與 danger zone，不重複 Hardware 的 chip、MAC、storage 等資訊。

## Login dialog

- Login 保留使用者原本 Network／Hardware 畫面脈絡。
- Login dialog 初次開啟及每次重新開啟時，`loginNotice` 以中性提示顯示「Enter the administrator password to unlock these settings.／請輸入管理員密碼以解鎖這些設定。」；登入中狀態與登入錯誤可取代此提示，錯誤不得沿用到下次開啟。
- Login dialog 每次開啟完成後，初始焦點放在 password input，不放在唯讀 username。
- Login form、唯讀 username 與 password 使用 `autocomplete="off"`，避免主動觸發瀏覽器 credential autofill／save；此屬標準提示，瀏覽器或使用者設定仍可能覆寫。
- Login dialog 透過公開 `GET /api/auth` 取得並顯示目前 username，不從 Wi-Fi response 取得，也不自行寫死 `admin`。
- Login API 回傳 `unauthorized` 或 HTTP 401 時，顯示目前語言的管理員密碼錯誤訊息，不直接呈現 API 的英文 `invalid credentials`；未知錯誤才使用 API message 作 fallback。
- Desktop 使用置中 dialog；mobile 可使用 bottom dialog/sheet。
- `Sign in` 在左、`Cancel` 在右，兩個按鈕等寬。
- 登入錯誤顯示在表單附近，不清空 username。

## 狀態與錯誤回饋

- 初始載入、Refresh、login、scan、save、change password、hostname update 與 reset 都有可見 feedback。
- Wi-Fi 與 System 的 Save 初始及無變更時 disabled；只有表單相對載入值有變更且所有目前可見／適用欄位有效時才可操作。
- Wi-Fi `save-dock` 在捲動頁首、中段與頁尾時持續浮在 viewport 底部；desktop 與 mobile 都使用和 `page-area` 相同的 content inset，內層操作面必須與 `wifiForm` 左右邊界及寬度一致。Mobile 位於底部導覽上方，內容區需保留不被操作列遮住的底部空間。
- Wi-Fi dirty 比對只使用目前 mode 適用的分類；提交仍遵守完整 replacement contract，不適用分類從已載入 baseline 帶入，不送出其隱藏 draft。
- 有變更但必填或格式驗證未通過時，Save 保持 disabled，靠近操作顯示修正提示並標出無效欄位。
- 錯誤靠近相關操作；只有全域錯誤使用全域 notice。
- 非同步期間避免重複送出；request 失敗不把舊資料標為最新。
- Wi-Fi 儲存成功後不得固定等待短時間就假設原 IP 可用；應提示連線可能中斷、提供 mDNS 入口，並只做有限次狀態重試。
- STA connection status 使用有限間隔輪詢；AP/STA 共用 radio 導致短暫 transport failure 時可在 timeout 內重試，不得因此立即宣告 credential 錯誤。
- 從 AP 切換為 STA 或 AP + STA 時，Settings 必須呈現 candidate 驗證、持久化與最後 AP 套用的非同步進度；STA 驗證失敗時顯示已 rollback 且原 AP 可繼續使用，不得把未提交的表單值標示為已儲存。
- Safe transition 以最多 25 秒的有限輪詢涵蓋 15 秒 STA deadline、commit 與 5 秒 AP grace；transport failure 在期限內重試。只有 terminal `connected` 才把送出值設為表單 baseline 並清除 password input；terminal `failed` 保留 draft 並顯示原設定與 AP 已 rollback。
- 最終 STA mode 在 5 秒 AP grace period 內顯示成功取得的 STA IP 與 AP 即將關閉；最終 AP + STA 的 AP 設定有變時，提示使用者需依剛送出的新 SSID／IP 重新連線，不得回傳或顯示任何密碼。
- 常見操作訊息由 i18n 管理，API message 只作 fallback。
- 動態狀態使用適當 live region，不只依賴 alert 或顏色。
- Refresh 完成、掃描完成、System 儲存成功、Wi‑Fi 套用後進入正常 terminal state 等一般成功 notice 約 2.2 秒後自動消失；密碼修改成功提示可保留約 4 秒。
- 錯誤、Wi‑Fi 儲存後可能斷線／位址變更、Wi‑Fi 套用失敗及裝置重啟等需要使用者採取行動或保留脈絡的 notice 不得自動消失。
- 全域 notice 的 dismiss timer 必須由共用 helper 管理；任何較新的全域 notice 都要取消舊 timer，舊 timer 不得清除後來出現的訊息。

## Preview 與公開 Demo

- 同一份前端可透過明確 mock flag 在 `file://`、本機 HTTP server 或靜態網站啟用假 API；不得以 URL protocol 隱含決定是否使用假資料。
- 使用者可直接雙擊 `builtin-web/index.html` 開啟完整 preview；最小 source HTML 由本地 application modules 直接 mount shell 與目前頁面，不得依賴 build-time fragment 展開才產生畫面。
- Mock 啟用時在 topbar 的 Refresh 左側持續顯示小型 `PREVIEW／假資料` badge，避免訪客誤認為真實裝置資料；badge 開啟獨立 Preview dialog，放置測試登入資訊與 `Reset demo`，不使用固定 banner，也不把 Preview 內容加入公共 Menu。
- Preview badge、dialog 詳情與 Reset 動作必須跟隨目前介面語言，不得在英文模式混入中文提示。
- `Reset demo` 立即顯示進行中狀態，清除 preview session、回到 Network 並重新載入初始假資料；完成後必須顯示已重設訊息，語言偏好可保留。
- Preview 的 login、scan、save、password、hostname 與 factory reset 都必須可操作，並與正式 REST contract 的可觀察流程一致。
- 裝置正式頁不得載入 mock flag 或 adapter，也不得顯示 Preview UI。

## Responsive 與 Accessibility

至少支援 360、390、768、1280 px viewport：

- Desktop 切換內容高度不同的頁面時，即使垂直捲軸只有部分頁面出現，topbar 標題與主要卡片也不得水平位移；頁面需固定預留捲軸空間。

- 整頁不可水平溢出；必要表格只在自己的容器捲動。
- Desktop 的 Wi-Fi 基本設定在 1280 × 720 viewport 應同時看見 SSID、Password 與 Connect，不要求垂直捲動。
- Mobile 表單單欄，input 至少 16px 字級，主要互動目標高度至少 44px。
- 一般內文以 16px 為基準；欄位標籤、輔助文字、狀態文字與行動導覽不得用難以辨識的極小字級。
- Notice、preview badge、Preview dialog、Menu 與 sticky UI 不遮住主要操作。
- Captive portal 小視窗仍可完成基本 Wi-Fi 設定。
- Input、select、checkbox 都有 label；鍵盤可完成導覽、登入及設定。
- Focus 與 active navigation 清楚可見。
- Dialog 開啟時背景內容必須 inert，`Tab`／`Shift+Tab` 焦點限制在目前 dialog；關閉後焦點回到原觸發控制。既有 `Escape` 與 backdrop close 行為維持不變。
- 標題層級合理，動態字串插入 HTML 前 escape。

## 視覺與語言

- 採克制、安靜、清楚的控制台風格；層級主要來自 typography、spacing、alignment、border 與少量 surface 差異。
- Sticky topbar 使用獨立 surface、邊界與足夠下方間距，需和 Settings 子導覽清楚分層。
- 頁面只保留 shell、內容頁與主要 panel 三個持續可見層級；panel 內的設定導覽與提示使用分隔線或 accent line，不再各自形成帶陰影的內層卡片。
- 陰影主要保留給 Menu、notice 與 dialog 等暫時浮層；Wi-Fi `save-dock` 作為持續浮動的主要操作區可使用克制的陰影，topbar、Settings 子導覽與主要 panel 不使用陰影。
- Topbar 保留較充足的垂直空間以容納雙層標題；主要頁面卡片維持緊湊間距，desktop/tablet 為 10 px、mobile 為 8 px。
- 內容頁面與卡片標題不加 eyebrow／分類小字；topbar 依資訊架構規則以上方小頁名搭配下方主標題，除此之外同一處只保留一個清楚的主標題。
- Menu 與第二層 language submenu 不顯示方向箭頭，並維持緊湊寬度與至少 44px 的操作高度。
- Sidebar brand 由上到下顯示「Device name／裝置名稱」、目前 hostname 與連線狀態，搭配 globe 圖示；不使用無產品定義的字母縮寫。
- Network 的 STA 使用 Wi-Fi 圖示、AP 使用 signal 圖示；Hardware 導覽使用 CPU chip 圖示。
- 藍色只用於主要動作、active navigation、focus 與關鍵狀態。
- 避免大量漸層、blur、發光、複雜背景、過度陰影及每個數值一張卡片。
- 第一語言為英文，支援繁體中文 `zh-Hant`。
- 所有固定文案、placeholder、空狀態、常見錯誤及動態欄位名稱都要可翻譯。
- STA、AP、AP + STA、SSID、RSSI、channel 等術語保留原名。
- 現階段只提供 light theme；色彩仍以語意 theme token 集中管理，但不放入未啟用的 dark selector 與 token override。

## 不在前端範圍

- 圖片／電子紙內容編輯、OTA、雲端帳號、遠端管理、多使用者角色。
- Firmware 尚未支援的進階網路功能。
- 裝飾性大型資產或需要網際網路的 UI dependency。

## 待決產品問題

- Network／Hardware 僅手動 Refresh，或定時自動更新；若自動更新，間隔多久？
- 是否提示 captive portal browser 可能不穩，建議改用一般瀏覽器？
- Open AP 時是否顯示安全警告？

## 請求等待與離頁

- 一般操作等待最多 10 秒、掃描最多 20 秒；Wi-Fi 切換確認每次請求最多 3 秒，整體仍最多 25 秒。逾時停止 busy feedback 並提供雙語重新連線／確認結果提示，保留尚未提交的 Wi-Fi 草稿。
- Password、reset、Wi-Fi update 等寫入逾時代表結果未知，不自動重送；重新連線後先查狀態。逾時或離頁取消不當作認證失效，不清除 token；只有確認 unauthorized 才要求重新登入。
- 切頁取消該頁自有請求，不顯示裝置故障，也不以晚到結果改寫已離開的畫面或較新的操作。共享的公開狀態查詢可繼續供其他 consumer 使用。
