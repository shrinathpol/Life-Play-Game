// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ModdingExtraBPLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModdingExtra.h"
#include "Modules/ModuleManager.h"

UModdingExtraBPLibrary::UModdingExtraBPLibrary(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer) {}

float UModdingExtraBPLibrary::ModdingExtraSampleFunction(float Param) {
  return -1;
}

bool UModdingExtraBPLibrary::GetAllFilesFromFolder(TArray<FString> &Files,
                                                   FString RootFolderFullPath,
                                                   FString Ext) {
  if (RootFolderFullPath.Len() < 1)
    return false;

  IFileManager &FileManager = IFileManager::Get();

  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      FileManager.ConvertToAbsolutePathForExternalAppForRead(*RelativePath);

  if (Ext == "") {
    Ext = "*.*";
  } else {
    Ext = (Ext.Left(1) == ".") ? "*" + Ext : "*." + Ext;
  }

  FString FinalPathWithoutExt = FullPath + "/" + RootFolderFullPath + "/";

  FileManager.FindFilesRecursive(Files, *FinalPathWithoutExt, *Ext, true, false,
                                 false);

  if (Ext.Contains(TEXT("uasset"), ESearchCase::IgnoreCase) ||
      Ext == TEXT("*.*")) {
    FString CleanRoot = RootFolderFullPath;
    CleanRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (CleanRoot.StartsWith(TEXT("/"))) {
      CleanRoot.RightChopInline(1);
    }
    while (CleanRoot.EndsWith(TEXT("/"))) {
      CleanRoot.LeftChopInline(1);
    }

    const FName GamePath(*(TEXT("/Game/") + CleanRoot));
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssetsByPath(GamePath, Assets, true);

    FString CleanFullPath = FullPath;
    CleanFullPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (CleanFullPath.EndsWith(TEXT("/"))) {
      CleanFullPath.LeftChopInline(1);
    }

    for (const FAssetData &Asset : Assets) {
      FString PkgName = Asset.PackageName.ToString();
      if (PkgName.RemoveFromStart(TEXT("/Game/"))) {
        Files.AddUnique(CleanFullPath + TEXT("/") + PkgName + TEXT(".uasset"));
      }
    }
    UE_LOG(LogTemp, Display,
           TEXT("[ModdingExtra] GetAllFilesFromFolder('%s', '%s') -> %d total files (%d from AssetRegistry)"),
           *RootFolderFullPath, *Ext, Files.Num(), Assets.Num());
  }
  return true;
}

