# Technical Specification

本文件定義 `builtin-web/` 以外的 firmware 程式組織與工程約束。產品可觀察行為以 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md) 為準，對外 API contract 以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準。

## 程式進入點與執行流程

- `main.cpp` 是唯一 composition root，直接擁有具體 service、Arduino `setup()`／`loop()`、boot order 與每個 tick 的 poll order。
- 只供 `main.cpp` 使用的 service instances、subsystem callbacks 與 log helper 放 anonymous namespace，不另外建立只有一個使用者的 `FirmwareApp` wrapper。啟動順序、startup readiness 與 heartbeat health 由同一份 fixed-size subsystem registry 驅動，不另外維護平行 `xxxReady` flags。
- Poll 順序固定為 scan owner／HTTP continuation、e-paper control admission、runtime command、Wi-Fi state、網路服務同步、captive DNS、console、heartbeat。
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
| `api/device/*` | `GET /api/device`；組合 config、晶片資訊、immutable boot diagnostics 與 cached battery snapshot，並擁有對外 JSON shape。 |
| `api/web/*` | `GET /api/web`；把 build-time Web identity 映射為公開 response。 |
| `api/storage/*` | `GET /api/storage` 與 user-file list／delete response mapping。 |
| `api/auth/*` | `/api/auth` 與 `/api/auth/*` endpoints。 |
| `api/system/*` | `/api/system` 與 `/api/system/*` endpoints。 |
| `api/wifi/*` | `/api/wifi`、`/api/wifi/*` endpoints 與 typed payload mapping。 |
| `api/epaper/*` | E-paper capability、status、image metadata/download、gzip upload 與 action response mapping；不直接操作 GPIO、SPI 或 LittleFS。 |
| `modules/runtime/RuntimeActionScheduler.*` | 接收跨 task command，由 loop 唯一執行 Wi-Fi apply；system restart 必須委派 e-paper shutdown coordinator 核准後才執行。 |
| `modules/runtime/BootDiagnostics.*` | Boot reset reason 的單一 normalized owner；startup 只呼叫一次 `esp_reset_reason()`，保存 immutable snapshot，並將 project enum 映射為穩定 API 字串。 |
| `api/runtime/RuntimeEndpoints.*` | 組合 `EpaperService` 與 `RuntimeActionScheduler` 的短 cached snapshot，回 activity／phase／CPU／blocked resources；不掃 filesystem、不形成全域 lock。 |
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
| `board/BoardProfile.h`、`board/profiles/FireBeetle2Esp32C6Profile.h` | Build-time board/chip、可用 GPIO、bus route、battery divider 與 e-paper pin map；以 `static_assert` 阻止重複、禁止或未審查腳位。 |
| `modules/hardware/PinRegistry.*` | Fixed-size owner/role claim table；exclusive GPIO 衝突 fail startup，shared bus pin 只可由 bus owner claim。 |
| `modules/hardware/SpiBus.*` | `SPIClass` lifecycle、固定 SCK/MOSI route、mutex 與 transaction ownership；device driver 不得自行 `SPI.begin/end`。 |
| `modules/power/BatteryMonitor.*`、`modules/power/ArduinoBatteryAdc.*` | GPIO0 calibrated ADC adapter、固定樣本 median、單節鋰電池粗略百分比曲線與跨 task cached snapshot；不推測電池存在或充電狀態。 |
| `modules/epaper/EpaperPanelProfile.h` | 唯一 physical model／compile-time geometry 與 draw-time mounting flip，衍生 row/frame/image/compressed-limit。 |
| `modules/epaper/EpaperGzip.*`、`EpaperGzipReader.*` | Strict single-member gzip decoder 與保留 storage session 的 sequential logical reader；與 HTTP 無關。 |
| `modules/epaper/EpaperImageFormat.*` | 純函式與 streaming validator；解析固定 40-byte `EPDIMG` header、little-endian 欄位、generation、CRC32 與六色 nibble。 |
| `modules/epaper/EpaperFrameSource.*` | File／white／palette 的無 framebuffer streaming source；動態來源依 offset 直接產生 packed bytes。 |
| `modules/epaper/EpaperCooldown.*` | 180 秒 wrap-safe monotonic gate；倒數到期仍要求 persistent marker 清除成功才釋放。 |
| `modules/epaper/Epd7In3E.*` | 7.3inch E driver command、BUSY timeout、Power OFF／Deep Sleep 與 operation watchdog；透過 shared SPI bus 傳輸。 |
| `modules/epaper/CpuFrequencyGuard.*` | RAII 全晶片 frequency guard；panel wake 前切到並 read-back 80 MHz，cleanup 後恢復並 read-back 原頻率。 |
| `modules/epaper/EpaperSafetyStore.*` | Default NVS `epaper_meta` protection marker、read-back verification 與 boot recovery 判定；故障 fail closed。 |
| `modules/epaper/EpaperShutdownCoordinator.*` | Protocol shutdown、logical quiesce 與最終 software restart 核准；由 EpaperService worker 完成 runtime drain 後委派，失敗標記 unknown 並取消 restart。 |
| `modules/epaper/EpaperPowerProbe.*` | Build-flag gated、預設停用的 power-only bring-up；在 storage／Wi-Fi 啟動前，marker 與 80 MHz guard 成功後只做 initialize、Power OFF／Deep Sleep，不傳 frame、不 refresh。 |
| `modules/epaper/EpaperRefreshProbe.*` | Build-flag gated、預設停用的單次實機刷新 bring-up；在 storage／Wi-Fi 啟動前依序執行 marker、80 MHz guard、initialize、一次 transfer/refresh、Power OFF／Deep Sleep 與頻率恢復；任何階段失敗仍執行 cleanup。若測試中掉電，下一次 boot 可恢復 active marker，但不得自動重試 probe。 |
| `modules/epaper/EpaperService.*` | 唯一 operation/status owner、upload reservation、queue depth 1、worker、cooldown 與 stable error mapping。 |
| `modules/epaper/calibration/*` | 六色色準 model/service 與 `user_nvs` 雙 slot store；EPD code identity 固定，display RGB 可持久化調整。 |
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

