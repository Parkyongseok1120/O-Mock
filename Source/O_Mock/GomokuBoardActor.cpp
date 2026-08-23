// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBoardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GomokuGameState.h"
#include "GomokuRuleEngine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuBoardPresentation, Log, All);

AGomokuBoardActor::AGomokuBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BoardPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardPlane"));
	BoardPlane->SetupAttachment(SceneRoot);
	BoardPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardPlane->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoardPlane->SetRelativeLocation(FVector(0.f, 0.f, -12.f));
	BoardPlane->SetCastShadow(true);

	BoardPedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardPedestal"));
	BoardPedestal->SetupAttachment(SceneRoot);
	BoardPedestal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoardPedestal->SetRelativeLocation(FVector(0.f, 0.f, -100.f));
	BoardPedestal->SetCastShadow(true);

	GroundPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundPlane"));
	GroundPlane->SetupAttachment(SceneRoot);
	GroundPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundPlane->SetRelativeLocation(FVector(0.f, 0.f, -250.f));
	GroundPlane->SetRelativeScale3D(FVector(120.f, 120.f, 1.8f));
	GroundPlane->SetCastShadow(false);

	StoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer1"));
	StoneInstances->SetupAttachment(SceneRoot);
	StoneInstancesPlayer2 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer2"));
	StoneInstancesPlayer2->SetupAttachment(SceneRoot);
	StoneInstancesPlayer3 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer3"));
	StoneInstancesPlayer3->SetupAttachment(SceneRoot);
	StoneInstancesPlayer4 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer4"));
	StoneInstancesPlayer4->SetupAttachment(SceneRoot);
	BoardGridInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoardGridInstances"));
	BoardGridInstances->SetupAttachment(SceneRoot);
	BlockedCellInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlockedCellInstances"));
	BlockedCellInstances->SetupAttachment(SceneRoot);
	BoardFrameInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoardFrameInstances"));
	BoardFrameInstances->SetupAttachment(SceneRoot);
	HoverCellInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HoverCellInstances"));
	HoverCellInstances->SetupAttachment(SceneRoot);
	HillInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HillInstances"));
	HillInstances->SetupAttachment(SceneRoot);
	TreeTrunkInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeTrunkInstances"));
	TreeTrunkInstances->SetupAttachment(SceneRoot);
	TreeCanopyInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeCanopyInstances"));
	TreeCanopyInstances->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StoneMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterialFinder(
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (CubeMeshFinder.Succeeded())
	{
		BoardPlane->SetStaticMesh(CubeMeshFinder.Object);
		BoardPedestal->SetStaticMesh(CubeMeshFinder.Object);
		GroundPlane->SetStaticMesh(CubeMeshFinder.Object);
	}
	if (BasicMaterialFinder.Succeeded())
	{
		BoardPlane->SetMaterial(0, BasicMaterialFinder.Object);
		BoardPedestal->SetMaterial(0, BasicMaterialFinder.Object);
		GroundPlane->SetMaterial(0, BasicMaterialFinder.Object);
	}

	UInstancedStaticMeshComponent* StoneComponents[] = {
		StoneInstances.Get(), StoneInstancesPlayer2.Get(), StoneInstancesPlayer3.Get(), StoneInstancesPlayer4.Get()
	};
	for (UInstancedStaticMeshComponent* Component : StoneComponents)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (StoneMeshFinder.Succeeded())
		{
			Component->SetStaticMesh(StoneMeshFinder.Object);
		}
		if (BasicMaterialFinder.Succeeded())
		{
			Component->SetMaterial(0, BasicMaterialFinder.Object);
		}
	}
	UInstancedStaticMeshComponent* SurfaceComponents[] = {
		BoardGridInstances.Get(), BlockedCellInstances.Get(), BoardFrameInstances.Get()
	};
	for (UInstancedStaticMeshComponent* Component : SurfaceComponents)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CubeMeshFinder.Succeeded())
		{
			Component->SetStaticMesh(CubeMeshFinder.Object);
		}
		if (BasicMaterialFinder.Succeeded())
		{
			Component->SetMaterial(0, BasicMaterialFinder.Object);
		}
	}
	HoverCellInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (StoneMeshFinder.Succeeded())
	{
		HoverCellInstances->SetStaticMesh(StoneMeshFinder.Object);
	}
	if (BasicMaterialFinder.Succeeded())
	{
		HoverCellInstances->SetMaterial(0, BasicMaterialFinder.Object);
	}

	UInstancedStaticMeshComponent* EnvironmentComponents[] = {
		HillInstances.Get(), TreeTrunkInstances.Get(), TreeCanopyInstances.Get()
	};
	for (UInstancedStaticMeshComponent* Component : EnvironmentComponents)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (BasicMaterialFinder.Succeeded())
		{
			Component->SetMaterial(0, BasicMaterialFinder.Object);
		}
	}
	if (SphereMeshFinder.Succeeded())
	{
		HillInstances->SetStaticMesh(SphereMeshFinder.Object);
	}
	if (StoneMeshFinder.Succeeded())
	{
		TreeTrunkInstances->SetStaticMesh(StoneMeshFinder.Object);
	}
	if (ConeMeshFinder.Succeeded())
	{
		TreeCanopyInstances->SetStaticMesh(ConeMeshFinder.Object);
	}

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(SceneRoot);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(SceneRoot);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetRelativeRotation(FRotator(-38.f, -32.f, 0.f));
	SunLight->SetIntensity(8.0f);
	SunLight->SetLightColor(FLinearColor(1.0f, 0.88f, 0.72f));
	SunLight->SetAtmosphereSunLight(true);
	SunLight->SetCastShadows(true);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetIntensity(1.0f);
	SkyLight->SetRealTimeCapture(true);

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(SceneRoot);
	HeightFog->SetFogDensity(0.008f);
	HeightFog->SetFogHeightFalloff(0.18f);
	HeightFog->SetVolumetricFog(true);
	HeightFog->SetVolumetricFogDistance(9000.f);

	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	VolumetricCloud->SetupAttachment(SceneRoot);
	if (CloudMaterialFinder.Succeeded())
	{
		VolumetricCloud->SetMaterial(CloudMaterialFinder.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	CameraBoom->SetRelativeRotation(FRotator(-24.f, -35.f, 0.f));
	CameraBoom->TargetArmLength = 2400.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 14.f;

	BoardCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BoardCamera"));
	BoardCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	BoardCamera->SetRelativeLocation(FVector::ZeroVector);
	BoardCamera->SetRelativeRotation(FRotator::ZeroRotator);
	BoardCamera->SetProjectionMode(ECameraProjectionMode::Perspective);
	BoardCamera->bOverrideAspectRatioAxisConstraint = true;
	BoardCamera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
	BoardCamera->SetFieldOfView(55.f);
}

void AGomokuBoardActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Keep the placed Blueprint useful in the editor viewport before PIE starts.
	// Runtime board dimensions still replace this preview through replicated match state.
	ApplyBoardSize(BoardSizeX, BoardSizeY);
	RebuildEnvironmentScenery();
}

