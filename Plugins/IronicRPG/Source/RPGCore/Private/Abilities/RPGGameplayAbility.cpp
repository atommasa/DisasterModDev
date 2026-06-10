// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/RPGGameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/AnimNotify_TriggerEffect.h"

#include "Characters/BaseCharacter.h"
#include "Characters/CharacterAsset.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"

#include "Assets/RPGAssetLibrary.h"

#include "Kismet/KismetSystemLibrary.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown, "Ability.Cooldown", "This tag is used for ability cooldown.")

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Data_EffectSpecIndex, "Ability.Data.EffectSpecIndex", "Index of the ability effect spec used by RPG execution calculation.")

void URPGGameplayAbility::InitAbilityFrom(const UAbilityAsset* InAbilityAsset)
{
	if (!InAbilityAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGGameplayAbility initialized with a null asset!"));
		return;
	}

	// Get asset Id
	AbilityAsset = InAbilityAsset;

	// Get effect specs from asset
	for (FAbilityEffectSpec Formula : InAbilityAsset->GetEffectSpecs())
	{
		Formula.TargetResolver = DuplicateObject<UAbilityTargetResolver>(
			Formula.TargetResolver,
			this // Set GA as the outer
		);

		EffectSpecs.Add(Formula);
	}

	// Initialize ability cost spec from asset
	AbilityCostSpec = InAbilityAsset->GetDefaultCostSpec();

	// Initialize movement and cancelable properties from asset
	bCanMoveWhileCasting = InAbilityAsset->GetbCanMoveWhileCasting();
	bIsCancelable = InAbilityAsset->GetbCanBeInterrupted();

	// Initialize ability tags from asset
	AbilityTags.AppendTags(InAbilityAsset->GetAbilityTags());
	CancelAbilitiesWithTag.AppendTags(InAbilityAsset->GetCancelWithTags());
	BlockAbilitiesWithTag.AppendTags(InAbilityAsset->GetBlockWithTags());
	ActivationOwnedTags.AppendTags(InAbilityAsset->GetActivationOwnedTags());
	ActivationRequiredTags.AppendTags(InAbilityAsset->GetActivationRequiredTags());
	ActivationBlockedTags.AppendTags(InAbilityAsset->GetActivationBlockedTags());
	SourceRequiredTags.AppendTags(InAbilityAsset->GetSourceRequiredTags());
	SourceBlockedTags.AppendTags(InAbilityAsset->GetSourceBlockedTags());
	TargetRequiredTags.AppendTags(InAbilityAsset->GetTargetRequiredTags());
	TargetBlockedTags.AppendTags(InAbilityAsset->GetTargetBlockedTags());

}

void URPGGameplayAbility::PreLoadRequiredAnimations() const
{
	if (!AbilityAsset)
	{
		return;
	}

	if (ABaseCharacter* Avatar = Cast<ABaseCharacter>(GetCurrentActorInfo()->AvatarActor.Get()))
	{
		for (const FGameplayTagContainer& MontageTagContainer : AbilityAsset->GetRequireAnimTagContainers())
		{
			FCharacterAnimInput InputStruct;
			InputStruct.AnimTagContainer = MontageTagContainer;

			FCharacterAnimEntry OutputStruct;

			Avatar->GetCharacterAnimEntry(InputStruct, OutputStruct);

			if (!OutputStruct.Montage.IsNull())
			{
				UAssetManager::GetStreamableManager().RequestAsyncLoad(
					OutputStruct.Montage.ToSoftObjectPath(),
					FStreamableDelegate::CreateLambda([WeakThis = TWeakObjectPtr<const URPGGameplayAbility>(this)]()
						{
							if (!WeakThis.IsValid())
							{
								return;
							}

							UE_LOG(LogTemp, Log, TEXT("Preloaded montage for ability: %s"), *WeakThis->GetName());
						})
				);
			}
		}
	}
}

URPGGameplayAbility::URPGGameplayAbility(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URPGGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (AbilityCostSpec.CostPolicy == ECostPolicy::OnActivation && !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!bCanMoveWhileCasting)
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
		}
	}

	UAbilityTask_WaitGameplayEvent* WaitStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Ability_TriggerEffect_Start,
		nullptr,
		false, // OnlyTriggerOnce
		true // OnlyMatchExact
	);

	if (WaitStartTask)
	{
		WaitStartTask->EventReceived.AddDynamic(this, &URPGGameplayAbility::OnStartEventReceived);
		WaitStartTask->ReadyForActivation();
	}

	UAbilityTask_WaitGameplayEvent* WaitEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Ability_TriggerEffect_End,
		nullptr,
		false, // OnlyTriggerOnce
		true // OnlyMatchExact
	);
	
	if (WaitEndTask)
	{
		WaitEndTask->EventReceived.AddDynamic(this, &URPGGameplayAbility::OnEndEventReceived);
		WaitEndTask->ReadyForActivation();
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StartResolveTargets(EEffectApplyPolicy::ByActivation);
}

void URPGGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StartResolveTargets(EEffectApplyPolicy::BeforeEndAbility);

	if (!bCanMoveWhileCasting)
	{
		// TODO: 應該要恢復原本的移動模式，而不是直接設成走路模式
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* URPGGameplayAbility::GetCooldownTags() const
{
	FGameplayTagContainer* CooldownTags = const_cast<FGameplayTagContainer*>(Super::GetCooldownTags());
	if (!CooldownTags)
	{
		return nullptr;
	}

	CooldownTags->AppendTags(AbilityCostSpec.CooldownTags);

	return CooldownTags;
}

void URPGGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();
	if (CooldownEffect)
	{
		FGameplayEffectSpec* NewSpec = new FGameplayEffectSpec(CooldownEffect, MakeEffectContext(Handle, ActorInfo), GetAbilityLevel(Handle, ActorInfo));
		FGameplayEffectSpecHandle SpecHandle(NewSpec);

		if (SpecHandle.IsValid())
		{
			const float Duration = AbilityCostSpec.CooldownDuration.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
			if (Duration <= 0.f)
			{
				// If duration is zero or negative, do not apply cooldown
				return;
			}

			SpecHandle.Data.Get()->SetSetByCallerMagnitude(
				Ability_Cooldown,
				Duration
			);
			
			// Apply the cooldown effect to the owner
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}
}

UGameplayEffect* URPGGameplayAbility::GetCooldownGameplayEffect() const
{
	if (!CooldownGameplayEffect)
	{
		UClass* EffectClass = CooldownGameplayEffectClass;
		if (!EffectClass)
		{
			EffectClass = UGameplayEffect::StaticClass();
		}

		CooldownGameplayEffect = NewObject<UGameplayEffect>(
			GetTransientPackage(),
			EffectClass,
			NAME_None,
			EObjectFlags::RF_Transactional
		);

		// Set duration policy to "HasDuration"
		CooldownGameplayEffect->DurationPolicy = EGameplayEffectDurationType::HasDuration;

		// Set set by caller
		FSetByCallerFloat SetByCallerFloat;
		SetByCallerFloat.DataTag = Ability_Cooldown;
		CooldownGameplayEffect->DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerFloat);
	}

	return CooldownGameplayEffect;
}

bool URPGGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		// Apply cost entries
		ApplyCostEntries(ActorInfo->AbilitySystemComponent.Get(), CostGE, GetAbilityLevel(Handle, ActorInfo));

		UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (ensure(AbilitySystemComponent))
		{
			if (!AbilitySystemComponent->CanApplyAttributeModifiers(CostGE, GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo)))
			{
				const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;

				if (OptionalRelevantTags && CostTag.IsValid())
				{
					OptionalRelevantTags->AddTag(CostTag);
				}

				return false;
			}
		}
	}

	return true;
}

void URPGGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		ActorInfo->AbilitySystemComponent->ApplyGameplayEffectToSelf(
			CostGE,
			GetAbilityLevel(Handle, ActorInfo),
			MakeEffectContext(Handle, ActorInfo)
		);
	}
}

UGameplayEffect* URPGGameplayAbility::GetCostGameplayEffect() const
{
	if (!CostGameplayEffect)
	{
		UClass* EffectClass = CostGameplayEffectClass;
		if (!EffectClass)
		{
			EffectClass = UGameplayEffect::StaticClass();
		}

		CostGameplayEffect = NewObject<UGameplayEffect>(
			GetTransientPackage(),
			EffectClass,
			NAME_None,
			EObjectFlags::RF_Transactional
		);
	}

	return CostGameplayEffect;
}

void URPGGameplayAbility::ApplyCostEntries(UAbilitySystemComponent* ASC, UGameplayEffect* CostEffect, float Level) const
{
	if (bHasApplyCostEntries)
	{
		return;
	}

	if (!CostEffect || !ASC)
	{
		return;
	}
	
	for (const FAbilityCostEntry& CostEntry : AbilityCostSpec.CostEntries)
	{
		float CostValue = CostEntry.Magnitude.GetValueAtLevel(Level);
		float CurrentValue = ASC->GetNumericAttribute(CostEntry.Attribute);

		switch (CostEntry.CalcType)
		{
			case EMagnitudeCalcType::Flat:
				// CostValue is already flat, do nothing
				break;

			case EMagnitudeCalcType::PercentOfMax:
			{
				float MaxValue = 0.0f;
				URPGAttributeSet::HasMaxClampAttribute(
					ASC,
					CostEntry.Attribute,
					MaxValue
				);

				CostValue = MaxValue * CostValue; // max * percentage
				break;
			}

			case EMagnitudeCalcType::PercentOfCurrent:
			{
				CostValue = CurrentValue * CostValue; // current * percentage
				break;
			}
		}

		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = CostEntry.Attribute;

		if (CostEntry.bCanEndure && CostValue == CurrentValue)
		{
			ModifierInfo.ModifierOp = EGameplayModOp::Override;
			ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(1.0f);
		}
		else
		{
			ModifierInfo.ModifierOp = EGameplayModOp::Additive;
			ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(-CostValue);
		}
		                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
		CostEffect->Modifiers.Add(ModifierInfo);
	}

	bHasApplyCostEntries = true;
}

