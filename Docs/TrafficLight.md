# OSI Traffic Light System

ASAM OSI (Open Simulation Interface) 仕様に基づく信号機の状態管理システム。
外部シミュレータ（esmini等）やco-simulationブリッジから信号状態を受け取り、UE上の信号機アクターに反映する。

既存の `ATrafficLight` / `ATrafficLightController`（タイマー駆動）とは独立した補完的システム。

## アーキテクチャ

```
外部システム (esmini, co-sim bridge, gRPC, etc.)
    |
    v
[受信アクター / 受信サブシステム]  ← ユーザーがBPまたはC++で自由に実装
    |
    | UTrafficLightSubsystem::UpdateTrafficLightById(Id, State)
    | UTrafficLightSubsystem::UpdateTrafficLightsBatch(Updates)
    v
+------------------------------------------+
| UTrafficLightSubsystem (WorldSubsystem)  |
|                                          |
|  StateCache: TMap<int32, State>          |
|  OnTrafficLightStateUpdated: Delegate    |
|                                          |
|  1. StateCache[Id] = NewState            |
|  2. OnTrafficLightStateUpdated           |
|     .Broadcast(Id, NewState)             |
+------------------------------------------+
    |
    | Delegate broadcast
    v
+------------------------+  +------------------------+  +------------------------+
| Traffic Light A        |  | Traffic Light B        |  | Traffic Light C        |
| SignalInfo.SignalId = 1 |  | SignalInfo.SignalId = 2 |  | SignalInfo.SignalId = 3 |
| Id==SignalId? -> 処理   |  | Id==SignalId? -> 処理   |  | Id==SignalId? -> 処理   |
+------------------------+  +------------------------+  +------------------------+
```

**設計のポイント**:
- Subsystemはアクターの参照を一切持たない。状態管理とイベントブロードキャストのみに特化し、アクター側が自分でデリゲートにBindする（Observer pattern）。
- `UWorldSubsystem` のため、すべてのActorの `BeginPlay` より前に初期化される（順序保証）。
- レベルへのアクター配置は不要。`GetWorld()->GetSubsystem<UTrafficLightSubsystem>()` でアクセス。
- IDフィルタリングは `USignalInfoComponent::SignalId` を直接参照。自動配置時は `FSignalGenerator` が設定し、手動配置時はDetailsパネルから編集可能。

## ファイル構成

```
Source/OpenDRIVE/
  Public/
    OsiTrafficLightTypes.h             -- Enum 3種 + 構造体 2種（ヘッダのみ）
    BPI_TrafficLightUpdate.h           -- 信号機アクター側 Blueprint Interface
    BPI_TrafficLightHandlerUpdate.h    -- ハンドラー側 Blueprint Interface
    SignalInfoComponent.h              -- 信号メタデータコンポーネント
    TrafficLightSubsystem.h            -- WorldSubsystem（状態キャッシュ + ブロードキャスト）
    OsiTrafficLightActor.h             -- Blueprintable ベースクラス（単体信号用）
    OsiTrafficLightActorCached.h       -- 状態キャッシュ付きサブクラス
    OsiTrafficLightAssemblyActor.h     -- アセンブリ用ベースクラス（複数信号用）
    SignalAssemblyMapping.h            -- アセンブリマッピング データアセット
  Private/
    BPI_TrafficLightUpdate.cpp
    BPI_TrafficLightHandlerUpdate.cpp
    SignalInfoComponent.cpp
    TrafficLightSubsystem.cpp
    OsiTrafficLightActor.cpp
    OsiTrafficLightActorCached.cpp
    OsiTrafficLightAssemblyActor.cpp
    SignalAssemblyMapping.cpp
```

## 型定義 (OsiTrafficLightTypes.h)

すべて ASAM OSI `TrafficLight.Classification` に対応。

### Enum

| Enum | 値 | 対応OSI定義 |
|------|----|------------|
| `EOsiTrafficLightColor` | UNKNOWN, OTHER, RED, YELLOW, GREEN, BLUE, WHITE | Classification.Color |
| `EOsiTrafficLightIcon` | UNKNOWN, OTHER, NONE, ARROW_STRAIGHT_AHEAD, ... (25値) | Classification.Icon |
| `EOsiTrafficLightMode` | UNKNOWN, OTHER, OFF, CONSTANT, FLASHING, COUNTING | Classification.Mode |

