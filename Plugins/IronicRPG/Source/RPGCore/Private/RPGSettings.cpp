// Copyright Ironic Studio. All Rights Reserved.


#include "RPGSettings.h"

URPGSettings::URPGSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
void URPGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	
}
#endif // WITH_EDITOR