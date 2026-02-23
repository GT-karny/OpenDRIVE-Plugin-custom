# Python API リファレンス

OpenDRIVEプラグインのPythonスクリプティングAPIリファレンス。

すべてのAPIは `UOpenDriveEditorSubsystem` を通じて提供される。

## 目次

- [概要](#概要)
- [前提条件](#前提条件)
- [OpenDRIVEアセット管理](#opendriveアセット管理)
- [スプライン生成](#スプライン生成)
  - [生成・クリア](#生成クリア)
  - [ジオメトリ設定](#ジオメトリ設定)
  - [道路・ジャンクション フィルタ](#道路ジャンクション-フィルタ)
  - [レーン方向フィルタ](#レーン方向フィルタ)
  - [レーンタイプ フィルタ](#レーンタイプ-フィルタ)
  - [レーンポジション フィルタ](#レーンポジション-フィルタ)
  - [自動タグ](#自動タグ)
- [信号生成](#信号生成)
- [使用例](#使用例)

---

## 概要

Python API は `UOpenDriveEditorSubsystem`（エディターサブシステム）を介してアクセスする。

```python
import unreal

subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)
```

### スクリプト実行方法

UE エディタの Output Log または Python コンソールから実行する:

```
py "E:/path/to/your_script.py"
```

> C++ の関数名（PascalCase）は Python では自動的に snake_case に変換される。
> 例: `SetSplineOffset()` → `set_spline_offset()`

---

## 前提条件

### 1. World Settings クラスの設定

`DefaultEngine.ini` に以下を追加:

```ini
[/Script/Engine.Engine]
WorldSettingsClassName=/Script/OpenDRIVE.OpenDriveWorldSettings
```

### 2. OpenDRIVEアセットのインポート

`.xodr` ファイルを Content Browser にドラッグ＆ドロップ、または標準のインポート手順でインポートする。

---

## OpenDRIVEアセット管理

### `set_open_drive_asset(asset)` → `bool`

World Settings に OpenDRIVE アセットを設定し、RoadManager に読み込む。

| パラメータ | 型 | 説明 |
|-----------|------|------|
| `asset` | `OpenDriveAsset` | `unreal.load_asset()` で読み込んだアセット |

**戻り値**: 設定・読み込みに成功した場合 `True`

```python
asset = unreal.load_asset("/Game/OpenDRIVE/YourRoadNetwork")
if asset:
    success = subsystem.set_open_drive_asset(asset)
```

### `get_open_drive_asset()` → `OpenDriveAsset`

現在 World Settings に設定されている OpenDRIVE アセットを取得する。

**戻り値**: `OpenDriveAsset` または `None`

```python
current = subsystem.get_open_drive_asset()
if current:
    unreal.log(f"Current asset: {current.get_name()}")
```

---

## スプライン生成

OpenDRIVE の道路データからレーンスプラインアクター（`AOpenDriveLaneSpline`）を生成する。

### 生成・クリア

#### `generate_lane_splines()`

現在の設定に基づいてレーンスプラインを生成する。生成されたスプラインは World Outliner の `OpenDriveSplines/Road_<id>` フォルダに整理される。

```python
subsystem.generate_lane_splines()
```

#### `clear_generated_splines()`

このサブシステムで生成したスプラインをすべて削除する。

```python
subsystem.clear_generated_splines()
```

---

### ジオメトリ設定

#### `set_spline_offset(offset)`

スプラインの Z-オフセットを設定する。地面との Z-fighting 回避用。

| パラメータ | 型 | 単位 | デフォルト |
|-----------|------|------|-----------|
| `offset` | `float` | cm | `20.0` |

#### `set_spline_step(step)`

スプラインポイントの生成間隔を設定する。値が小さいほど精密だが、生成が遅くなる。

| パラメータ | 型 | 単位 | デフォルト |
|-----------|------|------|-----------|
| `step` | `float` | m | `5.0` |

#### `set_spline_mode(mode)`

スプラインの基準位置を設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `mode` | `int` | `0` |

| 値 | モード | 説明 |
|----|--------|------|
| `0` | Center | レーン中央 |
| `1` | Inside | レーン内側（中心線側）の境界 |
| `2` | Outside | レーン外側の境界 |

---

### 道路・ジャンクション フィルタ

#### `set_generate_roads(b_generate)`

通常道路（非ジャンクション）のレーンを生成するかどうかを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_generate` | `bool` | `True` |

#### `set_generate_junctions(b_generate)`

ジャンクション道路のレーンを生成するかどうかを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_generate` | `bool` | `True` |

---

### レーン方向フィルタ

OpenDRIVE のレーン ID 規則: `0` = 基準線、正の値 = 左側レーン、負の値 = 右側レーン

#### `set_generate_left_lanes(b_generate)`

左側レーン（ID > 0）を生成するかどうかを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_generate` | `bool` | `True` |

#### `set_generate_right_lanes(b_generate)`

右側レーン（ID < 0）を生成するかどうかを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_generate` | `bool` | `True` |

---

### レーンタイプ フィルタ

#### `set_lane_type_filter(...)`

全レーンタイプのフィルタを一括設定する。

```python
subsystem.set_lane_type_filter(
    b_driving=True,       # 走行車線
    b_sidewalk=True,      # 歩道
    b_biking=True,        # 自転車レーン
    b_parking=True,       # 駐車レーン
    b_shoulder=False,     # 路肩
    b_restricted=False,   # 制限区域
    b_median=False,       # 中央分離帯
    b_other=False,        # その他
    b_reference=False     # 基準線（ID=0）
)
```

| パラメータ | 説明 | デフォルト |
|-----------|------|-----------|
| `b_driving` | 走行車線 | `True` |
| `b_sidewalk` | 歩道 | `True` |
| `b_biking` | 自転車レーン | `True` |
| `b_parking` | 駐車レーン | `True` |
| `b_shoulder` | 路肩 | `True` |
| `b_restricted` | 制限区域 | `True` |
| `b_median` | 中央分離帯 | `True` |
| `b_other` | その他 | `True` |
| `b_reference` | 基準線（ID=0） | `True` |

---

### レーンポジション フィルタ

#### `set_lane_position_filter(filter_mode)`

レーンの位置に基づくフィルタモードを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `filter_mode` | `int` | `0` |

| 値 | モード | 説明 |
|----|--------|------|
| `0` | All | 全レーンを生成 |
| `1` | Outermost Only | 各サイドの最外レーン（全タイプ） |
| `2` | Outermost Driving Only | 各サイドの最外走行車線のみ |
| `3` | Innermost Only | 各サイドの最内レーン（全タイプ） |
| `4` | Innermost Driving Only | 各サイドの最内走行車線のみ |
| `5` | Specific Index | 指定インデックスのレーン |

#### `set_specific_lane_index(index)`

レーンポジションフィルタが `5`（Specific Index）のときに使用するレーンインデックスを設定する。中心線から数えて 1-based。

| パラメータ | 型 | 有効範囲 |
|-----------|------|---------|
| `index` | `int` | `1` 以上 |

```python
# 中心線から2番目のレーンのみ生成
subsystem.set_lane_position_filter(5)  # Specific Index
subsystem.set_specific_lane_index(2)
```

#### `set_generate_outermost_driving_lane_only(b_outermost_only)`

後方互換ラッパー。内部的に `set_lane_position_filter(2)` または `set_lane_position_filter(0)` を呼び出す。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_outermost_only` | `bool` | `False` |

---

### 自動タグ

生成されたスプラインアクターには以下のタグが自動付与される。`actor.tags` から参照できる。

| タグ | 形式 | 説明 |
|------|------|------|
| `Road_<id>` | `Road_5` | 道路ID |
| `L` / `R` | | 左側 / 右側 |
| `Lane_<N>` | `Lane_1` | レーンインデックス（1-based、中心から。全タイプ） |
| `Outermost` | | 最外レーン（全タイプ） |
| `Innermost` | | 最内レーン（全タイプ） |
| `Driving<N>` | `Driving1` | 走行車線インデックス（1-based、中心から。走行車線のみ） |
| `OutermostDriving` | | 最外走行車線 |
| `InnermostDriving` | | 最内走行車線 |

World Outliner 上のフォルダ: `OpenDriveSplines/Road_<id>`

---

## 信号生成

OpenDRIVE の信号データからアクターを生成する。`USignalTypeMapping` データアセットで信号タイプとスポーンするアクタークラスを対応付ける。

### `generate_signals(mapping_asset)`

信号アクターを生成する。

| パラメータ | 型 | 説明 |
|-----------|------|------|
| `mapping_asset` | `SignalTypeMapping` | 信号タイプ→アクタークラスの対応を定義したデータアセット |

```python
mapping = unreal.load_asset("/Game/OpenDRIVE/SignalTypeMapping")
if mapping:
    subsystem.generate_signals(mapping)
```

生成されたアクターには `USignalInfoComponent` が付与され、OpenDRIVE の信号メタデータ（SignalId, RoadId, Type, SubType 等）が自動設定される。

World Outliner 上のフォルダ: `Signals/Road_<id>`

> SignalTypeMapping の作成方法は [UserManual.md](../UserManual.md) を参照。

### `clear_generated_signals()`

このサブシステムで生成した信号アクターをすべて削除する。

```python
subsystem.clear_generated_signals()
```

### `set_flip_signal_orientation(b_flip)`

信号アクターの向きを 180 度反転するかどうかを設定する。

| パラメータ | 型 | デフォルト |
|-----------|------|-----------|
| `b_flip` | `bool` | `False` |

---

## 使用例

### アセット設定 → スプライン生成 → 信号生成

```python
import unreal

subsystem = unreal.get_editor_subsystem(unreal.OpenDriveEditorSubsystem)

# 1. アセット設定
asset = unreal.load_asset("/Game/OpenDRIVE/YourRoadNetwork")
if asset:
    subsystem.set_open_drive_asset(asset)

# 2. 前回の生成結果をクリア
subsystem.clear_generated_splines()
subsystem.clear_generated_signals()

# 3. スプライン生成
subsystem.set_spline_offset(20.0)
subsystem.set_spline_step(5.0)
subsystem.set_spline_mode(0)  # Center

subsystem.set_generate_roads(True)
subsystem.set_generate_junctions(True)

subsystem.set_lane_type_filter(
    b_driving=True,
    b_sidewalk=True,
    b_biking=True,
    b_parking=True,
    b_shoulder=False,
    b_restricted=False,
    b_median=False,
    b_other=False,
    b_reference=False
)

subsystem.generate_lane_splines()

# 4. 信号生成
mapping = unreal.load_asset("/Game/OpenDRIVE/SignalTypeMapping")
if mapping:
    subsystem.set_flip_signal_orientation(False)
    subsystem.generate_signals(mapping)
```

### 最外走行車線のみ生成

```python
subsystem.set_lane_position_filter(2)  # Outermost Driving Only
subsystem.generate_lane_splines()
```

### 右側レーンのみ生成

```python
subsystem.set_generate_left_lanes(False)
subsystem.set_generate_right_lanes(True)
subsystem.generate_lane_splines()
```

### サンプルスクリプト

プラグイン同梱のサンプルスクリプト（`Samples/PythonScript/`）:

| ファイル | 説明 |
|---------|------|
| `set_opendrive_asset.py` | アセット設定と確認 |
| `generate_splines.py` | スプライン生成の設定例 |
| `generate_signals.py` | 信号生成の設定例 |
| `generate_all.py` | アセット設定→スプライン→信号を一括実行 |