## E-paper hardware、format 與 runtime

- Target 是 DFRobot FireBeetle 2 ESP32-C6（ESP32-C6FH4、4 MB flash、無 PSRAM）與 Waveshare 7.3inch e-Paper HAT (E)。固定 signal mapping 為 SCK GPIO23、MOSI GPIO22、CS GPIO18、DC GPIO1、RST GPIO14、BUSY GPIO21；BUSY 是 active-low `INPUT_PULLUP`，避免 controller reset 期間輸出呈高阻時漂浮；不配置 MISO。第一版不新增 power-enable，也不修改 `partitions.csv`。
- Board profile 必須核對 exposed pin、ESP32-C6 strapping／flash／USB-JTAG 限制及板上保留功能。Boot 最早先設 CS high、DC low、RST inactive-high；這只建立 logical quiesce，不能回報 protocol shutdown。Waveshare HAT 的 RST low 同時控制板上 power switch，logical quiesce 與 2 秒 prewake 不得長時間拉低；driver initialize 保留 high 20 ms、low 2 ms、high 20 ms 的短 reset pulse。
- `main.cpp` 必須在 Serial、status LED、storage 與 network subsystem 之前啟動 `epaper_hardware`：先原子 claim e-paper pins、設定安全 latch 與方向，再初始化 write-only shared SPI、transport 和 driver 的 `Quiesced` software state。此階段不得呼叫 panel initialize、frame transfer 或 refresh；startup log 必須明確標示 `logical_quiesce` 與 `refresh=disabled`，任一步失敗則保持 `FAIL` 且不送 panel command。
- `EpaperPanelProfile.h` 的 `kModel` 與 `Active = Geometry<width, height>` 是唯一 production identity。修改 model／width／height 會同步更新 format validator、frame sources、0x61 big-endian resolution command 與 API。Compile-time 檢查非空 model、正尺寸、even width、uint16 resolution range、乘積與 uint32／size_t 邊界。
- `EPDIMG` v1 layout 不變；尺寸與 frame/image bytes 由 active geometry 推導。目前 production example 總長 192,040 bytes：magic `EPDIMG\0\0`、version 1、header size 40、width 800、height 480、frame bytes 192000、frame CRC32、non-zero uint64 generation。所有 multibyte field 為 little-endian；generation 對外以 decimal string 表達。
- Packed frame 是 row-major，左 pixel 在 high nibble、右 pixel 在 low nibble；只允許 code `0,1,2,3,5,6`。Validator 可跨任意 input chunk 邊界收資料，依序驗證 header、每個 nibble、完整長度與 CRC，不配置 192 KB framebuffer。
- Dynamic white source 對任何合法 offset 回 `0x11`；palette source 產生 4-pixel black border，內部依序為 black／white／yellow／red／blue／green vertical bars。兩者與 file source 使用相同 streaming interface。
- `EpaperCalibrationService` 是六色 display RGB 的 canonical owner。色序固定為 code `0,1,2,3,5,6`；protocol RGB 不可修改，六組 display RGB 必須互異。`epcal_a`／`epcal_b` 保存 36 個 hex 字元，`epcal_meta/active` 保存 active slot；inactive slot 先寫 colors、最後寫 schema、read-back 驗證後才切 marker。未知 active schema 不 fallback 到舊 slot、不自動覆寫，直到明確 reset。
- 色準 store 使用獨立 `ArduinoPreferencesBackend` 但同屬 `user_nvs`。空 store 只在 RAM 發布 firmware defaults，不因 boot 寫 flash；settings／完整 reset erase 整個 partition，因此自然清除色準，data reset 不影響。Service mutex 內完成 store transaction並只在成功後發布新 snapshot；API 與 editor 不得直接碰 NVS。
- Compressed stored file 是 `/files/epaper-current.epd.gz`，logical name 為 `epaper-current.epd`。`UserDataStorage` 仍是 LittleFS 與 operation gate 唯一 owner；e-paper 只能透過 internal read seam 與既有 temp/atomic rename 流程整合，不得另開 filesystem owner。Generic PUT／DELETE 在 policy layer 對 fixed name 回 `reserved_file`。
- Upload 先取得 e-paper reservation 與 storage gate，再配置 decoder，避免 busy download 造成重複 dictionary allocation。HTTP compressed index 只用於 temp write；decoder output 獨立計算 logical offset。完整 compressed byte count、gzip finish、EPDIMG finish 通過後，才 flush／close／reopen size 驗證與 atomic rename；不建立 raw file。Upload decoder 在排入 draw 前釋放。
- `EpaperGzip` 使用 vendored miniz 3.0.2 raw tinfl（MIT；`miniz/README.md` 記錄來源及本地 subset configuration），host 與 ESP32 編譯同一 decoder；project-prefixed symbol 避免誤連到 ESP32-C6 ROM 中 ABI 不相容的舊版 tinfl。不包含 compressor／archive API。32 KiB dictionary 與約 8.2 KiB inflate state 放 heap，由 unique owner 管理。每次最多產生 dictionary 剩餘區段，交付 consumer 時使用有界 I/O；不把 dictionary 放在 worker stack。
- Gzip header 累計最多 1024 bytes，嚴格驗 magic、CM=8、reserved flags、FEXTRA／FNAME／FCOMMENT／FHCRC；trailer 驗 CRC32 與 ISIZE。只接受單一 member，拒絕 trailing bytes。解壓輸出上限精確為 active imageBytes，超過即失敗；compressed 上限為 imageBytes + floor(imageBytes/100) + 2048，也受新鮮 storage quota 限制。
- `EpaperGzipReader` 透過 `UserDataStorage::beginDownload(..., retainAtEof=true)` 保留 operation gate，compressed EOF 不自動釋放；只有該 session owner 可 rewind／close。Rewind 從 gzip 開頭重設 decoder，不提供任意 compressed seek。Generic download 保持原有 EOF auto-close 行為。
- Reader 的 compressed input 與 skip/tail scratch 各 4 KiB，隨 reader 配置在 heap，避免 skip/read 與 tail validation 巢狀呼叫疊加 4 KiB 暫存，耗盡 8 KiB worker stack。單一 reader／decoder 約 49 KiB；未翻轉時考慮 refresh 期間舊 reader 與新 download reader 短暫並存，額外峰值約 58 KiB；目前 800×480 開啟翻轉時另需最多 16 KiB band，約 65 KiB。這是結構大小估算，未含 allocator／HTTP／LittleFS overhead，仍需實板量 heap high-water mark。Band 在 transfer 後、釋放 storage gate 前銷毀。
- Stored draw 在 worker 的 validation pass 完整檢查 gzip 與 EPDIMG，未成功不得設定 marker、改頻率或喚醒 panel；validation reader 釋放後，建立第二個 reader，跳过 header，再用 sequential `StoredFrameSource` 提供 driver 4 KiB reads。Manual refresh 的 admission 另保留原有同步 validation。第二 pass 最後一批 frame 交付前驗完 trailer，失敗走 SourceReadFailed 與既有 shutdown。Transfer 後立即釋放 decoder／storage，physical refresh 期間可開始 download。
- `EpaperOrientedFrameSource` 在 wake 前配置最多 16 KiB 的完整 row band；若單一 row 大於 16 KiB，最多配置一列（resolution range 下小於 32 KiB）。Horizontal flip 倒序列內 byte 並交換 high/low nibble；vertical flip 倒序 band 內 row，每個反向 band 從 gzip 開頭 inflate/discard prefix。預設不 flip 時直接走原 sequential source。此取捨避免完整 framebuffer／raw temp／猜測控制器方向 bit，但 vertical flip 有額外多次解壓成本。
- Logical download 先完整 validation，再 rewind 並 skip logical Range 起點；尾端 Range 發送最後一批前 drain／validate 剩餘 gzip。先行失敗回 storage_error；response 已開始後 read failure 由固定 Content-Length 的短 response 結束，不把截短資料當成功。Disconnect 關閉 matching reader，late cleanup 不影響新 session。
- CRC32 使用 immutable 256-entry lookup table，palette 驗證維持六色 code contract。Cached `EpaperTimingDiagnostics` 記錄 upload inflate/validation、stored validation、frame read（含 flip 重解壓）、SPI transfer phase（含 source read）及 upload/action 至 cleanup 的 total ms 與本次 operation 的 download failure count；由既有有 USB backpressure 保護的 heartbeat 印出，不在 I/O hot path 寫 Serial。目標預算以未翻轉 production profile 為基準：160 MHz validation <250 ms、80 MHz transfer 解壓增量 <400 ms、stored refresh 增量 <700 ms；需要實板比較，不能由 host 時間推斷。
- `EpaperService` 使用短 mutex 保存完整 cached status，mutex 內不得操作 filesystem、SPI、NVS、JSON 或 Serial。Queue depth 固定 1；實體 draw 在低優先序 dedicated FreeRTOS worker 執行，BUSY wait sleep/yield，frame 每 4 KiB yield。
- Worker 必須在 panel wake 前持久化 `stage: active` 並 read-back，再取得全晶片 frequency guard、設定並 read-back 80 MHz。80 MHz 涵蓋 prewake、initialize、transfer、refresh、Power OFF／Deep Sleep cleanup；全部 exit path 經 shutdown coordinator 後才恢復 160 MHz。
- `markActive()` 失敗可發生在寫入前、寫入後讀回失敗或值不符；prewake 分支不呼叫 shutdown coordinator，而是對已知未上電面板清除並讀回 marker。清除成功仍維持本次 service `unavailable`、禁止重畫，但不設 recoveryRequired，允許既有 restart admission；清除未確認時設 recoveryRequired，保留 restart 拒絕。真正 wake 後的 shutdown、boot active marker 與 unknown panel 仍走原本 fail-closed 路徑。
- 成功或 wake 後失敗都依序嘗試 Power OFF `0x02`/`0x00`、BUSY wait、Deep Sleep `0x07`/`0xA5`。只有 protocol shutdown 與 `stage: shutdown_confirmed` read-back 成功才開始 180 秒 cooldown；timeout、SPI failure 或 shutdown failure 設 `panel_state: unknown` 並永久 fail closed 到完整 power cycle。
- `main.cpp` 在任何依賴 reset reason 的 subsystem 前 capture `BootDiagnostics`，後續 Device API、e-paper status 與 marker recovery 都只使用同一份 normalized snapshot，不再次讀 SDK，也不允許 runtime event 改寫。`epaper_meta` namespace 位於 default `nvs`，不屬於 settings/data/factory reset 會清除的 `user_nvs` 或 `userdata`。Confirmed marker 重啟 180 秒 cooldown；active marker 只有 snapshot 為 `PowerOn` 才透過 `EpaperSafetyStore` clear/read-back 恢復，因本專案規定 MCU 與 HAT 共用板上 3.3 V；其他 reset reason 記錄 interrupted 並禁止自動 draw。一次性實板 recovery build 可在操作者已確認共同斷電後用 compile flag 提供同等證據，但 release 設定必須固定關閉。
- Runtime status 只組合既有 service cached snapshot。`blocked_resources` 在 validation 是 `epaper,userdata`，transfer 是 `epaper,userdata,spi`，physical refresh 與 cooldown 只保留 `epaper`；不得由 `busy` 推導 Wi-Fi/HTTP 不可用或建立全域 mutex。
- 每秒 serial heartbeat 只在 USB CDC 已連線且 TX buffer 至少有 1536 bytes 空間時輸出；USB 連線不代表 host 正在讀取。TX buffer 為 2048 bytes、TX timeout 為 0，backpressure 時略過 heartbeat；runtime timer、cooldown 與 Wi-Fi state machine 不得依賴 serial drain。
- 全專案只有 shutdown coordinator 最終核准點可呼叫 `ESP.restart()`。到期 software restart 先發布 `restarting`、拒絕新 operation、bounded quiesce worker；本次 boot 曾 wake panel 時，protocol shutdown 失敗必須取消 restart並保存 `epaper_shutdown_failed`。

