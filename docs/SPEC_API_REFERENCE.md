# API Reference

日期：2026-07-26

本文件是提供整合開發者使用的 REST／serial API contract。產品行為與待決需求記錄於 [SPEC_BEHAVIOR.md](SPEC_BEHAVIOR.md)；內部 dispatcher 組織不屬於本文件。

Base URL：

```text
http://<device-ip>
http://<hostname>.local
```

預設 factory 設定：

| 設定 | 預設值 |
| --- | --- |
| hostname | `esp32-device` |
| Wi-Fi mode | `ap` |
| AP SSID | `<hostname>-<mac後四碼>` |
| AP password | 關閉，open AP |
| fallback to AP | 開啟 |
| admin username | `admin` |
| admin password | `password` |

## 目前範圍

- 使用 `ESPAsyncWebServer`。
- 網路 transport 僅支援 HTTP；USB serial 的 `api ...` adapter 共用 JSON resource contract。
- 靜態頁面與明確標示為公開的 `GET` API 不需要登入；Wi-Fi scan 與 auth session 需要 Bearer token。
- `POST` / `PUT` / `DELETE` 類寫入 API 預設需要 Bearer token；auth login/verify 與本文件列出的 e-paper action 是明確公開例外。
- REST JSON response 使用統一 envelope。
- STA 連線成功後會啟動 mDNS，並註冊 `_http._tcp` port `80`。
- AP/setup/fallback AP active 時會啟用 DNS wildcard captive portal，將 AP client 的任意 domain 查詢回覆為裝置 AP IP。
- Wi-Fi 設定支援單組 STA credential、STA DHCP/static IPv4、單組 AP SSID、AP default/static IPv4、AP 密碼開關與 STA 失敗 fallback AP。
- `userdata` 支援受保護的 generic file list、raw HTTP upload／download、single byte range 與 delete；e-paper 使用 reserved fixed file 與獨立 public resource family，不能由 generic PUT／DELETE 繞過 operation gate。

目前不在範圍內：

- 尚未實作完整 device config REST endpoint。
- 尚未持久化 AP DHCP server、AP channel、AP max clients、scan policy 等進階欄位。

## Response Envelope

REST API 的 JSON response 統一使用以下格式。欄位名稱固定為 `success`；`sucess` 不屬於 contract。

```json
{
  "success": true,
  "data": {},
  "message": "ok"
}
```

Serial console 的 `api METHOD PATH [token=<token>] [json]` 使用同一套 method/path/payload 語意，回傳同樣 envelope。裸 console 指令如 `status`、`wifi`、`storage` 維持人類可讀輸出。

錯誤 response 的 `data.code` 是給 client 判斷的穩定代碼；`message` 是人類可讀 fallback，不應作為程式分支依據。欄位相關錯誤使用 `data.fields` 回報 JSON path：

```json
{
  "success": false,
  "data": {
    "code": "invalid_field",
    "fields": ["interfaces.sta.ip_config.address"]
  },
  "message": "invalid STA IPv4 address"
}
```

目前固定 error code：

```text
invalid_json
payload_too_large
missing_field
invalid_field
unsupported_field
subnet_overlap
unauthorized
rate_limited
storage_error
storage_busy
storage_unavailable
content_length_required
unsupported_media_type
unsupported_transport
range_not_satisfiable
upload_incomplete
insufficient_storage
runtime_unavailable
wifi_scan_failed
wifi_scan_busy
wifi_connect_busy
wifi_connect_requires_ap
epaper_busy
invalid_epaper_image
epaper_image_not_found
epaper_unavailable
reserved_file
not_found
```

所有 `/api/...` HTTP error，包括空 body、malformed JSON、錯誤的 JSON `Content-Type`、超過大小限制與未知 route，都必須回上述 envelope，不得回裸文字或空的 HTTP error body。需要 JSON body 的 endpoint 只接受 `application/json`（允許 media type parameter），body 上限為 2048 bytes；超過時回 HTTP `413` 與 `payload_too_large`，其他 JSON transport 格式錯誤回 HTTP `400` 與 `invalid_json`。Serial `api ...` 的 malformed JSON 同樣回 `invalid_json`，超過 console input capacity 時回 `payload_too_large`。

設定更新在寫入前會先確認 runtime scheduler 可用；不可用時回 HTTP `503` 與 `runtime_unavailable`，且不修改 persisted config、session 或 NVS。一般成功 response 代表設定已完整持久化，且對應 runtime action 已保證由主迴圈執行；`PUT /api/wifi` 明確回 HTTP `202` 的 safe transition 是例外，代表完整 candidate 只在 RAM 排隊，必須再查詢 connection status 才能知道是否已持久化或 rollback。

## Auth

除明列的公開例外外，所有 `POST` / `PUT` / `DELETE` 預設需要 Bearer token。`/api/storage/files` collection 與 item 的 `GET` 也需要 Bearer token；未來若新增免登入例外，必須明確記錄在本文件。
Dynamic user-file item route 會先驗證 token，才開始 HTTP streaming storage session、判斷 serial raw transport 不支援，或執行 JSON delete；因此缺少或無效 token 固定先回 `401 unauthorized`。

公開例外：

- `POST /api/auth/login`
- `POST /api/auth/verify`
- `GET /api/epaper`
- `GET /api/epaper/status`
- `POST /api/epaper/image`
- `GET /api/epaper/image`
- `GET /api/epaper/image/download`
- `POST /api/epaper/image/refresh`
- `POST /api/epaper/image/white`
- `POST /api/epaper/image/palette`
- `GET /api/runtime/status`

token 放在 HTTP header：

```http
Authorization: Bearer <token>
```

token 使用 opaque random token。裝置只在 runtime 保存目前有效 token，重開機後失效；後登入者會讓前一個 token 失效。

範例：

```bash
curl -H 'Authorization: Bearer <token>' \
  -H 'Content-Type: application/json' \
  -X POST http://<device-ip>/api/wifi/reconnect \
  -d '{}'
```

## Endpoint 總表