void AGomokuBoardActor::BeginPlay()
{
	Super::BeginPlay();

	// BP_GomokuBoard existed while this camera was orthographic and can retain
	// serialized relative-transform overrides from that old attachment. The
	// spring arm owns all orbit positioning now, so the camera must sit exactly
	// on its socket in both upgraded assets and freshly constructed actors.
	if (BoardCamera)
	{
		if (CameraBoom)
		{
			CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
			CameraBoom->SetRelativeRotation(FRotator(-24.f, -35.f, 0.f));
			BoardCamera->AttachToComponent(CameraBoom,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale, USpringArmComponent::SocketName);
		}
		BoardCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		BoardCamera->SetProjectionMode(ECameraProjectionMode::Perspective);
		BoardCamera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
		BoardCamera->SetFieldOfView(55.f);
	}

	if (UWorld* World = GetWorld())
	{
		if (AGomokuGameState* GS = World->GetGameState<AGomokuGameState>())
		{
			GS->OnMatchRestarted.AddDynamic(this, &AGomokuBoardActor::HandleMatchRestarted);
			GS->OnReplicatedBoardChanged.AddDynamic(this, &AGomokuBoardActor::HandleReplicatedBoardChanged);
			if (UGomokuRuleEngine* Engine = GS->GetRuleEngine())
			{
				const FGomokuMatchConfig Cfg = Engine->GetMatchConfig();
				ApplyBoardSize(Cfg.BoardSizeX, Cfg.BoardSizeY);
				RefreshFromReplicatedBoard();
			}
			else if (GS->ReplicatedBoardSizeX > 0 && GS->ReplicatedBoardSizeY > 0)
			{
				RefreshFromReplicatedBoard();
			}
		}
	}

	ConfigureStoneComponent(StoneInstances, FLinearColor(0.03f, 0.03f, 0.03f));
	ConfigureStoneComponent(StoneInstancesPlayer2, FLinearColor(0.92f, 0.92f, 0.92f));
	ConfigureStoneComponent(StoneInstancesPlayer3, FLinearColor(0.15f, 0.45f, 1.0f));
	ConfigureStoneComponent(StoneInstancesPlayer4, FLinearColor(0.95f, 0.12f, 0.08f));
	ConfigureStoneComponent(BoardGridInstances, FLinearColor(0.12f, 0.08f, 0.035f));
	ConfigureStoneComponent(BlockedCellInstances, FLinearColor(0.45f, 0.08f, 0.06f));
	ConfigureStoneComponent(BoardFrameInstances, FLinearColor(0.18f, 0.07f, 0.025f));
	ConfigureStoneComponent(HoverCellInstances, FLinearColor(1.0f, 0.68f, 0.12f));
	ConfigureStoneComponent(HillInstances, FLinearColor(0.12f, 0.24f, 0.16f));
	ConfigureStoneComponent(TreeTrunkInstances, FLinearColor(0.20f, 0.085f, 0.035f));
	ConfigureStoneComponent(TreeCanopyInstances, FLinearColor(0.055f, 0.30f, 0.12f));
	auto ConfigureStaticMaterial = [this](UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		if (!Component || !Component->GetMaterial(0))
		{
			return;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Component->GetMaterial(0), this);
		if (!Material)
		{
			return;
		}
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Component->SetMaterial(0, Material);
		RuntimeMaterials.Add(Material);
	};
	if (BoardPlane && BoardPlane->GetMaterial(0))
	{
		UMaterialInstanceDynamic* BoardMaterial = UMaterialInstanceDynamic::Create(BoardPlane->GetMaterial(0), this);
		if (BoardMaterial)
		{
			const FLinearColor BoardColor(0.76f, 0.48f, 0.20f);
			BoardMaterial->SetVectorParameterValue(TEXT("Color"), BoardColor);
			BoardMaterial->SetVectorParameterValue(TEXT("BaseColor"), BoardColor);
			BoardPlane->SetMaterial(0, BoardMaterial);
			RuntimeMaterials.Add(BoardMaterial);
		}
	}
	ConfigureStaticMaterial(BoardPedestal, FLinearColor(0.16f, 0.075f, 0.032f));
	ConfigureStaticMaterial(GroundPlane, FLinearColor(0.08f, 0.26f, 0.105f));
	FitCameraToBoard();
	UE_LOG(LogGomokuBoardPresentation, Display,
		TEXT("3D board presentation ready: class=%s grid=%d frame=%d hills=%d trees=%d"),
		*GetClass()->GetName(), BoardGridInstances ? BoardGridInstances->GetInstanceCount() : 0,
		BoardFrameInstances ? BoardFrameInstances->GetInstanceCount() : 0,
		HillInstances ? HillInstances->GetInstanceCount() : 0,
		TreeCanopyInstances ? TreeCanopyInstances->GetInstanceCount() : 0);
}

void AGomokuBoardActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (AGomokuGameState* GS = World->GetGameState<AGomokuGameState>())
		{
			GS->OnMatchRestarted.RemoveDynamic(this, &AGomokuBoardActor::HandleMatchRestarted);
			GS->OnReplicatedBoardChanged.RemoveDynamic(this, &AGomokuBoardActor::HandleReplicatedBoardChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGomokuBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PlayerController->GetViewportSize(ViewportX, ViewportY);
		if (ViewportX > 0 && ViewportY > 0 && LastCameraViewportSize != FIntPoint(ViewportX, ViewportY))
		{
			FitCameraToBoard();
		}
	}
	for (int32 i = AnimatingStones.Num() - 1; i >= 0; --i)
	{
		FGomokuAnimatingStone& Animation = AnimatingStones[i];
		UInstancedStaticMeshComponent* Component = Animation.Component.Get();
		if (!Component)
		{
			AnimatingStones.RemoveAtSwap(i);
			continue;
		}
		FTransform T;
		if (!Component->GetInstanceTransform(Animation.InstanceIndex, T, true))
		{
			AnimatingStones.RemoveAtSwap(i);
			continue;
		}
		FVector Scale = T.GetScale3D();
		Scale = FMath::VInterpTo(Scale, Animation.TargetScale, DeltaSeconds, StoneAnimSpeed);
		T.SetScale3D(Scale);
		Component->UpdateInstanceTransform(Animation.InstanceIndex, T, true, true, true);
		if (Scale.Equals(Animation.TargetScale, 0.01f))
		{
			T.SetScale3D(Animation.TargetScale);
			Component->UpdateInstanceTransform(Animation.InstanceIndex, T, true, true, true);
			AnimatingStones.RemoveAtSwap(i);
		}
	}
	UpdateHoverIndicator();
}

