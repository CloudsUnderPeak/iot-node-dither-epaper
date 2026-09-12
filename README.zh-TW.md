# IOT-Node Dither E-Paper

[English](README.md)

**打開瀏覽器，讓喜歡的圖片成為六色電子紙上的風景。**

這是一套以 ESP32 驅動的電子紙應用，把圖片編輯、抖色、無線更新畫面與裝置管理整合在同一個地端網頁。拿起手機或電腦，連上裝置、調整圖片，就能送到電子紙顯示，不需要安裝專用 App，也不需要雲端帳號。

從圖片到電子紙，需要把影像處理、裝置連線與面板更新串在一起。IOT-Node Dither E-Paper 將這段流程整合成一套可直接操作、也能繼續延伸的應用：瀏覽器負責構圖與抖色，ESP32 負責保存圖片、驅動面板與回報裝置狀態，讓你專注在想呈現的畫面。

目前搭配 **DFRobot FireBeetle 2 ESP32-C6** 與 **Waveshare 7.3inch e-Paper HAT (E)，800 × 480 六色電子紙**。

## 從一張圖片，到一幅電子紙畫面

1. **連上裝置。** 加入裝置的 Wi-Fi，開啟網頁介面。
2. **準備圖片。** 匯入 PNG、JPEG 或 WebP，裁切構圖，調整亮度、對比與飽和度。
3. **預覽抖色。** 將圖片轉換成面板的六色色盤，預覽有限色彩呈現的細節。
4. **送上螢幕。** 按下「繪製到電子紙」，上傳結果並更新畫面。
5. **留待下次創作。** 下載 `.dither.png` 圖片專案，之後重新匯入，就能還原原圖與編輯設定。

可以把它做成個人照片展示、插畫相框，或作為下一個連網電子紙作品的起點。

## 連網基礎 × 瀏覽器創作

這個電子紙版本建立在兩個專案的能力之上，將裝置管理與圖片創作接成同一段使用體驗。

