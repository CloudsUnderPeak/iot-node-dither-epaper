# Technical Specification

本文件定義 `builtin-web/` 以外的 firmware 程式組織與工程約束。產品可觀察行為以 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md) 為準，對外 API contract 以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準。

## 程式進入點與執行流程

- `main.cpp` 是唯一 composition root，直接擁有具體 service、Arduino `setup()`／`loop()`、boot order 與每個 tick 的 poll order。
- 只供 `main.cpp` 使用的 service instances、subsystem callbacks 與 log helper 放 anonymous namespace，不另外建立只有一個使用者的 `FirmwareApp` wrapper。啟動順序、startup readiness 與 heartbeat health 由同一份 fixed-size subsystem registry 驅動，不另外維護平行 `xxxReady` flags。
- Poll 順序固定為 runtime command、Wi-Fi state、網路服務同步、captive DNS、console、heartbeat。
- 不建立 service locator 或通用 application framework。Interface 只用在具有故障注入或 host test 需求的硬體／持久化邊界，不為一般單一實作額外建立抽象層。

```text
HTTP ──┐
       ├─> ApiRouter ─> URL-matched Endpoints ─> ConfigService / AuthService
Serial ┘                          │
                                 └─> RuntimeActionScheduler coalesced actions
                                                   │
Arduino loop <─────────────────────────────────────┘
     ├─> WifiManager state machine
     ├─> mDNS / captive DNS
     └─> console / heartbeat
```

## 模組責任

| 模組 | 責任 |
| --- | --- |
| `src/main.cpp` | Service 組裝、boot、主迴圈順序與服務健康 log；有序 subsystem descriptor 同時驅動 start callback、readiness、detail reporter 與 heartbeat，避免平行旗標及單一長 format string／argument list，不實作 feature business rule。 |
| `core/SubsystemRegistry.h` | Fixed-size、無 heap 的 subsystem lifecycle descriptor；start 結果與 optional runtime health callback 共同決定 READY／FAIL。 |
| `core/SemaphoreGuard.h` | FreeRTOS semaphore 的窄 RAII guard；優先用於具有多個 early return 的短生命週期 lock。 |
| `api/ApiRouter.*` | 透過 `ApiRouterDeps` 一次接收必備 reference；單一 static route catalogue 列出 method、完整 URL／parameter pattern、matcher、HTTP binding、direct handler 與明確 public exception，未標註 access 的 route 結構上預設需要授權。 |
| `api/shared/*` | Transport-neutral types、response/JSON serialization 與 JSON reader。 |
| `api/alive/*` | `GET /api/alive`。 |
| `api/device/*` | `GET /api/device`。 |
| `api/web/*` | `GET /api/web`；把 build-time Web identity 映射為公開 response。 |
| `api/storage/*` | `GET /api/storage` 與 user-file list／delete response mapping。 |
| `api/auth/*` | `/api/auth` 與 `/api/auth/*` endpoints。 |
| `api/system/*` | `/api/system` 與 `/api/system/*` endpoints。 |
| `api/wifi/*` | `/api/wifi`、`/api/wifi/*` endpoints 與 typed payload mapping。 |
| `modules/runtime/RuntimeActionScheduler.*` | 接收跨 task command，由 loop 唯一執行 Wi-Fi apply 或 restart。 |
| `modules/config/ConfigService.*` | active config 的唯一 owner；提供同步 snapshot、完整 commit 與型別化原子欄位群組更新。 |
| `modules/config/model/*` | Config type、factory defaults、scalar/cross-field validation 與 IPv4 計算。 |
| `modules/config/storage/*` | NVS schema、雙 slot 儲存、read-back verification 與 active slot commit。 |
| `modules/wifi/WifiManager.*` | non-blocking STA/AP runtime state machine、單一 RAM STA connection transaction 與同步 status snapshot。 |
| `modules/wifi/WifiScanner.*` | scan busy/cooldown、RSSI 門檻、排序、top 20 與 encryption mapping 的唯一 owner。 |
| `modules/wifi/WifiRadio.*` | HTTP/loop task 對 Arduino Wi-Fi driver 的共用 mutex。 |
| `modules/http/*` | HTTP transport、static assets 與 response send adapter。 |
| `modules/storage/EmbeddedWebAssets.*` | 查找 release build 編譯進 app image 的 raw／gzip 前端資產，並公開 build-time Web source 與輸出樹 SHA-256；也接受 `none`／null hash 的合法空資產狀態。 |
| `modules/storage/FlashStorage.*` | 讀取實際 flash partition table、running app image 大小與 app 剩餘容量。 |
| `modules/storage/UserDataStorage.*` | 獨立 user-data LittleFS、stable capacity、single-operation gate，以及 upload／download／list／delete／inspection sessions。 |
| `modules/storage/UserDataPath.*` | Console userdata path normalization；不接受 parent traversal、shell／glob 字元或截斷。 |
| `modules/storage/UserFilePolicy.*` | User-file 公開檔名、MIME、strict size、single Range 與 opaque cursor 的 pure policy。 |
| `modules/storage/StorageLifecycle.*` | 預設 NVS 中的獨立 partition 初始化旗標、reset intent 與 early-boot erase／format 協調。 |
| `modules/storage/StorageCapacity.h` | User upload quota 的 value／保留量／對齊計算。 |
| `modules/console/*` | Human diagnostics、userdata inspection、typed config staging／commit 與 REST-equivalent `api ...` serial adapter。 |