## Config persistence

- `ConfigService` 是唯一 active config owner；其他模組只保存 service pointer 並取得 value snapshot。
- Production 由 `main.cpp` 將 `ArduinoPreferencesBackend` 注入 `PreferencesConfigStore`，再將 store 注入 `ConfigService`；兩個 interface 都是持久化故障注入與 host test 的窄邊界，不承載 business rule。雙 slot、schema/read-back 與 active marker commit 演算法仍由 `PreferencesConfigStore` 唯一實作。
- 一般 endpoint 的設定更新使用 `updateWifi()`、`updateSystem()`、`updateHostname()`；密碼更新只委派 `AuthService::changePassword()`，由它呼叫 `ConfigService::updateAdminPassword()`：每個方法在同一個 service mutex 內從最新 active config 合併指定欄位群組、驗證、持久化並發布。`updateSystem()` 同時回報 hostname／TX power 的實際變動並略過 no-op NVS write；完整 `commit()` 只供 boot/self-test 等明確的完整替換流程。
- NVS 不保存整個 C++ struct/blob，也沒有 `kConfigBlobKey`。
- Current schema 使用 `devcfg_a`、`devcfg_b` 兩個 per-key slot，以及 `devcfg_meta/active` marker。
- Save 寫入 inactive slot，逐欄完成後最後寫 schema marker，再 read-back 驗證，最後切換 active marker；失敗時 active config 不更新。
- Boot 先讀 active slot，再嘗試另一個有效 slot。空 NVS 才建立 factory defaults。
- 本專案不 migration 舊 blob、舊 schema 或一般舊 key layout；`wifi_tx_dbm` 是唯一明列的同 schema compatible-missing key，缺少時保留其他欄位並在 RAM 使用 15，boot 不補寫。該 U8 key 存在但型別錯誤或值不在 2–20 時視為 slot corruption；不支援的資料不在 boot 自動覆寫，runtime 以 recovery defaults 保持 AP 可達。
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
| E-paper operation/status | `EpaperService` 短 mutex 與 queue depth 1；upload/action producer，dedicated worker consumer。 |
| E-paper display calibration | `EpaperCalibrationService` mutex；完整 profile replacement，持久化成功後才發布 canonical snapshot。 |
| SPI bus | `SpiBus` mutex與唯一 lifecycle owner；BUSY wait 不持有 transaction。 |
| E-paper protection marker | `EpaperSafetyStore`／default NVS `epaper_meta`；write 後 read-back，failure fail closed。 |
| Runtime activity | `RuntimeEndpoints` 唯讀組合 `EpaperService`／`RuntimeActionScheduler` cached snapshot；不作 admission gate。 |
| Boot diagnostics | `BootDiagnostics` 在 startup capture 後不可變；`ApiRouterDeps` 以 const reference 注入，Device endpoint 與 e-paper 只讀 snapshot。 |
| Battery sample | `BatteryMonitor` critical section；loop producer 更新完整 voltage/timestamp，HTTP/serial reader 只取 cached snapshot。ADC 取樣與百分比計算不在 critical section 內。 |
| Console staged config | `ConsoleConfigCommand`／`ConfigStaging`，只在 console runtime RAM；dirty bitset 與 fixed-size secret buffers。 |

