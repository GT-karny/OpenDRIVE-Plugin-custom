# スプライン自動タグ一覧

スプライン生成時、各 `AOpenDriveLaneSpline` アクターに自動で付与されるタグのリファレンス。
タグは `AActor::Tags` (`TArray<FName>`) に格納され、Blueprint / Python（`actor.tags`）から参照できる。

---

## タグ一覧

| タグ | 形式例 | 付与条件 | 説明 |
|------|--------|----------|------|
| `Road_<id>` | `Road_0`, `Road_5` | 全スプライン | 道路ID |
| `L` | — | LaneId > 0 | 左側レーン |
| `R` | — | LaneId < 0 | 右側レーン |
| `Lane_<N>` | `Lane_1`, `Lane_2` | LaneId != 0 | レーンインデックス（全レーンタイプ） |
| `Outermost` | — | LaneId != 0 かつ該当サイドの最外レーン | 最外レーン（全レーンタイプ） |
| `Innermost` | — | LaneId != 0 かつ該当サイドの最内レーン | 最内レーン（全レーンタイプ） |
| `Driving<N>` | `Driving1`, `Driving2` | LaneType == Driving | 走行車線インデックス（走行車線のみ） |
| `OutermostDriving` | — | LaneType == Driving かつ該当サイドの最外走行車線 | 最外走行車線 |
| `InnermostDriving` | — | LaneType == Driving かつ該当サイドの最内走行車線 | 最内走行車線 |

---

## インデックスの計算ルール

### `Lane_<N>` — 全レーンタイプ対象

- **1-based**、中心（リファレンスレーン）から外側に向かって番号が増加
- 左右それぞれ独立にカウント
- 全レーンタイプ（Driving, Sidewalk, Biking, Parking, Shoulder, Restricted, Median, Other）が対象
- リファレンスレーン（LaneId == 0）には付与されない

### `Driving<N>` — 走行車線のみ

- **1-based**、中心から外側に向かって番号が増加
- 左右それぞれ独立にカウント
- `LaneType == Driving` のレーンのみが対象

### サイド判定

| LaneId | サイド | ソート順（中心から） |
|--------|--------|----------------------|
| > 0 | 左（`L`） | 1, 2, 3 … （昇順） |
| < 0 | 右（`R`） | -1, -2, -3 … （降順） |
| == 0 | リファレンス | サイドタグなし |

### Outermost / Innermost 判定

各サイド（左・右）ごとに、全レーンタイプを含むリストの先頭が **Innermost**、末尾が **Outermost**。
`OutermostDriving` / `InnermostDriving` は走行車線のみのリストで同様に判定。

レーンが1つしかない場合、`Outermost` と `Innermost` の両方が付与される。

---

## 具体例

### 例1: 片側2車線道路（右2車線 Driving）

Road ID = 3、右側に LaneId -1（Driving）、-2（Driving）

| アクター | タグ |
|----------|------|
| LaneSpline_Road3_Lane-1 | `Road_3`, `R`, `Lane_1`, `Innermost`, `Driving1`, `InnermostDriving` |
| LaneSpline_Road3_Lane-2 | `Road_3`, `R`, `Lane_2`, `Outermost`, `Driving2`, `OutermostDriving` |

### 例2: 左1車線 + 右2車線（Driving + Sidewalk）

Road ID = 7、左側に LaneId 1（Driving）、右側に LaneId -1（Driving）、-2（Sidewalk）

| アクター | タグ |
|----------|------|
| LaneSpline_Road7_Lane1 | `Road_7`, `L`, `Lane_1`, `Outermost`, `Innermost`, `Driving1`, `OutermostDriving`, `InnermostDriving` |
| LaneSpline_Road7_Lane-1 | `Road_7`, `R`, `Lane_1`, `Innermost`, `Driving1`, `OutermostDriving`, `InnermostDriving` |
| LaneSpline_Road7_Lane-2 | `Road_7`, `R`, `Lane_2`, `Outermost` |

> LaneId -2 は Sidewalk のため `Driving<N>` タグは付与されない。

### 例3: リファレンスレーン

Road ID = 0、LaneId 0（Reference）

| アクター | タグ |
|----------|------|
| LaneSpline_Road0_Lane0 | `Road_0` |

> リファレンスレーンには `Road_<id>` のみ付与される（サイド・インデックス・位置タグなし）。

---

## World Outliner フォルダ

エディタ環境（`WITH_EDITOR`）では、スプラインアクターは World Outliner 上で道路ごとにフォルダ分けされる。

```
OpenDriveSplines/
  Road_0/
    LaneSpline_Road0_Lane-1
    LaneSpline_Road0_Lane-2
  Road_3/
    LaneSpline_Road3_Lane1
    ...
```

---

## 参照

- ソースコード: `Source/OpenDRIVEEditor/Private/SplineGenerator.cpp` L260–323
- Python API での利用: [PythonAPI.md](PythonAPI.md) — 「自動タグ」セクション