| Method | Path | Auth | 用途 |
| --- | --- | --- | --- |
| `GET` | `/` | 否 | 韌體內嵌管理頁。 |
| `GET` | `/index.html` | 否 | 韌體內嵌管理頁。 |
| `GET` | `/api/alive` | 否 | 基本 API 存活檢查。 |
| `GET` | `/api/device` | 否 | 裝置與 runtime 資訊。 |
| `GET` | `/api/web` | 否 | 目前 Web bundle 的來源與 SHA-256，或無前端狀態。 |
| `GET` | `/api/wifi` | 否 | Wi-Fi runtime 狀態與可設定欄位。 |
| `GET` | `/api/wifi/scan` | 是 | 掃描附近 AP；只回 RSSI 大於 `-75 dBm` 的網路，最多 20 筆、單一掃描工作且至少間隔 10 秒。 |
| `POST` | `/api/wifi/connect` | 是 | 送出 SSID／password，非同步連線並在驗證成功後由 firmware 自動保存。 |
| `GET` | `/api/wifi/connect` | 是 | 查詢簡化 connect 或完整 Wi-Fi safe transition 的結果。 |
| `GET` | `/api/auth` | 否 | 取得登入所需的公開 auth 資訊。 |
| `POST` | `/api/auth/login` | 否 | 驗證帳密並回傳 token；會使前一個 token 失效。 |
| `POST` | `/api/auth/verify` | 否 | 只確認帳密是否正確，不發 token。 |
| `GET` | `/api/auth/session` | 是 | 驗證 Bearer token 是否仍有效，不發新 token。 |
| `POST` | `/api/auth/logout` | 是 | 撤銷目前 Bearer token。 |
| `PUT` | `/api/auth/password` | 是 | 修改 admin password；成功後目前 session 失效。 |
| `PUT` | `/api/wifi` | 是 | 更新並持久化 Wi-Fi 設定。 |
| `POST` | `/api/wifi/reconnect` | 是 | 重新套用目前 Wi-Fi 設定。 |
| `PUT` | `/api/system` | 是 | 更新 hostname 等系統設定。 |
| `POST` | `/api/system/reset` | 是 | 清空全部使用者設定與檔案並重啟。 |
| `POST` | `/api/system/reset/settings` | 是 | 只清空使用者設定並重啟。 |
| `POST` | `/api/system/reset/data` | 是 | 只清空使用者檔案並重啟。 |
| `GET` | `/api/storage` | 否 | 儲存區用途、能力與容量狀態。 |
| `GET` | `/api/storage/files` | 是 | 分頁列出公開 user files。 |
| `PUT` | `/api/storage/files/{name}` | 是 | 以 raw HTTP body 建立或原子替換檔案。 |
| `GET` | `/api/storage/files/{name}` | 是 | 下載完整檔案或單一 byte range。 |
| `DELETE` | `/api/storage/files/{name}` | 是 | 刪除檔案。 |
| `GET` | `/api/epaper` | 否 | 固定 panel、format、檔名、cooldown 與能力資訊。 |
| `GET` | `/api/epaper/status` | 否 | 動態 operation、retry、CPU、stored image 與 brownout 狀態。 |
| `POST` | `/api/epaper/image` | 否 | 上傳固定 `EPDIMG` 並自動排程 draw；raw HTTP only。 |
| `GET` | `/api/epaper/image` | 否 | 固定 image 的 header、CRC、generation 與 validity metadata。 |
| `GET` | `/api/epaper/image/download` | 否 | 下載完整固定 image 或 single byte range；raw HTTP only。 |
| `POST` | `/api/epaper/image/refresh` | 否 | 重新驗證並重畫 stored image。 |
| `POST` | `/api/epaper/image/white` | 否 | 動態產生全白 frame 並 draw。 |
| `POST` | `/api/epaper/image/palette` | 否 | 動態產生六色測試 frame 並 draw。 |
| `GET` | `/api/runtime/status` | 否 | 跨模組 activity、phase、CPU 與 blocked-resource 概況。 |

AP/setup/fallback AP active 時，裝置會啟用 DNS wildcard captive portal；SoftAP DHCP 將 AP IP 發布為 DNS server，並以 DHCP captive portal option 發布 portal URI。AP client 查詢任意 domain 都會回裝置 AP IP。純 STA LAN 模式不啟用 DNS wildcard，避免影響一般網路。DHCP captive portal option、DNS interception 與 detection endpoint 都只能提高 client 發現率；作業系統不保證自動顯示 portal，直接開啟 AP IP 的 `/` 始終是正式入口。

Captive portal detection endpoints 皆為 `GET`、不需登入，在 AP active 時由未知 GET route 的統一 fallback redirect 到 AP IP 的 `/`：

```text
/connecttest.txt
/wpad.dat
/generate_204
/redirect
/hotspot-detect.html
/canonical.html
/success.txt
/ncsi.txt
/startpage
```

一般未知頁面 `GET` request 由 AP 介面抵達且 AP active 時，也由同一 fallback redirect 到設定頁；AP + STA runtime 中，由 STA LAN 介面抵達的未知 GET 不得導向 AP IP。直接連線 AP IP 的 `/` 會載入內建 `index.html`。但 `/api/...` 與 `/assets/...` 不會被 captive redirect 攔截；API 或 asset 不存在時仍回正常 404。

## 輸入字串限制

| 欄位 | 限制 |
| --- | --- |
| `hostname` | 1 到 31 字元；只允許英文字母、數字、`-`；不可用 `-` 開頭或結尾。 |
| STA/AP SSID | 最多 32 字元；需要對應 mode 時不可為空；只允許 printable ASCII。 |
| STA security | `interfaces.sta.security`；第一階段支援 `"wpa"` 與 `"open"`。 |
| STA static IPv4 | `address`、`gateway`、`netmask` 必填；DNS 最多兩筆。address 與 gateway 必須是同 subnet 的可用 host address。 |
| AP IPv4 | 傳入 `interfaces.ap.ip_config` 時 `mode` 必填，支援 `"default"` 與 `"static"`。`default` 不得傳入 `address`／`netmask`；`static` 必須傳入兩者。目前 SoftAP DHCP 限制 static netmask 為 `/24` 到 `/28`。 |
| admin username | 固定為 `admin`，不提供修改。 |
| admin/STA password | 8 到 63 個 printable ASCII 字元（ASCII `0x20`–`0x7E`）。STA open security 不使用 password。 |
| user filename | 1 到 64 bytes ASCII；第一字元為英數，後續只允許英數、`.`、`_`、`-`；不允許連續 `..`、slash、backslash、空白或 percent encoding。 |
| password mask | `********` 表示保留原 password，不會寫入 NVS。 |
| 其他 JSON string | 最多 128 字元；只允許 printable ASCII。 |

