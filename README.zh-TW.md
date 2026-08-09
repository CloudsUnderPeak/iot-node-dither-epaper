# IOT-Node-Bedrock

[English](README.md)

**[開啟線上 Demo](https://cloudsunderpeak.github.io/iot-node-bedrock/)**

為 ESP32 IoT 產品準備的可重用 Wi-Fi 與裝置監控基石。

IOT-Node-Bedrock 是讓 ESP32 作為 IoT 節點時可直接延伸的基礎平台。它提供每個連網裝置都需要的基本能力，包括 AP／STA mode 設定、首次連線引導、fallback connectivity、設定保存與地端裝置監控，讓不同產品能在其上加入自己的感測器、控制邏輯、自動化與操作介面。

目前以 **DFRobot FireBeetle 2 ESP32-C6** 為開發與發行驗證目標。

## 為什麼做這個專案

任何 ESP32 IoT 裝置在執行自身產品功能前，都需要可靠地連上網路、維持可管理狀態、保存網路設定，並提供基本裝置狀態。若每個感測器、控制器、gateway 或設備都重新實作這些底層能力，不只浪費開發時間，也容易產生不一致的行為。

IOT-Node-Bedrock 將這些共通需求整理成可重用的基石平台。你可以沿用它的 Wi-Fi modes、地端管理介面、REST API、serial console 與裝置監控，再向上建立真正屬於自己 IoT 產品的硬體與軟體功能。

## 使用體驗

1. ESP32 第一次開機，自動建立專屬 Wi-Fi。
2. 使用手機或電腦連上裝置，開啟內建管理頁。
3. 搜尋並選擇附近的 Wi-Fi，輸入連線資訊。
4. 儲存後立即套用；若連線失敗，可自動保留備援 AP，避免裝置失聯。

日常使用時，不必登入就能查看網路、硬體與儲存空間狀態；需要變更設定時，才進入受保護的管理區。

## 特色

- **完全地端運作**：沒有 CDN、雲端帳號或外部網路，離線環境也能完成設定。
- **免安裝 App**：使用瀏覽器即可操作，支援桌面與行動裝置版面。
- **友善的第一次設定**：預設 AP、captive portal 與清楚的 Wi-Fi 設定流程，降低新手門檻。
- **不容易失聯**：STA 連線失敗或中斷時，可啟用 fallback AP 保留管理入口。
- **不只設定 Wi-Fi**：可查看裝置狀態、調整 hostname、管理登入密碼與執行 factory reset。
- **中英文介面**：內建 English 與繁體中文，可即時切換。
- **為整合而設計**：網頁、REST API 與 serial console 共用同一套行為，方便產品客製與自動化。
- **前端可替換**：管理頁與韌體包在同一個 app image，仍可換成自己的品牌、版面與產品功能。

## 適合誰

- 需要可重用 Wi-Fi 設定與基本監控能力的 ESP32 IoT 裝置 maker 與產品團隊。
- 不希望為裝置設定流程另外開發手機 App 的專案。
- 需要在展場、教室、實驗室或封閉網路中運作的設備。
- 想在可重用連網基礎上加入感測器、控制功能或產品邏輯的 firmware 開發者。
- 需要透過 REST API 或 AI／自動化工具管理裝置的應用。

## 體驗 Demo

靜態 Demo 使用假裝置資料，不需要 ESP32 即可體驗公開狀態頁、登入、Wi-Fi 掃描與各項設定流程。

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

開啟 [http://localhost:8000/](http://localhost:8000/)，並使用以下測試帳號進入 Settings：

```text
Username: admin
Password: password
```

專案也已準備 GitHub Pages workflow；在 repository 的 **Settings → Pages → Build and deployment → Source** 選擇 **GitHub Actions** 後，即可發布可分享的線上 Demo。

## 快速開始

環境需要 GNU Make、Python 3 與 PlatformIO CLI。建立產品網頁、firmware 與可燒錄 image：

```bash
make build
```

確認目前連接埠後，以單一命令完成清理、重建、驗證與燒錄：

```bash
pio device list
make deploy PORT=/dev/ttyACM0
```

燒錄完成後，尋找名稱類似 `esp32-device-XXXX` 的 Wi-Fi，連線並開啟 captive portal 或裝置 AP 位址，即可開始設定。

> 開發板的連接埠、pins、flash 與 USB 設定不可直接套用其他型號；移植前請先依目標板官方資料確認。

## 建立你自己的版本

這個 repository 同時是一個可直接 fork 的完整應用，以及可抽出的 ESP32 Wi-Fi foundation：

- 修改本專案內建管理頁時，請編輯 `builtin-web/`。
- 產品前端在 `user-web-project/` 開發。預設 `make build`（以及明確的
  `make build WEB=user`）會先重建該專案，再把 minify、gzip-only 的正式
  產物原子匯入被忽略的 `user-web/` 並包入 firmware。
- 需要內建管理頁時使用 `make build WEB=builtin`。`WEB=auto` 仍可消費
  既有 user import，沒有有效 import 時則退回 builtin。
- 若要建立只提供 API 的 firmware，明確執行 `make build WEB=none`；
  REST 與 serial API 仍可使用，但不提供設定頁。
- 想加入產品功能，可沿用既有登入、設定與 REST API 基礎。
- 想自動化管理，可直接串接 REST API，不必操作瀏覽器。

常用開發命令：

```bash
make build                              # 重建 user web 與完整 firmware 快照
make build WEB=builtin                  # 內建前端加 firmware
make build WEB=user                     # 重建使用者前端加 firmware
make build WEB=none                     # 不含任何前端的 firmware
make prepare-user-web                   # 只重建並匯入 user-web-project
make deploy WEB=none PORT=/dev/ttyACM0  # 建置、驗證並燒錄 API-only firmware
make web [WEB=...] [WEB_PROCESS=...]    # 只處理前端
make demo WEB_PROCESS=none              # 建立 build/latest/web 靜態 Demo
make esp                                # 由目前正式 web 建立 firmware
make verify [IMAGE=build/.../firmware.img]
make flash PORT=/dev/ttyACM0 [IMAGE=...] # 燒錄已驗證的既有快照
make clean                              # 移除 latest 與匯入的 user web，保留快照
make clean all                          # 移除 latest 與所有時間戳快照
make test                               # 建置並執行全部自動化測試
make test-web                           # 執行前端瀏覽器契約測試
```

`user-web/` 是 generated output，不應手動編輯。子專案產物已完成 minify
與 gzip，因此 `WEB_PROCESS=auto` 對它解析為 `none` 並保留原樣；內建前端
則解析為 `minify-gzip`。對 precompressed user import 明確指定
`minify-gzip` 會被拒絕。`WEB=auto` 永遠不會自動選擇無前端模式。

每次完整 firmware build 都會建立不可變的台北時間快照，並以內容完全
相同的實體副本替換 `latest/`：

```text
build/
├── latest/
│   ├── web-manifest.json
│   ├── web/
│   ├── binary/
│   │   ├── manifest.json
│   │   ├── bootloader.bin
│   │   ├── partitions.bin
│   │   ├── boot_app0.bin
│   │   └── firmware.bin
│   └── firmware.img
└── 20260726_0428/
    └── ...結構與 latest 相同...
```

`firmware.img` 是使用自訂副檔名的 deterministic tar.gz，archive 根目錄
只有 manifest 與四個 binary。所選前端會編譯進 `firmware.bin`，因此
不會另外出現 frontend binary；`WEB=none` snapshot 仍保留空的 `web/`
目錄，並在 manifests 記錄無前端狀態。專案持久產物的壓縮技術刻意只
保留 gzip：前端 `.gz` 與這個經 gzip 壓縮的 TAR package。

## 文件

產品行為、前端體驗、REST API 與工程設計都從[規格文件索引](docs/SPEC_INDEX.md)開始閱讀。若只是想試用或客製介面，不需要先理解 firmware 內部架構。

## 使用前注意

本專案目前定位為原型開發、受控實驗室與可信任本機網路中的連網基礎，並非可直接部署到不受信任環境的 hardened product。正式產品發布前，請依使用情境另外評估首次設定驗證、HTTPS、登入限制、Secure Boot、Flash Encryption 與 NVS Encryption。
