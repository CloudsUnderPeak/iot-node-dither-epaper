# 電子紙彩色低電量提示實作計畫

狀態：待實作。這份文件是目前有效的開發計畫，不代表現有 firmware 已支援。僅新增計畫及規格導引，不變更程式、燒錄或面板狀態。

## 1. 目標與範圍

已確認需求：鋰聚合物電池電量偏低時，在刷新後圖片的「觀看方向右上角」顯示彩色低電量符號；圖片上傳 API 可傳入表示直式／橫式的布林值。使用者尚無低電量實測值，初版門檻由資料研究提出。

建議第一版：在正常圖片刷新前判斷並合成圖示，一次完成整屏刷新。適用上傳後自動 draw、HTTP／serial 觸發的 stored refresh。白畫面與色票測試維持純測試內容。電量變化不自行觸發額外刷新；深睡眠不為本功能新增喚醒。充電後的提示於下一次符合解除條件的圖片刷新移除。

本次不新增電量計晶片、供電來源辨識、充電動畫、可調門檻 UI 或低電壓禁止刷新的產品規則。低電量提示不是過放保護，也不保證裝置在任意低電壓下仍能刷新。

## 2. 現況與實作缺口

- 目標為 FireBeetle 2 ESP32-C6 與 Waveshare 7.3inch e-Paper HAT (E)，800×480、六色 packed frame；無 PSRAM。
- `BatteryMonitor` 已每 10 秒讀七筆 calibrated ADC、取 median 並還原電池端 mV。`drawing` 期間暫緩取樣；API 讀 cached snapshot。
- 現有 `estimated_percent` 是自訂電壓曲線的粗估，沒有電流／溫度補償。
- 前端把 480×800 直式圖片順時針轉成 800×480，再編碼 EPDIMG v1；40-byte header 不保存觀看方向。
- `EpaperService` 驗證 gzip／EPDIMG 後串流傳送，`EpaperOrientedFrameSource` 再處理面板 mounting flip。Production 水平、垂直 flip 均為 true。
- 現有上傳為 `POST /api/epaper/image`，binary gzip body；成功即 queue draw。不能把布林欄位直接塞入 JSON body。

## 3. 低電量門檻研究與建議

### 資料證據