## Static Page

### `GET /`

從 app image 內嵌資產提供 `/index.html`；content type 與 content encoding 依實際 raw／gzip build output 決定。

一般 frontend bundle 缺少 index 時：

```json
{
  "success": false,
  "data": {
    "code": "not_found"
  },
  "message": "index not found"
}
```

明確以 `WEB=none` 建置時，`GET /` 與 `GET /index.html` 都回 HTTP 404：

```json
{
  "success": false,
  "data": {
    "code": "not_found"
  },
  "message": "frontend not bundled"
}
```

### `GET /assets/...`

從 app image 內嵌資產提供 UI 的 CSS 與 JavaScript。`WEB=none` 沒有可匹配的資產，因此使用一般 404 行為。前端結構記錄於 [SPEC_FRONTEND_TECHNICAL.md](SPEC_FRONTEND_TECHNICAL.md)。

## Alive

### `GET /api/alive`

成功：

```json
{
  "success": true,
  "data": {},
  "message": "ok"
}
```

## Device

### `GET /api/device`

成功範例：

```json
{
  "success": true,
  "data": {
    "chip_model": "ESP32-C6",
    "chip_revision": 0,
    "cpu_cores": 1,
    "flash_mb": 4,
    "heap_used_percent": 24,
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "hostname": "esp32-device",
    "config_state": "persisted",
    "config_recovery_reason": "none"
  },
  "message": "ok"
}
```

`config_state` 可能為 `persisted`、`factory_defaults_created` 或 `recovery_defaults`。只有 recovery path 會讓 `config_recovery_reason` 不是 `none`；目前可能為 `unsupported_schema`、`storage_error` 或 `invalid_persisted_config`，且不包含設定值或其他敏感內容。

## Web

### `GET /api/web`

公開、唯讀，不需要 Bearer token。回報目前 app image 的 Web bundle identity：

```json
{
  "success": true,
  "data": {
    "source": "builtin",
    "sha256": "5ca5c74f06bbfb3205d9754690cf955f685f0cd08f9ee0b53f8045f23a5c7709"
  },
  "message": "ok"
}
```

`source` 可能為 `builtin`、`user` 或 `none`。Builtin／user 的 `sha256` 是 64 個小寫十六進位字元，對 build 完成後、實際編譯進 firmware 的完整 Web 輸出樹計算；它與同一 release snapshot 的 `web-manifest.json` 及 `binary/manifest.json` 中 `web_sha256` 相同。檔案相對路徑、檔案大小、檔案內容或 raw／gzip 輸出改變都會改變此值。

明確以 `WEB=none` 建置時：

```json
{
  "success": true,
  "data": {
    "source": "none",
    "sha256": null
  },
  "message": "ok"
}
```

此時 release snapshot 仍以內部 `web_sha256` 驗證空的 `web/` artifact，但裝置 API 使用 `null` 表示沒有可供辨識的 Web bundle。

此 resource 不回傳 version、檔案數量、payload 大小或 `user_web_sha256`。它只能識別 Web bundle，不代表整包 firmware image hash。Serial adapter 可使用同一 contract：

```text
api GET /api/web
```

## Auth Endpoints

### `GET /api/auth`

回傳登入所需的公開 auth 資訊，不回傳 password、token 或 session 狀態。目前 username 固定為 `admin`；client 從此 resource 讀取 username。

成功：

```json
{
  "success": true,
  "data": {
    "username": "admin"
  },
  "message": "ok"
}
```

### `POST /api/auth/login`

驗證管理帳密並建立新的 runtime session。登入成功會使先前 token 失效。

Request body：

```json
{
  "username": "admin",
  "password": "password"
}
```

成功：

```json
{
  "success": true,
  "data": {
    "authenticated": true,
    "token_type": "Bearer",
    "token": "<opaque-token>"
  },
  "message": "authenticated"
}
```

帳密錯誤時回 `401`，`data.authenticated` 為 `false`。缺少 username/password 或 body 不是 JSON object 時回 `400`。

### `POST /api/auth/verify`

只驗證帳密，不建立 session、不發 token，也不使現有 token 失效。Request body 與 login 相同。

帳密吻合時：

```json
{
  "success": true,
  "data": {
    "authenticated": true
  },
  "message": "credentials valid"
}
```

帳密不吻合仍回正常 envelope，`data.authenticated` 為 `false`、message 為 `credentials invalid`。缺少必要欄位或 body 格式錯誤時回 `400`。

### `GET /api/auth/session`

驗證目前 Bearer token 是否有效。此 endpoint 不發新 token。

成功：

```json
{
  "success": true,
  "data": {
    "authenticated": true
  },
  "message": "authenticated"
}
```

### `POST /api/auth/logout`

撤銷 request 使用的目前 Bearer token。成功後該 token 不得再存取任何受保護 endpoint。

成功：

```json
{
  "success": true,
  "data": {},
  "message": "logged out"
}
```

token 缺少或無效時回 `401`：

```json
{
  "success": false,
  "data": {
    "code": "unauthorized",
    "authenticated": false
  },
  "message": "unauthorized"
}
```

### `PUT /api/auth/password`

修改 admin password。此 endpoint 需要 Bearer token，不要求 old password。成功後目前 session 失效，使用者需用新 password 重新登入。若目前 `interfaces.ap.password_enabled` 為 `true`，成功 response 完成後會立即排程裝置重啟，使 AP 使用新密碼；為 `false` 時不重啟 Wi-Fi 或裝置。

Request body：

