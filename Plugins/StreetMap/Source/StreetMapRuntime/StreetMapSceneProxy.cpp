// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapSceneProxy.h"
#include "StreetMapRuntime.h"
#include "StreetMapComponent.h"
#include "SceneManagement.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"


FStreetMapSceneProxy::FStreetMapSceneProxy(const UStreetMapComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent),
	VertexFactory(GetScene().GetFeatureLevel(), "FStreetMapSceneProxy"),
	MaterialInterface(nullptr),
	StreetMapComp(InComponent),
	CollisionResponse(InComponent->GetCollisionResponseToChannels())
{

}

void FStreetMapSceneProxy::Init(const UStreetMapComponent* InComponent, const TArray< FStreetMapVertex >& Vertices, const TArray< uint32 >& Indices)
{
	// Copy index buffer
	IndexBuffer32.Indices = Indices;

	MaterialInterface = nullptr;
	this->MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetShaderPlatform());


	// Copy vertex data
	const int32 NumVerts = Vertices.Num();
	TArray<FDynamicMeshVertex> DynamicVertices;
	DynamicVertices.SetNumUninitialized(NumVerts);

	for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++)
	{
		const FStreetMapVertex& StreetMapVert = Vertices[VertIdx];
		FDynamicMeshVertex& Vert = DynamicVertices[VertIdx];
		Vert.Position = FVector3f(StreetMapVert.Position);
		Vert.Color = StreetMapVert.Color;
		Vert.TextureCoordinate[0] = FVector2f(StreetMapVert.TextureCoordinate);
		Vert.SetTangents(FVector3f(StreetMapVert.TangentX), FVector3f(FVector::CrossProduct(StreetMapVert.TangentZ, StreetMapVert.TangentX)), FVector3f(StreetMapVert.TangentZ));
	}

	VertexBuffer.InitFromDynamicVertex(&VertexFactory, DynamicVertices);

	// Enqueue initialization of render resource
	InitResources();

	// Set a material
	{
		if (InComponent->GetNumMaterials() > 0)
		{
			MaterialInterface = InComponent->GetMaterial(0);
		}

		// Use the default material if we don't have one set
		if (MaterialInterface == nullptr)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}
}

FStreetMapSceneProxy::~FStreetMapSceneProxy()
{
	VertexBuffer.PositionVertexBuffer.ReleaseResource();
	VertexBuffer.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffer.ColorVertexBuffer.ReleaseResource();
	IndexBuffer32.ReleaseResource();
	VertexFactory.ReleaseResource();
}


SIZE_T FStreetMapSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FStreetMapSceneProxy::InitResources()
{
	// Start initializing our vertex buffer, index buffer, and vertex factory.  This will be kicked off on the render thread.
	BeginInitResource(&VertexBuffer.PositionVertexBuffer);
	BeginInitResource(&VertexBuffer.StaticMeshVertexBuffer);
	BeginInitResource(&VertexBuffer.ColorVertexBuffer);
	BeginInitResource(&IndexBuffer32);
	BeginInitResource(&VertexFactory);
}


bool FStreetMapSceneProxy::MustDrawMeshDynamically(const FSceneView& View) const
{
	return (AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe) || IsSelected();
}


bool FStreetMapSceneProxy::IsInCollisionView(const FEngineShowFlags& EngineShowFlags) const
{
	return EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
}

FPrimitiveViewRelevance FStreetMapSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);

	const bool bAlwaysHasDynamicData = false;

	// Only draw dynamically if we're drawing in wireframe or we're selected in the editor
	Result.bDynamicRelevance = MustDrawMeshDynamically(*View) || bAlwaysHasDynamicData;
	Result.bStaticRelevance = !MustDrawMeshDynamically(*View);

	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}


bool FStreetMapSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}


void FStreetMapSceneProxy::MakeMeshBatch(FMeshBatch& Mesh, FMaterialRenderProxy* OverrideMaterialRenderProxy, bool bIsWireframe) const
{
	FMaterialRenderProxy* MaterialProxy = OverrideMaterialRenderProxy;
	if (MaterialProxy == nullptr)
	{
		UMaterialInterface* ActiveMaterial = MaterialInterface != nullptr ? MaterialInterface : StreetMapComp->GetDefaultMaterial();
		MaterialProxy = ActiveMaterial->GetRenderProxy();
	}

	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer32;
	Mesh.bWireframe = bIsWireframe;
	Mesh.VertexFactory = &VertexFactory;
	Mesh.MaterialRenderProxy = MaterialProxy;
	Mesh.CastShadow = true;
	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
	BatchElement.FirstIndex = 0;
	const int IndexCount = IndexBuffer32.Indices.Num();
	BatchElement.NumPrimitives = IndexCount / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer.PositionVertexBuffer.GetNumVertices() - 1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
}


void FStreetMapSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	const int IndexCount = IndexBuffer32.Indices.Num();
	if (VertexBuffer.PositionVertexBuffer.GetNumVertices() > 0 && IndexCount > 0)
	{
		const float ScreenSize = 1.0f;

		FMeshBatch MeshBatch;
		MakeMeshBatch(MeshBatch, nullptr, false);
		PDI->DrawMesh(MeshBatch, ScreenSize);
	}
}


void FStreetMapSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	const int IndexCount = IndexBuffer32.Indices.Num();
	if (VertexBuffer.PositionVertexBuffer.GetNumVertices() > 0 && IndexCount > 0)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FSceneView& View = *Views[ViewIndex];

			const bool bIsWireframe = AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe;

			FMaterialRenderProxy* OverrideMaterialRenderProxy = nullptr;
			if (bIsWireframe && GEngine->WireframeMaterial)
			{
				FColoredMaterialRenderProxy* WireframeMaterialRenderProxy = new FColoredMaterialRenderProxy(GEngine->WireframeMaterial->GetRenderProxy(), FLinearColor(0, 0.5f, 1.f));
				Collector.RegisterOneFrameMaterialProxy(WireframeMaterialRenderProxy);
				OverrideMaterialRenderProxy = WireframeMaterialRenderProxy;
			}

			if (MustDrawMeshDynamically(View))
			{
				const bool bInCollisionView = IsInCollisionView(ViewFamily.EngineShowFlags);
				const bool bCanDrawCollision = bInCollisionView && IsCollisionEnabled();

				if (!IsCollisionEnabled() && bInCollisionView)
				{
					continue;
				}

				if (bCanDrawCollision && OverrideMaterialRenderProxy == nullptr && GEngine->ShadedLevelColorationUnlitMaterial)
				{
					FColoredMaterialRenderProxy* CollisionMaterialRenderProxy = new FColoredMaterialRenderProxy(GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(), FColor::Cyan);
					Collector.RegisterOneFrameMaterialProxy(CollisionMaterialRenderProxy);
					OverrideMaterialRenderProxy = CollisionMaterialRenderProxy;
				}

				// Draw the mesh!
				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MakeMeshBatch(MeshBatch, OverrideMaterialRenderProxy, bIsWireframe);
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}
}


uint32 FStreetMapSceneProxy::GetMemoryFootprint(void) const
{
	return sizeof(*this) + GetAllocatedSize();
}