## API 與 transport 邊界

- HTTP 與 serial 對相同 method/path/body 共用 `ApiRouter`，不得各自實作 business rule。
- `ApiRouter.cpp` 是 URL 到 endpoint 的唯一可讀入口。Static route catalogue 同時包含 exact route 與受限的 `/api/storage/files/{name}` matcher，使用 fixed function pointer／captureless handler，不建立 Action enum → switch 的第二段 business 轉送。
- Route descriptor 的 access 預設為 protected；只有 behavior/API reference 明列的公開 route 可顯式標註 `Public`。Dispatcher 必須先完成 method/path match 與統一授權，再呼叫 handler，新增 route 不得依賴逐 branch 手寫授權。
- `HttpBinding` 明確區分 no-body、JSON body、query、raw upload 與 raw download；`ApiServer` 遍歷 catalogue，以 exact／directory matcher及 binding 選擇 transport callback，不維護第二份 API path registration。Raw streaming session 仍由 HTTP adapter 執行，Router preparation 共用 catalogue 的 path match 與 access policy；serial raw GET／PUT 維持 `unsupported_transport`。Unknown method/path 固定回 `404`；不改成全域 `401`，以保留 not-found contract。
- Route metadata 必須可由 native authorization matrix 遍歷，逐條驗證 method、path pattern、matcher、HTTP binding、public/protected、missing/invalid/valid token 與 handler status；source contract 只補充確認 HTTP adapter 確實遍歷 metadata，且未重新手寫 API route registration。
- HTTP streaming upload／download 在 `ApiRouter` 共用同一個 request preparation：依序驗證 token、解析單層 public filename，再進入 storage session；storage failure 統一交由 `UserFileEndpoints::fromStorageResult()` 映射，不在兩條流程複製 status contract。
- Resource API 可同步持久化設定，但 Wi-Fi、mDNS、captive DNS 與 restart side effect 只能送入 runtime scheduler。
- 需要 runtime side effect 的 persisted update 必須先確認 scheduler ready，再 commit，最後排程；scheduler 在 ready 後不得拒絕 action，避免已持久化卻回傳完整失敗。
- `RuntimeActionScheduler` 對 Wi-Fi apply 與 system restart 都採 earliest-deadline coalescing；`poll()` 先執行到期 restart，再處理 candidate rollback／commit 與一般 Wi-Fi apply，避免後來的 network action 延後既有 reset。
- HTTP adapter 只解析 transport 資料並送 response；Console 不依賴 HTTP helper。
- Raw file body 是 transport-specific streaming 例外：HTTP adapter 將 bytes 以 bounded chunk 交給 Router session API；serial adapter 只支援 JSON file list／delete，raw GET／PUT 回 `415 unsupported_transport`。
- REST response 使用結構化 JSON；若必須手工產生 JSON 字串，只能經過 `ApiResponse` 的完整 escape。
- 本專案不維護 API 版本或舊 payload 相容層。Contract 改動直接更新現行 resource、API reference、native test 與同一版本的 client。