```json
{
  "password": "New pass!"
}
```

成功：

```json
{
  "success": true,
  "data": {
    "session": "invalidated"
  },
  "message": "admin password updated"
}
```

## Wi-Fi

### `GET /api/wifi`

回傳目前 Wi-Fi runtime 狀態。此 endpoint 會回傳 SSID，但不回傳任何 password。

成功範例：

```json
{
  "success": true,
  "data": {
    "mode": "ap",
    "configured_mode": "ap",
    "fallback_to_ap": true,
    "interfaces": {
      "sta": {
        "enabled": false,
        "ssid": "",
        "security": "wpa",
        "state": "disabled",
        "ip": "0.0.0.0",
        "ip_config": {
          "mode": "dhcp",
          "address": "",
          "gateway": "",
          "netmask": "",
          "dns": []
        }
      },
      "ap": {
        "enabled": true,
        "state": "active",
        "ssid": "esp32-device-A1B2",
        "password_enabled": false,
        "ip": "192.168.4.1",
        "ip_config": {
          "mode": "default",
          "address": "192.168.4.1",
          "netmask": "255.255.255.0"
        }
      }
    }
  },
  "message": "ok"
}
```

`data.mode` 是目前 runtime 狀態；STA 連線失敗並 fallback 成功時，可能為 `ap_sta`。`data.configured_mode` 是持久化設定中的使用者意圖。

### `GET /api/wifi/scan`

需要有效 Bearer token，並限制掃描結果與頻率：

- 只回傳 RSSI 大於 `-75 dBm` 的網路；`-75 dBm` 本身不回傳。
- 通過門檻的網路中，最多回傳 RSSI 最強的 20 筆結果。
- 同一時間只允許一個掃描工作。
- 每次掃描開始或完成後，至少間隔 10 秒才能再次掃描。
- token 缺少或無效時回 `401`；rate limit 時回 `429`，`data.retry_after_seconds` 表示建議等待秒數。
- 持久化 STA 尚在連線並占用 radio 時回 `409 wifi_scan_busy`，`data.retry_after_seconds` 表示建議等待秒數；connect request 占用 radio 時回 `409 wifi_connect_busy`。

成功範例：

```json
{
  "success": true,
  "data": {
    "networks": [
      {
        "ssid": "example",
        "rssi": -54,
        "channel": 6,
        "encryption_type": 4,
        "encryption": "wpa2",
        "hidden": false
      }
    ]
  },
  "message": "ok"
}
```

### `POST /api/wifi/connect`

需要有效 Bearer token。只接受 SSID 與 optional password；空 password 表示 open network。若省略 password 且 SSID 等於目前已儲存的 WPA 網路，沿用裝置內既有 password。Firmware 使用 DHCP 與 AP + STA 連線，保留目前 active AP 作為管理入口；沒有 active AP 時回 `409 wifi_connect_requires_ap`。

Candidate credential 在驗證成功前只存 RAM。STA association、有效 IPv4 與 runtime subnet 檢查成功後，firmware 自動持久化；失敗不覆蓋原設定。成功不代表 Internet、DNS 或 gateway 可達。

```json
{
  "ssid": "example",
  "password": "example-password"
}
```

Accepted response 使用 HTTP `202`：

```json
{
  "success": true,
  "data": {
    "state": "connecting"
  },
  "message": "wifi connection started"
}
```

已有連線工作時回 `409 wifi_connect_busy`。Connect 與 scan 不同時操作 radio。

### `GET /api/wifi/connect`

需要有效 Bearer token。只回傳簡化狀態，不回傳 SSID、password、內部 transaction id 或 candidate payload。

```json
{
  "success": true,
  "data": {
    "state": "connected",
    "failure_code": "none",
    "ip": "192.168.1.50",
    "ap_shutdown_in_seconds": 0
  },
  "message": "ok"
}
```

`state` 只有 `idle`、`connecting`、`connected`、`failed`。`connected` 表示 credential 已驗證且完整 candidate 已持久化；最終 STA mode 的 AP 可能仍在 5 秒 grace period。`ap_shutdown_in_seconds` 只在該 grace period 大於 `0`，其他狀態為 `0`。

`failure_code` 為 `none`、`connect_timeout`、`station_disconnected`、`subnet_overlap`、`ip_configuration_failed`、`radio_unavailable`、`management_ap_unavailable`、`ap_configuration_failed`、`runtime_apply_failed` 或 `storage_error`。Failed candidate credential 與其他敏感 candidate 欄位會立即清除。

## Serial Console

Serial console 提供本機操作與測試入口，不取代 REST API。連線方式、完整輸入規則、human commands 與操作範例見 [SPEC_CONSOLE_REFERENCE.md](SPEC_CONSOLE_REFERENCE.md)；本節只定義 serial API contract。

人類 CLI 提供 diagnostics、userdata inspection 與 RAM-only config staging，輸出不使用 JSON envelope：

```text
help
status
device
wifi
storage
session
scan
ls [path] [offset]
stat <path>
config show|get|set|changes|revert|status|commit ...
```

`api ...` command 使用 REST method、path、payload 語意，回傳 REST JSON envelope。受保護寫入 command 使用顯式 `token=<token>`，避免 console 暗中保存授權狀態。

範例：

```text
api GET /api/wifi
api GET /api/web
api GET /api/wifi/scan token=<token>
api GET /api/auth
api POST /api/auth/login {"username":"admin","password":"password"}
api GET /api/auth/session token=<token>
api POST /api/auth/logout token=<token> {}
api PUT /api/wifi token=<token> {"mode":"ap","fallback_to_ap":true,"interfaces":{"sta":{"ssid":"","security":"wpa","ip_config":{"mode":"dhcp","address":"","gateway":"","netmask":"","dns":[]}},"ap":{"ssid":"esp32-device-A1B2","password_enabled":false,"ip_config":{"mode":"default"}}}}
api POST /api/wifi/reconnect token=<token> {}
api PUT /api/system token=<token> {"hostname":"esp32-device"}
api POST /api/system/reset token=<token> {}
api POST /api/system/reset/settings token=<token> {}
api POST /api/system/reset/data token=<token> {}
```

