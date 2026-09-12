# Serial Console Reference

日期：2026-07-21

本文件說明如何透過 USB serial 操作裝置 console。REST endpoint 的 request／response 欄位與完整錯誤定義仍以 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md) 為準；firmware 內部責任與限制以 [SPEC_TECHNICAL.md](SPEC_TECHNICAL.md) 為準。

## 連線方式

Serial baud rate 固定為 `115200`。每次連線前先確認目前板子的 serial port，不得沿用或猜測先前的 `COMx`／`/dev/ttyACM*`／`/dev/ttyUSB*`。

使用 PlatformIO 開啟可輸入指令的互動式 monitor：

```bash
pio device list
pio device monitor --environment firebeetle2_esp32c6 --port <confirmed-port> --baud 115200
```

Windows 範例中的 port 形式為 `COM7`，Linux 範例中的形式為 `/dev/ttyACM0`；以上都只是格式示意，實際值必須由當次裝置列表確認。輸入一整行命令後按 Enter 送出。

`tools/serial-monitor/monitor_serial.py` 只適合限時收集 boot／smoke-test log，不提供互動式命令輸入。

## 輸入規則

- 一次輸入一行，LF（Enter）代表命令結束；CR 會被忽略。
- 命令與 API method 都區分大小寫。請使用小寫 console command 與大寫 `GET`、`POST`、`PUT`、`DELETE`。
- 行首空白會被忽略；不需參數的 command 不要加多餘內容，需要參數的 `ls`、`stat`、`config` 與 `api` 必須遵守各自語法。
- 每行最多可接受 639 bytes（不含換行）。超過上限時整行會被捨棄；`api` command 回傳 `payload_too_large` JSON，其餘命令回傳 `Console input is too long. Line cleared.`。
- 每個 `api` command、token 與 JSON body 都必須放在同一行。JSON 字串內若需要空白可正常保留。
- Console 不會保存「目前登入身分」。每次呼叫受保護 endpoint 都要明確附上 `token=<token>`。

## Human commands

Human commands 提供狀態診斷、userdata inspection 與 RAM-only config staging。輸出是供人閱讀的文字，不使用 API JSON envelope；真正持久化設定時仍經過 `ApiRouter` 的既有 auth、validation 與 runtime rules。

| Command | 說明 |
| --- | --- |
| `help` 或 `?` | 列出內建命令與 `api` 語法。 |
| `status` | 顯示目前／已設定 Wi-Fi mode、STA/AP 狀態、storage readiness 與 admin session 狀態。 |
| `device` | 顯示 chip、revision、core、flash、free heap、MAC 與 hostname。 |
| `wifi` | 顯示目前與已儲存的 Wi-Fi 設定摘要、STA 連線及 setup AP 狀態；不顯示密碼內容。 |
| `scan` | 掃描附近 Wi-Fi；與 REST scan 共用 busy 狀態、RSSI 門檻、10 秒 cooldown 與最多 20 筆結果。 |
| `storage` | 顯示 flash partition、app image、內建前端與 user-data 容量。 |
| `session` | 顯示 runtime 是否存在有效 admin session，不顯示 token。 |
| `ls [path] [offset]` | 列出 userdata directory；每頁最多 32 筆。 |
| `stat <path>` | 顯示 userdata path 的 file／directory type 與 file size。 |
| `config ...` | 檢視、暫存、比較、revert 或依 group commit typed settings。 |

最基本的使用方式：

```text
help
status
wifi
scan
```

`scan` 在 STA connection test 或連線進行中會暫時無法執行；依 console 顯示稍後重試即可。未知命令會回傳 `Unknown command. Type help.`。

### Userdata inspection

```text
ls
ls /files
ls /files 32
stat /files/photo.jpg
```