void URPGGameplayAbility::ApplyEffectToTarget(int32 EffectSpecIndex, FAbilityEffectSpec& EffectSpec, const FGameplayAbilityTargetDataHandle& TargetDataHandle) const
{
	if (!EffectSpec.EffectClass)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}
	
	FGameplayEffectContextHandle ContextHandle = MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());

	ContextHandle.AddSourceObject(AbilityAsset);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		EffectSpec.EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

	Spec->SetSetByCallerMagnitude(
		Ability_Data_EffectSpecIndex,
		static_cast<float>(EffectSpecIndex)
	);

	ApplyGameplayEffectSpecToTarget(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		SpecHandle,
		TargetDataHandle
	);
}

void URPGGameplayAbility::StartResolveTargets(EEffectApplyPolicy ApplyPolicy, const FGameplayTag* EffectTagPtr)
{
	FGameplayTag EffectTag;

	if (ApplyPolicy == EEffectApplyPolicy::ByNotification && EffectTagPtr)
	{
		EffectTag = *EffectTagPtr;
	}

	for (FAbilityEffectSpec& Spec : EffectSpecs)
	{
		if (Spec.ApplyPolicy == ApplyPolicy)
		{
			if (EffectTag == Spec.EffectTag && Spec.TargetResolver)
			{
				if (!Spec.TargetResolver->OnTargetResolved.IsBound())
				{
					Spec.TargetResolver->OnTargetResolved.AddDynamic(this, &URPGGameplayAbility::OnTargetResolved);
				}

				Spec.TargetResolver->StartResolveTargets(CurrentActorInfo->AvatarActor.Get());
			}
		}
	}
}

void URPGGameplayAbility::OnStartEventReceived(FGameplayEventData Payload)
{
	FGameplayTag EffectTag;

	if (const UAnimNotifyState_TriggerEffect* NotifyState = Cast<UAnimNotifyState_TriggerEffect>(Payload.OptionalObject))
	{
		EffectTag = NotifyState->EffectTag;
	}
	else if (const UAnimNotify_TriggerEffect* Notify = Cast<UAnimNotify_TriggerEffect>(Payload.OptionalObject))
	{
		EffectTag = Notify->EffectTag;
	}
	
	if (!EffectTag.IsValid())
	{
		return;
	}

	StartResolveTargets(EEffectApplyPolicy::ByNotification, &EffectTag);
}

void URPGGameplayAbility::OnEndEventReceived(FGameplayEventData Payload)
{
	if (const UAnimNotifyState_TriggerEffect* NotifyState = Cast<UAnimNotifyState_TriggerEffect>(Payload.OptionalObject))
	{
		const FGameplayTag& FormulaTag = NotifyState->EffectTag;

		for (FAbilityEffectSpec& Spec : EffectSpecs)
		{
			if (FormulaTag == Spec.EffectTag && Spec.TargetResolver)
			{
				Spec.TargetResolver->EndResolveTargets();
			}
		}
	}
}

void URPGGameplayAbility::OnTargetResolved(UAbilityTargetResolver* Resolver, const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	if (!Resolver)
	{
		return;
	}

	FGameplayAbilityTargetDataHandle NewTargetDataHandle;
	TArray<AActor*> TargetActors = UAbilitySystemBlueprintLibrary::GetAllActorsFromTargetData(TargetDataHandle);

	for (AActor* TargetActor : TargetActors)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			if (TargetASC->HasAllMatchingGameplayTags(TargetRequiredTags) && !TargetASC->HasAnyMatchingGameplayTags(TargetBlockedTags))
			{
				NewTargetDataHandle.Append(UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(TargetActor));
			}
		}
	}

	for (int32 i = 0; i < EffectSpecs.Num(); ++i)
	{
		FAbilityEffectSpec& EffectSpec = EffectSpecs[i];
		if (Resolver == EffectSpec.TargetResolver)
		{
			ApplyEffectToTarget(i, EffectSpec, NewTargetDataHandle);

			break;
		}
	}
}

