# Behavior Specification

本文件定義內建前端以外的產品需求與可觀察行為，類似 PM SPEC。REST request/response 的精確格式以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準；firmware 程式組織以 [SPEC_TECHNICAL.md](SPEC_TECHNICAL.md) 為準；內建網頁體驗另見 [SPEC_FRONTEND_BEHAVIOR.md](SPEC_FRONTEND_BEHAVIOR.md)。

## 產品定位

本專案是供 ESP32 IoT 裝置延伸的 Wi-Fi 與基本裝置監控基石平台。它提供通用的 AP／STA mode 設定、斷線備援、設定保存與裝置狀態查詢，讓感測器、控制器、gateway 或其他產品能在穩定的連網基礎上加入自己的硬體與軟體功能。新手不需要先理解序列埠、NVS 或 provisioning，即可連上裝置預設 AP，透過裝置網頁設定 Wi-Fi 並查看基本狀態。

Wi-Fi 設定以 REST API 為核心，內建網頁只是 REST client。同一套能力必須能由下列入口重用：

- 使用者操作內建網頁。
- AI 或自動化工具直接呼叫 REST API。
- 其他 ESP32 IoT 專案複用 Wi-Fi 與基本監控 foundation，並可在 `user-web-project/` 開發自己的前端，由正式 build 匯入後包進 firmware。

專案同時是可直接 fork 的完整 app 與可抽出的 IoT connectivity foundation。允許為復用性保留少量架構成本，但使用 foundation 的專案應只需最低程度修改。預設值應保持通用，不長期綁定特定開發板。

## 使用者角色

- **第一次使用的新手**：只想連上 ESP32 AP，開啟網頁完成 Wi-Fi 設定。
- **管理者**：需要修改 Wi-Fi、hostname、AP 行為、管理密碼或執行 factory reset。
- **完全地端使用者**：裝置不能連外網，仍需透過 AP 使用網頁與 API。
- **AI／自動化工具**：不依賴瀏覽器即可查詢狀態、修改設定及控制裝置。
- **韌體開發者**：複用 Wi-Fi setup、REST API 與內嵌管理頁，或替換前端。

## 開箱與第一次設定

空 NVS 或 factory reset 後：

- 裝置以 AP mode 啟動。
- 預設 hostname 為 `esp32-device`。
- 預設 AP SSID 為 `<hostname>-<mac後四碼>`，降低多台裝置撞名的機會。
- 預設 AP 不啟用密碼。
- 使用者可連上 AP，開啟內建頁面並透過 REST API 儲存 Wi-Fi 設定。
- 常見 captive portal detection endpoint 應導向設定頁或 AP IP。
- AP、setup 或 fallback AP active 時啟用 DNS wildcard captive portal，將 AP client 的 domain query 導向裝置 AP IP。
- SoftAP DHCP 必須將裝置 AP IP 發布為 DNS server，並發布 captive portal URI，使同時存在其他上網介面的 client 仍有機會把 portal probe 送到裝置。
- Captive redirect 只攔截由 AP 介面抵達的未知 GET；AP + STA 時不得將 STA LAN 的未知 GET 導向 AP IP。
- 作業系統是否自動彈出 captive portal 屬於 client 行為，不作為唯一入口；一般 builtin／user frontend build 的使用者必須始終可直接開啟 AP IP 的 `/`。明確以 `WEB=none` 建置的 API-only firmware 不提供設定頁，captive portal 仍可導向 AP IP，但該頁回報 frontend 未包入。
- 純 STA LAN mode 不得啟用 DNS wildcard captive portal。
- mDNS 只在 STA connected 時啟用。

## 狀態查詢與 REST-first 操作

