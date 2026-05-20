# UE5 DynamicMesh ― OpenDRIVE 道路メッシュ生成のための C++ 事前調査

調査日: 2026-05-17
対象エンジン: **Unreal Engine 5.3**
本ドキュメントは OpenDRIVE プラグインから道路メッシュをランタイム/エディタ生成する目的の **事前調査** であり、実装方針の確定資料ではない。

API・ヘッダ構成の交差検証はローカルにあった UE 5.7 ツリー(`E:\UnrealEngine\UE_5.7`)で行ったが、本書で挙げる関数・クラスはすべて UE 5.3 公式ドキュメントで存在を確認済み。Geometry Script は UE 5.0 から導入され、**5.3 で Beta** に昇格した状態。本書で挙げる主要 API(`UDynamicMesh` / `UDynamicMeshComponent` / `UDynamicMeshPool` / `AppendSweepPolygon` / `AppendSweepPolyline` / `AppendTriangulatedPolygon` / `AppendSimpleExtrudePolygon` / `AppendRevolvePath` / `AppendBuffersToMesh` / `CopyMeshToStaticMesh` / `EditMesh` / `ProcessMesh` / `RecomputeNormals` 等)はすべて UE 5.3 時点で利用可能。

5.3 → 以降で増えた可能性のある関数(`MeshSculptLayersFunctions`、`MeshGeodesicFunctions`、一部の高度 sampling 系など)は本書の中心スコープ外なので、必要になった時点で 5.3 の `GeometryScriptingCore/Public/GeometryScript/` を直接参照して再確認すること。

---

## 1. クラス階層の全体像

| 層 | クラス | 役割 | モジュール |
|---|---|---|---|
| 低レベル | `UE::Geometry::FDynamicMesh3` | 三角形メッシュ実体。接続性付きインデックスメッシュ。属性レイヤ(法線、UV×8、頂点色、マテリアルID、PolyGroup) | `GeometryCore` |
| UObject ラッパ | `UDynamicMesh` | `FDynamicMesh3` を保持する `UObject`。GC・BP 連携・変更通知 | `GeometryFramework` |
| コンポーネント | `UDynamicMeshComponent` | レンダリング用 `UPrimitiveComponent`。マテリアルセット・コリジョン管理 | `GeometryFramework` |
| 高レベル API | `UGeometryScriptLibrary_*` | Blueprint/Python と共有の関数群。C++ から `static` 呼び出し可 | `GeometryScriptingCore` |
| プール | `UDynamicMeshPool` | 一時メッシュの再利用(GC 圧削減) | `GeometryFramework` |

主要ヘッダ位置(UE 5.3 ソース)。5.3 / 5.7 で配置は同じ:
- `Engine/Source/Runtime/GeometryFramework/Public/UDynamicMesh.h`
- `Engine/Source/Runtime/GeometryFramework/Public/Components/DynamicMeshComponent.h`
- `Engine/Source/Runtime/GeometryCore/Public/DynamicMesh/DynamicMesh3.h`
- `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Public/GeometryScript/*.h`

---

## 2. Build.cs に追加するモジュール

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "GeometryCore",            // FDynamicMesh3 本体
    "GeometryFramework",       // UDynamicMesh / UDynamicMeshComponent / UDynamicMeshPool
    "GeometryScriptingCore",   // UGeometryScriptLibrary_* 高レベル API
});

// エディタ側で StaticMesh アセット書き出しまでやるなら:
if (Target.bBuildEditor) {
    PublicDependencyModuleNames.Add("GeometryScriptingEditor");
}
```

現状 [Source/OpenDRIVE/OpenDRIVE.Build.cs](../Source/OpenDRIVE/OpenDRIVE.Build.cs) には Geometry 系モジュールは未追加。実装着手時に上記を追加する。

---

## 3. C++ 利用パターン3種

### A. 高レベル: Geometry Script を C++ から呼ぶ(推奨)

UFunction の `static` を直接呼ぶだけ。Blueprint と挙動は完全一致。OpenDRIVE 用途で主軸になるアプローチ。

```cpp
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

UDynamicMesh* Target = MeshComponent->GetDynamicMesh();
Target->Reset();

// 2D 断面(車線断面)を 3D パス(センターライン + s 毎の FTransform)に沿ってスイープ
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
    Target,
    PrimitiveOptions,           // FGeometryScriptPrimitiveOptions
    FTransform::Identity,
    LaneCrossSection2D,         // TArray<FVector2D> 断面
    SweepPath,                  // TArray<FTransform> s 刻みの位置/姿勢/スケール
    /*bLoop=*/ false,
    /*bCapped=*/ true,
    /*StartScale=*/ 1.0f,
    /*EndScale=*/ 1.0f,
    /*RotationDeg=*/ 0.0f
);

UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Target, /*opts*/{});
MeshComponent->NotifyMeshUpdated();
```

OpenDRIVE 用に特に有用な関数 (`MeshPrimitiveFunctions.h`):
- **`AppendSweepPolyline`** / **`AppendSweepPolygon`** ― 道路の本命。`TArray<FTransform>` がそのまま `s` パラメータ刻みのフレームになる
- `AppendRevolvePath` ― ラウンドアバウト/ループ
- `AppendSimpleExtrudePolygon` ― 縁石・標識など単純押し出し
- `AppendTriangulatedPolygon` ― 交差点の任意ポリゴン面(制約付き Delaunay)

### B. 中レベル: バッファ一括追加

OpenDRIVE のように「s ステップごとに左右レーンの t を評価して頂点を吐く」格子構造に最も素直。

```cpp
FGeometryScriptSimpleMeshBuffers Buffers;
Buffers.Vertices  = ...;   // TArray<FVector>
Buffers.Normals   = ...;
Buffers.UV0       = ...;   // U=s/length, V=t/width 等
Buffers.Triangles = ...;   // TArray<FIntVector>
FGeometryScriptIndexList NewTris;
UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
    Target, Buffers, NewTris, /*MaterialID=*/0);
```

`FGeometryScriptSimpleMeshBuffers` の構成は `MeshBasicEditFunctions.h` の冒頭に定義(Vertices/Normals/UV0..UV7/VertexColors/Triangles/TriGroupIDs)。

### C. 低レベル: `EditMesh` ラムダで `FDynamicMesh3` 直接編集

最大限の柔軟性が要る場合(例: 車線境界線を edge group として保持、PolyGroup を交差点パッチごとに振る、隣接情報を直接構築)。

```cpp
Target->EditMesh([&](UE::Geometry::FDynamicMesh3& M)
{
    M.EnableAttributes();
    M.EnableTriangleGroups();
    int v0 = M.AppendVertex(FVector3d(...));
    int v1 = M.AppendVertex(FVector3d(...));
    int v2 = M.AppendVertex(FVector3d(...));
    M.AppendTriangle(v0, v1, v2, /*GroupID*/ LaneId);
    // 属性: M.Attributes()->PrimaryUV()->SetElement(...) など
}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown);
```

`UDynamicMesh.h` のコメント上、公式エントリは `ProcessMesh`(const) と `EditMesh`(mut)。`GetMeshRef()` も残っているが「将来削除候補」と明記されている。

---

## 4. コンポーネントへの貼り付けとマテリアル

```cpp
// UDynamicMesh ごと差し替え可能(所有権移譲)
MeshComponent->SetDynamicMesh(MyMesh);

// マテリアル ID ごとにスロットを割り当て
TArray<UMaterialInterface*> Mats = { AsphaltMat, MarkingMat, SidewalkMat };
MeshComponent->ConfigureMaterialSet(Mats, /*bDeleteExtraSlots=*/true);