**[IOT-Node-Bedrock](https://github.com/CloudsUnderPeak/iot-node-bedrock) 是裝置端的基礎。** 本專案從它延伸，沿用 Wi-Fi 設定、AP／STA 模式、備援連線、設定保存、裝置狀態查詢，以及 REST API 與序列主控台的架構。對使用者來說，第一次連線與後續管理都有現成入口；對開發者來說，可以在既有連網能力上繼續加入電子紙功能。

**[Embedded Web Dithering](https://github.com/CloudsUnderPeak/embedded-web-dithering) 提供圖片創作介面。** 本專案採用它的 [six-color-epaper 分支](https://github.com/CloudsUnderPeak/embedded-web-dithering/tree/six-color-epaper)，將瀏覽器中的裁切、影像調整、色盤映射與抖色流程帶進裝置網頁。配合電子紙裝置模式，編輯器會對應面板尺寸與六色色盤，讓預覽後的圖片能直接送往螢幕。

**本專案把兩者連到實體電子紙。** 前端隨韌體一起燒錄，透過裝置 API 完成圖片傳送、繪製狀態查詢與面板測試；儲存在裝置上的六色色準也會回到編輯器，供選色與預覽使用。從連上 Wi-Fi、調整圖片到更新面板，都能在同一個網頁介面完成。

## 為什麼選擇這個專案

- **編輯器就住在裝置裡。** 網頁隨韌體一起提供，圖片在瀏覽器內處理；只要連上裝置，即使沒有外部網路也能操作。
- **為六色電子紙準備。** 裝置模式自動對應面板尺寸與色盤，支援橫向、直向裁切、抖色預覽與直接繪製。
- **讓色盤更貼近你的面板。** 在面板測試頁調整並儲存六色 RGB，編輯器會將校色結果用於選色與預覽。
- **作品可以接著改。** `.dither.png` 在一般看圖軟體中是圖片，重新匯入編輯器則能還原編輯內容；裝置離線或冷卻期間，也能下載圖片專案。
- **連網基礎已經備妥。** AP、STA、AP + STA、設定保存與可選的備援 AP，讓電子紙具備可延伸的 Wi-Fi 管理能力。
- **裝置狀態隨手可查。** 查看網路、儲存空間與硬體資訊，目標板也提供實測電池電壓與估算電量百分比。
- **融入你的操作方式。** 瀏覽器、REST API、序列主控台與隨附的 Python 圖片工具都能使用，介面支援繁體中文與 English。

## 線上體驗

**[開啟 Demo](https://cloudsunderpeak.github.io/iot-node-dither-epaper/)**

## 沒有硬體也能在本機體驗

下載專案後，用瀏覽器開啟 [`user-web-project/index.html`](user-web-project/index.html)：雙擊檔案，或在瀏覽器選擇「開啟檔案」，即可透過 `file://` 體驗。專案已包含 Demo 圖片，不必啟動伺服器。

也可以在專案根目錄啟動本機網頁伺服器：

```bash
python3 -m http.server 8000 --directory user-web-project
```

開啟 [localhost:8000](http://localhost:8000/)，即可體驗圖片編輯與模擬裝置頁面。兩種本機預覽方式都使用假裝置資料，不會操作實體面板；預覽管理帳密為 `admin` / `password`。

獨立的內建管理介面也提供模擬 Demo：

```bash
make demo WEB_PROCESS=none
python3 -m http.server 8000 --directory build/latest/web
```

開啟相同的本機網址，即可體驗內建管理介面。

## 快速開始

準備目標開發板、面板、合適的電源與可傳輸資料的 USB 連線，並在電腦安裝 **GNU Make、Python 3 與 PlatformIO CLI**。面板通電前，請依官方硬體文件確認接線與供電需求；其他開發板或面板型號需要另行確認移植設定。

建置包含電子紙產品前端的韌體：

```bash
make build
```

確認開發板的序列埠，再建置、驗證並燒錄：

```bash
pio device list
make deploy PORT=/dev/ttyACM0
```

將 `/dev/ttyACM0` 換成實際連接埠。`make deploy` 會先清理並重新建置，再進行燒錄。

裝置首次啟動後：

1. 用手機或電腦加入 `esp32-device-XXXX`。
2. 開啟自動導向的設定頁，或直接造訪 `http://192.168.4.1/`。
3. 開始準備圖片；若要設定 Wi-Fi 或裝置，使用 `admin` / `password` 登入。
4. 面板顯示可用時，將圖片繪製到電子紙。

預設建置會重新編譯 `user-web-project/` 並包入韌體，不需要另外上傳網頁檔案系統。

## 延伸成你的作品

你可以從這裡繼續打造自己的電子紙相框或展示裝置：沿用 IOT-Node-Bedrock 的連網與管理能力，在 Embedded Web Dithering 的編輯流程上調整介面，再透過電子紙 API 串接自己的圖片來源或自動化工具。

如果你的作品以其他感測器或控制器為主，可以從 [IOT-Node-Bedrock](https://github.com/CloudsUnderPeak/iot-node-bedrock) 的通用基礎開始；如果重點是圖片處理與有限色彩顯示，則可參考 [Embedded Web Dithering](https://github.com/CloudsUnderPeak/embedded-web-dithering)。這個 repository 提供的是兩者與六色電子紙整合後的應用起點。

| 想做什麼 | 從這裡開始 |
| --- | --- |
| 客製圖片編輯器與產品介面 | [`user-web-project/`](user-web-project/) |
| 改用內建管理介面 | `make build WEB=builtin` 與 [`builtin-web/`](builtin-web/) |
| 建置僅提供 REST 與序列存取的韌體 | `make build WEB=none` |
| 用 Python 轉圖並傳送到裝置 | [電子紙工具](tools/epaper/README.md) |
| 串接裝置控制與自動化 | [REST API 參考](docs/SPEC_API_REFERENCE.md) |
| 透過序列埠操作 | [主控台參考](docs/SPEC_CONSOLE_REFERENCE.md) |
| 了解建置、驗證與燒錄 | [開發工具](tools/README.md)與[發行流程](tools/release-build/README.md) |

產品前端原始碼放在 `user-web-project/`，透過 Git subtree 與上游的 [six-color-epaper 分支](https://github.com/CloudsUnderPeak/embedded-web-dithering/tree/six-color-epaper) 維持同步。客製時請修改這個目錄；`user-web/` 與 `build/` 為產生的輸出。開發與 subtree 命令請見 [Makefile](Makefile)，架構與行為說明請見[規格索引](docs/SPEC_INDEX.md)。

## 連接前先了解

- **畫面更新有固定間隔。** 每次完成實體繪製後，會進入 180 秒冷卻。介面會顯示剩餘時間，期間仍可繼續在本機編輯圖片。
- **目前支援特定面板。** 韌體只接受固定的 `EPDIMG` 格式，不直接接收 PNG 或 JPEG；產品編輯器與 Python 工具會代為轉換。
- **請在可信任的地端網路使用。** 初始 AP 不設密碼，預設管理密碼為 `password`。