### 新增 API resource

1. 先確認 behavior，並在 `SPEC_API_REFERENCE.md` 定義 method、path、auth、payload 與錯誤。
2. 在 `ApiRouter.cpp` 的 static catalogue 加入 method、完整 URL／parameter pattern、matcher、`HttpBinding` 與 direct handler；只有公開例外才顯式標註 `Public`，其他 route 使用預設 protected。
3. 依 `/api/<segment>` 在同名目錄的 `*Endpoints` decode 並建立 response；共用 domain 行為放 service。
4. Runtime side effect 排入 scheduler，不在 HTTP callback 或 resource handler 直接執行。
5. 一般 API 不修改 `ApiServer` 或 serial adapter；只有新增 catalogue 尚未支援的新 transport binding 時才擴充 adapter。
6. 執行 native test、firmware build；有 runtime/hardware 變更時再做實板驗證。

## Storage resource

- `/api/storage` 回傳 `flash`、`app`、`user`；endpoint 擁有對外 JSON shape。Flash snapshot 列出 bootloader／partition table 固定區域及實際 partition offset／size，app snapshot 使用 running partition 與 `ESP.getSketchSize()`，user capacity 來自同一次 filesystem snapshot。
- `app0` 固定為 `0x10000`–`0x200000`，大小 1984 KiB。Builtin／user 的 `build/latest/web/` raw 或 gzip 前端由 release tool 產生 C++ byte table並編譯進 firmware；同一 generated header 保存經 manifest 驗證的 source 與實際輸出樹 `web_sha256`，供 `/api/web` 直接讀取，不在裝置上重新掃描或 hash 資產。`WEB=none` 產生 count／payload 為零的標準 sentinel table、source `none` 與 null runtime hash，`EmbeddedWebAssets::begin()` 必須將此合法空狀態視為 ready。Release 不建立獨立 frontend filesystem image。`app.capacity.frontend_bundled` 與 `frontend_payload_bytes` 分別回報是否包入前端及其 payload；`firmware_image_bytes` 已包含前端 payload，`available_bytes` 是 app partition 尚可供整包 firmware build 使用的位元組數。
- `userdata` physical partition 固定為 `0x200000`–`0x3e8000`，大小 1952 KiB，使用 LittleFS-compatible `spiffs` subtype。對外 quota 從 raw total/free 各扣除 64 KiB reserve，再向下對齊 4 KiB allocation unit；`available_bytes` 與 `max_upload_bytes` 使用同一值，表示當下可接受的單一新檔 payload 上限。
- `user_nvs` 固定為 `0x3e8000`–`0x3f0000`，大小 32 KiB。`ArduinoPreferencesBackend` 將 `PreferencesConfigStore` 的 `devcfg_a`、`devcfg_b`、`devcfg_meta` namespace 都明確指定此 partition；前端 20 KiB 預設 `nvs` 除 ESP32 系統與 PHY 資料外，另以 `storage_meta` namespace 保存本專案的三個 storage lifecycle key，reset API 不得 erase 該 partition。
- `UserDataStorage` 是所有 user volume handle 的唯一 owner。Binary semaphore 形成 HTTP upload／download／list／delete 與 console `ls`／`stat` 共用的 non-blocking single-operation gate；busy 立即返回，不跨 callback 持有 mutex，也不允許第二個 writer 或 reader 同時打開 filesystem handle。
- 公開檔案固定在 `/files/<name>`，internal upload path 固定為 `/files/.upload.tmp`，boot mount 時清除 stale temp，list／inspection 永遠隱藏它。Upload 在取得 gate 後用新鮮 raw capacity 計算 preflight limit，最多 4 KiB 一次寫入；完整 byte count 後 flush／close、重新 open 驗證 size，再直接 rename temp over target。Target 不先刪除；rename 失敗保留原 target 並移除 temp。
- Download session 在 begin 時解析單一 byte range、open／seek 並保存 remaining bytes；response filler 每次最多讀 4 KiB，完成或 disconnect 都關閉 session。Empty file 在建立 response 前先釋放 gate。List 使用 opaque `v1:<offset>` cursor；collection 改變時 cursor 不保證 snapshot consistency。
- `uploadCapacity()` 只回最後一次完整操作後的 stable snapshot。Active operation 不發布 temporary write 中間值；upload commit／abort 與 delete 完成後才更新 snapshot。
- `StorageLifecycle` 使用 `storage_meta/user_nvs_init` 與 `storage_meta/userdata_init` 兩個獨立 bool，不使用 bitmask 或 layout version；任一旗標不存在／為 `false` 時，只初始化對應 partition。旗標為 `true` 後，NVS open 或 LittleFS mount failure 不得自動清除內容。兩個 user partition 不保存或推導彼此的 metadata。
- `storage_meta/reset_pending` 是字串 `none`、`settings`、`data` 或 `all`。Reset endpoint 先 read-back 驗證 pending intent，再排程 restart；early boot 在 Config、Wi-Fi 與 HTTP 前執行指定的 erase／format，逐一把成功完成的初始化旗標設為 `true`，最後才把 pending 改回 `none`。中途斷電時下次 boot 重試同一 scope。
- 完整 factory reset erase 整個 `user_nvs` 並格式化 `userdata`；settings reset 只 erase `user_nvs`，data reset 只格式化 `userdata`。一般 release 只寫 bootloader、partition table、boot_app0 與 app image；app image 可依明確的 `WEB=none` 選擇不含前端。Release manifest 必須描述 `userdata` 與 `user_nvs`，並驗證所有 image 都不與兩者的位址範圍重疊。

