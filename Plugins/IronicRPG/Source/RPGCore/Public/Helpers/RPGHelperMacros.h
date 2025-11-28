// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/*----------------------------------------------------------------------------
    Common Macros
----------------------------------------------------------------------------*/

/**
 * @brief A macro to bind a member function to a lambda with no parameters.
 *
 * @return A lambda that captures `this` and calls the member function.
 */
#define BIND_TFUNCTION_NoResult(FuncName, ...) \
	[&]() { this->FuncName(__VA_ARGS__); }

/**
 * @brief A macro to bind a member function to a lambda with one parameter.
 * 
 * @return A lambda that captures `this` and calls the member function with the provided arguments.
 */
#define BIND_TFUNCTION_OneResult(FuncName, ...) \
	[&](auto&& Result) { this->FuncName(Result, ## __VA_ARGS__); }

/*----------------------------------------------------------------------------
    WITH_EDITOR Macros
----------------------------------------------------------------------------*/
#if WITH_EDITOR

/**
 * When an UPROPERTY() array's length need to be limited by a value. 
 * Needs to be wrapped in PROP_CHANGE_COMMIT()
 * 
 * @param ArrProp		An array property which its length is limited by an int32 property
 * @param LimitProp		An int32 property which its value limit an array's length
 */
#define CLAMPED_ARRAY(ArrProp, LimitProp)                                                   \
        static const FName LimitPropName = GET_MEMBER_NAME_CHECKED(std::remove_pointer<decltype(this)>::type, LimitProp); \
        FProperty* LimitProperty = this->GetClass()->FindPropertyByName(LimitPropName);      \
        if (!LimitProperty)                                                                  \
        {                                                                                    \
            return;                                                                          \
        }                                                                                    \
                                                                                             \
        FIntProperty* TypedLimitProperty = CastField<FIntProperty>(LimitProperty);           \
        if (!TypedLimitProperty)                                                             \
        {                                                                                    \
            return;                                                                          \
        }                                                                                    \
                                                                                             \
        const void* LimitPtr = TypedLimitProperty->ContainerPtrToValuePtr<void>(this);       \
        int32 LimitNum = TypedLimitProperty->GetPropertyValue(LimitPtr);                     \
                                                                                             \
        if (ArrProp.Num() > LimitNum)                                                        \
        {                                                                                    \
            ArrProp.SetNum(LimitNum);                                                        \
        }

/**
 * @brief Register multiple properties via matching macros in PostEditChangeProperty.
 *
 * You must call this macro and wrap all property changing macros inside it.
 *
 * Example:
 * @code
 * PROP_CHANGE_COMMIT()
 * {
 *     CLAMPED_ARRAY(ArrayA, MaxA);
 *     CLAMPED_ARRAY(ArrayB, MaxB);
 * }
 * @endcode
 */
#define PROP_CHANGE_COMMIT()                                                               \
private:                                                                                     \
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override\
    {                                                                                        \
        Super::PostEditChangeProperty(PropertyChangedEvent);                                 \
        PropertiesChanged(PropertyChangedEvent);                                                                 \
    }                                                                                        \
                                                                                             \
    void PropertiesChanged(FPropertyChangedEvent& PropertyChangedEvent)

#else // WITH_EDITOR

#define CLAMPED_ARRAY(ArrProp, LimitProp)
#define PROP_CHANGE_COMMIT()   

#endif // WITH_EDITOR