void AGomokuBoardActor::ApplyBoardSize(int32 InSizeX, int32 InSizeY)
{
	BoardSizeX = FMath::Max(1, InSizeX);
	BoardSizeY = FMath::Max(1, InSizeY);
	if (BoardPlane)
	{
		const float Width = BoardSizeX * GetEffectiveCellSize();
		const float Height = BoardSizeY * GetEffectiveCellSize();
		BoardPlane->SetRelativeScale3D(FVector(Width / 100.f, Height / 100.f, 0.24f));
		if (BoardPedestal)
		{
			BoardPedestal->SetRelativeScale3D(FVector((Width + 240.f) / 100.f, (Height + 240.f) / 100.f, 1.8f));
		}
	}
	RebuildBoardGrid();
	FitCameraToBoard();
}

void AGomokuBoardActor::RebuildBoardGrid()
{
	if (!BoardGridInstances || !BlockedCellInstances || !BoardFrameInstances || !HoverCellInstances)
	{
		return;
	}

	BoardGridInstances->ClearInstances();
	BlockedCellInstances->ClearInstances();
	BoardFrameInstances->ClearInstances();
	HoverCellInstances->ClearInstances();
	DisplayedHoveredCell = FIntPoint(-1, -1);
	const float EffectiveCellSize = GetEffectiveCellSize();
	const float Width = BoardSizeX * EffectiveCellSize;
	const float Height = BoardSizeY * EffectiveCellSize;
	const float Left = -Width * 0.5f;
	const float Bottom = -Height * 0.5f;
	const float Thickness = FMath::Clamp(EffectiveCellSize * 0.035f, 1.0f, 3.0f);

	for (int32 Y = 0; Y <= BoardSizeY; ++Y)
	{
		FTransform Line;
		Line.SetLocation(FVector(0.f, Bottom + Y * EffectiveCellSize, 2.f));
		Line.SetScale3D(FVector(Width / 100.f, Thickness / 100.f, 0.025f));
		BoardGridInstances->AddInstance(Line);
	}
	for (int32 X = 0; X <= BoardSizeX; ++X)
	{
		FTransform Line;
		Line.SetLocation(FVector(Left + X * EffectiveCellSize, 0.f, 2.f));
		Line.SetScale3D(FVector(Thickness / 100.f, Height / 100.f, 0.025f));
		BoardGridInstances->AddInstance(Line);
	}

	const float FrameWidth = FMath::Clamp(EffectiveCellSize * 0.34f, 12.f, 28.f);
	const float FrameDepth = 0.22f;
	FTransform FrameRail;
	FrameRail.SetLocation(FVector(0.f, Bottom - FrameWidth * 0.5f, -5.f));
	FrameRail.SetScale3D(FVector((Width + FrameWidth * 2.f) / 100.f, FrameWidth / 100.f, FrameDepth));
	BoardFrameInstances->AddInstance(FrameRail);
	FrameRail.SetLocation(FVector(0.f, -Bottom + FrameWidth * 0.5f, -5.f));
	BoardFrameInstances->AddInstance(FrameRail);
	FrameRail.SetLocation(FVector(Left - FrameWidth * 0.5f, 0.f, -5.f));
	FrameRail.SetScale3D(FVector(FrameWidth / 100.f, (Height + FrameWidth * 2.f) / 100.f, FrameDepth));
	BoardFrameInstances->AddInstance(FrameRail);
	FrameRail.SetLocation(FVector(-Left + FrameWidth * 0.5f, 0.f, -5.f));
	BoardFrameInstances->AddInstance(FrameRail);
}

