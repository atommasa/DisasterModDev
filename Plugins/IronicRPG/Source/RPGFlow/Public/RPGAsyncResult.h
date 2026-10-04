// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

template<typename TValue = void>
struct TRPGAsyncResult;

template<typename TValue>
struct TRPGAsyncResult
{
	bool bSucceeded = false;
	TValue Value{};
	FName ErrorCode;
	FString ErrorMessage;

	static TRPGAsyncResult Success(TValue InValue)
	{
		TRPGAsyncResult Result;
		Result.bSucceeded = true;
		Result.Value = MoveTemp(InValue);
		return Result;
	}

	static TRPGAsyncResult Failure(
		FName InErrorCode,
		FString InErrorMessage)
	{
		TRPGAsyncResult Result;
		Result.bSucceeded = false;
		Result.ErrorCode = InErrorCode;
		Result.ErrorMessage = MoveTemp(InErrorMessage);
		return Result;
	}

	[[nodiscard]] bool IsSuccess() const
	{
		return bSucceeded;
	}
};

template<>
struct TRPGAsyncResult<void>
{
	bool bSucceeded = false;
	FName ErrorCode;
	FString ErrorMessage;

	static TRPGAsyncResult Success()
	{
		TRPGAsyncResult Result;
		Result.bSucceeded = true;
		return Result;
	}

	static TRPGAsyncResult Failure(
		FName InErrorCode,
		FString InErrorMessage)
	{
		TRPGAsyncResult Result;
		Result.bSucceeded = false;
		Result.ErrorCode = InErrorCode;
		Result.ErrorMessage = MoveTemp(InErrorMessage);
		return Result;
	}

	[[nodiscard]] bool IsSuccess() const
	{
		return bSucceeded;
	}
};