- `GET /api/device`、`GET /api/web`、`GET /api/storage`、`GET /api/wifi`、`GET /api/auth`、全部 e-paper 專用 endpoint 與 `GET /api/runtime/status` 公開；Wi-Fi scan／connect、auth session 與 generic user-file endpoint（包含 list／download）需要有效 Bearer token。
- 電池狀態沿用公開唯讀的 `GET /api/device`，在既有裝置資訊內回報 GPIO0 板載分壓量得的電池電壓與由單節鋰電池電壓曲線推算的估計百分比，不另設 `/api/power`。本功能不修改硬體，無法辨識是否安裝電池、外部電源或充電狀態；response 不提供 `charge_state`、`charging`、`battery_present` 或 `power_source`，也不得用電壓變化推測並宣稱正在充電。
- `GET /api/web` 只回報目前 app image 的 Web 狀態：包入前端時為 `builtin`／`user` 來源與實際輸出檔案樹 SHA-256；明確以 `WEB=none` 建置時為 `source: "none"` 與 `sha256: null`。不公開 version、檔案數量、payload 大小或 user source hash。非 null hash 用於對照 release manifest 與辨識 Web bundle 是否為預期版本，不代表整包 firmware image hash。
- `GET /api/storage` 回報完整 flash partition、1984 KiB `app0` 的整包韌體／前端／剩餘容量、32 KiB `user_nvs`，以及獨立 `userdata` 的可上傳容量。前端若存在就是目前 app image 的一部分，不占用獨立 filesystem partition；`WEB=none` 回報 `frontend_bundled: false` 與零 frontend payload。
- `user.capacity.available_bytes` 必須等於 `user.limits.max_upload_bytes`，代表以當下狀態可接受的單一新檔 payload 上限。計算先保留 64 KiB，再向下對齊 4 KiB allocation unit；前端直接把這個數字呈現為可用空間，不加入模糊化的免責文字。
- `userdata` 提供受保護的 file list、raw HTTP upload、raw HTTP download／single-range download 與 delete；公開檔名限制為 1–64 bytes ASCII，第一字元必須為英數，後續只允許英數、`.`、`_`、`-`，且不允許連續 `..`。
- 上傳開始前必須以新鮮 capacity snapshot 驗證完整 `Content-Length`，超過當下 `max_upload_bytes` 就在寫入前拒絕。Upload 使用單一 internal temporary file、4 KiB bounded chunk、verified close 與 atomic rename；新檔成功回 `201`，替換成功回 `200`，任何失敗都不得截斷原正式檔案或公開 temporary file。
- 同一時間只允許一個 user-data 檔案操作；HTTP 與 console 共用 non-blocking gate，競爭時立即回 busy，不等待。操作期間 `/api/storage` 只回最後完成操作後的穩定容量 snapshot。
- `GET /api/wifi` 可回傳 SSID、mode、IPv4 等非密碼資訊，但 hostname 由 `GET /api/device` 提供。
- API 永遠不回傳 Wi-Fi password、admin password 或 token 以外的敏感認證資料。
- API response 使用一致 envelope，成功欄位固定為 `success`。
- REST API 不加入 version path，維持 `/api/...`，也不維護舊 payload 或舊 response 的相容層；contract 改動由 firmware、API reference 與 client 同一版本一起更新。
- Wi-Fi 設定 request 應先完成 HTTP response，再立即套用或啟動明確記錄的非同步 safe transition，避免切換網路中斷 response；API 不回傳內部 transaction id 或 commit window。
- API contract 與實作必須同步；精確 method、path、auth 與 payload 不在本文件重複定義。

## 管理認證與 session

