#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

struct FGtaMapPolyline
{
	TArray<FVector2D> Points;
	FVector2D BoundsMin = FVector2D::ZeroVector;
	FVector2D BoundsMax = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 1.5f;
};

struct FGtaMapData
{
	FString MapLabel;
	TArray<FGtaMapPolyline> Polylines;
	FVector2D WorldMin = FVector2D(-50000.0, -50000.0);
	FVector2D WorldMax = FVector2D(50000.0, 50000.0);
};

struct FGtaMapBlip
{
	FVector2D WorldPos = FVector2D::ZeroVector;
	FString Label;
	FLinearColor Color = FLinearColor::White;
};

struct FGtaMapViewState
{
	FVector2D PlayerPos2D = FVector2D::ZeroVector;
	float PlayerYawDeg = 0.0f;
	float CameraYawDeg = 0.0f;
	bool bHasWaypoint = false;
	FVector2D WaypointWorld2D = FVector2D::ZeroVector;
	FString LocationLabel = TEXT("CentralLondon");
	TArray<FGtaMapBlip> NpcBlips;
	TSharedPtr<FGtaMapData> MapData;
};

DECLARE_DELEGATE_TwoParams(FOnGtaMapWaypointChanged, bool /*bHasWaypoint*/, FVector2D /*WorldPos2D*/);

class SGtaMapCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGtaMapCanvas)
		: _bFullScreen(false)
		, _MinimapSize(FVector2D(210.0, 128.0))
	{}
		SLATE_ARGUMENT(TSharedPtr<FGtaMapViewState>, ViewState)
		SLATE_ARGUMENT(bool, bFullScreen)
		SLATE_ARGUMENT(FVector2D, MinimapSize)
		SLATE_EVENT(FOnGtaMapWaypointChanged, OnWaypointChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void CenterOnPlayer();

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedPtr<FGtaMapViewState> ViewState;
	bool bFullScreen = false;
	FVector2D MinimapSize = FVector2D(210.0, 128.0);
	FOnGtaMapWaypointChanged OnWaypointChanged;

	FVector2D PanCenterWorld = FVector2D::ZeroVector;
	float PixelsPerCm = 0.0045f;
	bool bIsDragging = false;
	FVector2D LastLocalMouse = FVector2D::ZeroVector;

	FVector2D WorldToScreen(const FVector2D& WorldPt, const FVector2D& Size) const;
	FVector2D ScreenToWorld(const FVector2D& ScreenPt, const FVector2D& Size) const;
};

/**
 * Dual-mode GTA Tactical Map Widget:
 * - Bottom-left rotating radar minimap (bFullScreen = false)
 * - Full-screen interactive world map with GPS waypoints & fast travel (bFullScreen = true)
 */
class SLifePlayGtaMapWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLifePlayGtaMapWidget)
		: _bFullScreen(false)
		, _MinimapHAlign(HAlign_Right)
		, _MinimapVAlign(VAlign_Bottom)
		, _MinimapSize(FVector2D(210.0, 128.0))
		, _MinimapPadding(FMargin(18.0f))
	{}
		SLATE_ARGUMENT(TSharedPtr<FGtaMapViewState>, ViewState)
		SLATE_ARGUMENT(bool, bFullScreen)
		SLATE_ARGUMENT(EHorizontalAlignment, MinimapHAlign)
		SLATE_ARGUMENT(EVerticalAlignment, MinimapVAlign)
		SLATE_ARGUMENT(FVector2D, MinimapSize)
		SLATE_ARGUMENT(FMargin, MinimapPadding)
		SLATE_EVENT(FOnGtaMapWaypointChanged, OnWaypointChanged)
		SLATE_EVENT(FSimpleDelegate, OnRequestTeleport)
		SLATE_EVENT(FSimpleDelegate, OnRequestClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void CenterOnPlayer();

	virtual bool SupportsKeyboardFocus() const override { return bFullScreen; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedPtr<FGtaMapViewState> ViewState;
	bool bFullScreen = false;
	FVector2D MinimapSize = FVector2D(210.0, 128.0);
	FOnGtaMapWaypointChanged OnWaypointChanged;
	FSimpleDelegate OnRequestTeleport;
	FSimpleDelegate OnRequestClose;

	TSharedPtr<SGtaMapCanvas> Canvas;
};
