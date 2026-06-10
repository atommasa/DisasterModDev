// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeCutsceneNodeInfo.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

bool UNarrativeCutsceneNodeInfo::ExecuteNode()
{
	ALevelSequenceActor* OutActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
		GetWorld(), Cutscene, FMovieSceneSequencePlaybackSettings(), OutActor);

	if (Player)
	{
		Player->Play();

		return true;
	}

	return false;
}