`ls` 未傳 path 時從 userdata root `/` 開始；offset 預設 `0`，有下一頁時 console 會列出可直接使用的 `More: ls ...` 命令。Path 可傳相對或絕對形式，重複 slash 與 `.` component 會正規化；任何 `..` component、backslash、shell／glob metacharacter、空白、non-ASCII 或超過 127 bytes 的結果都拒絕，不做截斷。

`ls`／`stat` 與 HTTP file API 共用同一個 non-blocking operation gate；其他 file operation 進行中時顯示 `Filesystem error: user storage is busy`。Internal `.upload.tmp` 不會由任何 inspection path 顯示。

### Config staging

```text
config show [prefix]
config get <key>
config set <key>=<json-value>
config changes [wifi|system|auth]
config revert <group-or-key>
config status
config commit <wifi|system|auth> token=<token>
```

`config set` 的右側必須是一個完整 JSON value，所以 string 要保留雙引號、boolean 不加引號、DNS 使用 array。Set 只更新 RAM overlay，不寫 NVS、不套用 runtime；每次 show 或 commit 都把 overlay 疊在最新 persisted snapshot 上。成功 commit 才清除該 group，失敗保留 staging 供修正或重試；重開機會清除所有 staging。

```text
config show wifi.sta
config set system.hostname="lab-device"
config set wifi.fallback_to_ap=false
config set wifi.sta.ip_config.dns=["1.1.1.1","8.8.8.8"]
config changes
config revert wifi.sta.ip_config.dns
config commit system token=<token>
```

可用 key：

| Key | Type / values | Access |
| --- | --- | --- |
| `system.hostname` | JSON string | read/write |
| `auth.username` | JSON string | read-only |
| `auth.password` | JSON string | write-only secret |
| `auth.password_set` | JSON boolean | read-only derived state |
| `wifi.mode` | `"sta"`, `"ap"`, `"ap_sta"` | read/write |
| `wifi.fallback_to_ap` | JSON boolean | read/write |
| `wifi.sta.ssid` | JSON string | read/write |
| `wifi.sta.security` | `"wpa"`, `"open"` | read/write |
| `wifi.sta.password` | JSON string | write-only secret |
| `wifi.sta.password_set` | JSON boolean | read-only derived state |
| `wifi.sta.ip_config.mode` | `"dhcp"`, `"static"` | read/write |
| `wifi.sta.ip_config.address` | IPv4 JSON string | read/write |
| `wifi.sta.ip_config.gateway` | IPv4 JSON string | read/write |
| `wifi.sta.ip_config.netmask` | netmask JSON string | read/write |
| `wifi.sta.ip_config.dns` | JSON array，最多兩個 IPv4 string | read/write |
| `wifi.ap.ssid` | JSON string | read/write |
| `wifi.ap.password_enabled` | JSON boolean | read/write |
| `wifi.ap.ip_config.mode` | `"default"`, `"static"` | read/write |
| `wifi.ap.ip_config.address` | IPv4 JSON string | read/write |
| `wifi.ap.ip_config.netmask` | netmask JSON string | read/write |

Secret value 不能由 `show`／`get` 讀取；`changes` 只顯示 `<updated>`。`config commit wifi` 使用完整 Wi-Fi replacement contract，可能回 accepted safe-transition 提示；依提示查詢 `api GET /api/wifi/connect token=<token>`。`config commit auth` 成功後目前 session 會失效，必須重新登入。

## API command 語法

```text
api METHOD PATH [token=<token>] [json]
```

各欄位規則：

| 欄位 | 規則 |
| --- | --- |
| `METHOD` | 必須是大寫 `GET`、`POST`、`PUT` 或 `DELETE`。 |
| `PATH` | 必須是完整且精確的 `/api/...` path，不含 hostname、query string 或尾端 `/`。 |
| `token=<token>` | 選填；若有，必須緊接在 path 後並放在 JSON body 前。受保護 endpoint 必填。 |
| `json` | 選填的單一 JSON value；需要 request body 的 endpoint 應傳 JSON object。不要在整段 JSON 外再加 shell quote。 |