- 系統只有一組管理帳密；username 固定為 `admin`，不提供修改。
- 公開 auth API 提供目前登入 username；client 從 auth resource 讀取，不從 Wi-Fi resource 取得。
- 預設 admin password 為 `password`，第一次登入後不強制修改。
- admin 與 STA password 統一為 8 到 63 個 printable ASCII 字元（ASCII `0x20`–`0x7E`）。
- 修改 password 必須已登入，但 API 不要求 old password；新密碼二次確認屬於 client 流程。
- 登入成功取得 opaque random token。此設計是 ESP32 效能與本機驗證需求的取捨。
- REST API 以 `Authorization: Bearer <token>` 傳遞 token。
- 裝置只在 runtime 保存目前有效 token；token 不做時間到期，重開機後失效。
- 同一時間只承認最後一次登入取得的 token；後登入會使前一個 token 失效。
- 修改 admin password 後，後續登入必須使用新密碼。
- AP password 已啟用時修改 admin password，response 完成後必須立即重啟裝置，使 SoftAP 從 boot path 使用新 credential；該次重啟同時使 runtime token 失效。AP password 未啟用時只需持久化新密碼並撤銷目前 session，不重啟 Wi-Fi 或裝置。
- 登出必須由受保護 API 撤銷裝置 runtime token，不能只清除 browser storage。
- 除 login、credential verify 等明確例外外，所有 `POST`、`PUT`、`DELETE` 預設需要有效 Bearer token。新增例外必須同步寫入本文件與 API reference。

## Wi-Fi 基本行為

### Mode

- 支援 AP、STA、AP + STA，以及供 API/runtime 使用的 Off。
- Wi-Fi 最大 TX power 是裝置層級持久化設定 `wifi_tx_dbm`，factory default 為 15 dBm，可設 2–20 的整數。20 代表解除專案額外限制並交由平台目前允許的最大功率政策控制；它不繞過晶片、PHY 或地區限制，也不保證每個封包的實際發射功率。
- 使用者設定 STA 並成功連線後，AP 自動關閉。
- 使用者設定 AP + STA 並成功連線後，AP 保留。
- AP-only 不主動切換 STA，也不要求連外網。
- API 必須區分使用者設定意圖與實際 runtime，例如 `configured_mode` 與 `mode`。
- Public Wi-Fi mode 使用 `off`、`sta`、`ap`、`ap_sta`；REST 設定不接受 `off`，避免關閉唯一管理入口。

### STA 與 fallback

- STA 基本設定包含 SSID、security 與 optional password。
- `interfaces.sta.security` 第一階段支援 `wpa` 與 `open`。
- request 未包含 `interfaces.sta.password` 時，不處理、不清空、不覆蓋既有密碼。
- open Wi-Fi 必須由 security 明確指定，不以空 password 推斷。
- STA 連線失敗或斷線且 `fallback_to_ap` 開啟時，runtime mode 轉為 AP + STA，以避免裝置失聯。
- fallback 需要 AP 設定；使用者未提供時採預設 AP 設定。

### STA 連線與認證確認

- 受保護的 connect API 只接受 SSID 與 optional password，不向 client 暴露 test id、commit window 或 commit endpoint。
- Firmware 使用 non-blocking runtime state machine 連線；成功條件是 STA association 完成、取得非 `0.0.0.0` IPv4，並通過 runtime subnet overlap 檢查，不代表 Internet、DNS 或 gateway 可達。
- 連線期間保留目前 active AP 作為管理入口；沒有 active AP 時拒絕開始，避免 API client 在確認結果前失聯。
- Candidate credential 只存 RAM。認證成功後由 firmware 內部自動持久化為 AP + STA／DHCP 設定；失敗或儲存失敗不覆蓋原設定，並清除 candidate credential。
- 同一時間只允許一個 STA connect；connect 與 Wi-Fi scan 不得同時操作 radio，連線 timeout 為 15 秒。
- Client 只需開始連線並查詢 `idle`、`connecting`、`connected` 或 `failed`；`connected` 只在 credential 已驗證且設定已持久化後發布。
- `PUT /api/wifi` 是完整 mode replacement；內建頁面的 AP／STA／AP + STA mode 設定使用此 API。Connect API 仍供只需安全加入既有網路的簡化 client 使用。

### AP 到 STA／AP + STA 的安全切換

