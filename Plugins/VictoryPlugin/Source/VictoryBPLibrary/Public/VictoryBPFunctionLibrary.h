/*
	By Rama
*/
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VictoryBPFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EJoyImageFormats : uint8
{
	JPG		UMETA(DisplayName = "JPG        "),
	PNG		UMETA(DisplayName = "PNG        "),
	BMP		UMETA(DisplayName = "BMP        "),
	ICO		UMETA(DisplayName = "ICO        "),
	EXR		UMETA(DisplayName = "EXR        "),
	ICNS	UMETA(DisplayName = "ICNS        ")
};

UCLASS()
class VICTORYBPLIBRARY_API UVictoryBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Max of all array entries. Returns -1 if the supplied array is empty. Returns the index of the max value as well as the value itself. */
	UFUNCTION(BlueprintPure, Category = "Victory BP Library|Array")
	static void MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue);

	/** Max of all array entries. Returns -1 if the supplied array is empty. Returns the index of the max value as well as the value itself. */
	UFUNCTION(BlueprintPure, Category = "Victory BP Library|Array")
	static void MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue);

	/** Min of all array entries. Returns -1 if the supplied array is empty. Returns the index of the min value as well as the value itself. */
	UFUNCTION(BlueprintPure, Category = "Victory BP Library|Array")
	static void MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue);

	/** Min of all array entries. Returns -1 if the supplied array is empty. Returns the index of the min value as well as the value itself. */
	UFUNCTION(BlueprintPure, Category = "Victory BP Library|Array")
	static void MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue);

	UFUNCTION(BlueprintPure, Category = "Victory BP Library|String")
	static void Victory_GetStringFromOSClipboard(FString& FromClipboard);

	UFUNCTION(BlueprintCallable, Category = "Victory BP Library|String")
	static void Victory_SaveStringToOSClipboard(const FString& ToClipboard);

	/** Load a Texture2D from a JPG,PNG,BMP,ICO,EXR,ICNS file! IsValid tells you if file path was valid or not. Enjoy! -Rama */
	UFUNCTION(BlueprintPure, Category = "Victory BP Library|Load Texture From File", meta = (Keywords = "image png jpg jpeg bmp bitmap ico icon exr icns"))
	static UTexture2D* Victory_LoadTexture2D_FromFile(const FString& FullFilePath, EJoyImageFormats ImageFormat, bool& IsValid, int32& Width, int32& Height);
};