Critical section 內不得執行 NVS、JSON、Wi-Fi、Serial 或其他長操作。Scan 與 STA connect/apply 都不得用等待完成的 loop；scan 由固定 poll 推進。

## Battery monitoring

- DFRobot FireBeetle 2 ESP32-C6 的 GPIO0 固定為板載電池分壓輸入，board profile 使用 `BatterySense{0, 2, 1}` 表達官方範例的二倍還原。GPIO0 仍為 `BoardReserved`，generic `PinRegistry` claim 必須拒絕；不得為了 ADC 將它改成一般可用 GPIO。
- Production adapter 明確使用 12-bit resolution、`ADC_11db` 與 `analogReadMilliVolts()`。`BatteryMonitor` 每次連續讀七筆 calibrated pin mV，取 median 後依 profile divider 還原電池端 mV；boot 在 API 啟動前先發布第一筆完整 sample，之後最短每 10 秒更新，e-paper `drawing` 期間暫緩取樣。
- 單次 ADC read failure 不發布部分結果，保留最後完整 sample。Snapshot 以 unsigned monotonic subtraction 計算 `sample_age_ms`；endpoint 不直接操作 ADC，也不因 battery sample 不可用而讓整個 device resource 失敗。
- `estimated_percent` 使用 piecewise-linear anchors `(3300,0)`、`(3400,5)`、`(3550,25)`、`(3700,50)`、`(3870,75)`、`(4200,100)`。`2500`–`3300` mV clamp 為 `0`、`4200`–`4400` mV clamp 為 `100`，其餘範圍不提供 estimate。此值只代表 voltage-derived approximation，不是 coulomb counter 或 fuel gauge。
- 現有硬體只提供 battery-line voltage；charger status 沒有連到可讀 GPIO。Firmware 不建立 presence／power-source／charging heuristic，API 不輸出對應欄位。未接電池時 charger 或 floating node 仍可能形成 plausible voltage，client 不得以非 null 讀值判斷已安裝電池。

