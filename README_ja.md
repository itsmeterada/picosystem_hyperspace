# Hyperspace for PicoSystem

[English README](README.md)

PICO-8ゲーム「Hyperspace」（J-Fry作）をPicoSystemハンドヘルドコンソールに移植したものです。

![スクリーンショット](screenshot.png)

## 概要

Hyperspaceは、もともとPICO-8用に作成された3Dスペースシューティングゲームです。この移植版は、RP2040マイクロコントローラを搭載した小型携帯ゲーム機PicoSystemで完全なゲーム体験を提供します。

### 特徴

- テクスチャマッピング付きフル3Dソフトウェアラスタライゼーション
- RP2040での最適なパフォーマンスのための固定小数点演算
- 4種類の敵：小惑星、小型機、中型機、ボス
- 回避用のバレルロール
- オートファイアとマニュアルファイアモード
- ハイスコアの永続保存

## ビルド方法

### 必要なもの

- [Pico SDK](https://github.com/raspberrypi/pico-sdk)（v1.5.0以降）
- [Pico Extras](https://github.com/raspberrypi/pico-extras)
- CMake 3.12以降
- ARM GCCツールチェーン

### 環境設定

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
```

### ビルド手順

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

生成された`hyperspace.uf2`ファイルをブートローダーモードのPicoSystemにコピーしてください。

## 操作方法

| ボタン | アクション |
|--------|-----------|
| 十字キー | 機体の移動 |
| A | レーザー発射 |
| X | バレルロール |

## 技術的詳細

### 固定小数点演算

オリジナルのPICO-8ゲームは浮動小数点数を使用していますが、RP2040にはハードウェアFPUがありません。この移植版では、[libfixmath](https://github.com/PetteriAimworthy/libfixmath)を使用して、すべての浮動小数点演算をQ16.16固定小数点演算に変換しています。

主な変換:
- `float` → `fix16_t`（32ビット符号付き固定小数点）
- `sin()/cos()` → `fix16_sin()/fix16_cos()`（ラジアンベース）
- `sqrt()` → `fix16_sqrt()`
- 乗算: `a * b` → `fix16_mul(a, b)`
- 除算: `a / b` → `fix16_div(a, b)`

ゲームの値域ではオーバーフローチェックが不要なため、パフォーマンス向上のために`FIXMATH_NO_OVERFLOW`フラグを有効にしています。

### 3Dレンダリングパイプライン

1. **メッシュ読み込み**: PICO-8カートリッジから抽出した埋め込みマップメモリからメッシュをデコード
2. **行列変換**: 3x4行列で回転と平行移動を処理
3. **投影**: 設定可能なFOVによる透視投影
4. **三角形ラスタライズ**: 重心座標補間によるスキャンラインベース
5. **テクスチャマッピング**: 透視補正付きUV座標
6. **ライティング**: 滑らかなグラデーションのためのディザリング付き三角形単位ライティング

### 最適化

PicoSystem版とGBA版で共通の最適化:

| 最適化 | 説明 |
|--------|------|
| 挿入ソート | 小さな三角形配列（20要素未満）ではqsortより高速 |
| スキャンライン勾配 | ピクセル毎の除算を避けるため重心勾配を事前計算 |
| 高速マクロ | `SGET_FAST`/`PSET_FAST`で内部ループの境界チェックを省略 |
| ビットマスク剰余 | `% 2^n`を`& (2^n-1)`に置換（2のべき乗の除数用） |
| 早期カリング | カメラ背後または画面外の三角形をスキップ |

### メモリレイアウト

| セクション | サイズ | 説明 |
|-----------|-------|------|
| Text | 約90KB | コード |
| BSS | 約160KB | ランタイムデータ |
| スプライトシート | 16KB | 128x128 4ビットピクセル |
| マップメモリ | 4KB | メッシュ定義 |
| スクリーンバッファ | 14.4KB | 120x120 8ビットピクセル |

総RAM使用量はRP2040の264KB中約160KBです。

### 画面解像度

- PicoSystemネイティブ: 240x240ピクセル
- ゲーム解像度: 120x120ピクセル（PIXEL_DOUBLEモード）
- オリジナルPICO-8: 128x128ピクセル

アスペクト比を維持しながらPicoSystemのピクセルダブリングモードに適合させるため、解像度をわずかに縮小（128→120）しています。

### カラーパレット

ゲームはPICO-8の16色パレットを使用し、PicoSystemの16ビットカラーフォーマット（RGBA4444）に変換しています:

```c
#define PS_RGB(r, g, b) ((((r) >> 4) & 0xf) | (0xf << 4) | ((((b) >> 4) & 0xf) << 8) | ((((g) >> 4) & 0xf) << 12))
```

### データ抽出

スプライトとマップデータは、オリジナルのPICO-8 `.p8`カートリッジファイルから抽出しています:

- `__gfx__`セクション → `hyperspace_spritesheet[128][128]`
- `__map__`セクション → `hyperspace_map[0x1000]`

`convert_p8.py`スクリプトがこの変換を自動化します。

## プロジェクト構成

```
picosystem_hyperspace/
├── main.c                 # メインゲームコード（SDL2/PICO-8から移植）
├── hyperspace_data.h      # 埋め込みスプライト・マップデータ
├── convert_p8.py          # PICO-8データ抽出スクリプト
├── CMakeLists.txt         # ビルド設定
├── picosystem_hardware/   # PicoSystem HAL
│   ├── picosystem_hardware.c
│   ├── picosystem_hardware.h
│   └── ...
├── libfixmath/            # 固定小数点数学ライブラリ
│   ├── fix16.c
│   ├── fix16.h
│   ├── fix16_trig.c
│   └── ...
└── gba/                   # ゲームボーイアドバンス版
    ├── main_gba.c         # GBA固有の実装
    ├── raster_arm.s       # 手書きARMアセンブリ
    ├── Makefile           # devkitARMビルド
    └── README.md          # GBA版ドキュメント
```

## GBA版

`gba/`ディレクトリにゲームボーイアドバンス版もあります。詳細は[gba/README.md](gba/README.md)を参照してください。

PicoSystem版との主な違い:
- Mode 5ビットマップ（160x128、15ビットカラー）
- BG2アフィン変換でフルスクリーン（240x160）に拡大
- 内部ループ用の手書きARMアセンブリ
- DMAによる高速画面クリア
- SRAMによるハイスコア保存

## 関連プロジェクト

- [Hyperspace SDL2](https://github.com/itsmeterada/hyperspace) - デスクトッププラットフォーム向けSDL2移植版（Windows、macOS、Linux）

## クレジット

- **オリジナルゲーム**: [Hyperspace](https://www.lexaloffle.com/bbs/?tid=41663) by J-Fry (PICO-8)
- **SDL2移植**: [itsmeterada](https://github.com/itsmeterada/hyperspace)
- **PicoSystem移植**: itsmeterada
- **GBA移植**: itsmeterada
- **libfixmath**: [PetteriAimworthy/libfixmath](https://github.com/PetteriAimworthy/libfixmath)

## ライセンス

この移植版は教育目的および個人使用のために提供されています。オリジナル作者の権利を尊重してください。