注意：

- human CLI 輸出是給人看的文字。
- `api ...` 輸出是給自動化或 AI 驗證用的 JSON envelope。
- Serial console 不提供 `wifi set ...` 這類第二套設定語法。
- `api GET /api/storage/files` 與 `api DELETE /api/storage/files/{name}` 可由 serial 使用；serial command 不接受 query string，所以多頁 inspection 應改用 `ls`。Raw file `GET`／`PUT` 只支援 HTTP；token 有效時在 serial 回 `415 unsupported_transport`，缺少或無效 token 仍先回 `401 unauthorized`。
- token 仍由 `AuthService` runtime 保存；重開機後失效。

### `PUT /api/wifi`

更新完整 Wi-Fi desired configuration。一般成功時設定已寫入 NVS，且已保證在 response 送出後由 runtime 套用。若 persisted mode 是 AP、runtime management AP active，且目標 mode 是 STA 或 AP + STA，則改為 safe transition：完整 candidate 只放 RAM、沿用現有 AP 驗證 STA，成功後才由 firmware 自動持久化與完成最終 mode。

Request body：

```json
{
  "mode": "sta",
  "fallback_to_ap": true,
  "interfaces": {
    "sta": {
      "ssid": "MyWiFi",
      "security": "wpa",
      "password": "MyWifi-2026!",
      "ip_config": {
        "mode": "static",
        "address": "192.168.1.50",
        "gateway": "192.168.1.1",
        "netmask": "255.255.255.0",
        "dns": ["192.168.1.1", "1.1.1.1"]
      }
    },
    "ap": {
      "ssid": "esp32-device-A1B2",
      "password_enabled": false,
      "ip_config": {
        "mode": "static",
        "address": "192.168.4.1",
        "netmask": "255.255.255.0"
      }
    }
  }
}
```

`mode` 支援：

```text
sta
ap
ap_sta
```

`off` 只可能出現在 `GET /api/wifi` 的 runtime 狀態；REST `PUT /api/wifi` 不接受 `off`，避免關閉唯一管理入口。

成功範例：

```json
{
  "success": true,
  "data": {},
  "message": "wifi updated"
}
```

AP 到 STA／AP + STA 的 safe transition 使用 HTTP `202`：

```json
{
  "success": true,
  "data": {
    "state": "connecting"
  },
  "message": "wifi transition started"
}
```

Client 接著輪詢 `GET /api/wifi/connect`。此 transition 使用單一 association 與 15 秒總 deadline，分成三個 5 秒觀察區間但不在區間交界重新連線。失敗時回復原 persisted config 與 management AP；成功後最終 STA mode 保留 AP 5 秒，最終 AP + STA 的 AP 變更最後套用。

備註：

- `interfaces.sta.password` 傳入 `********` 代表保留原 STA password。
- `interfaces.sta.password` 欄位不存在時，不處理、不清空、不覆蓋既有 STA password。
- `interfaces.sta.security` 第一階段支援 `"wpa"` 與 `"open"`。
- 開放 Wi-Fi 不使用空字串隱含；必須傳入 `"security": "open"` 讓使用者明確確認。
- `interfaces.sta.security` 為 `"open"` 時不使用 STA password。
- `interfaces.sta.ip_config.mode` 支援 `"dhcp"` 與 `"static"`。切回 DHCP 時會清除已保存的 static IPv4/DNS 欄位。
- `interfaces.ap.ip_config.mode` 支援 `"default"` 與 `"static"`。`"default"` 使用 firmware 預設 address/netmask；`"static"` 使用 request 指定的值。AP 不是 DHCP client，因此不支援 `"dhcp"`。
- `GET /api/wifi` 在 AP `ip_config.mode` 為 `"default"` 時仍回傳解析後的預設 `address` 與 `netmask`；`PUT /api/wifi` 選擇 `"default"` 時不得傳入這兩個欄位。
- AP client 的自動配址由 SoftAP DHCP server 負責，與 AP 自己的 `ip_config.mode` 無關。
- AP 與 STA 可能同時啟用時，已知的 static subnet 若重疊會在持久化前回 `400`。AP+STA 使用 STA DHCP 且連線後才發現重疊時，會拒絕 STA 套用並保留 AP runtime 入口。
- `interfaces.ap.password` 目前不會被持久化；AP password 由 admin password 提供，並由 `interfaces.ap.password_enabled` 控制是否啟用。
- 一般 `200` 更新若改變 `interfaces.ap.password_enabled`，設定持久化與成功 response 完成後會立即排程裝置重啟，runtime session 因 reboot 失效。其他一般 Wi-Fi 欄位只重新套用 Wi-Fi runtime。AP→STA／AP + STA 的 `202` safe transition 不提前 reboot，AP credential 由驗證成功後的 final apply 套用。
- `sta` 連線失敗且 fallback 開啟時，runtime `mode` 會變成 `ap_sta`；若使用者未設定 AP，使用預設 AP 設定。
- `configured_mode` 表示使用者設定意圖，`mode` 表示目前 runtime 狀態。
- 若傳入 AP DHCP、channel 等尚未支援的欄位，API 會回 `400`，且不會持久化或套用 request 中的任何設定。

完整更新規則：

- `PUT /api/wifi` 是完整且原子性的 desired configuration replacement，不是 partial merge。
- `mode`、`fallback_to_ap`、`interfaces`、`interfaces.sta` 與 `interfaces.ap` 必填。
- 除 STA password 外，範例中的所有非密碼設定欄位都必須提供；`ip_config.mode` 會決定哪些 address 欄位必填或必須為空／不存在。
- request 含未知欄位、缺少欄位、型別錯誤或跨欄位衝突時回 `400`，不寫入或套用任何部分。
- 一般更新在所有驗證通過後才完整寫入 NVS，response 完成後立即套用；AP password enabled/disabled 變更依上述規則改為重啟裝置。
- AP 到 STA／AP + STA 的 safe transition 在所有驗證通過後只排入 RAM；STA association、有效 IPv4 與 runtime subnet 檢查成功後才原子寫入完整 candidate。失敗不得覆寫原設定。
- Safe transition 或簡化 connect 尚未完成時，另一個 Wi-Fi update、connect、reconnect 或 scan 回 `409 wifi_connect_busy`，不得中斷正在驗證的 candidate。
- Runtime scheduler 不可用時在寫入 NVS 前回 `503 runtime_unavailable`，不產生 partial success。

