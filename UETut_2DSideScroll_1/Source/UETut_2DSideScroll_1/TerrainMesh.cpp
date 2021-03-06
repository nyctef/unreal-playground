//#include "ProceduralMeshesPrivatePCH.h"
#include "TerrainMesh.h"
#include <algorithm>

DEFINE_STAT(STAT_GenerateMesh);
DEFINE_STAT(STAT_CheckMapData);
DEFINE_STAT(STAT_CreateFVecs);
DEFINE_STAT(STAT_BuildQuads);
DEFINE_STAT(STAT_SetupBuffers);

ATerrainMesh::ATerrainMesh()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	//MeshComponent->bShouldSerializeMeshData = false;
	MeshComponent->SetupAttachment(RootComponent);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ATerrainMesh::PostActorCreated()
{
	Super::PostActorCreated();
	SetupBuffers(Size);
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void ATerrainMesh::PostLoad()
{
	Super::PostLoad();
	SetupBuffers(Size);
	GenerateMesh();
}

void ATerrainMesh::SetupBuffers(FVector Size)
{
	UE_LOG(LogTemp, Log, TEXT("GenerateMesh() start"));
	SCOPE_CYCLE_COUNTER(STAT_SetupBuffers);

	if (updateTextureRegion) {
		delete updateTextureRegion;
	} 
	
	updateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, Size.X, Size.Z);

	auto sx = (int32)Size.X;
	auto sy = (int32)Size.Y;
	auto sz = (int32)Size.Z;

	int32 VertexCount = (sx + 1) * (sz + 1);
	MapData.Init(true, sx * sz);

	// add a border of space around the edges of the map
	for (int32 x = 0; x < sx; x++) {
		MapData[x] = false;
		MapData[(sx*sz) - x - 1] = false;
	}
	for (int32 z = 0; z < sz; z++) {
		MapData[z*sx] = false;
		MapData[(z*sx) + (sx - 1)] = false;
	}
}

FORCEINLINE const FConstBitReference IsSolidAt(const TBitArray<FDefaultBitArrayAllocator>& mapData, int32 rowSize, int32 x, int32 z) {
	return mapData[(z*rowSize) + x];
}

FORCEINLINE void SetIsSolidAt(TBitArray<FDefaultBitArrayAllocator>& mapData, int32 rowSize, int32 x, int32 z, bool value) {
	//UE_LOG(LogTemp, Log, TEXT("SetIsSolidAt %d,%d with row size %d to %d"), x, z, rowSize, value);
	mapData[(z*rowSize) + x] = value;
}