- 目前 runtime 有 active management AP，且完整 Wi-Fi replacement 要從 AP 切換為 STA 或 AP + STA 時，firmware 必須先以 RAM candidate 執行安全切換，不得先覆寫持久化設定，也不得先將整個 Wi-Fi radio 切為 off。
- 測試期間沿用目前 active AP 的 SSID、密碼與 IP，runtime 暫時切為 AP + STA；client 可經由仍在運作的 AP 查詢非同步切換狀態。AP 與 STA 共用 radio 或 channel migration 導致的短暫 transport failure 不等於 candidate 失敗。
- Candidate STA 的成功條件是 association 完成、取得非 `0.0.0.0` IPv4，並通過目前管理 AP 與最終 AP 設定適用的 runtime subnet overlap 檢查；不要求 Internet、DNS 或 gateway 可達。
- Candidate 使用單一 STA association，總 deadline 為 15 秒；產品語意分成三個 5 秒觀察區間，但區間交界不得重新呼叫 connect 或重設 DHCP。到期仍未成功才停止底層連線並 rollback；明確的不可恢復 driver/runtime error 可提早失敗。
- Candidate STA 成功後才原子持久化 Wi-Fi replacement。持久化時只合併 Wi-Fi 欄位到最新 active config，不得覆蓋測試期間完成的 hostname 或管理密碼更新。寫入失敗、STA 測試失敗或 runtime subnet 衝突都必須清除 candidate、保留舊持久化設定並恢復原 management AP。
- 最終 mode 為 STA 時，firmware 先發布持久化成功與 STA IP，保留 AP 5 秒 grace period 供 client 取得 terminal status，再關閉 AP。
- 最終 mode 為 AP + STA 時，若 AP 設定未變則沿用 active AP；若 AP SSID、密碼開關或 IP 設定有變，必須等 STA 驗證與完整設定持久化成功後才最後套用。此步驟允許既有 AP client 因新 SSID、credential、IP 或 channel migration 而短暫斷線並重新連線。
- 最後套用新 AP 失敗時，不得留下只有部分新 Wi-Fi 設定生效的持久化狀態；firmware 必須只 rollback 舊 Wi-Fi 欄位、保留同期完成的其他設定更新，並盡力恢復原 management AP。

### Wi-Fi scan

- scan result 至少提供 `ssid`、`rssi`、`channel`、`encryption_type`、`encryption` 與 `hidden`。
- REST API 只回傳 RSSI 大於 `-75 dBm` 的網路，並從中最多回 RSSI 最強的 20 筆掃描結果；前端仍可做顯示篩選。
- 同一時間只允許一個掃描工作；scan 與 connect 不同時操作 radio，每次掃描至少間隔 10 秒。
- 持久化 STA 尚在連線時 scan 回可重試 busy，不得將 driver 的 connecting conflict 誤報為一般 scan failure；STA application timeout 必須同時停止底層連線，使 fallback AP 上的後續 scan 可用。

## 進階網路行為

- 第一批 IPv4 支援 STA DHCP/static address、gateway、netmask、最多兩筆 DNS，以及 AP default/static IP/netmask。
- AP `default` 使用 firmware 預設 IP/netmask；AP `static` 使用管理者指定值。AP 自己不是 DHCP client，連入 AP 的 client 自動配址則由 SoftAP DHCP server 負責。
- AP 與 STA 可能同時啟用時，subnet 不可重疊。
- 已知 static subnet 衝突必須在持久化前拒絕。
- STA DHCP runtime 衝突時拒絕 STA 套用並保留 AP，不自動變更 AP subnet。
- 未支援的 AP channel、自訂 DHCP range、hidden SSID 等能力不得宣稱已套用。
- `POST /api/wifi/reconnect` 用於重新套用目前持久化設定。
- Wi-Fi 設定更新必須先完成完整 request、型別、欄位與跨欄位驗證；全部通過後才原子性持久化並套用，不允許 partial success。
- 一般 Wi-Fi mode、SSID、IP 與 fallback 變更在 response 後立即套用 runtime，不要求整機重啟；AP password enabled/disabled 改變 AP 認證邊界，持久化成功與 response 完成後必須立即重啟裝置。AP→STA／AP + STA safe transition 仍先完成 candidate 驗證，再由既有 final apply 套用 AP 認證，不得以提前 reboot 破壞 management AP。
- STA 後續斷線時必須更新 runtime state；fallback 開啟時啟用 AP，重新連線後依 configured mode 決定關閉或保留 AP。