## Config persistence

- `ConfigService` 是唯一 active config owner；其他模組只保存 service pointer 並取得 value snapshot。
- Production 由 `main.cpp` 將 `ArduinoPreferencesBackend` 注入 `PreferencesConfigStore`，再將 store 注入 `ConfigService`；兩個 interface 都是持久化故障注入與 host test 的窄邊界，不承載 business rule。雙 slot、schema/read-back 與 active marker commit 演算法仍由 `PreferencesConfigStore` 唯一實作。
- 一般 endpoint 只能使用 `updateWifi()`、`updateHostname()` 或 `updateAdminPassword()`：每個方法在同一個 service mutex 內從最新 active config 合併指定欄位群組、驗證、持久化並發布。完整 `commit()` 只供 boot/self-test 等明確的完整替換流程。
- NVS 不保存整個 C++ struct/blob，也沒有 `kConfigBlobKey`。
- Current schema 使用 `devcfg_a`、`devcfg_b` 兩個 per-key slot，以及 `devcfg_meta/active` marker。
- Save 寫入 inactive slot，逐欄完成後最後寫 schema marker，再 read-back 驗證，最後切換 active marker；失敗時 active config 不更新。
- Boot 先讀 active slot，再嘗試另一個有效 slot。空 NVS 才建立 factory defaults。
- 本專案不 migration 舊 blob、舊 schema 或舊 key layout；不支援的資料不在 boot 自動覆寫，runtime 以 recovery defaults 保持 AP 可達。
- `ConfigService` 保存 boot load outcome；boot log、heartbeat 與 device API 公開 non-sensitive `config_state`／recovery reason，不能把 recovery defaults 誤報為正常 persisted load。
- Reset endpoint 先確認 runtime scheduler ready，再由 `StorageLifecycle` 持久化 reset scope，最後排程 guaranteed restart；設定清除由下一次 early boot erase 整個 `user_nvs` 完成，不逐一維護 namespace 清單。
- 一般 `PUT /api/wifi` 在 commit 後排程 Wi-Fi apply；若 `apPasswordEnabled` 相對 active config 改變，則改排程 system restart。AP password 已啟用時的 admin password update 同樣排程 system restart；未啟用時只撤銷 session。AP→STA／AP + STA candidate flow 不提前 reboot，仍由 `WifiManager` final apply 處理 AP credential。
- 新增 persisted field 時：加入 type/default/validation、加入 slot read/write/equality、必要時 bump current schema、補 native/build/board test；不建立 previous-version migration。
- `ConfigStaging` 使用 fixed-size `DeviceConfig` overlay 與 dirty bitset，不使用 heap-backed map；每次 show／commit 都把 dirty values 疊到最新的 `ConfigService::snapshot()`，因此不會把未 staged 的舊 snapshot 寫回。Set 只接受 key registry 定義的 JSON type，commit 以 `wifi`、`system`、`auth` group 經 `ApiRouter` 送往既有 endpoint。
- Secret key 只可 set，不可 show/get；changes 只顯示 `<updated>`。覆寫、revert、clear／destructor 與暫時組出的 token buffer都明確清零；成功 commit 才清除該 group，失敗保留 staging 供修正或重試。