void ATerrainMesh::GenerateMesh()
{
	UE_LOG(LogTemp, Log, TEXT("GenerateMesh() start"));
	SCOPE_CYCLE_COUNTER(STAT_GenerateMesh);

	auto sx = (int32)Size.X;
	//auto sy = (int32)Size.Y;
	auto sz = (int32)Size.Z;

	Vertices.Reset();
	Triangles.Reset();

	auto Left = 0.0f;
	auto Bottom = 0.0f;
	auto Right = +Size.X;
	auto Top = +Size.Z;

	// ref: https://en.wikipedia.org/wiki/Marching_squares
	for (int mapZ = 0; mapZ < sz - 1; mapZ++) {
		for (int mapX = 0; mapX < sx - 1; mapX++) {
			uint8 cell = 0;
			{
				SCOPE_CYCLE_COUNTER(STAT_CheckMapData);

				if (IsSolidAt(MapData, sx, mapX, mapZ)) { cell += 1; }
				if (IsSolidAt(MapData, sx, mapX + 1, mapZ)) { cell += 2; }
				if (IsSolidAt(MapData, sx, mapX + 1, mapZ + 1)) { cell += 4; }
				if (IsSolidAt(MapData, sx, mapX, mapZ + 1)) { cell += 8; }
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_CreateFVecs);
				auto cellLeftInner = FVector(Left + mapX - 0.5, 1, Bottom + mapZ);
				auto cellLeftOuter = FVector(Left + mapX - 0.5, 0, Bottom + mapZ);
				auto cellBottomInner = FVector(Left + mapX, 1, Bottom + mapZ - 0.5);
				auto cellBottomOuter = FVector(Left + mapX, 0, Bottom + mapZ - 0.5);
				auto cellRightInner = FVector(Left + mapX + 0.5, 1, Bottom + mapZ);
				auto cellRightOuter = FVector(Left + mapX + 0.5, 0, Bottom + mapZ);
				auto cellTopInner = FVector(Left + mapX, 1, Bottom + mapZ + 0.5);
				auto cellTopOuter = FVector(Left + mapX, 0, Bottom + mapZ + 0.5);

				// TODO: make sure normals are correct + check if vertex ordering is important

				{
					SCOPE_CYCLE_COUNTER(STAT_BuildQuads);
					switch (cell) {
					case 0b0000:
						break;
					case 0b0001:
						BuildQuad(Vertices, Triangles, cellBottomInner, cellBottomOuter, cellLeftOuter, cellLeftInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0010:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellBottomOuter, cellBottomInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0011:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellLeftOuter, cellLeftInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0100:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0101:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellBottomOuter, cellBottomInner, FVector::ZeroVector, FVector::ZeroVector);
						BuildQuad(Vertices, Triangles, cellLeftInner, cellLeftOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0110:
						BuildQuad(Vertices, Triangles, cellTopInner, cellTopOuter, cellBottomOuter, cellBottomInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b0111:
						BuildQuad(Vertices, Triangles, cellLeftInner, cellLeftOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1000:
						BuildQuad(Vertices, Triangles, cellLeftInner, cellLeftOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1001:
						BuildQuad(Vertices, Triangles, cellTopInner, cellTopOuter, cellBottomOuter, cellBottomInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1010:
						BuildQuad(Vertices, Triangles, cellBottomInner, cellBottomOuter, cellLeftOuter, cellLeftInner, FVector::ZeroVector, FVector::ZeroVector);
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1011:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellTopOuter, cellTopInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1100:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellLeftOuter, cellLeftInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1101:
						BuildQuad(Vertices, Triangles, cellRightInner, cellRightOuter, cellBottomOuter, cellBottomInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1110:
						BuildQuad(Vertices, Triangles, cellBottomInner, cellBottomOuter, cellLeftOuter, cellLeftInner, FVector::ZeroVector, FVector::ZeroVector);
						break;
					case 0b1111:
						break;
					}
				}
			}
		}
	}

	FBox BoundingBox = FBox(FVector::ZeroVector, Size);

	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, true, EUpdateFrequency::Infrequent);

	FrontVertices.Reset();
	FrontTriangles.Reset();

	BuildQuad(FrontVertices, FrontTriangles, FVector(Left, 1, Bottom), FVector(Right, 1, Bottom), FVector(Right, 1, Top), FVector(Left, 1, Top), FVector::RightVector, FVector::ZeroVector);
	MeshComponent->CreateMeshSection(1, FrontVertices, FrontTriangles, BoundingBox, false, EUpdateFrequency::Infrequent);

	// todo: free this next time
	DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);

	// todo: free this next time
	MapTexture = UTexture2D::CreateTransient(sx, sz);
	MapTexture->UpdateResource();

	// Initialize data
	data.Init(FColor::Blue, sx*sz);

	for (int i = 0; i < sx*sz; i++) {
		data[i] = MapData[i] ? FColor::Blue : FColor::Transparent;
	}

	MapTexture->UpdateTextureRegions((int32)0, (uint32)1, updateTextureRegion, (uint32)(4 * sx), (uint32)4, (uint8*)data.GetData());

	DynamicMaterial->SetTextureParameterValue(FName("Texture"), MapTexture);

	MeshComponent->SetMaterial(1, DynamicMaterial);
}

void ATerrainMesh::BuildQuad(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector BottomLeft, FVector BottomRight, FVector TopRight, FVector TopLeft, FPackedNormal Normal, FPackedNormal Tangent)
{
	int32 Index1 = InVertices.Add(FRuntimeMeshVertexSimple(BottomLeft, Normal, FRuntimeMeshTangent(0,0,0), FVector2D(0,0)));
	int32 Index2 = InVertices.Add(FRuntimeMeshVertexSimple(BottomRight, Normal, FRuntimeMeshTangent(0, 0, 0), FVector2D(1, 0)));
	int32 Index3 = InVertices.Add(FRuntimeMeshVertexSimple(TopRight, Normal, FRuntimeMeshTangent(0, 0, 0), FVector2D(1, 1)));
	int32 Index4 = InVertices.Add(FRuntimeMeshVertexSimple(TopLeft, Normal, FRuntimeMeshTangent(0, 0, 0), FVector2D(0, 1)));
	InTriangles.Add(Index1);
	InTriangles.Add(Index2);
	InTriangles.Add(Index3);
	InTriangles.Add(Index1);
	InTriangles.Add(Index3);
	InTriangles.Add(Index4);
	//InVertices[Index1].Tangent = InVertices[Index2].Tangent = InVertices[Index3].Tangent = InVertices[Index4].Tangent = Tangent;
}

#if WITH_EDITOR
void ATerrainMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ATerrainMesh, Size)))
	{
		SetupBuffers(Size);
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ATerrainMesh, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR

void ATerrainMesh::RemoveSquare(FVector holePos, FVector holeSize)
{
	UE_LOG(LogTemp, Log, TEXT("ATerrainMesh::RemoveSquare ( %f,%f , %f,%f ) in size %f,%f"), holePos.X, holePos.Z, holeSize.X, holeSize.Z, Size.X, Size.Z);

	auto minX = (int32)std::max(holePos.X - holeSize.X/2, 0.0f);
	auto maxX = (int32)std::min(holePos.X + holeSize.X/2, Size.X);
	auto minZ = (int32)std::max(holePos.Z - holeSize.Z/2, 0.0f);
	auto maxZ = (int32)std::min(holePos.Z + holeSize.Z/2, Size.Z);

	UE_LOG(LogTemp, Log, TEXT("ATerrainMesh::RemoveSquare from %d,%d to %d,%d"), minX, minZ, maxX, maxZ);

	for (auto mapZ = minZ; mapZ < maxZ; mapZ++) {
		for (auto mapX = minX; mapX < maxX; mapX++) {
			SetIsSolidAt(MapData, (int32)Size.X, mapX, mapZ, false);
		}
	}

	GenerateMesh();
}

void ATerrainMesh::RemoveCircle(FVector holePos, float holeDiameter)
{
	UE_LOG(LogTemp, Log, TEXT("ATerrainMesh::RemoveCircle ( %f,%f , %f ) in size %f,%f"), holePos.X, holePos.Z, holeDiameter, Size.X, Size.Z);

	auto holeRadius = holeDiameter / 2;

	auto minHole = -holeRadius;
	auto maxHole = +holeRadius;

	UE_LOG(LogTemp, Log, TEXT("ATerrainMesh::RemoveCircle from %f,%f to %f,%f"), holePos.X+minHole, holePos.Z+minHole, holePos.X+maxHole, holePos.Z+maxHole);

	for (auto holeZ = minHole; holeZ < maxHole; holeZ++) {
		for (auto holeX = minHole; holeX < maxHole; holeX++) {
			auto mapZ = holePos.Z + holeZ;
			auto mapX = holePos.X + holeX;

			if (mapZ < 0 || mapZ > Size.Z) { continue; }
			if (mapX < 0 || mapX > Size.X) { continue; }
			if (sqrt(holeX * holeX + holeZ * holeZ) > holeRadius) { continue; }

			SetIsSolidAt(MapData, (int32)Size.X, mapX, mapZ, false);
		}
	}

	GenerateMesh();
}