void AGomokuBoardActor::RebuildEnvironmentScenery()
{
	if (!HillInstances || !TreeTrunkInstances || !TreeCanopyInstances)
	{
		return;
	}

	HillInstances->ClearInstances();
	TreeTrunkInstances->ClearInstances();
	TreeCanopyInstances->ClearInstances();
	constexpr float GroundTopZ = -160.f;
	constexpr int32 HillCount = 18;
	for (int32 Index = 0; Index < HillCount; ++Index)
	{
		const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(HillCount);
		const float Radius = 3900.f + static_cast<float>((Index * 173) % 700);
		const float ScaleXY = 8.f + static_cast<float>((Index * 7) % 6);
		const float ScaleZ = 4.f + static_cast<float>((Index * 11) % 5) * 0.65f;
		FTransform Hill;
		Hill.SetLocation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius,
			GroundTopZ + ScaleZ * 42.f));
		Hill.SetScale3D(FVector(ScaleXY * 1.35f, ScaleXY, ScaleZ));
		HillInstances->AddInstance(Hill);
	}

	constexpr int32 TreeCount = 36;
	for (int32 Index = 0; Index < TreeCount; ++Index)
	{
		const float Angle = 2.f * PI * (static_cast<float>(Index) + 0.35f) / static_cast<float>(TreeCount);
		const float Radius = 2450.f + static_cast<float>((Index * 193) % 1150);
		const float TrunkScale = 0.32f + static_cast<float>(Index % 4) * 0.06f;
		const float TrunkHeightScale = 2.3f + static_cast<float>((Index * 5) % 5) * 0.25f;
		const FVector TreeBase(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, GroundTopZ);

		FTransform Trunk;
		Trunk.SetLocation(TreeBase + FVector(0.f, 0.f, TrunkHeightScale * 50.f));
		Trunk.SetScale3D(FVector(TrunkScale, TrunkScale, TrunkHeightScale));
		TreeTrunkInstances->AddInstance(Trunk);

		const float CanopyScale = 1.35f + static_cast<float>((Index * 3) % 5) * 0.16f;
		FTransform Canopy;
		Canopy.SetLocation(TreeBase + FVector(0.f, 0.f, TrunkHeightScale * 100.f + 115.f));
		Canopy.SetRotation(FQuat(FRotator(0.f, static_cast<float>((Index * 47) % 360), 0.f)));
		Canopy.SetScale3D(FVector(CanopyScale, CanopyScale, 3.2f + CanopyScale * 0.35f));
		TreeCanopyInstances->AddInstance(Canopy);
	}
}

void AGomokuBoardActor::AddBlockedCell(int32 X, int32 Y)
{
	if (!BlockedCellInstances || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return;
	}
	const float PadSize = GetEffectiveCellSize() * 0.82f;
	FTransform Pad;
	Pad.SetLocation(GridToWorld(X, Y) + FVector(0.f, 0.f, -5.f));
	Pad.SetScale3D(FVector(PadSize / 100.f, PadSize / 100.f, 0.06f));
	BlockedCellInstances->AddInstance(Pad);
}

