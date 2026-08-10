(function (app) {
    app.i18n['zh-TW'].helpMaxInputLongEdge = '大型輸入會縮小到工作圖最長邊 {value} px';
    app.i18n['zh-TW'].helpMaxResizeOutputSize = '{value} px';
    app.i18n['zh-TW'].helpBundle = {
        ui: {
            documents: '說明文件',
            breadcrumbs: '麵包屑導覽',
            documentPagination: '文件分頁',
            openDocuments: '瀏覽文件',
            closeDocuments: '關閉文件目錄',
            onThisPage: '本頁內容',
            previous: '上一篇',
            next: '下一篇',
            startGroup: '開始使用',
            algorithmsGroup: '演算法指南',
            overviewGroup: '總覽',
            before: '來源',
            after: '結果',
            comparisonHint: '拖曳以比較來源與結果',
            characteristics: '主要特性',
            bestFor: '適合情境',
            avoidWhen: '注意事項',
            controls: '相關控制',
            structure: '結構',
            targetColor: '目標顏色',
            nearestColor: '選出的調色盤顏色',
            distanceExplorerHint: '調整目標顏色，觀察各種距離公式會選出哪個調色盤顏色。',
            detailsPending: '尚未提供詳細使用說明。',
            noExtraControls: '沒有額外控制項',
            listSeparator: '、'
        },
        visuals: {
            family: {
                title: '同一來源的三種抖色家族',
                settings: '合成漸層 480x288 · E6 · 最近色 · Euclidean BT.709 · 100%',
                floyd: 'Floyd-Steinberg',
                floydCaption: '把量化誤差傳給鄰近像素，形成細緻而自然的紋理。',
                bayer: 'Bayer 8x8',
                bayerCaption: '使用快速、固定且可辨識的規律 threshold 圖樣。',
                dot: 'Dot Halftone',
                dotCaption: '群聚網點形成刻意的印刷質感。'
            },
            mapping: {
                title: '比較調色盤映射',
                settings: '合成漸層 480x288 · Bayer 8x8 · E6 · Euclidean BT.709 · 100%',
                nearest: '最近色',
                nearestCaption: '每個取樣只由一個最接近的調色盤顏色表示。',
                pair: '雙色混合',
                pairCaption: '讓兩個調色盤顏色分配像素，近似中間色。',
                tri: '三色混合',
                triCaption: '使用三個調色盤顏色保留更多顏色關係。'
            },
            distance: {
                title: '比較色彩距離',
                settings: '合成漸層 480x288 · Floyd-Steinberg · E6 · 最近色 · 100%',
                bt709: 'Euclidean BT.709',
                bt709Caption: '加權 RGB 強調綠色差異。',
                rgb: 'Euclidean RGB',
                rgbCaption: '三個 RGB 通道以相同權重參與平方距離，也是專案預設值。',
                ciede: 'CIEDE2000',
                ciedeCaption: '在 Lab 空間比較人眼感知差異，可能選出不同鄰近色。'
            }
        },
        documents: {
            home: {
                title: '說明中心',
                eyebrow: 'Embedded Web Dithering 文件',
                lead: '先了解完整操作流程，再認識抖色演算法、調色盤映射與色彩距離如何一起決定輸出。',
                sections: [
                    {
                        id: 'choose-a-path',
                        title: '選擇閱讀方向',
                        cards: [
                            { title: '第一次認識專案', body: '從用途、支援格式、隱私與專案限制開始。', linkId: 'introduction' },
                            { title: '立即處理一張圖片', body: '依序完成圖片輸入、裁切、調色盤、抖色與 PNG 匯出。', linkId: 'quick-start' },
                            { title: '理解輸出差異', body: '比較演算法家族，了解 Mapping 與 Distance 為何會改變像素。', linkId: 'dithering' }
                        ]
                    },
                    {
                        id: 'mental-model',
                        title: '先建立一個簡單觀念',
                        paragraphs: [
                            'Palette 決定輸出允許出現哪些顏色；Color Distance 衡量輸入色與候選色有多接近；Palette Mapping 決定用一色、兩色或三色表示輸入；Dither Algorithm 再把這些選擇分布到像素位置。'
                        ],
                        visual: { type: 'pipeline' }
                    },
                    {
                        id: 'recommended-order',
                        title: '建議閱讀順序',
                        bullets: [
                            '先讀專案介紹，確認工具符合目標顯示器的工作流程。',
                            '使用內建 Demo 完成一次快速操作。',
                            '透過抖色演算法文件選擇想要的紋理家族。',
                            'Palette 穩定後，再微調 Palette Mapping 與 Color Distance。'
                        ]
                    }
                ]
            },
            introduction: {
                title: '專案介紹',
                eyebrow: '開始使用',
                lead: 'Embedded Web Dithering 用來把一般圖片準備成適合電子紙與其他有限色彩顯示器的結果，所有處理都在瀏覽器內完成。',
                sections: [
                    {
                        id: 'what-it-does',
                        title: '這個專案可以做什麼',
                        paragraphs: [
                            '載入本機圖片後，可以裁切成目標比例、調整輸出尺寸與色調、選擇調色盤、套用抖色、檢查真實像素，最後匯出 PNG。',
                            '固定處理順序為 Crop、Resize、Adjust、Palette、Dither，讓每個色彩設定的效果保持可預期。'
                        ]
                    },
                    {
                        id: 'good-fit',
                        title: '適合的使用情境',
                        table: {
                            headers: ['使用情境', '能提供的幫助'],
                            rows: [
                                ['電子紙顯示器', '先以固定色票預覽圖片，再交給裝置使用。'],
                                ['嵌入式 LCD 或 LED 面板', '對齊精確輸出尺寸並降低顏色複雜度。'],
                                ['像素風與印刷風創作', '提供規律、噪聲、誤差擴散與網點紋理。']
                            ]
                        }
                    },
                    {
                        id: 'local-and-offline',
                        title: '本機與離線優先',
                        bullets: [
                            '來源圖片只在瀏覽器中處理，不會上傳到伺服器。',
                            '原始專案可直接開啟 index.html，不需要 npm、後端或 CDN。',
                            '執行時使用的資源都包含在專案內。'
                        ],
                        note: '切換 SPA 頁面時會保留目前圖片與設定，但重新整理或關閉頁面後不會還原工作區。'
                    },
                    {
                        id: 'formats',
                        title: '輸入與輸出',
                        table: {
                            headers: ['項目', '支援範圍'],
                            rows: [
                                ['輸入', 'PNG、JPEG/JPG、WebP'],
                                ['輸出', 'PNG'],
                                ['透明背景', '透明輸入像素會先與白色合成'],
                                ['編輯工作圖長邊', { key: 'helpMaxInputLongEdge', fact: 'maxInputLongEdge' }],
                                ['可設定的單邊輸出上限', { key: 'helpMaxResizeOutputSize', fact: 'maxResizeOutputSize' }]
                            ]
                        }
                    }
                ]
            },
            'quick-start': {
                title: '快速操作',
                eyebrow: '開始使用',
                lead: '從載入到匯出完整處理一張圖片。第一次使用時，內建 Demo 是最快的練習方式。',
                sections: [
                    {
                        id: 'load-and-prepare',
                        title: '1. 載入並準備圖片',
                        steps: [
                            { title: '載入來源', body: '把支援的檔案拖到空白預覽區、選擇「選擇檔案」，或從圖片輸入載入 Demo。' },
                            { title: '裁切', body: '選擇固定比例，接著縮放、平移、旋轉、反轉與設定填色。按下完成或收合裁切以進入編輯。' },
                            { title: '尺寸', body: '輸入目標像素寬度或高度，另一邊會依裁切比例連動。' }
                        ]
                    },
                    {
                        id: 'adjust-colors',
                        title: '2. 調整色調與顏色',
                        steps: [
                            { title: '調整', body: '在減色之前調整亮度、對比與飽和度。' },
                            { title: '調色盤', body: '保留「原始」以萃取代表色、選擇裝置 preset，或修改色票建立「自訂」。' },
                            { title: '抖色', body: '先使用 Floyd-Steinberg、最近色與 Euclidean RGB，每次只改一個設定。' }
                        ]
                    },
                    {
                        id: 'inspect-and-export',
                        title: '3. 檢查並匯出',
                        bullets: [
                            '「原圖」顯示 prepare 後、edit effects 前的圖片。',
                            '「結果」把處理後輸出縮放到預覽區。',
                            '「展開」以一個 CSS pixel 對應一個輸出 pixel，方便近距離檢查。',
                            '「匯出 PNG」會依設定尺寸重新執行正式 pipeline。'
                        ]
                    },
                    {
                        id: 'starter-recipe',
                        title: '穩定的起始設定',
                        table: {
                            headers: ['設定', '建議起點'],
                            rows: [
                                ['Palette', '原始，8 色'],
                                ['Dither Algorithm', 'Floyd-Steinberg'],
                                ['Palette Mapping', '最近色'],
                                ['Color Distance', 'Euclidean RGB'],
                                ['Error Strength', '100%'],
                                ['Serpentine', '關閉；單方向條紋明顯時再開啟']
                            ]
                        },
                        note: '請以預定顯示尺寸檢查匯出的 PNG 或「展開」畫面；縮放預覽可能會重採樣單像素圖樣。'
                    }
                ]
            },
            dithering: {
                title: '抖色演算法',
                eyebrow: '演算法指南',
                lead: '抖色只使用目前 Palette 的顏色，透過像素排列模擬原本不存在的色調。請依紋理與用途選擇演算法家族。',
                sections: [
                    {
                        id: 'what-is-dithering',
                        title: '抖色改變了什麼',
                        paragraphs: [
                            '沒有抖色時，有限色票容易產生大片平坦區域與突兀色帶。抖色把色帶轉換成可控制的紋理，讓視覺把點陣融合成中間色調。',
                            '本專案所有演算法都只輸出 Palette 顏色；它們改變顏色的空間分布，不會創造額外顏色。'
                        ],
                        visual: { type: 'comparison', set: 'family' }
                    },
                    {
                        id: 'family-comparison',
                        title: '選擇演算法家族',
                        table: {
                            headers: ['家族', '視覺特性', '常見用途'],
                            rows: [
                                ['Error Diffusion', '自然細粒、會跟隨影像細節', '照片、漸層、一般預設'],
                                ['Ordered / Blue Noise', '固定矩陣或高頻 mask', '像素風、UI、快速且可預期的輸出'],
                                ['Dot-based', 'class 排序點陣或群聚印刷網點', '特殊紋理、海報與印刷風格']
                            ]
                        },
                        cards: [
                            { title: 'Error Diffusion', body: '六種把量化誤差向後傳遞的演算法。', linkId: 'error-diffusion' },
                            { title: 'Ordered / Blue Noise', body: 'Bayer 矩陣與固定的 blue-noise-like mask。', linkId: 'ordered' },
                            { title: 'Dot-based Algorithms', body: 'Dot Diffusion 與群聚網點 Halftone 是兩種不同的點陣結構。', linkId: 'dot' }
                        ]
                    },
                    {
                        id: 'strength-controls',
                        title: '強度控制',
                        bullets: [
                            'Error Strength 調整 Error Diffusion 與 Dot Diffusion 傳遞的誤差。',
                            'Dither Strength 調整 Bayer 與 Blue Noise 的 threshold 影響。',
                            'Dot Density 調整 Dot Halftone 的群聚網點取樣密度。',
                            '切換演算法時，共用強度會重設為 100%。'
                        ]
                    }
                ]
            },
            'error-diffusion': {
                title: 'Error Diffusion',
                eyebrow: '抖色演算法',
                lead: '每個像素先映射到 Palette 顏色，再把 RGB 量化誤差傳給指定的後續鄰近像素。',
                sections: [
                    {
                        id: 'how-it-works',
                        title: '誤差如何移動',
                        paragraphs: ['Kernel 決定哪些尚未處理的像素會收到誤差與各自比例。小 kernel 快速而銳利；寬 kernel 通常更平滑，但會接觸更多鄰近像素。'],
                        visual: { type: 'kernel', title: 'Floyd-Steinberg kernel', rows: [['', '目前', '7/16'], ['3/16', '5/16', '1/16']] },
                        note: 'Serpentine 會逐列交替掃描方向，可減少支援此功能的演算法產生單方向條紋。'
                    },
                    {
                        id: 'algorithms',
                        title: '可用演算法',
                        capabilityFamily: 'error-diffusion',
                        algorithms: [
                            { id: 'floyd-steinberg', name: 'Floyd-Steinberg', summary: '精簡的兩列 kernel，也是最穩定的通用預設。', characteristics: '細緻、細節平衡、中等成本', bestFor: '照片、漸層、第一輪比較', avoid: '平坦區可能出現蟲狀紋理', controls: 'Error Strength；可選 Serpentine', structure: '4 個接收點，divisor 16' },
                            { id: 'atkinson', name: 'Atkinson', summary: '只擴散八分之六的誤差，保留更強對比。', characteristics: '銳利、中間調較亮、復古', bestFor: '線稿、人像、經典電腦點陣風格', avoid: '可能失去暗部或亮部階調', controls: 'Error Strength', structure: '跨三列的 6 個接收點' },
                            { id: 'jarvis', name: 'Jarvis-Judice-Ninke', summary: '密集 5x3 kernel，把誤差廣泛分布。', characteristics: '漸層平滑、紋理柔和、運算較多', bestFor: '照片與大面積色調轉換', avoid: '可能柔化小細節', controls: 'Error Strength；可選 Serpentine', structure: '12 個接收點，divisor 48' },
                            { id: 'sierra-lite', name: 'Sierra Lite', summary: '僅三個接收點，優先速度與邊緣清晰。', characteristics: '快速、銳利、方向性較高', bestFor: '小型素材與快速預覽', avoid: '條紋或顆粒較明顯', controls: 'Error Strength；可選 Serpentine', structure: '3 個接收點，divisor 4' },
                            { id: 'stevenson-arce', name: 'Stevenson-Arce', summary: '稀疏且寬廣的 kernel，把誤差分布到大範圍。', characteristics: '自然、局部方向性低、成本較高', bestFor: '大型藝術輸出與平滑區域', avoid: '可能有散點且處理較慢', controls: 'Error Strength', structure: '11x3 內 12 個稀疏接收點' },
                            { id: 'adaptive-fs-3x3', name: 'Adaptive FS 3x3', summary: '在 Floyd-Steinberg 擴散前加入局部 3x3 平均亮度。', characteristics: '局部色調適應、邊緣感知 bias', bestFor: '紋理照片與局部光線變化', avoid: '強烈局部變化旁可能有微小 halo', controls: 'Error Strength；可選 Serpentine', structure: 'Integral 亮度圖加 FS 擴散' }
                        ]
                    },
                    {
                        id: 'choosing',
                        title: '如何選擇',
                        bullets: [
                            '先從 Floyd-Steinberg 開始。',
                            '想要強對比、銳利復古紋理時嘗試 Atkinson。',
                            '大型圖片需要更平滑時比較 Jarvis-Judice-Ninke 或 Stevenson-Arce。',
                            '重視速度與銳利度時嘗試 Sierra Lite。',
                            '局部明暗變化很大時嘗試 Adaptive FS 3x3。'
                        ]
                    }
                ]
            },
            ordered: {
                title: 'Ordered 與 Blue Noise',
                eyebrow: '抖色演算法',
                lead: 'Threshold 演算法會把像素與可重複的空間 mask 比較，具有固定、可平行處理且結構清楚的特性。',
                sections: [
                    {
                        id: 'bayer-matrix',
                        title: 'Bayer threshold 結構',
                        paragraphs: ['Bayer matrix 會排列重複 tile 內的位置順位，不同順位在不同色調時切換 Palette 顏色，形成固定圖樣。'],
                        visual: { type: 'matrix', title: 'Bayer 4x4 rank matrix', rows: [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]] }
                    },
                    {
                        id: 'algorithms',
                        title: '可用演算法',
                        capabilityFamily: 'ordered',
                        algorithms: [
                            { id: 'bayer-4', name: 'Bayer 4x4', summary: '小型重複 tile，具有明顯的像素風規律。', characteristics: '快速、銳利、結構強烈', bestFor: '圖示、像素風、低解析輸出', avoid: '平滑照片中容易看見 4x4 重複', controls: 'Dither Strength', structure: '4x4 threshold matrix' },
                            { id: 'bayer-8', name: 'Bayer 8x8', summary: '較大的 matrix 提供更細階調並降低重複感。', characteristics: '快速、固定、紋理較細', bestFor: '一般 ordered dithering 與漸層', avoid: '仍可能看見 8x8 低頻圖樣', controls: 'Dither Strength', structure: '遞迴產生的 8x8 matrix' },
                            { id: 'blue-noise-64', name: 'Blue Noise 64', summary: '固定 64x64 blue-noise-like ranking mask，減少格狀重複。', characteristics: '高頻、較少 tile 感、結果固定', bestFor: 'Bayer 過度規律的漸層與彩色色票', avoid: '可能較顆粒，且仍會重複 64x64 mask', controls: 'Dither Strength', structure: 'Procedural void-and-cluster-style 64x64 rank mask' }
                        ]
                    },
                    {
                        id: 'strength',
                        title: 'Dither Strength',
                        paragraphs: ['100% 使用演算法設定的標準 threshold 影響；降低數值會減少空間變化並靠近直接映射，提高數值則會強化 mask。請用「展開」或匯出 PNG 檢查，縮放預覽可能隱藏或放大單像素結構。']
                    }
                ]
            },
            dot: {
                title: 'Dot-based 演算法',
                eyebrow: '抖色演算法',
                lead: 'Dot Diffusion 與 Dot Halftone 都會呈現點狀結構，但前者擴散誤差，後者則形成群聚印刷網點。',
                sections: [
                    {
                        id: 'not-the-same',
                        title: '兩種不同機制',
                        table: {
                            headers: ['演算法', '運作方式', '控制'],
                            rows: [
                                ['Dot Diffusion 8x8', '依 8x8 class 順序處理，再把誤差傳給較晚的 3x3 鄰近像素。', 'Error Strength'],
                                ['Dot Halftone', '取樣由中心向外成長的 clustered-dot threshold cell。', 'Dot Density']
                            ]
                        }
                    },
                    {
                        id: 'algorithms',
                        title: '可用演算法',
                        capabilityFamily: 'dot',
                        algorithms: [
                            { id: 'dot-diffusion-simple', name: 'Dot Diffusion 8x8', summary: '把 class 順序與局部 RGB 誤差分配結合。', characteristics: '結構可平行、點狀紋理、支援多色', bestFor: '取代逐列 Error Diffusion 的結構化選擇', avoid: '可能看見 8x8 class 紋理', controls: 'Error Strength', structure: '8x8 class matrix；3x3 較晚 class 接收點' },
                            { id: 'pattern-dots', name: 'Dot Halftone', summary: '網點從 cell 中心向外成長。', characteristics: '印刷感、強烈、圖像化', bestFor: '海報、編輯設計與復古印刷', avoid: 'Moiré、cell 接縫與細節減少', controls: 'Dot Density', structure: '由中心向外的群聚 threshold cell' }
                        ]
                    },
                    {
                        id: 'density',
                        title: '理解 Dot Density',
                        bullets: [
                            '50% 約為預設取樣網點密度的一半。',
                            '100% 保留標準 cell 取樣密度。',
                            '150% 約為兩倍取樣密度。',
                            'Dot Density 改變 cell 取樣，不是全域黑白 cutoff。'
                        ]
                    }
                ]
            },
            'palette-mapping': {
                title: '調色盤映射',
                eyebrow: '演算法指南',
                lead: 'Palette Mapping 決定目前輸入色要如何由固定 Palette 顏色表示，再交給 Dither Algorithm 分配像素。',
                sections: [
                    {
                        id: 'compare',
                        title: '使用一到三個 Palette 候選色',
                        visual: { type: 'comparison', set: 'mapping' },
                        table: {
                            headers: ['模式', '表示方式', '最低有效 Palette 色數'],
                            rows: [
                                ['最近色', '一個最接近的 Palette 顏色', '1 色'],
                                ['雙色混合', '兩個 Palette 顏色之間的最佳線段', '2 色'],
                                ['三色混合', '由三個鄰近候選形成的最佳三角形', '3 色']
                            ]
                        }
                    },
                    {
                        id: 'modes',
                        title: '各模式的行為',
                        algorithms: [
                            { name: '最近色', summary: '依目前 Color Distance 直接選出距離最小的 Palette 項目。', characteristics: '穩定、乾淨、快速', bestFor: '預設使用、小型 Palette、銳利素材', avoid: 'Palette 缺少中間色時會有平坦色帶', controls: 'Color Distance', structure: '搜尋 Palette 最近鄰' },
                            { name: '雙色混合', summary: '把輸入色投影到每一組 Palette 色對，選出最接近的混合點。', characteristics: '中間調較豐富、紋理適中', bestFor: '雙色漸層與 ordered mask', avoid: '顏色噪點與色對搜尋成本增加', controls: 'Color Distance 與演算法 threshold/error 行為', structure: '最佳線段與混合比例' },
                            { name: '三色混合', summary: '在最接近的六個候選中測試三角形並計算正規化 barycentric 權重。', characteristics: '顏色近似範圍廣、局部變化最多', bestFor: 'Pair Mix 無法保留色相的彩色 Palette', avoid: '噪點與 CPU 成本較高；threshold GPU path 會 fallback CPU', controls: 'Color Distance 與演算法 threshold/error 行為', structure: '最接近六色中的 20 個候選三角形' }
                        ]
                    },
                    {
                        id: 'algorithm-interaction',
                        title: '與 Dither Algorithm 的互動',
                        paragraphs: [
                            'Ordered、Blue Noise 與 Dot Halftone 會把 threshold 傳給 Pair Mix / Tri Mix，讓空間 mask 依計算出的比例選色。',
                            'Error Diffusion 沒有 threshold mask，Pair Mix / Tri Mix 會先輸出計算權重最高的顏色，再以實際輸出色擴散誤差。'
                        ],
                        note: '所有 Mapping 都只輸出 Palette 顏色。Pair Mix 少於兩色或 Tri Mix 少於三色時，實作會安全 fallback 到 Nearest Color。'
                    }
                ]
            },
            'color-distance': {
                title: '色彩距離',
                eyebrow: '演算法指南',
                lead: 'Color Distance 是比較輸入 RGB 與 Palette 顏色或混色候選的尺。換一把尺，就可能選出不同 Palette 像素。',
                sections: [
                    { id: 'interactive', title: '實際比較距離公式', visual: { type: 'distance-explorer' } },
                    {
                        id: 'metrics',
                        title: '可用公式',
                        table: {
                            headers: ['公式', '意義', '適用時機'],
                            rows: [
                                ['Euclidean BT.709', 'RGB 平方距離，紅 0.2126、綠 0.7152、藍 0.0722。', '亮度感知配對比通道等權重更重要。'],
                                ['Euclidean RGB', '三個通道使用相同權重的平方距離。', '需要專案預設，並讓各通道的數值差異等權計算。'],
                                ['Manhattan BT.709', '加權的通道絕對差。', '希望使用較簡單且有 BT.709 權重的角狀決策邊界。'],
                                ['Manhattan RGB', '相同權重的通道絕對差。', '希望得到可預期的逐通道配對。'],
                                ['CIEDE2000', 'sRGB 轉 Lab 後比較感知色差。', '人眼相似度比處理成本更重要。']
                            ]
                        }
                    },
                    { id: 'visual-comparison', title: '同一組像素使用不同距離尺', visual: { type: 'comparison', set: 'distance' } },
                    {
                        id: 'choosing',
                        title: '實際選擇方式',
                        bullets: [
                            '第一個選擇維持 Euclidean RGB。',
                            '需要亮度感知配對時，比較 Euclidean BT.709。',
                            '想要不同、逐通道線性的邊界時嘗試 Manhattan 版本。',
                            '小型 Palette 含有感知上接近的顏色，且 preview latency 可接受時嘗試 CIEDE2000。',
                            '先固定 Palette 與 Palette Mapping，再比較 Distance，避免一次改變太多變因。'
                        ]
                    }
                ]
            }
        }
    };
})(window.DitherApp);
