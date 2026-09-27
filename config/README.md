# 功能編譯設定

`features.ini` 是預設設定。每個功能只接受 `0`（不編入）或 `1`（編入）；指定設定檔中省略的功能視為 `0`，與 OpenWrt 的未選取功能一致。預設提供的 `features.ini` 明確開啟全部九個功能。未知欄位、重複欄位、錯誤值及不合法相依都會使建置失敗。這些設定決定韌體能力，不能透過 runtime API 補開。

| 設定 | 關閉時的結果 | 相依 |
| --- | --- | --- |
| `sleep` | 移除睡眠排程器與 `/api/sleep` 路由；系統時鐘仍可設定。 | 可獨立使用，允許無面板的定時睡眠。 |
| `epaper` | 移除面板服務、SPI、EPDIMG 處理與所有 `/api/epaper` 路由。保留既有板級安全腳位處理。 | 必須明確設 `storage=1`。 |
| `storage` | 移除 userdata filesystem 與 data-only reset；回收分區給 app。設定、內嵌網頁及 Flash 資訊仍保留。 | 關閉時也必須設 `epaper=0`、`user_files=0`。 |
| `auth` | 移除 session/token 服務與 `/api/auth` 路由；原有保護路由放行。 | AP 密碼仍保留，改用 `PUT /api/wifi/ap/password`。 |
| `user_files` | 移除 generic file list/upload/download/delete 路由及 console `ls`/`stat`。 | 必須明確設 `storage=1`；不影響 epaper 的專用圖片路由。 |
| `mdns` | 移除 `.local` 宣告與服務發現。 | Wi-Fi、IP 連線與 captive DNS 仍保留。 |
| `battery` | 移除電池 ADC 取樣；device power 欄位回 `null`。 | 無。 |
| `console` | 移除 human command 與 serial API adapter。 | 啟動／診斷 Serial log 仍保留。 |
| `status_led` | 移除 LED 模式指示；板級 boot 關燈與舊 hold 清理仍保留。 | 無；`sleep=0` 時一般開機後恆亮；預設 10% 亮度由 board profile 設定。 |

```sh
# 預設九個功能全開，使用專案自己的前端
make build

# 複製 features.ini 後依需要修改，再指定檔案
make build FEATURES=config/features.ini WEB=user

# 九個功能全關，保留 HTTP、Wi-Fi、設定與系統管理
make build FEATURES=config/profiles/minimal.ini WEB=none

# 相容舊指令：只覆寫所選設定檔的 sleep
make build FEATURES=config/features.ini SLEEP=0
```

`[sleep]` 的 `idle_timeout_seconds` 接受 1–86400，預設 600；`ignore_usb_host` 接受 0/1，預設 0。`SLEEP` 未傳入時使用設定檔，傳入時只覆寫 `sleep` 功能。直接執行 PlatformIO 時使用 `platformio.ini` 的 `custom_features`，可用 `custom_sleep` 明確覆寫 sleep；不要在 `build_flags` 重複定義功能 macro。

建置工具會產生固定內容的 effective INI、C/C++ header、JSON 與分區 CSV。Sleep 統一使用 `IOT_FEATURE_SLEEP` 作為唯一的 C/C++ 開關。原始碼以 `#if IOT_FEATURE_*` 包住相依整合，並由 source filter 排除未啟用模組的 `.cpp`。Release manifest 記錄九個功能、有效設定 SHA-256 與分區 layout，驗證時會檢查實際 `partitions.bin`。

| 分區 | `storage=1` | `storage=0` |
| --- | --- | --- |
| `app0` | `0x10000` / `0x1F0000`（1984 KiB） | `0x10000` / `0x3D8000`（3936 KiB） |
| `userdata` | `0x200000` / `0x1E8000`（1952 KiB） | 無，空間歸入 `app0` |
| `user_nvs` | `0x3E8000` / `0x8000` | 相同 |
| `coredump` | `0x3F0000` / `0x10000` | 相同 |

切換 storage 會改變分區用途，不保證保留既有上傳檔案。無 storage 韌體成功執行 early boot 時會撤銷 userdata 已初始化標記；重新啟用 storage 時重建 filesystem。若切換後從未啟動無 storage 韌體，標記無法證明舊檔案可用；需要明確 data reset／recovery。既有 mount failure 不會被一般啟動靜默格式化。設定分區位置不變。

能力查詢使用公開 `GET /api/features` 或 `GET /api/features?name=epaper`，詳細 contract 見 [API Reference](../docs/SPEC_API_REFERENCE.md)。功能 support 表示有編入，不保證硬體或服務目前 ready。

無板驗證：

```sh
make test-native
make test-tools
make test-web
make -C user-web-project test
make -C user-web-project test-production
python3 tools/feature-test/run.py --build
```

最後一項檢查所有 512 種設定的合法性，並編譯 14 組代表性設定、檢查被裁切服務的 ELF symbol；`--all` 可編譯全部 320 組合法設定。紀錄保存於 ignored `tmp/verification/`，不代表板上 smoke test。