### 構造体

| 構造体 | 用途 |
|--------|------|
| `FOsiTrafficLightState` | 信号1灯の状態（Color, Icon, Mode, Counter） |
| `FOsiTrafficLightBatchEntry` | バッチ更新用エントリ（TrafficLightId + State） |

`Counter` の単位は Icon に依存する:
- `COUNTDOWN_SECONDS` → 秒
- `COUNTDOWN_PERCENT` → %

## Blueprint Interface

### BPI_TrafficLightUpdate（アクター側）

信号機アクターが実装するインターフェース。

| メソッド | 説明 |
|----------|------|
| `OnTrafficLightUpdate(FOsiTrafficLightState)` | 状態更新を受け取る（単体信号用） |
| `OnTrafficLightAssemblyUpdate(int32 SignalId, FOsiTrafficLightState)` | アセンブリ内の個別信号の状態更新を受け取る |

`BlueprintNativeEvent` のため、C++ (`_Implementation`) でも Blueprint (Event Graph) でもオーバーライド可能。

### BPI_TrafficLightHandlerUpdate（ハンドラー側）

外部システムがSubsystemに状態を渡すためのインターフェース。

| メソッド | 説明 |
|----------|------|
| `UpdateTrafficLightById(int32 Id, FOsiTrafficLightState State)` | 1つの信号を更新 |
| `UpdateTrafficLightsBatch(TArray<FOsiTrafficLightBatchEntry> Updates)` | 複数信号を一括更新 |

## USignalInfoComponent

OpenDRIVE信号メタデータを保持するコンポーネント。`AOsiTrafficLightActor` にはデフォルトサブオブジェクトとして含まれる。

| プロパティ | 型 | 説明 |
|-----------|-----|------|
| `SignalId` | `int32` | 信号ID（IDフィルタリングに使用。EditAnywhere） |
| `RoadId` | `int32` | 道路ID |
| `Type` / `SubType` | `FString` | 信号の種類・サブタイプ |
| `Country` | `FString` | 国コード（"DEU", "JPN" 等） |
| `ControllerId` | `int32` | OpenDRIVE Controller ID（-1 = Controllerなし） |
| その他 | | S, T, Value, Unit, Text, bIsDynamic, Height, Width |

`FSignalGenerator` による自動配置時は全プロパティが自動設定される。
手動配置時は `SignalId` をDetailsパネルから編集する。

## UTrafficLightSubsystem

状態キャッシュ + デリゲートブロードキャストを行うWorldSubsystem。

| メンバ | 型 | 説明 |
|--------|----|------|
| `OnTrafficLightStateUpdated` | `FOnOsiTrafficLightStateUpdated` | 状態更新時にBroadcast（BlueprintAssignable） |
| `GetTrafficLightState()` | `bool(int32, FOsiTrafficLightState&)` | キャッシュから現在状態を取得 |
| `StateCache` | `TMap<int32, FOsiTrafficLightState>` | 各IDの最新状態を保持 |

`UWorldSubsystem` のため:
- すべてのActorの `BeginPlay` より前に `Initialize()` が実行される
- レベルへのアクター配置は不要
- `GetWorld()->GetSubsystem<UTrafficLightSubsystem>()` でアクセス

---

## 実装ガイド

### 1. 信号機アクターの作成（Blueprint — ベースクラス使用、推奨）

`AOsiTrafficLightActor` は Subsystem へのバインド、IDフィルタリングまでをC++で実装したベースクラス。
BPサブクラスを作成し、`OnTrafficLightUpdate` だけをオーバーライドすれば良い。
デフォルトで `USignalInfoComponent` を持ち、`SignalId` によるフィルタリングが自動で行われる。

#### Step 1: BPサブクラスの作成

1. Content Browser で右クリック → `Blueprint Class` → `All Classes` → `OsiTrafficLightActor` を選択
2. 名前を `BP_OsiTrafficLight` などにする
3. `Compile` をクリック → Interfaces セクションに `OnTrafficLightUpdate` イベントが表示される