void AGomokuBoardActor::FitCameraToBoard()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalController() || !BoardCamera || !CameraBoom)
	{
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	const float EffectiveCellSize = GetEffectiveCellSize();
	const float BoardWidth = BoardSizeX * EffectiveCellSize;
	const float BoardHeight = BoardSizeY * EffectiveCellSize;
	CameraBaseArmLength = FMath::Clamp(FMath::Max(BoardWidth, BoardHeight) * 2.65f, 1200.f, 4200.f);
	CameraBoom->TargetArmLength = CameraBaseArmLength * CameraDistanceMultiplier;
	PC->SetViewTarget(this);
	const FIntPoint NewViewportSize(ViewportX, ViewportY);
	if (LastCameraViewportSize != NewViewportSize)
	{
		LastCameraViewportSize = NewViewportSize;
		UE_LOG(LogGomokuBoardPresentation, Display,
			TEXT("Perspective orbit camera fitted: viewport=%dx%d arm=%.1f pitch=%.1f yaw=%.1f"),
			ViewportX, ViewportY, CameraBoom->TargetArmLength,
			CameraBoom->GetRelativeRotation().Pitch, CameraBoom->GetRelativeRotation().Yaw);
	}
}

void AGomokuBoardActor::OrbitCamera(float YawDelta, float PitchDelta)
{
	if (!CameraBoom)
	{
		return;
	}
	FRotator Rotation = CameraBoom->GetRelativeRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDelta);
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + PitchDelta, -82.f, -12.f);
	Rotation.Roll = 0.f;
	CameraBoom->SetRelativeRotation(Rotation);
}

void AGomokuBoardActor::ZoomCamera(float ZoomInput)
{
	if (!CameraBoom || FMath::IsNearlyZero(ZoomInput))
	{
		return;
	}
	CameraDistanceMultiplier = FMath::Clamp(
		CameraDistanceMultiplier * FMath::Pow(0.86f, ZoomInput), MinimumCameraDistanceMultiplier, 2.15f);
	CameraBoom->TargetArmLength = CameraBaseArmLength * CameraDistanceMultiplier;
	UE_LOG(LogGomokuBoardPresentation, Display, TEXT("Camera wheel zoom: input=%.2f arm=%.1f"),
		ZoomInput, CameraBoom->TargetArmLength);
}

void AGomokuBoardActor::ResetCameraView()
{
	CameraDistanceMultiplier = 1.f;
	if (CameraBoom)
	{
		CameraBoom->SetRelativeRotation(FRotator(-24.f, -35.f, 0.f));
	}
	FitCameraToBoard();
}

float AGomokuBoardActor::GetEffectiveCellSize() const
{
	return FMath::Max(CellSize, 1.0f);
}

FVector AGomokuBoardActor::GetStoneVisualScale() const
{
	const float DiameterScale = (GetEffectiveCellSize() * 0.72f) / 100.f;
	return FVector(DiameterScale, DiameterScale, 0.16f);
}

FVector AGomokuBoardActor::GetBoardOrigin() const
{
	const float EffectiveCellSize = GetEffectiveCellSize();
	return FVector(
		-((BoardSizeX - 1) * EffectiveCellSize) * 0.5f,
		-((BoardSizeY - 1) * EffectiveCellSize) * 0.5f,
		0.f);
}

FVector AGomokuBoardActor::GridToWorld(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return GetBoardOrigin();
	}
	const float EffectiveCellSize = GetEffectiveCellSize();
	const FVector Origin = GetBoardOrigin();
	return FVector(Origin.X + X * EffectiveCellSize, Origin.Y + Y * EffectiveCellSize, 8.f);
}

