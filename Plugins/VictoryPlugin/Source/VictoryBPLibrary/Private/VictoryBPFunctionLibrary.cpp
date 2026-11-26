/*
	By Rama
*/
#include "VictoryBPFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"

namespace
{
	EImageFormat GetJoyImageFormat(EJoyImageFormats JoyFormat)
	{
		switch (JoyFormat)
		{
			case EJoyImageFormats::JPG:  return EImageFormat::JPEG;
			case EJoyImageFormats::PNG:  return EImageFormat::PNG;
			case EJoyImageFormats::BMP:  return EImageFormat::BMP;
			case EJoyImageFormats::ICO:  return EImageFormat::ICO;
			case EJoyImageFormats::EXR:  return EImageFormat::EXR;
			case EJoyImageFormats::ICNS: return EImageFormat::ICNS;
			default:                     return EImageFormat::JPEG;
		}
	}
}

void UVictoryBPFunctionLibrary::MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue)
{
	IndexOfMaxValue = -1;
	MaxValue = 0.0f;

	for (int32 i = 0; i < FloatArray.Num(); ++i)
	{
		if (IndexOfMaxValue == -1 || FloatArray[i] > MaxValue)
		{
			MaxValue = FloatArray[i];
			IndexOfMaxValue = i;
		}
	}
}

void UVictoryBPFunctionLibrary::MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue)
{
	IndexOfMaxValue = -1;
	MaxValue = 0;

	for (int32 i = 0; i < IntArray.Num(); ++i)
	{
		if (IndexOfMaxValue == -1 || IntArray[i] > MaxValue)
		{
			MaxValue = IntArray[i];
			IndexOfMaxValue = i;
		}
	}
}

void UVictoryBPFunctionLibrary::MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue)
{
	IndexOfMinValue = -1;
	MinValue = 0.0f;

	for (int32 i = 0; i < FloatArray.Num(); ++i)
	{
		if (IndexOfMinValue == -1 || FloatArray[i] < MinValue)
		{
			MinValue = FloatArray[i];
			IndexOfMinValue = i;
		}
	}
}

void UVictoryBPFunctionLibrary::MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue)
{
	IndexOfMinValue = -1;
	MinValue = 0;

	for (int32 i = 0; i < IntArray.Num(); ++i)
	{
		if (IndexOfMinValue == -1 || IntArray[i] < MinValue)
		{
			MinValue = IntArray[i];
			IndexOfMinValue = i;
		}
	}
}

void UVictoryBPFunctionLibrary::Victory_GetStringFromOSClipboard(FString& FromClipboard)
{
	FPlatformApplicationMisc::ClipboardPaste(FromClipboard);
}

void UVictoryBPFunctionLibrary::Victory_SaveStringToOSClipboard(const FString& ToClipboard)
{
	FPlatformApplicationMisc::ClipboardCopy(*ToClipboard);
}

UTexture2D* UVictoryBPFunctionLibrary::Victory_LoadTexture2D_FromFile(const FString& FullFilePath, EJoyImageFormats ImageFormat, bool& IsValid, int32& Width, int32& Height)
{
	IsValid = false;
	Width = 0;
	Height = 0;

	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *FullFilePath))
	{
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(GetJoyImageFormat(ImageFormat));
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{
		return nullptr;
	}

	TArray64<uint8> UncompressedBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
	{
		return nullptr;
	}

	const int32 ImgWidth = ImageWrapper->GetWidth();
	const int32 ImgHeight = ImageWrapper->GetHeight();
	UTexture2D* LoadedT2D = UTexture2D::CreateTransient(ImgWidth, ImgHeight, PF_B8G8R8A8);
	if (!LoadedT2D || !LoadedT2D->GetPlatformData() || LoadedT2D->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	Width = ImgWidth;
	Height = ImgHeight;

	void* TextureData = LoadedT2D->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
	LoadedT2D->GetPlatformData()->Mips[0].BulkData.Unlock();
	LoadedT2D->UpdateResource();

	IsValid = true;
	return LoadedT2D;
}