## State ownership 與同步

| 狀態 | Owner / 同步方式 |
| --- | --- |
| Active `DeviceConfig` | `ConfigService` mutex；snapshot copy、完整 commit 與鎖內 field-group read-modify-write 全程序列化。 |
| Auth token | `AuthService` mutex；登入、驗證、撤銷不可競爭。 |
| Runtime actions | `RuntimeActionScheduler` critical section；同類 action 合併，HTTP/serial producer，loop consumer。 |
| `WifiStatus` | `WifiManager` critical section；讀者取得完整 snapshot。 |
| Wi-Fi scan/cooldown | `WifiScanner` mutex；scan scope 使用 `SemaphoreGuard` 確保所有 early return 釋放。 |
| Arduino Wi-Fi driver | `WifiRadio` mutex；scan 與 state-machine driver call 不同時執行。`WifiManager` 只依賴可注入的 `WifiDriver`／`MonotonicClock`，production adapter 才接觸 Arduino `WiFi`／`millis()`。 |
| User-data filesystem | `UserDataStorage` binary semaphore；所有 file／inspection operation non-blocking serialize，active session 由 session id 驗證。 |
| Console staged config | `ConsoleConfigCommand`／`ConfigStaging`，只在 console runtime RAM；dirty bitset 與 fixed-size secret buffers。 |

Critical section 內不得執行 NVS、JSON、Wi-Fi、Serial 或其他長操作。同步 scan 可能等待 Wi-Fi driver，但 STA connect/apply 本身不得用等待連線的 loop。

## Wi-Fi runtime