## Wi-Fi runtime

- `WifiManager` 每次成功啟用或轉換 STA、AP、AP + STA 後，必須立即以 `WifiDriver::setTxPower()` 套用最新 `DeviceConfig::wifiTxDbm`；初始 apply、fallback、candidate、finalize 與 rollback 路徑使用同一 helper。設定失敗必須使同步 mode apply 失敗。Off mode 不呼叫需要 active radio 的 TX-power API。
- ESP32-C6 adapter 對 2–19 dBm 以四分之一 dBm 單位送入 `esp_wifi_set_max_tx_power()`；configured 20 使用 SDK 允許的最高參數 84，表示解除專案額外限制，由 platform/country/PHY 映射最終上限。API 只宣告 configured 值，不把 setter success 當成 RF 測量。
- `WifiManager::apply()` 只設定 mode/IP、啟動 AP/STA 並發布 `connecting`，不等待 STA connected。
- `WifiManager` 透過注入的 `WifiDriver` 與 monotonic clock 執行狀態機；Arduino adapter 負責 framework API mapping，`WifiRadio` 仍負責跨 task driver serialization。持久化與 candidate STA 都必須在 association 且取得非 `0.0.0.0` IPv4 後才發布 connected。
- `WifiManager` 的 STA connect 或 AP→STA/AP+STA full-replacement request 只在 API task 複製 validated candidate、原 persisted snapshot 與排入狀態；Wi-Fi driver 操作、timeout、成功／失敗、final apply 與 persisted runtime restore 都由 loop 的 `poll()` 執行。
- Candidate 與 rollback snapshot 只存 RAM。連線時沿用目前 active AP，不呼叫會先關閉 radio 的完整 `apply()`；認證與 IPv4 成功後由 `RuntimeActionScheduler` 自動持久化，不提供 client commit endpoint。
- Connection state 以 mutex 保護 candidate 與內部 transaction id；mutex／critical section 內不得呼叫 NVS、Wi-Fi driver、JSON 或 Serial。Terminal failure 清除 candidate credential，公開狀態只映射為 `idle`、`connecting`、`connected`、`failed`。
- `WifiManager` 啟動 SoftAP 時，DHCP 必須把 AP IP 發布為 DNS server，並在 framework 支援時發布 DHCP captive portal URI；URI 發布失敗不得關閉仍可由 AP IP 直接管理的 AP。
- Candidate `poll()` 使用單一 association 與 15 秒 deadline；三個 5 秒觀察區間不重啟 driver connect。它同時處理目前／最終 AP subnet overlap、commit 後的 5 秒 STA grace、AP+STA 最後 AP reconfigure、NVS rollback 與舊 AP runtime restore。Application timeout 必須先停止底層 STA connect，再發布 `failed`，避免 ESP-IDF 持續以 connecting state 拒絕 scan。
- `RuntimeActionScheduler` 在 candidate verified 後以 `ConfigService::updateWifi()` 把 candidate 的 Wi-Fi 欄位合併到最新 active config；final AP apply 失敗時也只以同一入口回復 rollback snapshot 的 Wi-Fi 欄位。Candidate transaction active 時，一般 coalesced Wi-Fi apply 必須延後，不能用完整 `apply()` 中斷驗證。
- System endpoint 只改 TX power 時排入獨立 coalesced action；consumer 取得最新 config snapshot、持有 `WifiRadio` mutex 呼叫 `WifiManager::applyTxPower()`，不得 disconnect、切 mode 或 restart mDNS/captive DNS。STA transaction 期間延後且不消耗重試；driver failure 每隔一秒重試，總嘗試上限三次。hostname 與功率同時變動只排一次完整 Wi-Fi apply。
- 狀態改變由 `main.cpp` 的 loop 統一 restart mDNS/captive DNS；mDNS 只在 STA connected，captive DNS 只在 AP active。
- HTTP captive fallback 必須以 TCP connection 的 local IP 判斷 request 是否由 AP 介面抵達，不得只因 runtime 有 AP IP 就攔截 STA LAN request。
- API 必須區分 persisted `configured_mode` 與目前 runtime `mode`。
- `WifiScanner` 委派 `ArduinoWifiScanDriver` 作為唯一 `WiFi.scanNetworks()` 呼叫點；API 與 human console 共用 RSSI 大於 `-75 dBm` 的門檻、10 秒 cooldown 與最多 20 筆結果。持久化 STA application state 為 `connecting` 時由 `WifiManager` preflight scan 並回可重試 busy，不呼叫 driver scan。