| 來源 | 可支持的結論 | 適用限制 |
| --- | --- | --- |
| [DFRobot 板卡資料](https://wiki.dfrobot.com/dfr1075)、[電壓量測範例](https://wiki.dfrobot.com/dfr1075/docs/17359) | GPIO0 是電池量測點，範例以 pin mV × 2 還原。板卡資料列出 V1.0 的 HM6245 LDO 與 V1.1 的 TPS62A02 DC-DC。 | 尚未確認使用者實板版本，不把任一穩壓器當成確定配置。 |
| [PKCELL LP803860 2000mAh 規格](https://cdn-shop.adafruit.com/datasheets/LiIon2000mAh37V.pdf) | 此單節 LiPo 範例標稱 3.7 V、充電 4.2 V、放電截止 3.0 V。 | 只是代表性電池，並非使用者電池型號；截止值不能套用為本裝置警告值。 |
| [Adafruit Li-Ion／LiPoly 電壓說明](https://learn.adafruit.com/li-ion-and-lipoly-batteries?view=all) | 典型 3.7／4.2 V 電池在約 3.7 V 有較長平台，低端電壓下降較快。 | 教學曲線不提供所有電池通用的電壓百分比對照。 |
| [TI SLUAAA1，§2、§5](https://www.ti.com/lit/an/sluaaa1/sluaaa1.pdf) | 電壓估算 SOC 應針對電池與負載建立曲線；負載變化、老化、溫度與卸載回彈均影響結果。 | Wi-Fi 與電子紙不是固定電流，不能直接宣稱某 mV 等於 20%。 |
| [HM6245 規格，Electrical Characteristics](https://dfimg.dfrobot.com/5d57611a3416442fa39bffca/wiki/6f630301d84caf0e92266e3c5cf11edc.PDF) | 3.0–4.0 V 輸出版本在 200 mA 下 dropout 典型 0.13 V、最大 0.16 V；更大負載需要更多餘裕。 | 不能用此測試電流代表整機實際刷新尖峰。 |
| [TPS62A02 規格，§7.3.2](https://www.ti.com/lit/ds/symlink/tps62a02.pdf) | 100% duty 下最低穩壓輸入仍與輸出電壓、負載及路徑電阻有關；這是降壓器。 | 2.5 V 可運作的輸入規格，不表示輸入 2.5 V 時能輸出穩定 3.3 V。 |

### 初版工程暫定值

| 參數 | 提案 | 理由 |
| --- | --- | --- |
| 進入低電量 | 有效電池端量測 `voltage_mv <= 3600` | 以提早提示為目的，避免等到接近電芯放電截止才嘗試留下警告。 |
| 解除低電量 | 連續三筆新的有效量測 `voltage_mv >= 3750` | 150 mV 遲滯加上時間確認，減少負載解除後電壓回彈造成閃爍式判定。 |
| 中間區間 | `3600 < voltage_mv < 3750` 保持先前狀態 | 避免每次刷新切換圖示。 |
| 取樣新鮮度 | draw 決策使用 age ≤15000 ms 的完整 sample | 配合現有 10 秒週期；不得把過期 sample 當作當前電量。 |
| 首次有效讀值 | ≤3600 為低電量，其餘為正常 | 不需要等待三筆才能提示；開機在遲滯區間時採明確初始規則。 |

以上 **3.60／3.75 V 是本專案的工程推論與候選預設，不是廠商認證門檻，也不是保證能完成刷新的最低電壓**。適用假設為常見單節、滿充 4.2 V 的鋰離子／鋰聚合物電池，不適用 LiFePO4、多節或其他充電上限的電池。

3.60 V 相對 3.3 V rail 只有 0.30 V 的名目差；以 HM6245 的 200 mA 最大 dropout 為例，尚未計入供電路徑與電池內阻就已消耗 0.16 V。這只能說明應提早警告，不能推導出已足夠的刷新餘裕。若實測在更高電壓已出現 rail 下陷，應提高提示門檻。

現有百分比曲線把 3.60 V 算成約 33%、3.75 V 算成約 57%；因此第一版圖示只表達「電壓偏低／建議充電」，不附 20% 等數字，也不為此擅自重寫百分比曲線。

低電量 latch 保存在 RAM，不每次取樣寫 NVS。解除計數只計不同 sample，維持 10 秒取樣時三筆約跨 20 秒；中間或低值打斷解除計數，失效／過期資料也重置計數。缺 sample、越出現有可估計範圍或 feature 關閉視為 unknown，不把 unknown 說成電量充足。刷新時若 unknown，沿用本次 boot 最後已知 latch；本次 boot 從未有有效讀值則不加低電量符號，並保留可診斷原因。已知低電量遇到量測失敗不自動清除。

## 4. 上傳 API 提案

保留原 binary body，以 query 傳遞布林語意：

```http
POST /api/epaper/image?is_portrait=true
Content-Type: application/octet-stream
Content-Encoding: gzip
Content-Length: <compressed byte count>

<gzip EPDIMG v1 bytes>
```

| 欄位 | 規劃 |
| --- | --- |
| `is_portrait=true` | 原觀看畫面為直式；client 已依既有規則順時針旋轉成面板 W×H。 |
| `is_portrait=false` | 原觀看畫面為橫式；client 提供面板 W×H。 |
| 省略 | 預設 false；client 新版應每次明確傳值。 |
| wire 格式 | URL query 只有文字，嚴格接受小寫 `true`／`false`，內部轉為 bool。 |
| 非法值 | 空值、`1`／`0`、`yes`、`TRUE` 等回 `400 invalid_is_portrait`。 |
| 重複欄位 | 即使兩個值相同也拒絕，回 `400 invalid_is_portrait`。 |
| 其他 query／overflow | 回 `400 unsupported_field`，不建立 upload session。 |

此旗標只描述觀看方向，**不要求 firmware 再旋轉上傳圖片**。EPDIMG 寬高、frame 長度與 CRC 契約維持現行 W×H。布林不能表達任意四向旋轉；第一版只支援既有橫式與順時針編碼的直式。實體裝置旋轉但未重新指定圖片方向，不會被自動偵測。

沿用 framing 411 → encoding 415 的既有優先序，再驗證 query，通過後才取得 service／storage 資源。`ApiServer` 僅擷取參數、保留重複資訊；`ApiRouter` 擁有合法值、預設值與錯誤回應規則。

成功維持 `202` 並自動排程一次 draw，新增 `data.is_portrait` 回顯 bool。`GET /api/epaper/image` metadata 同樣新增 bool；其餘欄位保留。Stored refresh 不接受方向覆寫，使用圖片已存方向。Binary upload 仍 HTTP only；serial stored refresh 共用相同疊圖邏輯。

下載仍是原 logical EPDIMG，無圖示、無方向欄位。要完整匯出／重新上傳觀看方向的 client，須另保存 metadata 的 `is_portrait`；不可宣稱 raw EPDIMG 本身包含方向。GET metadata 與下載之間若圖片可能被替換，client 應比對下載 header 的 generation／CRC 與 metadata，不把兩次請求當成原子快照。

## 5. 方向與圖片的持久化

必要條件：方向和圖片同一次 commit，重啟後一致；上傳中斷、驗證失敗、空間不足時，舊圖片與舊方向一併保留。不可只把方向留在 RAM，也不可先改全域 NVS bool 再替換圖片。

建議實作方案：在 firmware 儲存的單一 gzip member header 加入內部 orientation metadata，沿用同一個 temporary file → atomic rename。Gzip 的 FEXTRA 與 FHCRC 可承載擴充欄位與 header 完整性檢查；依據 [RFC 1952 §2.3.1](https://www.rfc-editor.org/rfc/rfc1952)。這是待實作的內部儲存設計，不要求 client 自行編碼 extension。

- 上傳端照現行方式驗證完整原 gzip／EPDIMG；以有界 header buffer 產生 canonical 儲存 header，內含專案識別、schema、bool 及 FHCRC。實作時明定 subfield layout 與長度。
- 方向以 API query 為準；不得讓 client 自帶 FEXTRA 覆蓋它。正規化時捨棄原 optional metadata，原 header 的合法性與 FHCRC 仍須先驗證。
- 原 DEFLATE payload 與 trailer 可串流保留，不重壓縮、不建立 raw temporary image。EPDIMG header、generation、frame CRC 與 logical download bytes 不變。
- Commit 前重新驗證待存 gzip 與方向 metadata；reader 可區分缺少 metadata、合法 metadata、未知 schema／損壞 metadata。後兩種錯誤須明確拒絕，不能默認橫式掩蓋損壞。
- 現有沒有 metadata 的圖片明訂為橫式，沒有可靠方式回推原方向；原直式圖片需重新帶值上傳。只讀預設，不自動重寫舊檔。
- `Content-Length` 與 upload offset 計入站 bytes；`stored_size_bytes` 計正規化後實際大小。Admission 預留固定 header 增量與現有 replacement 空間，分開檢查 upload 上限與 stored 上限；不能假定二者大小相同。
- 方向隨 userdata 圖片刪除／data reset／完整 reset 一起清除，settings reset 保留。Gzip header 仍受現有 1024-byte 邊界限制。

若原型發現 header 正規化成本過高，可以重新評估有交易保障的 metadata 儲存；不可退回兩個無協調的 file／NVS 寫入。此項原子性是驗收條件。

## 6. 疊圖與方向計算

圖示提案：48×28 px 白底區塊，內含黑色電池外框與接點、紅色低電量格；距觀看畫面上緣與右緣各 12 px。使用固定 EPD 黑色 code 0、白色 code 1、紅色 code 3，不經照片抖色；圖案以幾何／小型 mask 產生，不依賴 emoji 字型。尺寸為初版視覺預設。

以面板 logical 座標 W=800、H=480、觀看座標 (u,v) 定義：

| 方向 | 觀看畫布 | 圖示區塊左上角 | 映射到傳輸前 logical 座標 |
| --- | --- | --- | --- |
| 橫式 false | 800×480 | (740,12) | x=u，y=v |
| 直式 true | 480×800 | (420,12) | x=799-v，y=u |

先在觀看座標畫正立符號，再套用上表旋轉，最後沿用面板 mounting flip；production 最後一步為 x'=799-x、y'=479-y。上述 direct formula 僅示範現行尺寸，程式須使用 active profile 的 W/H，不硬編碼 800／480。測試同時核對位置與符號朝向，不能只檢查落在哪一角。

建議資料路徑：`StoredFrameSource → LowBatteryOverlayFrameSource → EpaperOrientedFrameSource → SPI → 一次 refresh`。Overlay 以 frame offset 計算每個 nibble 的 logical 座標，只覆蓋圖示區域；其他 bytes 不變。支援任意 chunk／奇偶 pixel 邊界、rewind 與 close 傳遞，且不配置完整 framebuffer。

在 panel wake 前鎖定本次 draw 的方向與電量決策；整個串流與重複解壓期間不重新切換符號。ADC 維持單一 producer，worker 只讀 cached snapshot；不能在 worker 同時呼叫現有未支援多 producer 的 ADC sample。Age 不符合規則時採第 3 節 unknown 政策。沿用 validation、80 MHz guard、watchdog、shutdown marker、180 秒 cooldown，無額外 refresh。

## 7. 實作工作拆分

| 階段 | 工作 | 完成判準 |
| --- | --- | --- |
| 1 | API bool admission、gzip metadata 儲存／讀取、容量核算 | 非法 request 不動舊圖；新圖與方向重啟後一致；故障注入證明 commit 原子性。 |
| 2 | 獨立低電量判定 policy，接入 BatteryMonitor cached sample | 門檻、遲滯、三筆解除、新鮮度、未知值、boot 初始化與 monotonic wrap 正確。 |
| 3 | Overlay frame source 與一次 draw snapshot | 兩方向及四種 mounting flip 組合像素正確；原圖 CRC／下載不變。 |
| 4 | 更新 user frontend 與 host tool | 前端由正式輸出尺寸／encoder 的 rotated 結果帶值；host upload CLI 可明確指定，產生直式圖時同步傳值。 |
| 5 | 整合文件與實板校準 | 更新正式 API／技術／前端 SPEC，完成可重現方向與供電驗收後才宣稱支援。 |

主要涉及 `src/api/ApiRouter.*`、`src/api/epaper/EpaperEndpoints.*`、`src/modules/http/ApiServer.*`、`src/modules/power/`、`src/modules/epaper/`、`src/main.cpp`、`tools/epaper/epaper_tool.py` 與 `user-web-project/` source。前端實作前讀取該子專案規範；不直接修改 `user-web/` 或 `build/latest/web/`。Battery feature 關閉時正常顯示原圖；epaper feature 關閉時不引入額外依賴。

## 8. 驗證與發布條件

### 自動化

- API：true／false／省略、非法／重複／未知 query、encoding/framing 優先序；回顯與 stored metadata 相符。
- 儲存：兩方向替換、CRC 錯誤、中斷、timeout、quota、write／rename 故障、重啟讀回；舊無 metadata 的橫式預設及損壞 metadata 拒絕。
- Gzip：optional header、FHCRC、header 跨 chunk、大小上界、canonical header 增量、DEFLATE／trailer／logical bytes 不變。
- 電量：3599／3600／3601、3749／3750／3751 mV，freshness 邊界、同一 sample 不重複計數、ADC 失敗、unknown、boot 遲滯區間與 feature off。
- 圖示：橫式／直式 × mounting 四組，逐像素期望、圖示外 bytes 不變、分塊讀取／rewind，以及高低電量切換後只發一次 refresh。
- Client：正式 pipeline 依輸出尺寸帶正確 bool，upload 202 後不追加 refresh；raw 下載重傳時方向另行保存。

### 實板

1. 確認實板 revision、電池型號／容量／保護板與 datasheet；這些是校準輸入，目前不是已知事實。
2. 對照電表與 ADC 在 4.2、3.8、3.75、3.6 V 附近的誤差。先以合適測試設備驗證供電邊界，避免用電池保護截止當成整機測試目標。
3. 在 3.6 V 附近量測刷新全程的 BAT、3.3 V rail 與負載尖峰，包含 Wi-Fi 活動；現有 drawing 暫停 ADC，10 秒 cached sample 無法捕捉尖峰，需外部儀器。
4. 確認警告出現時仍能完成正常刷新與安全關機；若 3.6 V 餘裕不足，提高進入門檻並重新評估解除門檻。不得以關閉 brownout／縮短 cooldown 解決。
5. 橫直實際擺放檢查黑色電池外框與紅色低電量格正立、右上留白、深色與淺色照片可讀性；檢查充電回升後下一次刷新移除，以及重啟後 stored refresh 方向。
6. 電池內阻／老化、溫度與板卡版本不同會影響結果，發布說明須列已驗證條件，不能從單一電池推論所有電池。

Native、client 與 build 檢查可先完成；硬體未量測前只稱「暫定門檻」，不稱「安全刷新電壓已驗證」。未完成實板量測不妨礙開發功能，但屬發布前的驗證缺口。所有執行紀錄放 `tmp/verification/`，本文件只保存方法與驗收條件。

## 9. 尚待實作／量測收斂

- 使用者電池與板卡 revision，以及可接受的 ADC 誤差與刷新 rail 最低值。
- 3.60／3.75 V 是否需依實測提高；目前不宣稱對應固定剩餘容量。
- Gzip orientation subfield 的確切 byte layout、schema 與預留容量，在階段 1 原型中定案。
- 若未來要求「刷新當中才跌破門檻也立即補符號」或「待機時即時出現」，需另立需求；本版決策時間固定於本次刷新前。