// 部分更新通知で再生成コストを下げる
MeshComponent->FastNotifyPositionsUpdated();
MeshComponent->FastNotifyUVsUpdated();
```

コリジョン:
- `EnableComplexAsSimpleCollision()` で三角形メッシュをそのまま当たり判定に利用可(車両走行/ホイールトレース用に有効)
- 再生成は `UpdateCollision(true)`、重い場合は `bDeferCollisionUpdates`

---

## 5. 一時メッシュプール

道路はセグメント(s 区間)単位で組み立てて最後に結合する流れになりがち。GC 圧を避けるためアクター/ジェネレータが `UDynamicMeshPool` を1つ保持する設計が定石。

```cpp
UDynamicMesh* Tmp = Pool->RequestMesh();   // 空か Reset 済みが返る
// ... AppendSweepPolygon 等で構築 ...
UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Final, Tmp, FTransform::Identity);
Pool->ReturnMesh(Tmp);                      // または ReleaseAllComputeMeshes()
```

`ABaseDynamicMeshActor` 系のヘルパに `AllocateComputeMesh()` / `ReleaseComputeMesh()` があり、これも同じプール経由。

---

## 6. StaticMesh アセットへのベイク(エディタ専用)

`GeometryScript/MeshAssetFunctions.h` の `CopyMeshToStaticMesh` で LOD 指定して書き出せる(`GeometryScriptingEditor` 必須)。

OpenDRIVE インポートのワークフロー想定:
1. DynamicMesh で組み立て(ツール上で編集可能)
2. 確定後に StaticMesh アセット化 → Nanite/LOD/ナビメッシュ/ライティングビルドが効くようになる

---

## 7. OpenDRIVE → DynamicMesh への接続点(設計上のポイント)

1. **断面(t 方向)**
   lane の幅プロファイル + `<width>` / `<offset>` を `s` 刻みで評価し、左右端の `t` を出す。各 `s` で `FTransform`(位置=参照線、回転=ヘディング+superelevation、Scale 任意)を作って `SweepPath` に積む。

2. **断面ポリゴン**
   - 単一レーン1枚なら 2D は `(t_left, 0)-(t_right, 0)` の 2 点 → `AppendSweepPolyline`
   - 縁石付きで厚みも持たせるなら閉じたポリゴン → `AppendSweepPolygon`

3. **車線ごとに分離**
   lane ID を **PolyGroup** に格納すると、後段で「中央線抽出」「特定レーンだけ非表示」が `MeshSelectionFunctions` で簡単になる。

4. **UV**
   `AppendSweepPolygon` は `PolylineTexParamU` / `SweepPathTexParamV` を取れる。V に `s/totalLength` を渡せば道路長方向に連続 UV(レーンマーキングのテクスチャに直結)。

5. **交差点(junction)**
   スイープでは作れない。各 connecting road の終端断面を集めて `AppendTriangulatedPolygon`(constrained Delaunay)で面張り。

6. **精度・サンプリング**
   OpenDRIVE のクロソイドは離散化サンプリングが必要。`s` のステップを曲率に応じて適応的(高曲率では細かく)に取ると三角形数を抑えられる。RoadManager(esmini)で `GetXYZ` / `GetH` を s で叩いて FTransform 列を作るのが素直。

7. **コリジョン**
   道路面はシンプルでよいので Complex-as-Simple で十分。縁石・ガードレールは別コンポーネント or 別 PolyGroup → セレクションで分離して別アクタ化。

---

## 8. 注意点・落とし穴

- `GeometryScript` 関数の一部は内部で `ParallelFor` / `UE::Tasks::Launch()` を使う。**`EditMesh` 中に同じメッシュを別スレッドから触らないこと**。
- `UDynamicMesh::Reset()` は `FDynamicMesh3` を解放するが UObject は残る(プール再利用前提)。
- `GetMeshRef()` は将来削除候補。常に `EditMesh` / `ProcessMesh` を使う。
- `AppendSweep*` は **bLoop=false なら `bCapped` を見る**。道路区間の連続結合では端のキャップを切り、結合後に一括 Weld(`MergeVerticesPairs`)するほうがシームが消える。
- `UDynamicMeshComponent` のランタイム描画は速いが、**自動 LOD / Nanite 非対応**。広域マップなら最終的に StaticMesh ベイクが現実的。

---

## 9. 推奨アプローチ(まとめ)

| 段階 | 手段 |
|---|---|
| OpenDRIVE → サンプリング | RoadManager で s 刻みに `GetXYZ` / `GetH` → `TArray<FTransform>` |
| メッシュ生成 | `AppendSweepPolygon`(断面はレーン断面ポリゴン) |
| 属性 | PolyGroup = laneId、UV0.V = s / length |
| 交差点 | `AppendTriangulatedPolygon` |
| 編集中の表示 | `UDynamicMeshComponent` で動的に |
| 最終出力 | `CopyMeshToStaticMesh` でアセット化 |

---

## 10. 参考リソース

公式ドキュメント(UE 5.3):
- [Geometry Scripting Users Guide (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine?application_version=5.3)
- [Geometry Scripting Reference (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-reference-in-unreal-engine?application_version=5.3)
- [AppendSweepPolygon (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Primitives/AppendSweepPolygon?application_version=5.3)
- [UDynamicMesh API リファレンス (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GeometryFramework/UDynamicMesh?application_version=5.3)
- [UDynamicMeshComponent API リファレンス (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GeometryFramework/UDynamicMeshComponent?application_version=5.3)
- [UDynamicMeshPool ドキュメント (5.3)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GeometryFramework/UDynamicMeshPool?application_version=5.3)

コミュニティ/サンプル:
- [gradientspace: Geometry Script FAQ](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq)
- [gradientspace: Runtime mesh generation in UE4.26](http://www.gradientspace.com/tutorials/2020/10/23/runtime-mesh-generation-in-ue426)
- [bendemott/UE5-Procedural-Building (C++ 実装例)](https://github.com/bendemott/UE5-Procedural-Building)
- [DynamicBuilding.cpp 抜粋](https://github.com/bendemott/UE5-Procedural-Building/blob/main/DynamicBuilding.cpp)
- [Prajwal Shetty: Runtime mesh generation with LODs](https://prajwalshetty.com/ue5/Generating-Runtime-Mesh-In-Unreal-Engine/)
- [UE Forums: From ProceduralMesh to DynamicMesh](https://forums.unrealengine.com/t/from-proceduralmesh-to-dynamicmesh/661279)
