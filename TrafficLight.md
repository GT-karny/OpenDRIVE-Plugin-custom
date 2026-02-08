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
+-------------------+  +-------------------+  +-------------------+
| Traffic Light A   |  | Traffic Light B   |  | Traffic Light C   |
| MyId = 1          |  | MyId = 2          |  | MyId = 3          |
| Id==MyId? -> 処理 |  | Id==MyId? -> 処理 |  | Id==MyId? -> 処理 |
+-------------------+  +-------------------+  +-------------------+
```

**設計のポイント**:
- Subsystemはアクターの参照を一切持たない。状態管理とイベントブロードキャストのみに特化し、アクター側が自分でデリゲートにBindする（Observer pattern）。
- `UWorldSubsystem` のため、すべてのActorの `BeginPlay` より前に初期化される（順序保証）。
- レベルへのアクター配置は不要。`GetWorld()->GetSubsystem<UTrafficLightSubsystem>()` でアクセス。

## ファイル構成

```
Source/OpenDRIVE/
  Public/
    OsiTrafficLightTypes.h         -- Enum 3種 + 構造体 2種（ヘッダのみ）
    BPI_TrafficLightUpdate.h       -- 信号機アクター側 Blueprint Interface
    BPI_TrafficLightHandlerUpdate.h -- ハンドラー側 Blueprint Interface
    BPI_SignalAutoSetup.h          -- 自動配置インターフェース
    TrafficLightSubsystem.h        -- WorldSubsystem（状態キャッシュ + ブロードキャスト）
    OsiTrafficLightActor.h         -- Blueprintable ベースクラス（サンプル）
    OsiTrafficLightActorCached.h   -- 状態キャッシュ付きサブクラス（サンプル）
  Private/
    BPI_TrafficLightUpdate.cpp
    BPI_TrafficLightHandlerUpdate.cpp
    BPI_SignalAutoSetup.cpp
    TrafficLightSubsystem.cpp
    OsiTrafficLightActor.cpp
    OsiTrafficLightActorCached.cpp
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
| `OnTrafficLightUpdate(FOsiTrafficLightState)` | 状態更新を受け取る |

`BlueprintNativeEvent` のため、C++ (`_Implementation`) でも Blueprint (Event Graph) でもオーバーライド可能。

### BPI_TrafficLightHandlerUpdate（ハンドラー側）

外部システムがSubsystemに状態を渡すためのインターフェース。

| メソッド | 説明 |
|----------|------|
| `UpdateTrafficLightById(int32 Id, FOsiTrafficLightState State)` | 1つの信号を更新 |
| `UpdateTrafficLightsBatch(TArray<FOsiTrafficLightBatchEntry> Updates)` | 複数信号を一括更新 |

### BPI_SignalAutoSetup（自動配置）

エディタの `FSignalGenerator` がアクターを自動配置した後に呼ばれるインターフェース。
実装すると、OpenDRIVEの信号情報（`USignalInfoComponent`）から自動的にプロパティを設定できる。

| メソッド | 説明 |
|----------|------|
| `OnSignalAutoPlaced(USignalInfoComponent* SignalInfo)` | 自動配置後に呼ばれる。SignalInfoからID等を取得して設定 |

`AOsiTrafficLightActor` はデフォルトで `MyTrafficLightId = SignalInfo->SignalId` を設定する。
BPサブクラスでオーバーライドすれば、追加のプロパティ設定も可能。

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

#### Step 1: BPサブクラスの作成

1. Content Browser で右クリック → `Blueprint Class` → `All Classes` → `OsiTrafficLightActor` を選択
2. 名前を `BP_OsiTrafficLight` などにする
3. `Compile` をクリック → Interfaces セクションに `OnTrafficLightUpdate` イベントが表示される

※ `BPI_TrafficLightUpdate` インターフェースは親クラスに実装済みのため、手動で追加する必要はない。

#### Step 2: コンポーネントの追加

Viewport/Components パネルで信号機の見た目を構成する:

```
DefaultSceneRoot (Scene)  ← 親クラスで作成済み
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
2. 各インスタンスの Details パネルで `My Traffic Light Id` にユニークなIDを設定
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

#### Step 2: 変数の追加

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
[Branch: TrafficLightId == MyTrafficLightId]
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

| クラス | 毎回呼ばれる | 変化時のみ | オーバーライド対象 |
|--------|:---:|:---:|------------------|
| `AOsiTrafficLightActor` | o | - | `OnTrafficLightUpdate` |
| `AOsiTrafficLightActorCached` | - | o | `OnTrafficLightStateChanged` |

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

ベースクラスを使わずフルスクラッチで実装する場合:

```cpp
UCLASS()
class AMyTrafficLight : public AActor, public IBPI_TrafficLightUpdate
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Traffic Light")
    int32 MyTrafficLightId = 0;

    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
        Subsystem->OnTrafficLightStateUpdated.AddDynamic(
            this, &AMyTrafficLight::OnStateUpdated);
    }

    virtual void EndPlay(const EEndPlayReason::Type Reason) override
    {
        if (UTrafficLightSubsystem* Subsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>())
        {
            Subsystem->OnTrafficLightStateUpdated.RemoveDynamic(
                this, &AMyTrafficLight::OnStateUpdated);
        }
        Super::EndPlay(Reason);
    }

    UFUNCTION()
    void OnStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
    {
        if (TrafficLightId != MyTrafficLightId) return;
        Execute_OnTrafficLightUpdate(this, NewState);
    }

    virtual void OnTrafficLightUpdate_Implementation(
        const FOsiTrafficLightState& NewState) override
    {
        // ここで見た目を更新
    }
};
```

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

1. 信号機BPアクターをレベルに配置し、各インスタンスの `MyTrafficLightId` を設定
2. 外部システム（受信アクター等）がSubsystemに状態を送信

※ Subsystemはレベルへの配置不要（`UWorldSubsystem` として自動生成される）

#### エディタ自動配置（FSignalGenerator）

OpenDRIVEファイルの信号データに基づいて `FSignalGenerator` がアクターを自動配置する場合、
`IBPI_SignalAutoSetup` インターフェースを実装したアクターは `OnSignalAutoPlaced` が自動的に呼ばれる。

`AOsiTrafficLightActor` を継承したBPサブクラスを `USignalTypeMapping` に登録しておけば、
自動配置時に `MyTrafficLightId` が `SignalInfo->SignalId` から自動設定される。

```
FSignalGenerator::GenerateSignals()
  -> SpawnActor (SignalTypeMappingに基づく)
  -> SignalInfoComponent を作成・添付 (SignalId, Type, etc.)
  -> IBPI_SignalAutoSetup::Execute_OnSignalAutoPlaced(Actor, InfoComp)
     -> AOsiTrafficLightActor::OnSignalAutoPlaced_Implementation()
        -> MyTrafficLightId = SignalInfo->SignalId  // 自動設定
```

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