### `POST /api/wifi/reconnect`

重新套用目前已持久化的 Wi-Fi 設定。

成功：

```json
{
  "success": true,
  "data": {},
  "message": "wifi reconnecting"
}
```

## System

### `PUT /api/system`

更新裝置層級設定。hostname 不屬於 `/api/wifi`；`GET /api/device` 會回傳目前 hostname。修改 hostname 後會在 response 送出後重新套用網路服務與 mDNS。Runtime scheduler 不可用時在持久化前回 `503 runtime_unavailable`。

Request body：

```json
{
  "hostname": "esp32-device"
}
```

成功：

```json
{
  "success": true,
  "data": {
    "hostname": "esp32-device"
  },
  "message": "system updated"
}
```

### `POST /api/system/reset`

完整 factory reset。API 先在預設 `nvs/storage_meta/reset_pending` 持久化 `all`，response 送出後立即排程 `ESP.restart()`；early boot erase 整個 `user_nvs` 並格式化 `userdata`，完成後回到 factory default AP setup。此操作會清除所有 Wi-Fi、hostname、管理員設定與使用者檔案，但不清除 ESP32 系統／PHY NVS 或 coredump。

### `POST /api/system/reset/settings`

只重設設定。持久化 `reset_pending=settings` 並重啟；early boot erase 整個 `user_nvs`，保留 `userdata`。重啟後回到 factory default AP setup。

### `POST /api/system/reset/data`

只重設資料。持久化 `reset_pending=data` 並重啟；early boot 格式化 `userdata`，保留 `user_nvs` 內的 Wi-Fi、hostname 與管理員設定。

三個 endpoint 都需要有效 Bearer token，且不接受 request body 欄位。持久化 reset intent 前會確認 restart scheduler ready；否則回 `503 runtime_unavailable` 且不改變 pending 狀態。Reset intent 寫入或 read-back 驗證失敗時回 `500 storage_error`，不排程 restart。成功 response 相同：

成功：

```json
{
  "success": true,
  "data": {},
  "message": "reset scheduled; restarting"
}
```

## Storage

### `GET /api/storage`

成功範例：

```json
{
  "success": true,
  "data": {
    "flash": {
      "total_bytes": 4194304,
      "fixed_regions": {
        "bootloader_reserved_bytes": 32768,
        "partition_table_bytes": 4096
      },
      "partitions": [
        {
          "id": "nvs",
          "type": "data",
          "subtype": "nvs",
          "offset_bytes": 36864,
          "size_bytes": 20480
        },
        {
          "id": "otadata",
          "type": "data",
          "subtype": "ota",
          "offset_bytes": 57344,
          "size_bytes": 8192
        },
        {
          "id": "app0",
          "type": "app",
          "subtype": "ota_0",
          "offset_bytes": 65536,
          "size_bytes": 2031616
        },
        {
          "id": "userdata",
          "type": "data",
          "subtype": "spiffs",
          "offset_bytes": 2097152,
          "size_bytes": 1998848
        },
        {
          "id": "user_nvs",
          "type": "data",
          "subtype": "nvs",
          "offset_bytes": 4096000,
          "size_bytes": 32768
        },
        {
          "id": "coredump",
          "type": "data",
          "subtype": "coredump",
          "offset_bytes": 4128768,
          "size_bytes": 65536
        }
      }
    },
    "app": {
      "partition_id": "app0",
      "frontend_bundled": true,
      "capacity": {
        "total_bytes": 2031616,
        "firmware_image_bytes": 1257440,
        "frontend_payload_bytes": 39189,
        "available_bytes": 774176
      }
    },
    "user": {
      "partition_id": "userdata",
      "filesystem": "littlefs",
      "mounted": true,
      "capabilities": {
        "file_upload": true,
        "file_list": true,
        "file_download": true,
        "file_delete": true
      },
      "capacity": {
        "total_bytes": 1933312,
        "used_bytes": 327680,
        "available_bytes": 1605632
      },
      "limits": {
        "max_upload_bytes": 1605632,
        "reserved_bytes": 65536,
        "allocation_unit_bytes": 4096,
        "max_filename_bytes": 64
      }
    }
  },
  "message": "ok"
}
```

`flash.fixed_regions` 回報 partition table 以外仍占用 Flash 的固定區域；`bootloader_reserved_bytes` 是 bootloader 可使用的 `0x0000`–`0x8000` 範圍，不是當下 bootloader binary 的檔案大小。`flash.partitions[]` 是裝置目前實際 partition table 的公開摘要。位於 Flash 前段的預設 `nvs` 保存 ESP32 系統／PHY 資料及本專案獨立的 `storage_meta` 初始化／reset intent；尾端 32 KiB `user_nvs` 只保存可由 settings reset 整片清除的使用者設定。`app` 描述 running `app0`：`firmware_image_bytes` 是整包已燒錄韌體大小，已包含 `frontend_payload_bytes`，因此兩者不可相加；`available_bytes` 是 `0x10000`–`0x200000` app partition 尚可容納的 firmware build 大小。`WEB=none` 時 `frontend_bundled` 為 `false`、`frontend_payload_bytes` 為 `0`；builtin／user build 則為 `true` 與實際 embedded payload 大小。

`user` 是 file API 使用的獨立 persistent partition。`user.capacity` 已經扣除 `limits.reserved_bytes` 並向下對齊 `limits.allocation_unit_bytes`。它保證 `total_bytes = used_bytes + available_bytes`，且 `available_bytes = limits.max_upload_bytes`；client 可直接將這個值解讀為當下可接受的單一新檔 payload 上限。掛載失敗時 user capacity 與 upload limit 都為 `0`。