## 測試、logging 與嵌入式限制

- Native runner 為 `tools/native-test/test_native.sh`；Python tools 測試由 `make test-tools` 獨立執行。整體自動化測試涵蓋 config／IPv4 validation、ConfigService 原子欄位群組更新與故障隔離、PreferencesConfigStore slot／marker fault injection、AuthService session lifecycle／concurrency、WifiManager driver/clock seam 與 timeout／IPv4／disconnect／subnet overlap／commit-finalize-rollback failure 狀態、Wi-Fi payload、response codec、HTTP JSON transport parsing、user filename／MIME／Range／cursor／inspection path、battery divider／median／sample cadence／estimate/null contract、EPDIMG header／CRC／palette streaming validation、white／palette packed frame 與 wrap-safe cooldown、board restricted-pin／原子 claim／shared SPI ownership與安全 boot level、fake transport driver command／BUSY timeout／watchdog／shutdown failure、80 MHz set/read-back/restore、`epaper_meta` marker fault injection、power-only／single-refresh probe、protocol shutdown與 restart denial、e-paper hardware 早於 Serial 與其他 subsystem 且 release self-test/refresh disabled 的 source contract、e-paper/runtime 公開 route 與 reserved-file matrix、host EPDIMG/CRC/URL/gzip-upload/error/wait client、console config staging、subsystem registry start order/readiness/dynamic health、ApiRouter catalogue metadata 與 exact/dynamic runtime authorization matrix、Web identity response（包含 none／null）、generated metadata、空前端 artifact protection、web preview/minifier/deterministic gzip、release partition/manifest/path/hash protection，以及 metadata-driven HTTP registration source contract。Source contract lint 不代表 HTTP adapter、filesystem 或完整 service integration coverage。
- Firmware runtime 變更至少對既有 verified production `build/latest/web/` 執行 `make esp`；需同時重建預設產品 frontend 時執行 `make build` 或 `make build WEB=user`，builtin 使用 `make build WEB=builtin`，API-only firmware 使用 `make build WEB=none`。
- `user-web-project/` 是預設 frontend source，`user-web/` 是其 gzip-only generated import；`builtin-web/` 保存可明確選用的內建管理頁。`tools/web-build/` 負責 user import lifecycle、frontend selection／processing及 `WEB=none` 的已驗證空 production web；`tools/release-build/` 只消費 production web 並管理 generated header、binary、manifest、`firmware.img` 與快照。
- `user-web-project/` 以 `embedded-web-dithering` remote 的 `six-color-epaper` branch 作為 `--squash` Git subtree；它不是 submodule。`make user-web pull` 必須由乾淨 worktree 拉取並建立 subtree merge commit；`make user-web push` 只接受已提交的 prefix 變更並透過本機設定的 SSH push URL 推送。同步後一律以 `make build WEB=user` 重建 nested frontend、重新匯入並檢查 app partition；不得手動複製或編輯 `user-web/`。
- `VERSION` 保存純 `MAJOR.MINOR.PATCH`，目前為 `0.8.0`；顯示與 tag 使用 `v0.8.0`。Firmware 與 web manifest version 必須一致。本機 dirty build 允許且 manifest 記錄 `git_dirty: true`；official CI release 必須拒絕 dirty worktree。
- 每次完整 firmware build 先在 private staging 完成編譯、hash、package 與驗證，再建立 Asia/Taipei `YYYYMMDD_HHMM` 快照；同分鐘碰撞依序使用 `_02`、`_03`。`build/latest/` 是內容相同的實體副本，不使用 symlink。
- 完整 snapshot 固定含 root `web-manifest.json`、`web/`、`binary/` 與 `firmware.img`。`binary/` 含 `manifest.json`、`bootloader.bin`、`partitions.bin`、`boot_app0.bin`、`firmware.bin`；manifest 記錄 board/chip/flash/partition、image size/SHA-256、web selection/process/hash、version、Git commit/dirty。使用 user frontend 時另記錄 user source hash；builtin／none 不記錄 source hash。`WEB=none` 保留空的 `web/` 以維持固定 artifact shape，manifest 記錄 `web:none`、`web_process:none`、`frontend_delivery:none`、零檔案與零 payload，並以空樹 hash 做 artifact integrity 驗證。
- 專案持久產物的壓縮技術統一限定為 gzip：frontend precompression 使用 `.gz`，`firmware.img` 使用 deterministic tar.gz；TAR 只負責收集多個具名檔案，不是另一種壓縮。不得再新增 ZIP archive、Brotli、zstd、xz、bzip2 或額外外層壓縮包。Esptool `-z` 只屬於燒錄期間的 transient transport optimization，不產生或改變持久 artifact，因此不計入 release 壓縮格式。
- `firmware.img` 是使用自訂副檔名的 deterministic tar.gz。USTAR root 只能依序含 `manifest.json` 與四個 bin，所有 member 必須是 regular file、固定 mode／mtime／owner metadata；不得含 `binary/` 目錄、額外 outer archive、flasher、README、digital signature、`userdata` 或 `user_nvs` image。`build/latest/firmware.img` 與對應時間戳檔案必須 byte-identical。
- `make web` 或 `make demo` 完成後以 web-only `build/latest/` 取代舊 latest，避免 binary 與 web 錯配；`make esp` 必須拒絕 demo target。`make clean` 移除 latest、transient、舊版 generated output及 `user-web/` import但保留 `user-web/.gitignore`、主專案時間戳快照與 `user-web-project/build/` 歷史；只有 `make clean all` 移除主專案時間戳。Bare `make all` 必須失敗，且不建立產物。
- Flash 前必須驗證指定 snapshot 並只使用 sibling `binary/` discrete images，不得在 flash 時改用 `.pio/` 暫存 image。`make verify` 驗證 latest；`IMAGE=build/<timestamp>/firmware.img` 驗證／燒錄歷史 snapshot。把 `firmware.img` 單獨複製出去不符合 workspace-coupled full verification，必須連同 sibling web、web manifest 與 binary 保留。
- `make deploy PORT=<port>` 必須先確認呼叫者明確提供 port，再依序清除 latest 與 user import、依 `WEB`／`WEB_PROCESS` 完整重建、驗證並燒錄；缺少 port 時不得先清除或建置。預設與 `WEB=user` deploy 都必須強制重建並匯入 user frontend；builtin／none 不得執行 nested user build。
- Flash/monitor 前必須重新確認 serial port；不得猜測 GPIO、USB、partition、flash 或 board-specific 設定。
- Self-test 預設關閉；臨時 firmware hook 在整合前刪除。
- Serial/API/log 不得輸出 admin password、Wi-Fi password、token、MAC 或其他本機識別資料到可重用文件。
- Arduino `String` 仍用於 framework transport、JSON response 與短生命週期 token；沒有長期 heap/fragmentation 量測證據前不做全面機械替換，優先維持現有 bounded input 與 secret clearing。
- Build、upload、monitor、browser 與 board smoke-test 的日期結果只放 ignored `tmp/verification/`。