bool AGomokuBoardActor::WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const
{
	const float EffectiveCellSize = GetEffectiveCellSize();
	const FVector Origin = GetBoardOrigin();
	const float DX = (WorldLoc.X - Origin.X) / EffectiveCellSize;
	const float DY = (WorldLoc.Y - Origin.Y) / EffectiveCellSize;
	const int32 GX = FMath::RoundToInt(DX);
	const int32 GY = FMath::RoundToInt(DY);
	if (GX < 0 || GY < 0 || GX >= BoardSizeX || GY >= BoardSizeY)
	{
		return false;
	}
	const FVector Center = GridToWorld(GX, GY);
	const float Dist2D = FVector(WorldLoc.X - Center.X, WorldLoc.Y - Center.Y, 0.f).Size();
	if (Dist2D > EffectiveCellSize * 0.5f)
	{
		return false;
	}
	OutX = GX;
	OutY = GY;
	return true;
}

int32 AGomokuBoardActor::AddStoneAt(int32 X, int32 Y, int32 PlayerId)
{
	UInstancedStaticMeshComponent* Component = GetStoneComponentForPlayer(PlayerId);
	if (!Component || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return INDEX_NONE;
	}
	FTransform T;
	T.SetLocation(GridToWorld(X, Y));
	const FVector TargetScale = GetStoneVisualScale();
	T.SetScale3D(TargetScale * 0.01f);
	const int32 Index = Component->AddInstance(T);
	FGomokuAnimatingStone Animation;
	Animation.Component = Component;
	Animation.InstanceIndex = Index;
	Animation.TargetScale = TargetScale;
	AnimatingStones.Add(Animation);
	return Index;
}

void AGomokuBoardActor::ClearStones()
{
	AnimatingStones.Reset();
	UInstancedStaticMeshComponent* StoneComponents[] = {
		StoneInstances.Get(), StoneInstancesPlayer2.Get(), StoneInstancesPlayer3.Get(), StoneInstancesPlayer4.Get()
	};
	for (UInstancedStaticMeshComponent* Component : StoneComponents)
	{
		if (Component)
		{
			Component->ClearInstances();
		}
	}
}

void AGomokuBoardActor::HandleStonePlaced(const FIntPoint& Cell)
{
	AddStoneAt(Cell.X, Cell.Y, GetPlayerIdAtCell(Cell));
}

void AGomokuBoardActor::HandleMatchRestarted()
{
	ClearStones();
}

void AGomokuBoardActor::HandleReplicatedBoardChanged()
{
	RefreshFromReplicatedBoard();
}

void AGomokuBoardActor::RefreshFromReplicatedBoard()
{
	if (!GetWorld())
	{
		return;
	}

	const AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS || GS->ReplicatedBoardSizeX <= 0 || GS->ReplicatedBoardSizeY <= 0)
	{
		return;
	}

	const bool bMiniGame = GS->MatchPhase == EMatchPhase::MiniGamePlaying || GS->MatchPhase == EMatchPhase::MiniGameResult;
	const int32 SourceSizeX = bMiniGame ? 7 : GS->ReplicatedBoardSizeX;
	const int32 SourceSizeY = bMiniGame ? 7 : GS->ReplicatedBoardSizeY;
	const TArray<ECellState>& SourceCells = bMiniGame ? GS->MiniGamePuzzleCells : GS->ReplicatedBoardCells;
	if (SourceSizeX <= 0 || SourceSizeY <= 0)
	{
		return;
	}

	ApplyBoardSize(SourceSizeX, SourceSizeY);
	ClearStones();
	for (int32 Index = 0; Index < SourceCells.Num(); ++Index)
	{
		const ECellState State = SourceCells[Index];
		if (State < ECellState::Player1 || State > ECellState::Player4)
		{
			if (State == ECellState::Blocked)
			{
				const int32 X = Index % SourceSizeX;
				const int32 Y = Index / SourceSizeX;
				AddBlockedCell(X, Y);
			}
			continue;
		}

		const int32 X = Index % SourceSizeX;
		const int32 Y = Index / SourceSizeX;
		AddStoneAt(X, Y, UGomokuRuleEngine::CellStateToPlayerId(State));
	}
}

