// LifePlay UE 4.20 -> UE 5.8 Blueprint Migration Commandlet
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "LifePlayBlueprintFixCommandlet.generated.h"

UCLASS()
class ULifePlayBlueprintFixCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	ULifePlayBlueprintFixCommandlet();
	virtual int32 Main(const FString& Params) override;
};