## Runtime restart ownership

- `SystemRestartCoordinator` 使用 request／poll／progress：request 回 Accepted／AlreadyPending／Rejected，progress 為 Idle／Draining／Ready／Failed。Scheduler 只持有 `EpaperService` 的 admission owner；`EpaperShutdownCoordinator` 是 worker 完成 cleanup 後的最終硬體核准邊界，不是 scheduler 的同步旁路。
- Boot probe 到 runtime worker 的 ownership 以成功建立 worker 為交接點。交接後 loop 不寫 protection marker；cooldown poll 僅設 bounded control flag，worker 在每次最長 10 ms queue wait 前自行檢查 cooldown 並處理 control，不依賴 main loop 排程。Draw queue depth 保持 1，control 不占 draw slot。
- Snapshot mutex 只保護狀態與 admission。Cooldown clear／read-back 在 worker 鎖外執行，先保留 generation，完成後短鎖確認同代再發布；清除期間不開放 draw／upload。CPU snapshot 使用 owner 發布的 cached 值；marker stage 的唯讀跨 task snapshot 使用 atomic，持久化仍只有唯一 writer。
- 已接受 upload 保持 reservation 到後續 draw 入列；callback 只清理自己的 session，restart 不旁路關閉檔案。已排隊 validation／draw 也屬 drain 範圍。所有 driver cleanup、CPU restoration、storage release 完成才可 ACK；Ready 是不可逆的本次核准，即使測試 restart driver 返回也不再接新工作。
- Deadline 使用 unsigned monotonic subtraction：upload 30 秒、total 150 秒，從首次 drain 開始且不延展。Driver 預算為 2 秒 prewake、90 秒 operation watchdog、15 秒 power-off wait、2 秒 deep sleep；連同 upload 約 139 秒，餘量供短 I/O。Deadline 不強停 driver／task；超時 Failed 後由原 owner 收尾，未確認安全前保持 admission 關閉。
- Storage mount／worker startup failure 仍建立 restart 邊界。無 active worker 時只可核准已初始化且已知安全的 panel／marker；unknown 或 active marker 拒絕，不能因 userdata failure 永久阻斷安全 reset 修復路徑。

## Credential lifecycle

- 固定鎖順序是 `AuthService credential/session mutex → ConfigService mutex`。Login 在取得 credential mutex 後才讀密碼、比較並發布 token；changePassword 在相同 mutex 內保存，成功才撤銷 session，不在持鎖方法內呼叫會重鎖的 public invalidation helper。
- ConfigService 仍是設定與 NVS 的唯一 owner。密碼更新在 config lock 內依最新 committed AP password 開關確認 runtime readiness，僅接受 scheduler 可用性值，不反向呼叫 auth／scheduler。失敗保留 active credential 與 token，成功回傳是否需要 restart；同密碼成功更新仍撤銷 session。
- REST、serial `api` 與 typed config staging 的密碼更新共用 AuthEndpoints／AuthService；完整 config commit 只用於 boot/self-test。不得新增 runtime credential 寫入旁路。

## Stored-image snapshot publication