void AGomokuBoardActor::UpdateHoverIndicator()
{
	if (!HoverCellInstances || !GetWorld())
	{
		return;
	}

	const AGomokuGameState* GameState = GetWorld()->GetGameState<AGomokuGameState>();
	const FIntPoint DesiredCell = GameState && GameState->IsGameActive
		? GameState->HoveredCell
		: FIntPoint(-1, -1);
	if (DesiredCell == DisplayedHoveredCell)
	{
		return;
	}

	HoverCellInstances->ClearInstances();
	DisplayedHoveredCell = FIntPoint(-1, -1);
	if (DesiredCell.X < 0 || DesiredCell.Y < 0 || DesiredCell.X >= BoardSizeX || DesiredCell.Y >= BoardSizeY)
	{
		return;
	}

	const float DiameterScale = (GetEffectiveCellSize() * 0.82f) / 100.f;
	FTransform HoverTransform;
	HoverTransform.SetLocation(GridToWorld(DesiredCell.X, DesiredCell.Y) + FVector(0.f, 0.f, -5.0f));
	HoverTransform.SetScale3D(FVector(DiameterScale, DiameterScale, 0.035f));
	HoverCellInstances->AddInstance(HoverTransform);
	DisplayedHoveredCell = DesiredCell;
}

bool AGomokuBoardActor::ScreenToGrid(APlayerController* PlayerController, const FVector2D& ScreenPosition, FIntPoint& OutCell) const
{
	if (!PlayerController || !GetWorld())
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PlayerController, ScreenPosition, WorldOrigin, WorldDirection))
	{
		return false;
	}

	FVector HitLocation;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GomokuBoardScreenToGrid), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, WorldOrigin + WorldDirection * 10000.f, ECC_Visibility, Params))
	{
		HitLocation = Hit.ImpactPoint;
	}
	else
	{
		if (FMath::Abs(WorldDirection.Z) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float Distance = -WorldOrigin.Z / WorldDirection.Z;
		if (Distance < 0.f)
		{
			return false;
		}
		HitLocation = WorldOrigin + WorldDirection * Distance;
	}

	return WorldToGrid(HitLocation, OutCell.X, OutCell.Y);
}

void AGomokuBoardActor::ConfigureStoneComponent(UInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component || !Component->GetMaterial(0))
	{
		return;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Component->GetMaterial(0), this);
	if (!Material)
	{
		return;
	}
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Component->SetMaterial(0, Material);
	RuntimeMaterials.Add(Material);
}

UInstancedStaticMeshComponent* AGomokuBoardActor::GetStoneComponentForPlayer(int32 PlayerId) const
{
	switch (PlayerId)
	{
	case 2: return StoneInstancesPlayer2;
	case 3: return StoneInstancesPlayer3;
	case 4: return StoneInstancesPlayer4;
	default: return StoneInstances;
	}
}

int32 AGomokuBoardActor::GetPlayerIdAtCell(const FIntPoint& Cell) const
{
	const AGomokuGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr;
	if (!GS)
	{
		return 1;
	}
	if (const UGomokuRuleEngine* Engine = GS->GetRuleEngine())
	{
		return FMath::Max(1, UGomokuRuleEngine::CellStateToPlayerId(Engine->GetCellState(Cell.X, Cell.Y)));
	}
	if (GS->ReplicatedBoardSizeX > 0)
	{
		const int32 Index = Cell.Y * GS->ReplicatedBoardSizeX + Cell.X;
		if (GS->ReplicatedBoardCells.IsValidIndex(Index))
		{
			return FMath::Max(1, UGomokuRuleEngine::CellStateToPlayerId(GS->ReplicatedBoardCells[Index]));
		}
	}
	return 1;
}

void AGomokuBoardActor::OnScreenClick(int32 ScreenX, int32 ScreenY)
{
	if (!GetWorld())
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
	}

	FIntPoint Cell;
	if (!ScreenToGrid(PC, FVector2D(ScreenX, ScreenY), Cell))
	{
		return;
	}

	AGomokuGameState* GSState = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GSState || !GSState->IsGameActive)
	{
		return;
	}
	const int32 PlayerIndex = GSState->CurrentPlayerIndex;
	if (PlayerIndex < 0 || PlayerIndex >= GSState->LocalPlayerCount)
	{
		return;
	}
	GSState->HandlePlaceStone(PlayerIndex, Cell);
}
