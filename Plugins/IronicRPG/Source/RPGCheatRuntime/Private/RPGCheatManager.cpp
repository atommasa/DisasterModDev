// Copyright Ironic Studio. All Rights Reserved.


#include "RPGCheatManager.h"

void URPGCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	TArray<UClass*> CheatClasses;
	GetDerivedClasses(URPGCheatExtension::StaticClass(), CheatClasses, true);

	for (UClass* CheatClasse : CheatClasses)
	{
		if (CheatClasse && !CheatClasse->HasAllClassFlags(CLASS_Abstract))
		{
			AddCheatManagerExtension(NewObject<URPGCheatExtension>(this, CheatClasse));
		}
	}
}
