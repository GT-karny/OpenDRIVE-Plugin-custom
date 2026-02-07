# OSI Traffic Light System

ASAM OSI (Open Simulation Interface) 仕様に基づく信号機の状態管理システム。
外部シミュレータ（esmini等）やco-simulationブリッジから信号状態を受け取り、UE上の信号機アクターに反映する。

既存の `ATrafficLight` / `ATrafficLightController`（タイマー駆動）とは独立した補完的システム。

## アーキテクチャ

```
外部システム (esmini, co-sim bridge, etc.)
    |
    | BPI_TrafficLightHandlerUpdate::UpdateTrafficLightById(Id, State)
    | BPI_TrafficLightHandlerUpdate::UpdateTrafficLightsBatch(Updates)
    v
+------------------------------------------+
| ATrafficLightHandlerBase                 |
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

**設計のポイント**: ハンドラーはアクターの参照を一切持たない。状態管理とイベントブロードキャストのみに特化し、アクター側が自分でデリゲートにBindする（Observer pattern）。

## ファイル構成

```
Source/OpenDRIVE/
  Public/
    OsiTrafficLightTypes.h         -- Enum 3種 + 構造体 2種（ヘッダのみ）
    BPI_TrafficLightUpdate.h       -- 信号機アクター側 Blueprint Interface
    BPI_TrafficLightHandlerUpdate.h -- ハンドラー側 Blueprint Interface
    TrafficLightHandlerBase.h      -- ハンドラーベースクラス
  Private/
    BPI_TrafficLightUpdate.cpp
    BPI_TrafficLightHandlerUpdate.cpp
    TrafficLightHandlerBase.cpp
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

外部システムがハンドラーに状態を渡すためのインターフェース。

| メソッド | 説明 |
|----------|------|
| `UpdateTrafficLightById(int32 Id, FOsiTrafficLightState State)` | 1つの信号を更新 |
| `UpdateTrafficLightsBatch(TArray<FOsiTrafficLightBatchEntry> Updates)` | 複数信号を一括更新 |

## TrafficLightHandlerBase

状態キャッシュ + デリゲートブロードキャストを行うアクター。

| メンバ | 型 | 説明 |
|--------|----|------|
| `OnTrafficLightStateUpdated` | `FOnOsiTrafficLightStateUpdated` | 状態更新時にBroadcast（BlueprintAssignable） |
| `GetTrafficLightState()` | `bool(int32, FOsiTrafficLightState&)` | キャッシュから現在状態を取得 |
| `StateCache` | `TMap<int32, FOsiTrafficLightState>` | 各IDの最新状態を保持 |

`Blueprintable` のため、BP子クラスを作成してカスタマイズ可能。

---

## 実装ガイド

### 1. 信号機アクターの作成（Blueprint）

信号機の見た目を持つBPアクターを作成し、状態変化に応じて表示を切り替える。

#### Step 1: BPアクターの作成とインターフェース追加

1. Content Browser で右クリック → `Blueprint Class` → `Actor` を選択
2. 名前を `BP_OsiTrafficLight` などにする
3. BP を開き、`Class Settings` をクリック
4. Details パネル → `Interfaces` → `Add` → `BPI_TrafficLightUpdate` を検索して追加
5. `Compile` をクリック → Interfaces セクションに `OnTrafficLightUpdate` イベントが表示される

#### Step 2: コンポーネントの追加

Viewport/Components パネルで信号機の見た目を構成する:

```
DefaultSceneRoot (Scene)
  +-- SignalMesh (Static Mesh)      -- 信号機本体のメッシュ
  +-- RedLight (Point Light)        -- 赤ライト
  +-- YellowLight (Point Light)     -- 黄ライト
  +-- GreenLight (Point Light)      -- 緑ライト
```

※ 構成は自由。上記は一例。

#### Step 3: 変数の追加

My Blueprint パネル → Variables で以下を追加:

| 変数名 | 型 | デフォルト | 設定 |
|--------|----|-----------|------|
| `MyTrafficLightId` | Integer | 0 | `Instance Editable` をON（目のアイコン） |
| `HandlerRef` | `Traffic Light Handler Base` (Object Reference) | なし | 内部用 |

`MyTrafficLightId` を `Instance Editable` にすることで、レベルに配置した各インスタンスごとに異なるIDをDetailsパネルから設定できる。

#### Step 4: BeginPlay でハンドラーを探してデリゲートにBind

Event Graph:

```
Event BeginPlay
    |
    v
[Get All Actors Of Class]
  Actor Class: TrafficLightHandlerBase
    |
    Out Actors (配列) ──→ [Get (index 0)] ──→ [Cast To TrafficLightHandlerBase]
                                                  |
                                                  v (As Traffic Light Handler Base)
                                            [Set HandlerRef] ← 変数に保存
                                                  |
                                                  v
                                            [Bind Event to OnTrafficLightStateUpdated]
                                              Target: HandlerRef
                                              Event: ──→ [Create Event] ──→ "OnStateReceived" (カスタムイベントを作成)
```


**Bind Event の手順:**
1. `HandlerRef` をドラッグ → `Bind Event to On Traffic Light State Updated` を選択
2. `Event` ピンから線を引いて `Create Event` を選択
3. `Select Function` で `Create a matching function` を選択
   → 自動的に `TrafficLightId (int32)` と `NewState (FOsiTrafficLightState)` のパラメータを持つカスタムイベントが作成される

※ または `Event` ピンからドラッグして `Add Custom Event...` でも可

#### Step 5: コールバックでIDフィルタリング

作成されたカスタムイベント（例: `OnStateReceived`）内:

