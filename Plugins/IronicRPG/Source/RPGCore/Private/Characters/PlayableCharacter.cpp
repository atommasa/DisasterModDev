// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/PlayableCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/RPGSpringArmComponent.h"

APlayableCharacter::APlayableCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	constexpr float TargetArmLength = 500.f;
	CameraBoom = CreateDefaultSubobject<URPGSpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = TargetArmLength;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>("FollowCamera");
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

}