Parser 不支援參數重新排序。以下是正確與錯誤的 token 位置：

```text
api PUT /api/system token=<token> {"hostname":"esp32-device"}
api PUT /api/system {"hostname":"esp32-device"} token=<token>   # 錯誤
```

回應永遠是一行 REST-equivalent JSON envelope：

```json
{"success":true,"data":{},"message":"ok"}
```

失敗時可先看 `data.code`，再看 `message`：

```json
{"success":false,"data":{"code":"unauthorized","authenticated":false},"message":"unauthorized"}
```

Serial transport 不會顯示 HTTP status line，但錯誤仍沿用 REST contract 的 `data.code` 與 message。常見 parser 錯誤如下：

| 情況 | 結果 |
| --- | --- |
| 缺少 method 或 path | `missing_field`，並顯示完整 usage。 |
| method 小寫或不支援 | `invalid_field`。 |
| JSON 無法解析 | `invalid_json`。 |
| path／method 組合不存在 | `not_found`。 |
| 受保護 endpoint 沒有有效 token | `unauthorized`。 |

## 登入與 token

先登入取得 runtime token：

```text
api POST /api/auth/login {"username":"admin","password":"<admin-password>"}
```

成功回應的 `data.token` 是後續命令要使用的 `<token>`：

```text
api GET /api/auth/session token=<token>
api POST /api/auth/logout token=<token> {}
```

注意：

- 登入命令中的密碼會出現在 terminal 輸入與可能的 terminal history；操作時避免錄影、貼到 issue 或保存到可重用 log。
- Token 是敏感資料，不要寫入 tracked 文件或分享完整 console output。
- Token 只保存在 runtime，登出、變更 admin password 或重新開機後會失效。
- Login 與 credential verify 不需要 token；其他需要授權的 endpoint 以後續表格為準。

## 可用 API commands

下表只摘要 serial 下法；payload 欄位與回應內容請查閱 [SPEC_API_REFERENCE.md](SPEC_API_REFERENCE.md)。

| Method / path | Token | Body | 用途 |
| --- | --- | --- | --- |
| `GET /api/alive` | 不需要 | 無 | 確認 API dispatcher 可用。 |
| `GET /api/device` | 不需要 | 無 | 取得裝置與記憶體摘要。 |
| `GET /api/storage` | 不需要 | 無 | 取得 storage capacity。 |
| `GET /api/storage/files` | 需要 | 無 | 取得第一頁 user-file JSON list；serial 不支援 query string。 |
| `PUT /api/storage/files/{name}` | 不適用 | raw body 不支援 | Serial 固定回 `unsupported_transport`；請改用 HTTP。 |
| `GET /api/storage/files/{name}` | 不適用 | raw response 不支援 | Serial 固定回 `unsupported_transport`；請改用 HTTP。 |
| `DELETE /api/storage/files/{name}` | 需要 | 無 | 刪除 user file。 |
| `GET /api/auth` | 不需要 | 無 | 取得 admin username。 |
| `POST /api/auth/login` | 不需要 | credentials | 登入並取得 token。 |
| `POST /api/auth/verify` | 不需要 | credentials | 驗證 credentials，不建立 session。 |
| `GET /api/auth/session` | 需要 | 無 | 驗證 token 對應 session。 |
| `POST /api/auth/logout` | 需要 | 可省略或 `{}` | 撤銷目前 session。 |
| `PUT /api/auth/password` | 需要 | password | 變更 admin password，並使 session 失效。 |
| `GET /api/wifi` | 不需要 | 無 | 取得 Wi-Fi runtime 與 desired configuration。 |
| `GET /api/wifi/scan` | 需要 | 無 | 掃描附近 Wi-Fi。 |
| `POST /api/wifi/connect` | 需要 | SSID／password | 在保留 management AP 時測試並提交 STA credential。 |
| `GET /api/wifi/connect` | 需要 | 無 | 查詢 connect／safe-transition 進度。 |
| `PUT /api/wifi` | 需要 | 完整 Wi-Fi config | 更新 desired configuration。 |
| `POST /api/wifi/reconnect` | 需要 | 可省略或 `{}` | 重新套用已儲存 Wi-Fi 設定。 |
| `PUT /api/system` | 需要 | hostname、`wifi_tx_dbm` 至少一個 | 原子更新 system configured 值；功率接受 2–20 整數，20 表示解除專案額外上限。 |
| `POST /api/system/reset` | 需要 | 可省略或 `{}` | 清除 settings 與 user data，並重新啟動。 |
| `POST /api/system/reset/settings` | 需要 | 可省略或 `{}` | 只清除 settings，並重新啟動。 |
| `POST /api/system/reset/data` | 需要 | 可省略或 `{}` | 只清除 user data，並重新啟動。 |

