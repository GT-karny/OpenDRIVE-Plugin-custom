# OpenDRIVE Plugin - User Manual

## Signal Generation (Gen Signals)

OpenDRIVEファイルに定義された信号・標識を自動的にUnrealエディタ上にスポーンする機能です。

### 前提条件

1. OpenDRIVEファイル（.xodr）がロード済みであること
2. Signal Type Mapping アセットが作成済みであること（任意）

---

### Signal Type Mapping アセットの作成

信号タイプごとにスポーンするアクタークラスを指定するためのData Assetです。

1. **Content Browser** で右クリック → **Miscellaneous** → **Data Asset**
2. クラス選択で `SignalTypeMapping` を選択
3. アセットを開き、以下を設定:

| プロパティ | 説明 |
|-----------|------|
| **Mappings** | 信号タイプとアクタークラスの対応リスト |
| **Default Actor Class** | マッピングに一致しない信号用のデフォルトクラス |

#### Mappings の設定例

| Type | SubType | Country | Actor Class | Priority |
|------|---------|---------|-------------|----------|
| `1000001` | | | BP_TrafficLight | 10 |
| `274` | | | BP_SpeedLimitSign | 10 |
| | | | BP_GenericSign | 0 |

- **Type/SubType/Country**: 空欄の場合は「任意」として扱われます
- **Priority**: 複数のマッピングに一致する場合、Priorityが高いものが優先されます

---

### 使用手順

1. **OpenDRIVE Editor Mode** に入る
   - Editor上部の **Modes** パネルから「OpenDRIVE」を選択

2. **OpenDRIVEファイルのロード**
   - xodrファイルがロードされていることを確認
   - （Generate ボタンで道路を描画済みの状態）

3. **Signal Generation 設定**（オプション）
   - **Generate Signals**: チェックを入れると信号生成が有効
   - **Flip Orientation**: 信号の向きが逆の場合にチェック（180度回転）
   - **Mapping Asset**: 作成したSignalTypeMappingアセットを指定

4. **Gen Signals ボタン**をクリック
   - OpenDRIVEファイル内の全信号がスポーンされます

---

### 生成結果の確認

- **World Outliner**: `Signals/Road_X` フォルダに整理されます
- **Actor**: 各信号アクターには `SignalInfoComponent` が付与されます

#### SignalInfoComponent のプロパティ

| プロパティ | 説明 |
|-----------|------|
| Signal Id | OpenDRIVE内の信号ID |
| Road Id | 信号が属する道路ID |
| S | 道路に沿った位置（m） |
| T | 道路中心からの横方向オフセット（m） |
| Type | 信号タイプコード |
| Sub Type | サブタイプコード |
| Country | 国コード |
| Value | 信号の値（速度制限など） |
| Unit | 値の単位 |
| Text | 表示テキスト |
| Is Dynamic | 動的信号かどうか |
| Height / Width | 信号のサイズ（m） |

---

### トラブルシューティング

#### 信号がスポーンされない

- **Mapping Asset** が未設定の場合、または **Default Actor Class** が未設定の場合はスポーンされません
- Output Logに `No actor class for signal type=...` と表示されます

#### 信号の向きが逆

- **Flip Orientation** チェックボックスをオンにして再生成してください

#### 信号の位置がずれている

- OpenDRIVEファイルのz_offset設定を確認してください
- Unrealの座標系変換: X→X, Y→-Y, Z→Z（単位: m→cm）

---

### 技術的な注意点

- 座標変換: OpenDRIVE（右手系、メートル）→ Unreal（左手系、センチメートル）
- 回転: Heading(Yaw)は符号反転、Pitch/Rollはそのまま変換
- Signal の `orientation` 属性（+/-）は `Signal->GetH()` に既に含まれています