- Stored draw 與 metadata refresh 先取得 e-paper operation reservation／generation，再開啟 storage。Validation 使用 stack-local candidate，完整讀取並驗證後才在短鎖中發布本代 snapshot。已接受 operation 到發布期間不開放新 upload，gzip validation 持有 storage gate 到 candidate publication，避免新 image commit 被舊 candidate 覆蓋；generic routes 不得修改 reserved image filename。
- Busy、暫時 unavailable、open／partial read I/O error 保留最後已知 metadata，回報本次真正錯誤。只有 confirmed NotFound 清除 snapshot；完整讀取且確認 corrupt 才發布 present＋invalid 及 validation reason。Pinned Arduino VFS 的 read-open 以 `errno == ENOENT` 判定 missing，其餘失敗保守回 storage error。
- Reservation 與 file session 都由取得者釋放；restart admission 拒絕、validation failure 與 queue failure 不可釋放別人的 session。Runtime status 不為查 metadata 掃 filesystem。

## Deferred Wi-Fi scan

- `WifiScanner` 擁有唯一 operation id、固定最多 20 筆 result、interest、15 秒 request deadline 與 2 秒 stop cleanup deadline。State 為 Idle／Starting／Scanning／Stopping／Unavailable；完成結果只能由 matching id take，完成後維持 10 秒 cooldown，不把舊結果當作新 request 成功。
- `WifiRadio` 的 scan reservation 與短 mutex 分開。Scan admission 和 `WifiManager::queueStaTest()` 都使用 radio → manager state 的鎖順序；manager apply/poll/TX power 遇 reservation 不操作 driver，scheduler 延後 apply 而不遺失最新設定。Scanner poll 的 driver call 只短暫取得 radio mutex，不跨 scan 等待。
- Arduino adapter 使用 pinned async scan／scanComplete；獨立 SCAN_DONE event ACK 證明 completion／stop。Application timeout 先發布 terminal error，只有確認 driver 結束才 scanDelete／釋放 reservation；stop 未確認超過 2 秒則 fail closed，晚到 ACK 不靜默重新開放 unknown radio。
- Restart 先關閉 e-paper admission並啟動原有總 deadline；scheduler 以值傳入 radio 是否已完成 cleanup，未完成時 e-paper owner 不核准實體 restart。Wi-Fi cleanup 不碰 panel、不延長 150 秒 deadline；未知 radio 不以強制 restart 補救。
- ApiRouter 的 internal PendingRequest 綁定 operation id、method/path/principal；不進入公開 envelope。Router/Endpoints 統一 admission、錯誤映射與 completion 授權；HTTP/serial 不各自實作 scan policy。
- HTTP 使用 pinned ESPAsyncWebServer pause() 的 weak handle。單一 PendingResponseSlot 以短 mutex 管所有權，disconnect／completion 只有一個 take 成功；鎖外才呼叫 router／send，late callback 不影響新 id，不保存 raw request pointer。
- Serial API 使用相同 PendingRequest，human scan 共用 scanner id；等待期間回到 loop，維持單一執行中命令與既有有限輸入容量，收到 terminal 結果前不開始下一個 response。不得用 busy-wait 或無界 request list。

### Host verification and streaming callback boundary

`make test-native`, `make test-tools` and `make test-web` run independently;
`make test` runs each once without firmware compilation. `make test-all` builds,
verifies the resulting snapshot, then runs those tests. Prepare pinned native
headers with `pio pkg install -e firebeetle2_esp32c6`; alternatively point
`ARDUINOJSON_INCLUDE` at the ArduinoJson version pinned in `platformio.ini`.
Missing headers or browser are failures, not skipped tests.

`ApiServer` routes streaming operations through `StreamingSessionBridge` so a
body/filler callback and disconnect cannot read/write/close the same storage
session concurrently. The bridge serializes calls into `ApiRouter`; services
retain the operation gate, session IDs and file policy. It never encompasses
response sending, which may reenter disconnect. Session IDs still reject late
cleanup after a newer session starts. Native tests exercise the bridge, real
Router/endpoints and real UserDataStorage with an injected filesystem; they do
not establish ESPAsyncWebServer request lifetime guarantees on a device.

`UserFileHttpPreflight` 是 HTTP adapter 的授權與 transport shape 前置檢查；它先透過 Router catalogue 驗證 dynamic route 與 token，才檢查 query、body、Content-Length 與 Content-Type，不取得 storage gate。`ApiRouter` 持有單一 upload session 的 idle lease，接納與每次成功非空 write 更新 `millis()` 時戳。`ApiServer::poll()` 透過同一 bridge 每輪檢查 60 秒 wrap-safe idle 期限、abort matching session，脫離 bridge 後對仍有效的 paused weak request 回 408。Finish、disconnect、write failure 清除 matching lease；舊 session callback 不得清除新 lease。E-paper abort 同時釋放 decoder、temporary file 與 uploading state，status 記錄 `upload_timeout`。此 timeout 只涵蓋 callback 間閒置，不能 preempt 已阻塞的 filesystem 呼叫。

PR and master-push verification uses independent host/browser jobs and isolated
builtin/user/none firmware jobs. User source and production checks run through
that project's own targets before firmware import. Verification has read-only
repository permission and no deployment or hardware step.

Final restart approval additionally calls `UserDataStorage::reserveRestart()`.
This is a zero-wait acquisition of the existing operation gate with no filesystem
I/O. An active generic upload/download keeps restart draining; only its original
callback may close it. After acquisition the gate stays reserved through reboot,
so no new file session can enter between ACK and physical restart. The same
150-second total deadline cancels a drain that cannot acquire the gate. An
unmounted storage service still participates without blocking recovery reset;
its dependency is retained before runtime startup validation. New e-paper image
downloads are denied once e-paper restart admission closes.
