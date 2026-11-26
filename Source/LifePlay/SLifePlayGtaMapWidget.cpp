#include "SLifePlayGtaMapWidget.h"

#include "Fonts/SlateFontInfo.h"
#include "InputCoreTypes.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SGtaMapCanvas::Construct(const FArguments& InArgs)
{
	ViewState = InArgs._ViewState;
	bFullScreen = InArgs._bFullScreen;
	OnWaypointChanged = InArgs._OnWaypointChanged;

	PixelsPerCm = bFullScreen ? 0.0038f : 0.0068f;
	CenterOnPlayer();
	if (!bFullScreen)
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}
}

void SGtaMapCanvas::CenterOnPlayer()
{
	if (ViewState.IsValid())
	{
		PanCenterWorld = ViewState->PlayerPos2D;
	}
}

FVector2D SGtaMapCanvas::ComputeDesiredSize(float) const
{
	return bFullScreen ? FVector2D(920.0, 620.0) : FVector2D(256.0, 152.0);
}

FVector2D SGtaMapCanvas::WorldToScreen(const FVector2D& WorldPt, const FVector2D& Size) const
{
	const FVector2D HalfSize = Size * 0.5;
	if (!ViewState.IsValid())
	{
		return HalfSize;
	}

	if (bFullScreen)
	{
		const FVector2D Delta = WorldPt - PanCenterWorld;
		return FVector2D(HalfSize.X + Delta.X * PixelsPerCm, HalfSize.Y + Delta.Y * PixelsPerCm);
	}

	const FVector2D Delta = WorldPt - ViewState->PlayerPos2D;
	const float YawRad = FMath::DegreesToRadians(ViewState->CameraYawDeg);
	const FVector2D Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
	const FVector2D Right(-FMath::Sin(YawRad), FMath::Cos(YawRad));

	const double LocalX = FVector2D::DotProduct(Delta, Right);
	const double LocalY = -FVector2D::DotProduct(Delta, Fwd);
	return FVector2D(HalfSize.X + LocalX * PixelsPerCm, HalfSize.Y + LocalY * PixelsPerCm);
}

FVector2D SGtaMapCanvas::ScreenToWorld(const FVector2D& ScreenPt, const FVector2D& Size) const
{
	const FVector2D HalfSize = Size * 0.5;
	if (bFullScreen)
	{
		return PanCenterWorld + (ScreenPt - HalfSize) / PixelsPerCm;
	}
	return ViewState.IsValid() ? ViewState->PlayerPos2D : FVector2D::ZeroVector;
}