File list 有下一頁時，serial `api` parser 因不接受 query string而無法傳 `cursor`；請使用 `ls /files <offset>` 做本機 inspection，或改用 HTTP list endpoint。

## 常用範例

唯讀查詢：

```text
api GET /api/alive
api GET /api/device
api GET /api/wifi
api GET /api/storage
api GET /api/storage/files token=<token>
```

登入後掃描並測試 STA 連線：

```text
api POST /api/auth/login {"username":"admin","password":"<admin-password>"}
api GET /api/wifi/scan token=<token>
api POST /api/wifi/connect token=<token> {"ssid":"<wifi-ssid>","password":"<wifi-password>"}
api GET /api/wifi/connect token=<token>
```

`POST /api/wifi/connect` 是非同步操作。收到 `connecting` 後，重複查詢 `GET /api/wifi/connect`，直到 `state` 成為 `connected` 或 `failed`；不要在等待期間連續送出新的 connect command。

更新 hostname：

```text
api PUT /api/system token=<token> {"hostname":"esp32-device"}
```

只更新 Wi-Fi TX power；省略 hostname 不會改變它，`null` 不合法：

```text
api PUT /api/system token=<token> {"wifi_tx_dbm":15}
api PUT /api/system token=<token> {"wifi_tx_dbm":20}
```

變更 admin password：

```text
api PUT /api/auth/password token=<token> {"password":"<new-admin-password>"}
```

成功後舊 token 立即失效，必須用新密碼重新登入。

重新套用已儲存 Wi-Fi 設定：

```text
api POST /api/wifi/reconnect token=<token> {}
```

Reset commands 會持久化 reset intent，回應後排程重新啟動；這些操作會清除資料，執行前必須確認 scope：

```text
api POST /api/system/reset/settings token=<token> {}
api POST /api/system/reset/data token=<token> {}
api POST /api/system/reset token=<token> {}
```

## 設計邊界

- Human diagnostics 與 userdata inspection 是 read-only；`config set` 只建立 RAM overlay，不直接寫 NVS 或套用 driver。
- `config commit` 與所有 `api ...` 寫入都經 `ApiRouter`，不建立 `wifi set ...` 這類平行 business rules。
- Raw file transfer 是 HTTP-only；serial 只提供 JSON list／delete 與 human `ls`／`stat` inspection。
- Console output 可能與 firmware background log 交錯；自動化程式應只把完整 JSON 行視為 `api` response，且不得假設 human output 是穩定機器介面。

## Scan continuation

`scan` 與 `api GET /api/wifi/scan token=<token>` 都將工作交給唯一非阻塞 scanner；等待時主 loop 繼續服務 Wi-Fi、HTTP 與 runtime。Serial 最後只輸出一次對應結果，請等待結果後再送下一條命令。API 仍使用原本 200 networks／錯誤 envelope，不新增 202 輪詢指令；完成前 token 失效時回 401。Scan request deadline 為 15 秒，driver stop ACK 另有 2 秒 cleanup 期限，未確認時不釋放 radio。輸入容量及 overflow 規則不變。