※ `BPI_TrafficLightUpdate` インターフェースは親クラスに実装済みのため、手動で追加する必要はない。

#### Step 2: コンポーネントの追加

Viewport/Components パネルで信号機の見た目を構成する:

```
DefaultSceneRoot (Scene)  ← 親クラスで作成済み
  +-- SignalInfo (SignalInfoComponent)  ← 親クラスで作成済み（IDフィルタリング用）
  +-- SignalMesh (Static Mesh)      -- 信号機本体のメッシュ
  +-- RedLight (Point Light)        -- 赤ライト
  +-- YellowLight (Point Light)     -- 黄ライト
  +-- GreenLight (Point Light)      -- 緑ライト
```

※ 構成は自由。上記は一例。

#### Step 3: OnTrafficLightUpdate の実装

My Blueprint パネル → Interfaces → `OnTrafficLightUpdate` をダブルクリック → Event Graph にイベントノードが追加される。

```
Event OnTrafficLightUpdate (NewState: FOsiTrafficLightState)
    |
    v
[Break FOsiTrafficLightState]  ← NewState を構造体メンバに分解
    |
    |-- Color (EOsiTrafficLightColor)
    |-- Icon  (EOsiTrafficLightIcon)
    |-- Mode  (EOsiTrafficLightMode)
    |-- Counter (float)
    |
    v
[Switch on EOsiTrafficLightColor]  ← Color に応じて分岐
    |
    |-- RED    ──→ [Set Light Color] RedLight=ON,  Yellow=OFF, Green=OFF
    |-- YELLOW ──→ [Set Light Color] RedLight=OFF, Yellow=ON,  Green=OFF
    |-- GREEN  ──→ [Set Light Color] RedLight=OFF, Yellow=OFF, Green=ON
    |-- ...
```

**Mode の処理例:**
```
[Switch on EOsiTrafficLightMode]
    |
    |-- CONSTANT ──→ ライトをON（点灯）
    |-- FLASHING ──→ タイマーで点滅を開始
    |-- OFF      ──→ 全ライトをOFF
    |-- COUNTING ──→ Counter 値をテキストに表示
```

#### Step 4: レベルへの配置

1. `BP_OsiTrafficLight` をレベルに必要数配置
2. 各インスタンスの Details パネルで `Signal Info > Signal Id` にユニークなIDを設定
   - 例: 交差点の北側 = 1, 南側 = 2, 東側 = 3, 西側 = 4

※ Subsystemの配置は不要（自動で生成される）
※ BeginPlay でのSubsystem取得、デリゲートBind、IDフィルタリングはすべて親クラスで実装済み

### 2. 信号機アクターの作成（Blueprint — フルスクラッチ）

`AOsiTrafficLightActor` を使わず、すべてBPで構築する場合の手順。

<details>
<summary>フルスクラッチ手順（クリックで展開）</summary>

#### Step 1: BPアクターの作成とインターフェース追加

1. Content Browser で右クリック → `Blueprint Class` → `Actor` を選択
2. 名前を `BP_OsiTrafficLight` などにする
3. BP を開き、`Class Settings` をクリック
4. Details パネル → `Interfaces` → `Add` → `BPI_TrafficLightUpdate` を検索して追加
5. `Compile` をクリック → Interfaces セクションに `OnTrafficLightUpdate` イベントが表示される

#### Step 2: コンポーネントと変数の追加

コンポーネント:
- `SignalInfoComponent` を追加（`Signal Id` でフィルタリング）

または変数:

| 変数名 | 型 | デフォルト | 設定 |
|--------|----|-----------|------|
| `MyTrafficLightId` | Integer | 0 | `Instance Editable` をON |

#### Step 3: BeginPlay でSubsystemを取得してデリゲートにBind

```
Event BeginPlay
    |
    v
[Get World Subsystem]
  Class: TrafficLightSubsystem
    |
    v (TrafficLightSubsystem reference)
[Bind Event to OnTrafficLightStateUpdated]
  Event: ──→ [Create Event] ──→ "OnStateReceived"
```

#### Step 4: コールバックでIDフィルタリング