int32 SGtaMapCanvas::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 2.0 || Size.Y <= 2.0)
	{
		return LayerId;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", bFullScreen ? 9 : 8);

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));

	// 1. Dark Obsidian Map Background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f::ZeroVector)),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.020f, 0.028f, 0.045f, 0.96f)
	);
	++LayerId;

	if (ViewState.IsValid())
	{
		const FVector2D CenterWorld = bFullScreen ? PanCenterWorld : ViewState->PlayerPos2D;
		const float HalfDiagPx = FVector2D(Size.X * 0.5, Size.Y * 0.5).Size();
		const float ViewRadiusCm = (HalfDiagPx / FMath::Max(PixelsPerCm, 0.0002f)) + 3000.0f;
		const FVector2D CullMin = CenterWorld - FVector2D(ViewRadiusCm, ViewRadiusCm);
		const FVector2D CullMax = CenterWorld + FVector2D(ViewRadiusCm, ViewRadiusCm);

		// 2. Subtle Tactical Grid (50m = 5000cm spacing)
		{
			const float GridStepCm = (PixelsPerCm < 0.002f) ? 20000.0f : 5000.0f;
			const float StartX = FMath::FloorToFloat(CullMin.X / GridStepCm) * GridStepCm;
			const float StartY = FMath::FloorToFloat(CullMin.Y / GridStepCm) * GridStepCm;
			const FLinearColor GridColor(0.08f, 0.14f, 0.22f, 0.35f);
			TArray<FVector2f> GridSeg;
			GridSeg.SetNumUninitialized(2);

			for (float gx = StartX; gx <= CullMax.X; gx += GridStepCm)
			{
				GridSeg[0] = FVector2f(WorldToScreen(FVector2D(gx, CullMin.Y), Size));
				GridSeg[1] = FVector2f(WorldToScreen(FVector2D(gx, CullMax.Y), Size));
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), GridSeg, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}
			for (float gy = StartY; gy <= CullMax.Y; gy += GridStepCm)
			{
				GridSeg[0] = FVector2f(WorldToScreen(FVector2D(CullMin.X, gy), Size));
				GridSeg[1] = FVector2f(WorldToScreen(FVector2D(CullMax.X, gy), Size));
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), GridSeg, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}
		}
		++LayerId;

		// 3. Cached OpenStreetMap City Polylines (Roads, Buildings, Parks, Water)
		if (ViewState->MapData.IsValid())
		{
			TArray<FVector2f> ScreenPts;
			for (const FGtaMapPolyline& Poly : ViewState->MapData->Polylines)
			{
				if (Poly.Points.Num() < 2) continue;
				if (Poly.BoundsMax.X < CullMin.X || Poly.BoundsMin.X > CullMax.X ||
					Poly.BoundsMax.Y < CullMin.Y || Poly.BoundsMin.Y > CullMax.Y)
				{
					continue;
				}

				ScreenPts.Reset(Poly.Points.Num());
				for (const FVector2D& WorldPt : Poly.Points)
				{
					ScreenPts.Add(FVector2f(WorldToScreen(WorldPt, Size)));
				}
				const float ScaledThickness = bFullScreen
					? FMath::Clamp(Poly.Thickness * (PixelsPerCm / 0.004f), 1.0f, 5.0f)
					: Poly.Thickness;
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					ScreenPts,
					ESlateDrawEffect::None,
					Poly.Color,
					true,
					ScaledThickness
				);
			}
		}
		++LayerId;

		// 4. Magenta GTA GPS Route & Waypoint
		if (ViewState->bHasWaypoint)
		{
			const FVector2D PlayerScr = WorldToScreen(ViewState->PlayerPos2D, Size);
			const FVector2D WaypointScr = WorldToScreen(ViewState->WaypointWorld2D, Size);
			const FLinearColor GpsMagenta(0.95f, 0.25f, 0.90f, 0.95f);

			TArray<FVector2f> RoutePts = { FVector2f(PlayerScr), FVector2f(WaypointScr) };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), RoutePts, ESlateDrawEffect::None, FLinearColor(0.92f, 0.28f, 0.88f, 0.72f), true, 2.2f);

			const FVector2D ClampedWp(
				FMath::Clamp(WaypointScr.X, 10.0, Size.X - 10.0),
				FMath::Clamp(WaypointScr.Y, 10.0, Size.Y - 10.0)
			);
			const float WpHalf = 6.0f;
			TArray<FVector2f> Ring = {
				FVector2f(ClampedWp.X - WpHalf, ClampedWp.Y - WpHalf),
				FVector2f(ClampedWp.X + WpHalf, ClampedWp.Y - WpHalf),
				FVector2f(ClampedWp.X + WpHalf, ClampedWp.Y + WpHalf),
				FVector2f(ClampedWp.X - WpHalf, ClampedWp.Y + WpHalf),
				FVector2f(ClampedWp.X - WpHalf, ClampedWp.Y - WpHalf)
			};
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, GpsMagenta, true, 2.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(6.0f, 6.0f), FSlateLayoutTransform(FVector2f(ClampedWp.X - 3.0f, ClampedWp.Y - 3.0f))),
				WhiteBrush,
				ESlateDrawEffect::None,
				GpsMagenta
			);
		}
		LayerId += 2;

		// 5. Live NPC Blips
		for (const FGtaMapBlip& Blip : ViewState->NpcBlips)
		{
			const FVector2D BlipScr = WorldToScreen(Blip.WorldPos, Size);
			if (BlipScr.X < -8.0 || BlipScr.X > Size.X + 8.0 || BlipScr.Y < -8.0 || BlipScr.Y > Size.Y + 8.0)
			{
				continue;
			}
			const float BlipSz = bFullScreen ? 7.0f : 5.0f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(BlipSz, BlipSz), FSlateLayoutTransform(FVector2f(BlipScr.X - BlipSz * 0.5f, BlipScr.Y - BlipSz * 0.5f))),
				WhiteBrush,
				ESlateDrawEffect::None,
				Blip.Color
			);

			if (bFullScreen && !Blip.Label.IsEmpty())
			{
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2f(220.0f, 18.0f), FSlateLayoutTransform(FVector2f(BlipScr.X + 6.0f, BlipScr.Y - 6.0f))),
					Blip.Label,
					SmallFont,
					ESlateDrawEffect::None,
					Blip.Color
				);
			}
		}
		LayerId += 2;

		// 6. Player Directional Chevron
		{
			const FVector2D PlayerScr = WorldToScreen(ViewState->PlayerPos2D, Size);
			float ScreenAngleRad = 0.0f;
			if (bFullScreen)
			{
				ScreenAngleRad = FMath::DegreesToRadians(ViewState->PlayerYawDeg);
			}
			else
			{
				const float RelYaw = ViewState->PlayerYawDeg - ViewState->CameraYawDeg - 90.0f;
				ScreenAngleRad = FMath::DegreesToRadians(RelYaw);
			}

			const FVector2D Dir(FMath::Cos(ScreenAngleRad), FMath::Sin(ScreenAngleRad));
			const FVector2D Perp(-Dir.Y, Dir.X);
			const float TipLen = bFullScreen ? 10.0f : 8.5f;
			const float TailLen = bFullScreen ? 6.0f : 5.0f;
			const float Wing = bFullScreen ? 6.0f : 5.0f;

			const FVector2D Tip = PlayerScr + Dir * TipLen;
			const FVector2D Left = PlayerScr - Dir * TailLen - Perp * Wing;
			const FVector2D Notch = PlayerScr - Dir * (TailLen * 0.35f);
			const FVector2D Right = PlayerScr - Dir * TailLen + Perp * Wing;

			TArray<FVector2f> Chevron = { FVector2f(Tip), FVector2f(Left), FVector2f(Notch), FVector2f(Right), FVector2f(Tip) };
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				Chevron,
				ESlateDrawEffect::None,
				FLinearColor(0.22f, 0.92f, 1.0f, 1.0f),
				true,
				2.2f
			);
		}
		++LayerId;

		// 7. Radar North ("N") Compass Indicator
		if (!bFullScreen)
		{
			const float NorthYawRad = FMath::DegreesToRadians(-ViewState->CameraYawDeg - 90.0f);
			const FVector2D Half = Size * 0.5;
			const FVector2D NorthDir(FMath::Cos(NorthYawRad), FMath::Sin(NorthYawRad));
			const float RimR = FMath::Min(Half.X, Half.Y) - 12.0f;
			const FVector2D NPos = Half + NorthDir * RimR - FVector2D(5.0, 7.0);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(14.0f, 14.0f), FSlateLayoutTransform(FVector2f(NPos.X - 2.0f, NPos.Y))),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.03f, 0.05f, 0.08f, 0.88f)
			);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(16.0f, 16.0f), FSlateLayoutTransform(FVector2f(NPos.X + 1.0f, NPos.Y + 1.0f))),
				TEXT("N"),
				SmallFont,
				ESlateDrawEffect::None,
				FLinearColor(0.98f, 0.80f, 0.30f, 1.0f)
			);
		}
	}

	OutDrawElements.PopClip();
	return LayerId + 2;
}

