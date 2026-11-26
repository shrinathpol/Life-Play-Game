// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateTypes.h"
#include "ModdingExtraBPLibrary.generated.h"

UCLASS()
class MODDINGEXTRA_API UModdingExtraBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Execute Sample function", Keywords = "ModdingExtra sample test testing"), Category = "ModdingExtraTesting")
	static float ModdingExtraSampleFunction(float Param);

	UFUNCTION(BlueprintPure, Category = "ModdingExtra")
	static bool GetAllFilesFromFolder(TArray<FString>& Files, FString RootFolderFullPath, FString Ext);

	UFUNCTION(BlueprintPure, Category = "ModdingExtra", meta = (Keywords = "LoadTxt"))
	static bool LoadTxt(FString FileNameA, FString& SaveTextA);

	UFUNCTION(BlueprintCallable, Category = "ModdingExtra", meta = (Keywords = "SaveTxt"))
	static bool SaveTxt(FString SaveTextB, FString FileNameB);

	UFUNCTION(BlueprintPure, Category = "ModdingExtra")
	static FString ContentDirectory();

	UFUNCTION(BlueprintPure, Category = "ModdingExtra")
	static bool GetAllSubfolders(TArray<FString>& Folders, FString RootFolderFullPath);

	UFUNCTION(BlueprintPure, Category = "ModdingExtra", meta = (Keywords = "IsPointInsidePolygon"))
	static bool IsPointInsidePolygon(const TArray<FVector2D>& Polygon, const FVector2D Point);

	/** Pure UE 5.8 compatibility wrapper for UE4's pure GetAssetByObjectPath */
	UFUNCTION(BlueprintPure, Category = "ModdingExtra|Compat", meta = (DisplayName = "Get Asset By Object Path (Pure)"))
	static FAssetData GetAssetByObjectPathPure(const FName ObjectPath, bool bIncludeOnlyOnDiskAssets = false);

	/** Compatibility wrapper for UE4 Break EditableTextBoxStyle (preserves Font pin via TextStyle.Font) */
	UFUNCTION(BlueprintPure, Category = "ModdingExtra|Compat", meta = (DisplayName = "Break Editable Text Box Style (Compat)"))
	static void BreakEditableTextBoxStyleCompat(
		const FEditableTextBoxStyle& InStyle,
		FSlateBrush& BackgroundImageNormal,
		FSlateBrush& BackgroundImageHovered,
		FSlateBrush& BackgroundImageFocused,
		FSlateBrush& BackgroundImageReadOnly,
		FMargin& Padding,
		FSlateFontInfo& Font,
		FSlateColor& ForegroundColor,
		FSlateColor& BackgroundColor,
		FSlateColor& ReadOnlyForegroundColor,
		FSlateColor& FocusedForegroundColor,
		FMargin& HScrollBarPadding,
		FMargin& VScrollBarPadding,
		FScrollBarStyle& ScrollBarStyle);

	/** Compatibility wrapper for UE4 Make EditableTextBoxStyle (preserves Font pin via TextStyle.Font) */
	UFUNCTION(BlueprintPure, Category = "ModdingExtra|Compat", meta = (DisplayName = "Make Editable Text Box Style (Compat)"))
	static FEditableTextBoxStyle MakeEditableTextBoxStyleCompat(
		FSlateBrush BackgroundImageNormal,
		FSlateBrush BackgroundImageHovered,
		FSlateBrush BackgroundImageFocused,
		FSlateBrush BackgroundImageReadOnly,
		FMargin Padding,
		FSlateFontInfo Font,
		FSlateColor ForegroundColor,
		FSlateColor BackgroundColor,
		FSlateColor ReadOnlyForegroundColor,
		FSlateColor FocusedForegroundColor,
		FMargin HScrollBarPadding,
		FMargin VScrollBarPadding,
		FScrollBarStyle ScrollBarStyle);

	UFUNCTION(BlueprintPure, Category = "ModdingExtra|Compat")
	static FEditableTextBoxStyle SetEditableTextBoxStyleFont(FEditableTextBoxStyle InStyle, FSlateFontInfo Font);
};