```
Custom Event: OnStateReceived
  (TrafficLightId: int32, NewState: FOsiTrafficLightState)
    |
    v
[Equal (==)]  ← TrafficLightId と MyTrafficLightId を比較
    |
    v
[Branch]
    |
    |-- True ──→ [OnTrafficLightUpdate] ← Interfaces セクションから呼び出し (Message)
    |              Target: Self            NewState: NewState を接続
    |
    |-- False ──→ (何もしない)
```

**OnTrafficLightUpdate の呼び方:**
- 右クリック → `Call Function` ではなく `Interfaces` → `BPI Traffic Light Update` → `On Traffic Light Update` (Message) を使う
- または `Self` をドラッグ → `On Traffic Light Update` を検索

#### Step 6: OnTrafficLightUpdate の実装

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

#### Step 7: レベルへの配置

1. `TrafficLightHandlerBase` をレベルに1つ配置
2. `BP_OsiTrafficLight` をレベルに必要数配置
3. 各インスタンスの Details パネルで `My Traffic Light Id` にユニークなIDを設定
   - 例: 交差点の北側 = 1, 南側 = 2, 東側 = 3, 西側 = 4

#### 全体のノードフロー図

```
=== BeginPlay ===

Event BeginPlay
  → Get All Actors Of Class (TrafficLightHandlerBase)
  → Get [0]
  → Cast To TrafficLightHandlerBase
  → SET HandlerRef
  → Bind Event to OnTrafficLightStateUpdated
      → Custom Event "OnStateReceived"

=== OnStateReceived (デリゲートコールバック) ===

OnStateReceived(TrafficLightId, NewState)
  → TrafficLightId == MyTrafficLightId?
  → Branch
      True → OnTrafficLightUpdate(Self, NewState)

=== OnTrafficLightUpdate (インターフェース実装) ===

Event OnTrafficLightUpdate(NewState)
  → Break FOsiTrafficLightState
  → Switch on Color → ライト色切り替え
  → Switch on Mode  → 点灯/点滅/OFF制御
  → Switch on Icon  → 矢印表示切り替え（必要に応じて）
```

### 2. 信号機アクターの作成（C++）

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
        // ハンドラーを探してデリゲートにBind
        TArray<AActor*> Handlers;
        UGameplayStatics::GetAllActorsOfClass(
            GetWorld(), ATrafficLightHandlerBase::StaticClass(), Handlers);
        if (Handlers.Num() > 0)
        {
            auto* Handler = Cast<ATrafficLightHandlerBase>(Handlers[0]);
            Handler->OnTrafficLightStateUpdated.AddDynamic(
                this, &AMyTrafficLight::OnStateUpdated);
        }
    }

    UFUNCTION()
    void OnStateUpdated(int32 TrafficLightId, const FOsiTrafficLightState& NewState)
    {
        if (TrafficLightId != MyTrafficLightId) return;
        // BPI_TrafficLightUpdate の実装を呼ぶ
        OnTrafficLightUpdate(NewState);
    }

    virtual void OnTrafficLightUpdate_Implementation(
        const FOsiTrafficLightState& NewState) override
    {
        // ここで見た目を更新
        // NewState.Color, NewState.Icon, NewState.Mode, NewState.Counter
    }
};
```

### 3. 外部システムからの状態送信

```cpp
// ハンドラーを取得
auto* Handler = Cast<ATrafficLightHandlerBase>(HandlerActor);

// 個別更新
FOsiTrafficLightState State;
State.Color = EOsiTrafficLightColor::RED;
State.Icon = EOsiTrafficLightIcon::NONE;
State.Mode = EOsiTrafficLightMode::CONSTANT;
IBPI_TrafficLightHandlerUpdate::Execute_UpdateTrafficLightById(
    Handler, /*TrafficLightId=*/ 1, State);

// バッチ更新
TArray<FOsiTrafficLightBatchEntry> Updates;
Updates.Add({1, {EOsiTrafficLightColor::RED,   EOsiTrafficLightIcon::NONE, EOsiTrafficLightMode::CONSTANT, 0.f}});
Updates.Add({2, {EOsiTrafficLightColor::GREEN, EOsiTrafficLightIcon::NONE, EOsiTrafficLightMode::CONSTANT, 0.f}});
IBPI_TrafficLightHandlerUpdate::Execute_UpdateTrafficLightsBatch(
    Handler, Updates);
```

### 4. レベル配置

1. `TrafficLightHandlerBase`（またはBP子クラス）をレベルに1つ配置
2. 信号機BPアクターをレベルに配置し、各インスタンスの `MyTrafficLightId` を設定
3. 外部システムが `BPI_TrafficLightHandlerUpdate` 経由でハンドラーに状態を送信

## 既存システムとの関係

| 項目 | 既存システム | OSIシステム（本実装） |
|------|------------|---------------------|
| 状態モデル | `ETrafficLightState` (4値) | `FOsiTrafficLightState` (Color+Icon+Mode+Counter) |
| 更新方式 | タイマー駆動（Tick） | イベント駆動（Push + Delegate） |
| コントローラ | `ATrafficLightController` | `ATrafficLightHandlerBase` |
| アクター結合 | 直接参照 (`ATrafficLight*`) | 参照なし（Delegate broadcast） |

既存の `ATrafficLight` を拡張して `BPI_TrafficLightUpdate` を実装すれば、両システムを共存させることも可能。

## OSI仕様リファレンス

本実装の型定義は `osi3::TrafficLight::Classification` に1対1で対応:
- `Color` → `EOsiTrafficLightColor`
- `Icon` → `EOsiTrafficLightIcon`
- `Mode` → `EOsiTrafficLightMode`
- `counter` → `FOsiTrafficLightState::Counter`

詳細: [ASAM OSI Documentation](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/gen/structosi3_1_1TrafficLight.html)