bool UModdingExtraBPLibrary::GetAllSubfolders(TArray<FString> &Folders,
                                              FString RootFolderFullPath) {
  if (RootFolderFullPath.Len() < 1)
    return false;

  IFileManager &FileManager = IFileManager::Get();

  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      FileManager.ConvertToAbsolutePathForExternalAppForRead(*RelativePath);

  FString FinalPath = FullPath + "/" + RootFolderFullPath + "/";

  FileManager.FindFiles(Folders, *FinalPath, false, true);

  FString CleanRoot = RootFolderFullPath;
  CleanRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
  while (CleanRoot.StartsWith(TEXT("/"))) {
    CleanRoot.RightChopInline(1);
  }
  while (CleanRoot.EndsWith(TEXT("/"))) {
    CleanRoot.LeftChopInline(1);
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  TArray<FString> SubPaths;
  AssetRegistryModule.Get().GetSubPaths(TEXT("/Game/") + CleanRoot, SubPaths,
                                        false);
  for (const FString &SubPath : SubPaths) {
    const FString FolderName = FPaths::GetCleanFilename(SubPath);
    if (!FolderName.IsEmpty()) {
      Folders.AddUnique(FolderName);
    }
  }
  return true;
}

bool UModdingExtraBPLibrary::LoadTxt(FString FileNameA, FString &SaveTextA) {
  return FFileHelper::LoadFileToString(
      SaveTextA, *(FPaths::ProjectContentDir() + FileNameA));
}

bool UModdingExtraBPLibrary::SaveTxt(FString SaveTextB, FString FileNameB) {
  return FFileHelper::SaveStringToFile(
      SaveTextB, *(FPaths::ProjectContentDir() + FileNameB));
}

FString UModdingExtraBPLibrary::ContentDirectory() {
  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(
          *RelativePath);
  return FullPath;
}

bool UModdingExtraBPLibrary::IsPointInsidePolygon(
    const TArray<FVector2D> &Polygon, const FVector2D Point) {
  const int NumCorners = Polygon.Num();
  int PreviousCornerIndex = NumCorners - 1;
  bool bIsInside = false;

  for (int CornerIndex = 0; CornerIndex < NumCorners; CornerIndex++) {
    if (((Polygon[CornerIndex].Y < Point.Y &&
          Polygon[PreviousCornerIndex].Y >= Point.Y) ||
         (Polygon[PreviousCornerIndex].Y < Point.Y &&
          Polygon[CornerIndex].Y >= Point.Y)) &&
        (Polygon[CornerIndex].X <= Point.X ||
         Polygon[PreviousCornerIndex].X <= Point.X)) {
      bIsInside ^=
          (Polygon[CornerIndex].X +
               (Point.Y - Polygon[CornerIndex].Y) /
                   (Polygon[PreviousCornerIndex].Y - Polygon[CornerIndex].Y) *
                   (Polygon[PreviousCornerIndex].X - Polygon[CornerIndex].X) <
           Point.X);
    }

    PreviousCornerIndex = CornerIndex;
  }

  return bIsInside;
}

FAssetData UModdingExtraBPLibrary::GetAssetByObjectPathPure(
    const FName ObjectPath, bool bIncludeOnlyOnDiskAssets) {
  if (ObjectPath.IsNone()) {
    return FAssetData();
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  return AssetRegistryModule.Get().GetAssetByObjectPath(
      FSoftObjectPath(ObjectPath.ToString()), bIncludeOnlyOnDiskAssets);
}

void UModdingExtraBPLibrary::BreakEditableTextBoxStyleCompat(
    const FEditableTextBoxStyle &InStyle, FSlateBrush &BackgroundImageNormal,
    FSlateBrush &BackgroundImageHovered, FSlateBrush &BackgroundImageFocused,
    FSlateBrush &BackgroundImageReadOnly, FMargin &Padding,
    FSlateFontInfo &Font, FSlateColor &ForegroundColor,
    FSlateColor &BackgroundColor, FSlateColor &ReadOnlyForegroundColor,
    FSlateColor &FocusedForegroundColor, FMargin &HScrollBarPadding,
    FMargin &VScrollBarPadding, FScrollBarStyle &ScrollBarStyle) {
  BackgroundImageNormal = InStyle.BackgroundImageNormal;
  BackgroundImageHovered = InStyle.BackgroundImageHovered;
  BackgroundImageFocused = InStyle.BackgroundImageFocused;
  BackgroundImageReadOnly = InStyle.BackgroundImageReadOnly;
  Padding = InStyle.Padding;
  Font = InStyle.TextStyle.Font;
  ForegroundColor = InStyle.ForegroundColor;
  BackgroundColor = InStyle.BackgroundColor;
  ReadOnlyForegroundColor = InStyle.ReadOnlyForegroundColor;
  FocusedForegroundColor = InStyle.FocusedForegroundColor;
  HScrollBarPadding = InStyle.HScrollBarPadding;
  VScrollBarPadding = InStyle.VScrollBarPadding;
  ScrollBarStyle = InStyle.ScrollBarStyle;
}

FEditableTextBoxStyle UModdingExtraBPLibrary::MakeEditableTextBoxStyleCompat(
    FSlateBrush BackgroundImageNormal, FSlateBrush BackgroundImageHovered,
    FSlateBrush BackgroundImageFocused, FSlateBrush BackgroundImageReadOnly,
    FMargin Padding, FSlateFontInfo Font, FSlateColor ForegroundColor,
    FSlateColor BackgroundColor, FSlateColor ReadOnlyForegroundColor,
    FSlateColor FocusedForegroundColor, FMargin HScrollBarPadding,
    FMargin VScrollBarPadding, FScrollBarStyle ScrollBarStyle) {
  FEditableTextBoxStyle Result;
  Result.SetBackgroundImageNormal(BackgroundImageNormal);
  Result.SetBackgroundImageHovered(BackgroundImageHovered);
  Result.SetBackgroundImageFocused(BackgroundImageFocused);
  Result.SetBackgroundImageReadOnly(BackgroundImageReadOnly);
  Result.SetPadding(Padding);
  Result.SetFont(Font);
  Result.SetForegroundColor(ForegroundColor);
  Result.SetBackgroundColor(BackgroundColor);
  Result.SetReadOnlyForegroundColor(ReadOnlyForegroundColor);
  Result.SetFocusedForegroundColor(FocusedForegroundColor);
  Result.SetHScrollBarPadding(HScrollBarPadding);
  Result.SetVScrollBarPadding(VScrollBarPadding);
  Result.SetScrollBarStyle(ScrollBarStyle);
  return Result;
}

FEditableTextBoxStyle UModdingExtraBPLibrary::SetEditableTextBoxStyleFont(
    FEditableTextBoxStyle InStyle, FSlateFontInfo Font) {
  FEditableTextBoxStyle Result = InStyle;
  Result.SetFont(Font);
  return Result;
}