- `WifiManager::apply()` 只設定 mode/IP、啟動 AP/STA 並發布 `connecting`，不等待 STA connected。
- `WifiManager` 透過注入的 `WifiDriver` 與 monotonic clock 執行狀態機；Arduino adapter 負責 framework API mapping，`WifiRadio` 仍負責跨 task driver serialization。持久化與 candidate STA 都必須在 association 且取得非 `0.0.0.0` IPv4 後才發布 connected。
- `WifiManager` 的 STA connect 或 AP→STA/AP+STA full-replacement request 只在 API task 複製 validated candidate、原 persisted snapshot 與排入狀態；Wi-Fi driver 操作、timeout、成功／失敗、final apply 與 persisted runtime restore 都由 loop 的 `poll()` 執行。
- Candidate 與 rollback snapshot 只存 RAM。連線時沿用目前 active AP，不呼叫會先關閉 radio 的完整 `apply()`；認證與 IPv4 成功後由 `RuntimeActionScheduler` 自動持久化，不提供 client commit endpoint。
- Connection state 以 mutex 保護 candidate 與內部 transaction id；mutex／critical section 內不得呼叫 NVS、Wi-Fi driver、JSON 或 Serial。Terminal failure 清除 candidate credential，公開狀態只映射為 `idle`、`connecting`、`connected`、`failed`。
- `WifiManager` 啟動 SoftAP 時，DHCP 必須把 AP IP 發布為 DNS server，並在 framework 支援時發布 DHCP captive portal URI；URI 發布失敗不得關閉仍可由 AP IP 直接管理的 AP。
- Candidate `poll()` 使用單一 association 與 15 秒 deadline；三個 5 秒觀察區間不重啟 driver connect。它同時處理目前／最終 AP subnet overlap、commit 後的 5 秒 STA grace、AP+STA 最後 AP reconfigure、NVS rollback 與舊 AP runtime restore。Application timeout 必須先停止底層 STA connect，再發布 `failed`，避免 ESP-IDF 持續以 connecting state 拒絕 scan。
- `RuntimeActionScheduler` 在 candidate verified 後以 `ConfigService::updateWifi()` 把 candidate 的 Wi-Fi 欄位合併到最新 active config；final AP apply 失敗時也只以同一入口回復 rollback snapshot 的 Wi-Fi 欄位。Candidate transaction active 時，一般 coalesced Wi-Fi apply 必須延後，不能用完整 `apply()` 中斷驗證。
- 狀態改變由 `main.cpp` 的 loop 統一 restart mDNS/captive DNS；mDNS 只在 STA connected，captive DNS 只在 AP active。
- HTTP captive fallback 必須以 TCP connection 的 local IP 判斷 request 是否由 AP 介面抵達，不得只因 runtime 有 AP IP 就攔截 STA LAN request。
- API 必須區分 persisted `configured_mode` 與目前 runtime `mode`。
- `WifiScanner` 是唯一 `WiFi.scanNetworks()` 呼叫點；API 與 human console 共用 RSSI 大於 `-75 dBm` 的門檻、10 秒 cooldown 與最多 20 筆結果。持久化 STA application state 為 `connecting` 時由 `WifiManager` preflight scan 並回可重試 busy，不呼叫 driver scan。

## 測試、logging 與嵌入式限制