## System 行為

- hostname 是 system resource，不屬於 Wi-Fi resource。
- hostname 公開讀取由 device API 提供，修改使用受保護的 system API。
- Wi-Fi TX power configured 值由 device API 公開讀取，修改使用相同的受保護 system API；省略欄位保持原值，`null` 不代表解除上限且必須拒絕。
- hostname 更新 response 完成後，立即重新啟動相關網路服務與 mDNS。
- 只修改 Wi-Fi TX power 時不重新連線、不切換 mode、不重啟網路服務或 MCU；radio 為 Off 或 STA transaction 佔用中時保留設定並延後套用。即時套用失敗最多重試三次，configured 值仍保留，下一次 active mode 或後續更新再套用。
- factory reset 需要有效管理 session。
- reset 清除使用者設定並完成 response 後立即重新啟動；重啟後回到預設 AP setup，不得因缺少 STA credential 而失聯。
- 完整 factory reset 清除整個 `user_nvs` 並格式化 `userdata`；settings reset 只清除整個 `user_nvs`，data reset 只格式化 `userdata`。三種 reset 都在 response 完成後重啟，且只由使用者明確選擇的 scope 執行破壞性操作。
- 已排程的 reset/restart 優先於 Wi-Fi apply、candidate commit 與 rollback，不能因同時存在的網路 action 延後第一次可執行的重啟時機。
- 不 migration 舊 config blob、舊 schema 或一般舊 key layout；同 schema slot 只允許缺少後加入的 `wifi_tx_dbm`，此時在 RAM 使用 15 且不因 boot 補寫 flash。新 key 已存在但型別或範圍錯誤仍視為損壞。遇到不支援或損壞的 current slot 時，boot 使用 recovery defaults 保持 AP 可達，且不自動覆寫該資料。Boot log、heartbeat 與 device API 必須以不含敏感資料的狀態區分 persisted config、首次建立 factory defaults 與 recovery defaults。無產品用途的 boot counter 不持久化，避免每次開機寫 flash。

## E-paper 行為與安全邊界