```
Custom Event: OnStateReceived
  (TrafficLightId: int32, NewState: FOsiTrafficLightState)
    |
    v
[Branch: TrafficLightId == SignalInfo.SignalId]
    |-- True ──→ [OnTrafficLightUpdate(Self, NewState)]
    |-- False ──→ (何もしない)
```

#### Step 5: OnTrafficLightUpdate の実装

（ベースクラス使用時の Step 3 と同じ）

</details>

### 3. 信号機アクターの作成（Blueprint — キャッシュ付き）

`AOsiTrafficLightActorCached` は `AOsiTrafficLightActor` のサブクラスで、前回と同じ状態が送られてきた場合にスキップする。
マテリアル切り替えやライト ON/OFF など、重い処理を毎フレーム呼ばれたくない場合に有用。

#### 使い方

1. Content Browser で右クリック → `Blueprint Class` → `All Classes` → `OsiTrafficLightActorCached` を選択
2. `OnTrafficLightStateChanged` をオーバーライド（`OnTrafficLightUpdate` ではない点に注意）
3. 状態が実際に変化したときだけ呼ばれるので、見た目の更新処理をここに書く
4. `CachedState` 変数で現在の状態をBPから読み取り可能

```
Event OnTrafficLightStateChanged (NewState: FOsiTrafficLightState)
    |
    v
[Break FOsiTrafficLightState] → Color, Icon, Mode, Counter
    |
    v
[Switch on Color] → ライト切り替え
```

| クラス | 毎回呼ばれる | 変化時のみ | オーバーライド対象 | 信号数 |
|--------|:---:|:---:|------------------|:---:|
| `AOsiTrafficLightActor` | o | - | `OnTrafficLightUpdate` | 単体 |
| `AOsiTrafficLightActorCached` | - | o | `OnTrafficLightStateChanged` | 単体 |
| `AOsiTrafficLightAssemblyActor` | o | - | `OnTrafficLightAssemblyUpdate` | 複数 |

### 4. 信号機アクターの作成（C++）

`AOsiTrafficLightActor` を継承してC++で実装する場合:

```cpp
UCLASS()
class AMyTrafficLight : public AOsiTrafficLightActor
{
    GENERATED_BODY()

    virtual void OnTrafficLightUpdate_Implementation(
        const FOsiTrafficLightState& NewState) override
    {
        // ここで見た目を更新
        // NewState.Color, NewState.Icon, NewState.Mode, NewState.Counter
    }
};
```

BeginPlay/EndPlay でのSubsystemバインド、IDフィルタリングは親クラスで実装済み。
IDは `SignalInfo->SignalId` から自動的に読み取られる。

### 5. 外部システムからの状態送信

外部システム（gRPC受信アクター、esminiブリッジ等）からSubsystemに状態を送信する。
受信ロジックはユーザーがBlueprintまたはC++で自由に実装できる。

```cpp
// Subsystemを取得
UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>();

// 個別更新
FOsiTrafficLightState State;
State.Color = EOsiTrafficLightColor::RED;
State.Icon = EOsiTrafficLightIcon::NONE;
State.Mode = EOsiTrafficLightMode::CONSTANT;
IBPI_TrafficLightHandlerUpdate::Execute_UpdateTrafficLightById(
    Subsystem, /*TrafficLightId=*/ 1, State);

// バッチ更新
TArray<FOsiTrafficLightBatchEntry> Updates;
Updates.Add({1, {EOsiTrafficLightColor::RED,   EOsiTrafficLightIcon::NONE, EOsiTrafficLightMode::CONSTANT, 0.f}});
Updates.Add({2, {EOsiTrafficLightColor::GREEN, EOsiTrafficLightIcon::NONE, EOsiTrafficLightMode::CONSTANT, 0.f}});
IBPI_TrafficLightHandlerUpdate::Execute_UpdateTrafficLightsBatch(
    Subsystem, Updates);

// または直接呼び出し（インターフェースを介さない場合）:
Subsystem->UpdateTrafficLightById(1, State);
```

### 6. レベル配置

#### 手動配置