FReply SGtaMapCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bFullScreen)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = FVector2D(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	const FVector2D Size = MyGeometry.GetLocalSize();

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FVector2D WorldPt = ScreenToWorld(LocalPos, Size);
		OnWaypointChanged.ExecuteIfBound(true, WorldPt);
		return FReply::Handled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;
		LastLocalMouse = LocalPos;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	return FReply::Unhandled();
}

FReply SGtaMapCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SGtaMapCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bFullScreen && bIsDragging)
	{
		const FVector2D LocalPos = FVector2D(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		const FVector2D DeltaPx = LocalPos - LastLocalMouse;
		PanCenterWorld -= DeltaPx / FMath::Max(PixelsPerCm, 0.0002f);
		LastLocalMouse = LocalPos;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SGtaMapCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bFullScreen)
	{
		return FReply::Unhandled();
	}
	const float Factor = (MouseEvent.GetWheelDelta() > 0.0f) ? 1.18f : 0.85f;
	PixelsPerCm = FMath::Clamp(PixelsPerCm * Factor, 0.0006f, 0.030f);
	return FReply::Handled();
}

void SLifePlayGtaMapWidget::Construct(const FArguments& InArgs)
{
	ViewState = InArgs._ViewState;
	bFullScreen = InArgs._bFullScreen;
	OnWaypointChanged = InArgs._OnWaypointChanged;
	OnRequestTeleport = InArgs._OnRequestTeleport;
	OnRequestClose = InArgs._OnRequestClose;

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto GetInfoLine = [this]() -> FText
	{
		if (!ViewState.IsValid())
		{
			return FText::FromString(TEXT("LIFEPLAY GPS"));
		}
		if (ViewState->bHasWaypoint)
		{
			const float Meters = FVector2D::Distance(ViewState->PlayerPos2D, ViewState->WaypointWorld2D) / 100.0f;
			return FText::FromString(FString::Printf(TEXT("%s  •  GPS: %.0fm"), *ViewState->LocationLabel, Meters));
		}
		return FText::FromString(bFullScreen
			? FString::Printf(TEXT("%s  •  Right-Click to set GPS  •  Drag to pan  •  Wheel to zoom"), *ViewState->LocationLabel)
			: FString::Printf(TEXT("%s  •  [N] World Map"), *ViewState->LocationLabel));
	};

	if (!bFullScreen)
	{
		SetVisibility(EVisibility::HitTestInvisible);
		ChildSlot
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(20.0f, 0.0f, 0.0f, 54.0f))
		[
			SNew(SBox)
			.WidthOverride(256.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush)
				.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.88f))
				.Padding(2.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(2.0f)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush)
							.BorderBackgroundColor(FLinearColor(0.18f, 0.82f, 0.98f, 1.0f))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(Canvas, SGtaMapCanvas)
						.ViewState(ViewState)
						.bFullScreen(false)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(6.0f, 3.0f))
					[
						SNew(STextBlock)
						.Text_Lambda(GetInfoLine)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FLinearColor(0.75f, 0.88f, 0.98f))
					]
				]
			]
		];
		return;
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(940.0f)
		.HeightOverride(700.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush)
			.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.95f))
			.Padding(0.0f)
			[
				SNew(SVerticalBox)

				// Top Cyan Accent
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(3.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.18f, 0.82f, 0.98f, 1.0f))
					]
				]

				// Header & Controls
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(14.0f, 10.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("LIFEPLAY v1.0  //  GTA WORLD MAP & GPS")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FLinearColor(0.22f, 0.88f, 0.98f))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text_Lambda(GetInfoLine)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(FLinearColor(0.78f, 0.86f, 0.94f))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { CenterOnPlayer(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Center Player")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { OnRequestTeleport.ExecuteIfBound(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Teleport to GPS")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { OnWaypointChanged.ExecuteIfBound(false, FVector2D::ZeroVector); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Clear GPS")))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { OnRequestClose.ExecuteIfBound(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Close (N / Esc)")))
						]
					]
				]

				// Interactive City Map Canvas
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(FMargin(12.0f, 0.0f, 12.0f, 12.0f))
				[
					SAssignNew(Canvas, SGtaMapCanvas)
					.ViewState(ViewState)
					.bFullScreen(true)
					.OnWaypointChanged(OnWaypointChanged)
				]
			]
		]
	];
}

void SLifePlayGtaMapWidget::CenterOnPlayer()
{
	if (Canvas.IsValid())
	{
		Canvas->CenterOnPlayer();
	}
}

FReply SLifePlayGtaMapWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::N)
	{
		OnRequestClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