- 目標面板固定為 Waveshare 7.3inch e-Paper HAT (E)，800×480、每 pixel 4-bit packed code；第一版只接受總長 192,040 bytes 的 `EPDIMG`，不接受裸 frame、PNG、JPEG 或 BMP。
- 固定圖片名稱為 `epaper-current.epd`，保存於 `userdata:/files/`。成功 upload 以 atomic replace 更新並自動排程 draw；中止、格式錯誤或儲存失敗必須保留舊圖。Generic file PUT／DELETE 不得修改此 reserved file。
- `white` 動態輸出 192,000 bytes `0x11`；`palette` 動態輸出 4-pixel 黑框與 black／white／yellow／red／blue／green直條。兩者不建立檔案，`refresh` 只重畫最近一次有效 upload。
- 六色顯示色準固定依 EPD code `0,1,2,3,5,6`（黑、白、黃、紅、藍、綠）保存。面板測試頁可即時預覽並一次儲存完整六色 RGB；未按儲存的草稿不得寫入 flash。已儲存值會成為抖色、色距、固定色票與 Result 預覽的共同來源，但 EPDIMG 協定色碼不變。
- 色準 GET／PUT／reset 與其他 e-paper endpoint 一樣公開，不要求管理員登入。色準保存在 `user_nvs`；settings／完整 reset 會清除，data reset 保留。遇到未知持久化 schema 時只提供 recovery defaults，且必須明確 reset 後才能寫入新值。
- 對外狀態固定為 `idle`、`uploading`、`queued`、`drawing`、`cooldown`、`unavailable`；client 只依 `can_upload`、`can_draw` 與 `retry_after_seconds` 判斷，不解析 message。
- 每次實體 draw 在 panel wake 前必須把 CPU 切到並 read-back 確認 80 MHz，直到 Power OFF／Deep Sleep cleanup 完成後才恢復 160 MHz；不得以 160 MHz fallback，也不得關閉 brownout detector。
- 每次 draw 後只有 Power OFF `0x02`/`0x00`、BUSY wait、Deep Sleep `0x07`/`0xA5` 與 persistent marker read-back 全部成功，才開始完整 180 秒 cooldown。倒數使用 monotonic wrap-safe 時差，秒數向上取整，任何圖片或 action 都沒有例外。
- Cooldown 到期後仍須成功清除並 read-back protection marker才回 `idle`；marker 操作失敗時繼續禁止 draw。
- RST inactive-high、CS high、DC low 只代表 `logical_quiesce`，不能宣稱面板已 Power OFF／Deep Sleep。Waveshare HAT 的 RST low 會控制板上 power switch，因此只有 initialize 的短 reset pulse 可以拉低，閒置與 prewake 不得長時間保持 low。若 panel 已 wake 但 protocol shutdown 失敗，狀態固定為 `unavailable`／`panel_state: unknown`，要求 MCU 與 HAT 一起完整斷電，不得靠等待或 software restart 解鎖。
- Boot 不自動重畫。發現 `shutdown_confirmed` marker 時從本次 boot 重新保守等待 180 秒；發現 `active` marker 時，只有 reset reason 明確為 power-on，且 MCU 與 HAT 依規定共用同一個 3.3 V 電源，才將它視為完整共同斷電並以 read-back 驗證清除。Brownout、watchdog、panic、software reset、燒錄後 reset 或其他非協調 reset 一律記錄 interrupted 並 fail closed。
- 所有可控制的 software restart 必須先拒絕新 draw、quiesce worker，並在需要時完成 Power OFF／Deep Sleep；shutdown 失敗時取消 restart。不可攔截 reset 的 residual risk 由 marker 在下次 boot 診斷，不能宣稱已由軟體消除。
- Draw 在低優先序 dedicated worker 執行，BUSY wait 必須 yield，frame 每 4 KiB yield；Wi-Fi、HTTP、console、heartbeat 與 runtime scheduler 在刷新期間仍須可排程。CPU 降頻造成的 latency 與供電穩定性需以實板驗證。
- 全部 e-paper 專用 endpoint 與 `GET /api/runtime/status` 是明確 public exception；同網路 client 可上傳、下載及觸發 draw 是已接受的可信任網路風險，180 秒 cooldown 不是 authentication 或 abuse protection。Generic user-file API 權限不變。

## Serial console 行為

- Serial console 同時提供人類可讀 CLI 與 `api METHOD PATH [token=<token>] [json]` adapter。
- `status`、`device`、`wifi`、`scan`、`storage`、`ls`、`stat` 等 CLI 提供人類可讀診斷；`ls`／`stat` 只檢視 `userdata`，與檔案 API 共用 operation gate，且不顯示 internal temporary file。
- `config` CLI 只在 RAM 建立 typed staging overlay；`show`／`changes` 不回顯秘密值，`commit <group> token=<token>` 必須經 `ApiRouter` 使用既有 Wi-Fi、system 或 auth business rules，成功後才清除該 group 的 staging。
- 直接驗證 REST contract 的 serial 操作使用 `api ...`，共用 REST envelope 與 business logic。File list 與 delete 可使用 serial adapter；raw file upload／download 只支援 HTTP，serial 呼叫明確回 `unsupported_transport`。
- 不建立 `wifi set ...` 之類的第二套設定命令。
- 任何 serial output 都不得輸出 admin password、Wi-Fi password 或 token。