- Native test 唯一入口為 `tools/native-test/test_native.sh`；目前測試 config／IPv4 validation、ConfigService 原子欄位群組更新與故障隔離、PreferencesConfigStore slot／marker fault injection、AuthService session lifecycle／concurrency、WifiManager driver/clock seam 與 timeout／IPv4／disconnect／subnet overlap／commit-finalize-rollback failure 狀態、Wi-Fi payload、response codec、HTTP JSON transport parsing、user filename／MIME／Range／cursor／inspection path、console config staging、subsystem registry start order/readiness/dynamic health、ApiRouter catalogue metadata 與 exact/dynamic runtime authorization matrix、Web identity response（包含 none／null）、generated metadata、空前端 artifact protection、web preview/minifier/deterministic gzip、release partition/manifest/path/hash protection，以及 metadata-driven HTTP registration source contract。Source contract lint 不代表 HTTP adapter 或 filesystem integration coverage。
- Firmware runtime 變更至少對既有 verified production `build/latest/web/` 執行 `make esp`；需同時重建 frontend 時執行 `make build`，API-only firmware 使用 `make build WEB=none`。
- `builtin-web/` 是專案預設 frontend source，`user-web/` 接受使用者 prebuilt static output；`tools/web-build/` 也可明確產生 `WEB=none` 的已驗證空 production web。`tools/release-build/` 只消費 production web 並管理 generated header、binary、manifest、`firmware.img` 與快照。
- `VERSION` 保存純 `MAJOR.MINOR.PATCH`，目前為 `0.8.0`；顯示與 tag 使用 `v0.8.0`。Firmware 與 web manifest version 必須一致。本機 dirty build 允許且 manifest 記錄 `git_dirty: true`；official CI release 必須拒絕 dirty worktree。
- 每次完整 firmware build 先在 private staging 完成編譯、hash、package 與驗證，再建立 Asia/Taipei `YYYYMMDD_HHMM` 快照；同分鐘碰撞依序使用 `_02`、`_03`。`build/latest/` 是內容相同的實體副本，不使用 symlink。
- 完整 snapshot 固定含 root `web-manifest.json`、`web/`、`binary/` 與 `firmware.img`。`binary/` 含 `manifest.json`、`bootloader.bin`、`partitions.bin`、`boot_app0.bin`、`firmware.bin`；manifest 記錄 board/chip/flash/partition、image size/SHA-256、web selection/process/hash、version、Git commit/dirty。使用 user frontend 時另記錄 user source hash；builtin／none 不記錄 source hash。`WEB=none` 保留空的 `web/` 以維持固定 artifact shape，manifest 記錄 `web:none`、`web_process:none`、`frontend_delivery:none`、零檔案與零 payload，並以空樹 hash 做 artifact integrity 驗證。
- 專案持久產物的壓縮技術統一限定為 gzip：frontend precompression 使用 `.gz`，`firmware.img` 使用 deterministic tar.gz；TAR 只負責收集多個具名檔案，不是另一種壓縮。不得再新增 ZIP archive、Brotli、zstd、xz、bzip2 或額外外層壓縮包。Esptool `-z` 只屬於燒錄期間的 transient transport optimization，不產生或改變持久 artifact，因此不計入 release 壓縮格式。
- `firmware.img` 是使用自訂副檔名的 deterministic tar.gz。USTAR root 只能依序含 `manifest.json` 與四個 bin，所有 member 必須是 regular file、固定 mode／mtime／owner metadata；不得含 `binary/` 目錄、額外 outer archive、flasher、README、digital signature、`userdata` 或 `user_nvs` image。`build/latest/firmware.img` 與對應時間戳檔案必須 byte-identical。
- `make web` 或 `make demo` 完成後以 web-only `build/latest/` 取代舊 latest，避免 binary 與 web 錯配；`make esp` 必須拒絕 demo target。`make clean` 移除 latest、transient與舊版 generated output但保留時間戳快照；只有 `make clean all` 移除時間戳。Bare `make all` 必須失敗，且不建立產物。
- Flash 前必須驗證指定 snapshot 並只使用 sibling `binary/` discrete images，不得在 flash 時改用 `.pio/` 暫存 image。`make verify` 驗證 latest；`IMAGE=build/<timestamp>/firmware.img` 驗證／燒錄歷史 snapshot。把 `firmware.img` 單獨複製出去不符合 workspace-coupled full verification，必須連同 sibling web、web manifest 與 binary 保留。
- `make deploy PORT=<port>` 必須先確認呼叫者明確提供 port，再依序清除 latest、依 `WEB`／`WEB_PROCESS` 完整重建、驗證並燒錄；缺少 port 時不得先清除或建置。
- Flash/monitor 前必須重新確認 serial port；不得猜測 GPIO、USB、partition、flash 或 board-specific 設定。
- Self-test 預設關閉；臨時 firmware hook 在整合前刪除。
- Serial/API/log 不得輸出 admin password、Wi-Fi password、token、MAC 或其他本機識別資料到可重用文件。
- Arduino `String` 仍用於 framework transport、JSON response 與短生命週期 token；沒有長期 heap/fragmentation 量測證據前不做全面機械替換，優先維持現有 bounded input 與 secret clearing。
- Build、upload、monitor、browser 與 board smoke-test 的日期結果只放 ignored `tmp/verification/`。