四個 `capabilities.file_*` 表示目前 firmware 提供的 list／upload／download／delete contract。`reserved_bytes` 是固定容量政策，即使 filesystem 未掛載仍維持回報 65536；此時 capacity 與 `max_upload_bytes` 才歸零。Upload 在取得 user storage operation gate 後重新計算同一個 limit；先前查詢結果不保留額度。檔案操作進行中 capacity 回最後一個完整操作結束後的穩定 snapshot。

### `GET /api/storage/files`

需要 Bearer token。只接受 query `limit` 與 `cursor`，不接受 request body；`limit` 預設 50、範圍 1–100。`cursor` 是 opaque string，client 不得解析或自行產生；檔案集合在翻頁期間改變時不保證 snapshot consistency。

成功範例：

```json
{
  "success": true,
  "data": {
    "files": [
      {
        "name": "photo.jpg",
        "size_bytes": 12345,
        "media_type": "image/jpeg"
      }
    ],
    "next_cursor": "v1:50"
  },
  "message": "ok"
}
```

最後一頁的 `next_cursor` 為 `null`。未知／重複 query、無效 cursor、超出範圍的 limit 回 `400`；operation gate 已被其他 file／inspection operation 使用時回 `409 storage_busy`。

### `PUT /api/storage/files/{name}`

需要 Bearer token。Request body 是檔案的 raw bytes，不是 JSON、form 或 multipart；client 應使用 `Content-Type: application/octet-stream`，也可完全省略 Content-Type。`multipart/form-data`、`application/x-www-form-urlencoded` 與 `text/plain` 會被 HTTP framework 當 form／plain-post 解析，因此明確回 `415 unsupported_media_type`，不接受其內容相依的模糊行為。Request 必須提供可嚴格解析且與實際 body 完全相等的 `Content-Length`，不支援 `Transfer-Encoding`；缺少或無效長度回 `411 content_length_required`。超過開始操作時重新計算的 limit 回 `413 payload_too_large`，並在 `data.max_upload_bytes` 回報該次限制。此 endpoint 不接受 query fields。

新檔成功回 `201`：

```json
{
  "success": true,
  "data": {
    "name": "photo.jpg",
    "size_bytes": 12345,
    "created": true
  },
  "message": "file uploaded"
}
```

替換既有檔案成功回 `200` 且 `created` 為 `false`。Firmware 先串流到不可見的 internal temporary file，完成 byte-count 與 close／size 驗證後才 atomic rename over target；斷線、short write、驗證或 rename 失敗不截斷原 target。單次寫入 chunk 最大 4096 bytes；這是 firmware memory bound，不限制 HTTP client 的完整 body 大小。

### `GET /api/storage/files/{name}`

需要 Bearer token。成功 body 是 raw file bytes，不使用 JSON envelope；不接受 query fields 或 request body。完整下載回 `200`，並回：

```text
Accept-Ranges: bytes
Cache-Control: no-store
X-Content-Type-Options: nosniff
Content-Disposition: inline; filename="photo.jpg"
```

只接受一個 RFC byte range：`bytes=start-end`、`bytes=start-` 或 `bytes=-suffix-length`。有效 range 回 `206` 與精確 `Content-Range`；multiple range、空檔上的 range 或超出檔案的 range 回 `416 range_not_satisfiable`，並回 `Content-Range: bytes */<file-size>`。

已知 extension 的 media policy 如下；比對不區分大小寫。其他 extension 使用 `application/octet-stream` 與 `attachment` disposition。

| Extension | `Content-Type` | Disposition |
| --- | --- | --- |
| `.txt` | `text/plain; charset=utf-8` | inline |
| `.json` | `application/json` | inline |
| `.mp4` | `video/mp4` | inline |
| `.png` | `image/png` | inline |
| `.jpg`, `.jpeg` | `image/jpeg` | inline |
| `.bmp` | `image/bmp` | inline |
| `.gif` | `image/gif` | inline |
| `.webp` | `image/webp` | inline |

### `DELETE /api/storage/files/{name}`

需要 Bearer token，不接受 query fields 或 request body。成功回 `200`：

```json
{
  "success": true,
  "data": {
    "name": "photo.jpg",
    "deleted": true
  },
  "message": "file deleted"
}
```

檔案不存在回 `404 not_found`；filesystem 未掛載回 `503 storage_unavailable`；operation gate busy 回 `409 storage_busy`。所有 file endpoint 都不公開 internal temporary file。

## E-paper

本節全部 endpoint 都是公開例外，不要求 Bearer token。這代表同網路 client 可讀取／替換固定圖片並觸發 draw；180 秒 cooldown 是面板保護，不是 authentication。JSON status、metadata 與 action 可由 HTTP 或 serial `api ...` 使用；raw upload/download 只支援 HTTP。

`EPDIMG` request 固定為 192,040 bytes：40-byte little-endian header 後接 192,000-byte packed frame。Header layout 依序為 8-byte magic `EPDIMG\0\0`、uint32 version `1`、uint32 header size `40`、uint32 width `800`、uint32 height `480`、uint32 frame bytes `192000`、uint32 CRC32 與 non-zero uint64 generation。每 byte 的 high／low nibble 分別是左／右 pixel，只允許 `0,1,2,3,5,6`。

### `GET /api/epaper`

只回 build/board profile 固定資訊，不得包含 runtime state、目前 CPU、stored file availability 或 last result。成功回 `200`：

```json
{
  "success": true,
  "data": {
    "panel": {"model": "waveshare-7in3e", "width": 800, "height": 480, "colors": 6, "color_codes": [0, 1, 2, 3, 5, 6]},
    "image": {"name": "epaper-current.epd", "format": "epdimg", "header_bytes": 40, "frame_bytes": 192000, "upload_bytes": 192040},
    "refresh": {"cpu_mhz": 80, "cooldown_seconds": 180, "automatic_on_boot": false},
    "capabilities": {"upload": true, "metadata": true, "download": true, "refresh": true, "white": true, "palette": true}
  },
  "message": "ok"
}
```

### `GET /api/epaper/status`

只回 cached dynamic snapshot。成功回 `200`：