## 可替換與地端運作

- Firmware source 不手寫 HTML；產品前端在 `user-web-project/` 開發，`user-web/` 只保存其 generated import，內建管理頁則在 `builtin-web/`。預設 `make build` 與明確的 `make build WEB=user` 都先重建 user frontend、完整替換 `user-web/`，再把 gzip 靜態資源編譯進 `app0` 與韌體一起燒錄。
- 預設 `WEB=user`；明確 `WEB=builtin` 使用內建管理頁，`WEB=auto` 只在已有有效 `user-web/` import 時選 user，否則使用 builtin，且不主動重建 user frontend。只有明確指定 `WEB=none` 才建立不含任何前端的 firmware，所有 REST／serial API 仍可使用，但 `/` 與 `/index.html` 回 404 `not_found`／`frontend not bundled`。
- 替換頁只需遵守 REST API contract，不得依賴 firmware 內部 class。
- Builtin／user build 的 AP IP 上必須可使用設定頁與 REST API；`WEB=none` 的 AP IP 只提供 REST API。寫入 API 仍受 session 保護。
- 裝置及內建頁面不得依賴 CDN、雲端帳號或外部網際網路才能完成設定。
- 使用者檔案固定使用獨立的 `userdata` partition。一般韌體／內建網頁 release 不包含此 partition 的 image，也不得覆寫其位址範圍。
- Wi-Fi、Hostname、管理員等使用者設定固定寫入尾端 32 KiB `user_nvs`。前端預設 `nvs` 仍由 ESP32 系統／PHY 使用，但本專案另以 `storage_meta` namespace 保存 `user_nvs_init`、`userdata_init` 與 `reset_pending`；reset API 不得 erase 預設 `nvs` partition。一般 release 不包含 `user_nvs` image，也不得覆寫其位址範圍。
- 專案仍在早期開發，這次 partition cutover 不維護舊 layout、版本或檔案遷移。預設 `nvs/storage_meta` 中各 partition 的具名初始化旗標不存在或為 `false` 時，boot 分別重建 `user_nvs` 或格式化 `userdata`；旗標為 `true` 後若開啟或掛載失敗則保留內容，不自動 erase／format。`user_nvs` 與 `userdata` 不保存、讀取或重建彼此的初始化狀態。
- User storage 的 list、read、write、delete 與 console inspection 全部由同一個 owner 序列化；啟動時清除失敗操作留下的 internal temporary file。一般 release 不建立或燒錄 user-data image。

## Security threat model

目前預設行為定位為受控實驗室、開發與可信任本機網路上的 setup foundation，不是可直接部署於不受信任環境的 hardened product。Open factory AP、公開預設管理密碼、純 HTTP、plaintext NVS credential 與 browser `localStorage` token 都是已知限制；能接近裝置無線範圍、同一 LAN、瀏覽器 profile 或實體 flash 的攻擊者不在目前防護保證內。

若產品要離開受控環境，發布前必須另行決定並驗證 login throttling／temporary lockout、首次設定或 physical-presence flow、credential storage、HTTPS 或其他 transport protection、Secure Boot、Flash Encryption 與 NVS Encryption；不能只沿用本 foundation 的預設值並宣稱為 production-secure。

## 不在目前範圍

- 內建電子紙內容管理 UI 與 MCU 端 PNG／JPEG／BMP decode、resize、quantize、dithering pipeline
- OTA
- 雲端帳號或遠端管理
- 多使用者或角色權限

## 待決需求

這些問題只有在開始實作對應功能時才需決議；一次最多確認 5 題。

- Static IP 套用前是否需要 connectivity test 或 rollback？
- AP DHCP server 是否允許關閉？
- AP channel 是否需要 country code／地區限制？
- 是否需要把 Wi-Fi foundation 的 public C++ module API 文件化？