1. 信号機BPアクターをレベルに配置し、各インスタンスの `Signal Info > Signal Id` を設定
2. 外部システム（受信アクター等）がSubsystemに状態を送信

※ Subsystemはレベルへの配置不要（`UWorldSubsystem` として自動生成される）

#### エディタ自動配置（FSignalGenerator）

OpenDRIVEファイルの信号データに基づいて `FSignalGenerator` がアクターを自動配置する。

**個別モード**（デフォルト）:
`AOsiTrafficLightActor` のBPサブクラスを `USignalTypeMapping` に登録しておけば、
自動配置時にデフォルトの `SignalInfoComponent` に全メタデータが自動設定される。

```
FSignalGenerator::GenerateSignals() [個別モード]
  -> SpawnActor (SignalTypeMappingに基づく)
  -> 既存の SignalInfoComponent を検索 (なければ新規作成)
  -> SignalId, RoadId, Type, ControllerId, etc. を設定
  -> アクターは BeginPlay で SignalInfo->SignalId を使ってフィルタリング
```

**アセンブリモード**（Enable Assembly ON）:
近接・同方向の信号をグループ化し、`USignalAssemblyMapping` に基づいてアセンブリアクターをスポーン。
詳細は「信号機アセンブリ」セクションを参照。

```
FSignalGenerator::GenerateSignals() [アセンブリモード]
  -> Road単位で信号を収集
  -> Union-Find で距離+ヘディングによりグループ化
  -> グループ内の信号タイプ集合で SignalAssemblyMapping を検索
  -> SpawnActor + 各構成信号の SignalInfoComponent を付与
```

## 信号機アセンブリ (Signal Assembly)

### 概要

日本の交通信号機は、三色信号＋矢印信号（左折・直進・右折）など複数のヘッドが一体の構造になっている。
OpenDRIVEでは各ヘッドが別々の Signal オブジェクトとして記述されるため、一体型のアセットを配置しにくい。

**アセンブリ機能**は、同一Road上で近接かつ同方向の信号を自動的にグループ化し、1つのアクターとしてスポーンする。

### アーキテクチャ

```
FSignalGenerator (Assembly Mode)
  |
  |  1. Road単位で全信号を収集
  |  2. Union-Find で距離+ヘディングによりグループ化
  |  3. グループの信号タイプ集合で USignalAssemblyMapping を検索
  |
  v
+-----------------------------------------------+
| Assembly Actor (AOsiTrafficLightAssemblyActor) |
|                                               |
|  SignalInfoComponent[0]: SignalId=100 (三色)   |
|  SignalInfoComponent[1]: SignalId=101 (矢印左) |
|  SignalInfoComponent[2]: SignalId=102 (矢印右) |
|                                               |
|  ManagedSignalIds: {100, 101, 102}            |
|  OnSubsystemStateUpdated で全IDをフィルタ      |
|  → OnTrafficLightAssemblyUpdate(Id, State)    |
+-----------------------------------------------+
```

### グループ化条件

- **同一Road内のみ**（異なるRoad間ではグループ化しない）
- **距離**: 信号間の3D距離 < `AssemblyDistanceThreshold`（デフォルト 5.0m）
- **ヘディング**: 信号の向きの差 < `AssemblyHeadingTolerance`（デフォルト 15.0°）

グループ化にはUnion-Findアルゴリズムを使用。推移的にマージされる（A-B間が近く、B-C間が近ければ A,B,C は同一グループ）。

### USignalAssemblyMapping

信号タイプの組み合わせからアクタークラスを決定するデータアセット。

```cpp
USTRUCT()
struct FSignalAssemblyMappingEntry
{
    TArray<FString> RequiredTypes;       // 必要な信号タイプの集合
    TSubclassOf<AActor> ActorClass;      // スポーンするアクター
    int32 Priority = 0;                  // マッチング優先度
};
```

**マッチングロジック**: グループ内の信号タイプ集合が `RequiredTypes` を**すべて含む**エントリのうち、最も Priority が高いものを選択。マッチしない場合は `DefaultActorClass` にフォールバック。