```json
{
  "success": true,
  "data": {
    "state": "cooldown",
    "phase": null,
    "busy": true,
    "can_upload": false,
    "can_draw": false,
    "can_download": true,
    "retry_after_seconds": 173,
    "cpu_mhz": 160,
    "panel_state": "sleeping",
    "shutdown_method": "power_off_then_deep_sleep",
    "recovery_required": null,
    "stored_image": {"available": true, "valid": true},
    "last_operation": {"source": "uploaded", "result": "success", "error_code": "none"},
    "last_reset_reason": "software",
    "brownout_detected": false,
    "brownout_during_draw": false
  },
  "message": "ok"
}
```

- `state` 固定為 `idle`、`uploading`、`queued`、`drawing`、`cooldown`、`unavailable`；drawing `phase` 固定為 `prewake`、`initializing`、`transferring`、`refreshing`、`powering_off`、`sleeping`、`quiescing` 或 `null`。
- `panel_state` 固定為 `inactive`、`active`、`sleeping`、`unknown`；`shutdown_method` 固定為 `none`、`power_off_then_deep_sleep`、`logical_only`。
- `logical_only` 必須搭配 `state: unavailable`、`panel_state: unknown`、`recovery_required: full_power_cycle`、`can_draw: false`，不得表示成功 safe-off。
- `retry_after_seconds` 在 cooldown 中向上取整；無可自行到期 cooldown 時為 `null`。Brownout + active marker 映射為 `last_operation.result: interrupted`、`error_code: brownout`、兩個 brownout bool 為 `true`，並要求 full power cycle。

### `POST /api/epaper/image`

Body 是 raw `EPDIMG`，不是 JSON、form 或 multipart；`Content-Type` 可為 `application/octet-stream` 或省略，`Content-Length` 必須精確等於 `192040`。成功 atomic commit 後回 `202` 與 `state: queued`，client再輪詢 status。中止、invalid frame 或 storage failure 不覆蓋舊檔、不排程 draw。Serial 回 `415 unsupported_transport`。

### `GET /api/epaper/image`

回 fixed file 的 cached/validated metadata，不回 raw bytes。Generation 固定以 decimal string、CRC32 固定以 8-digit uppercase hex string 表示：

```json
{
  "success": true,
  "data": {
    "name": "epaper-current.epd",
    "format": "epdimg",
    "media_type": "application/octet-stream",
    "size_bytes": 192040,
    "header_bytes": 40,
    "frame_bytes": 192000,
    "width": 800,
    "height": 480,
    "generation": "1234567890123456789",
    "crc32": "89ABCDEF",
    "valid": true
  },
  "message": "ok"
}
```

不存在回 `404 epaper_image_not_found`；存在但 validation失敗回 `422 invalid_epaper_image`，可在 data 回不含 raw bytes 的 stable `reason`。

### `GET /api/epaper/image/download`

HTTP body 是 raw 192,040-byte fixed file，不使用 JSON success envelope。完整下載回 `200`，single Range 與 generic download 相同，回 `206` 或 `416 range_not_satisfiable`。Response 至少包含：

```text
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="epaper-current.epd"
Accept-Ranges: bytes
Cache-Control: no-store
X-Content-Type-Options: nosniff
```

Serial 回 `415 unsupported_transport`。Userdata gate 正被 upload、validation 或 frame read 使用時回 `409 storage_busy`；純 physical refresh 與 cooldown 已釋放 storage gate，可下載。

### `POST /api/epaper/image/refresh`

不接受 body fields；重新驗證並重畫 fixed file，不寫檔。無有效 stored image 回 `404 epaper_image_not_found`，接受後回 `202`。

### `POST /api/epaper/image/white`

不接受 body fields；動態串流 `0x11` 共 192,000 bytes，不建立或替換 user file，接受後回 `202`。

### `POST /api/epaper/image/palette`

不接受 body fields；動態產生 4-pixel black border及 black／white／yellow／red／blue／green vertical bars，不建立 pixel array 或 user file，接受後回 `202`。

E-paper synchronous error mapping：

| HTTP | code | 情境 |
| ---: | --- | --- |
| `409` | `epaper_busy` | uploading、queued、drawing、cooldown；cooldown 另回 `retry_after_seconds`。 |
| `422` | `invalid_epaper_image` | header、尺寸、generation、palette、CRC 或 byte count 錯誤。 |
| `404` | `epaper_image_not_found` | metadata／refresh／download 沒有有效 fixed file。 |
| `503` | `epaper_unavailable` | pin、SPI、worker、safety store 未 ready或 panel state unknown。 |
| `403` | `reserved_file` | Generic file PUT／DELETE 嘗試修改 `epaper-current.epd`。 |

Storage error沿用 `storage_busy`、`storage_unavailable`、`insufficient_storage`、`storage_error`。已回 `202` 後的 async failure只寫入 `last_operation.error_code`，至少包含 `cpu_frequency_failed`、`brownout`、`busy_timeout`、`operation_watchdog_timeout`、`frame_read_failed`、`panel_init_failed`、`refresh_failed`、`power_off_failed`、`sleep_failed`、`panel_state_unknown`；software restart shutdown失敗另由 runtime 診斷回 `epaper_shutdown_failed`。

## Runtime Status

### `GET /api/runtime/status`

公開、唯讀，回 cached cross-module activity，不掃 filesystem、不等待 bus，也不作全域 admission lock：

```json
{
  "success": true,
  "data": {
    "state": "degraded",
    "busy": true,
    "activity": "epaper_draw",
    "phase": "refreshing",
    "cpu_mhz": 80,
    "normal_cpu_mhz": 160,
    "blocked_resources": ["epaper"],
    "last_reset_reason": "software",
    "brownout_detected": false
  },
  "message": "ok"
}
```

`state` 固定為 `normal`、`degraded`、`restarting`。Validation時 blocked resources為 `epaper,userdata`，frame transfer為 `epaper,userdata,spi`，physical refresh與 cooldown只為 `epaper`；`busy: true` 不表示 HTTP 或 Wi-Fi 全部不可用。E-paper retry/cooldown的權威仍是 `/api/epaper/status`。
