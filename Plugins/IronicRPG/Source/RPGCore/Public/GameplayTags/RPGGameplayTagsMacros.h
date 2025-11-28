// Copyright Ironic Studio. All Rights Reserved.

#pragma once

/**
 * Macro to enable new gameplay tags in a class.
 */
#define ENABLE_NEW_TAGS(TagClass) \
private: \
	static TagClass GameplayTags; \
	static TMap<FName, TSet<FGameplayTag>> TagRestrictions; \

 /**
  * Macro to start defining new gameplay tags outside of a class.
  */
#define START_NEW_TAGS(TagClass) \
	TagClass TagClass##::GameplayTags; \
	TMap<FName, TSet<FGameplayTag>> TagClass##::TagRestrictions;

/**
 * Macro to define a new gameplay tag.
 * 
 * @param Tags - The struct instance where the tag will be stored.
 * @param TagName - The name of the tag variable in the struct.
 * @param TagString - The string representation of the tag (e.g., "Section.Character.MainParty").
 * @param Comment - A comment describing the tag.
 */
#define NEW_GAMEPLAY_TAG(TagName, TagString, Comment) \
	GameplayTags.TagName = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagString), Comment);

/**
 * Macro to define a new local gameplay tag with restrictions.
 * 
 * @param TagName - The name of the tag variable in the struct.
 * @param TagString - The string representation of the tag (e.g., "Section.Character.MainParty").
 * @param Comment - A comment describing the tag.
 * @param Restriction - The type of restriction applied to the tag (e.g., Character, Item).
 */
#define NEW_LOCAL_GAMEPLAY_TAG(TagName, TagString, Comment, Restriction) \
	NEW_GAMEPLAY_TAG(TagName, TagString, Comment) \
	TagRestrictions.FindOrAdd(#Restriction).Add(GameplayTags.TagName);