**例**:
| RequiredTypes | ActorClass | Priority | 説明 |
|---------------|-----------|----------|------|
| `["1000001", "1000011"]` | `BP_TrafficLight_3Color_Arrow` | 10 | 三色＋矢印 |
| `["1000001"]` | `BP_TrafficLight_3Color` | 0 | 三色のみ |

### AOsiTrafficLightAssemblyActor

複数の `USignalInfoComponent` を保持するアセンブリ用ベースクラス。

| 項目 | 単体 (`AOsiTrafficLightActor`) | アセンブリ (`AOsiTrafficLightAssemblyActor`) |
|------|------|------|
| SignalInfoComponent | 1つ | 複数（構成信号ごと） |
| IDフィルタリング | 1つのSignalId | ManagedSignalIds（TSet） |
| 状態更新イベント | `OnTrafficLightUpdate(State)` | `OnTrafficLightAssemblyUpdate(SignalId, State)` |
| BPでの使い方 | Colorで分岐 | SignalIdで分岐 → 各ヘッドの見た目を更新 |

#### BPでの実装例

```
Event OnTrafficLightAssemblyUpdate (SignalId: int32, NewState: FOsiTrafficLightState)
    |
    v
[Get Signal Info Components]  ← 構成信号のリストを取得
    |
    v
[Switch on SignalId]
    |-- 100 (三色信号) ──→ [Switch on Color] → 赤/黄/緑ライト切り替え
    |-- 101 (矢印左)   ──→ [Branch: Mode==CONSTANT?] → 矢印ライトON/OFF
    |-- 102 (矢印右)   ──→ [Branch: Mode==CONSTANT?] → 矢印ライトON/OFF
```

#### C++での実装

```cpp
UCLASS()
class AMyTrafficLightAssembly : public AOsiTrafficLightAssemblyActor
{
    GENERATED_BODY()

    virtual void OnTrafficLightAssemblyUpdate_Implementation(
        int32 SignalId, const FOsiTrafficLightState& NewState) override
    {
        // SignalId に基づいて対応するサブライトの見た目を更新
    }
};
```

### エディタUI設定

Signal タブの下部にアセンブリ設定が表示される:

| 設定 | デフォルト | 説明 |
|------|-----------|------|
| Enable Assembly | OFF | アセンブリモードの有効/無効 |
| Distance (m) | 5.0 | グループ化の距離閾値 |
| Heading Tol (°) | 15.0 | グループ化のヘディング許容差 |
| Assembly Mapping | (なし) | `USignalAssemblyMapping` データアセット |

アセンブリ無効時は従来の個別信号生成（`USignalTypeMapping` ベース）が使用される。

### Controller ID

OpenDRIVE の `<controller>` 要素は複数の信号をグループ化する概念だが、1つのControllerが交差点全方向の信号を含むことがあるため、**アセンブリのグループ化条件には使用しない**。

`ControllerId` は `USignalInfoComponent` にメタデータとして保持される（個別/アセンブリ両モード）。将来のランタイム信号制御機能の土台として利用可能。

---

## 既存システムとの関係

| 項目 | 既存システム | OSIシステム（本実装） |
|------|------------|---------------------|
| 状態モデル | `ETrafficLightState` (4値) | `FOsiTrafficLightState` (Color+Icon+Mode+Counter) |
| 更新方式 | タイマー駆動（Tick） | イベント駆動（Push + Delegate） |
| コントローラ | `ATrafficLightController` | `UTrafficLightSubsystem` (WorldSubsystem) |
| アクター結合 | 直接参照 (`ATrafficLight*`) | 参照なし（Delegate broadcast） |
| 初期化 | BeginPlayで相互参照 | Subsystemが先に初期化（順序保証） |

既存の `ATrafficLight` を拡張して `BPI_TrafficLightUpdate` を実装すれば、両システムを共存させることも可能。

## OSI仕様リファレンス

本実装の型定義は `osi3::TrafficLight::Classification` に1対1で対応:
- `Color` → `EOsiTrafficLightColor`
- `Icon` → `EOsiTrafficLightIcon`
- `Mode` → `EOsiTrafficLightMode`
- `counter` → `FOsiTrafficLightState::Counter`

詳細: [ASAM OSI Documentation](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/gen/structosi3_1_1TrafficLight.html)
