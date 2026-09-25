#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChoiceSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

/** Records choices locally and sends each choice as a separate HTTP request. */
UCLASS(Config=Game)
class BATYSKAF_API UChoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable)
	void ShowCurrentValues();

	UFUNCTION(BlueprintCallable)
	void SaveCSV();

	/** Value must be 0 or 1. Starts sending immediately, independently of SaveCSV. */
	UFUNCTION(BlueprintCallable)
	void AddRow(float Time, int32 Value);

	UFUNCTION(BlueprintPure, Category="Choices|HTTP")
	FString GetChoiceSessionId() const { return SessionId; }

	/** Includes decisions waiting for retry or blocked by a configuration error. */
	UFUNCTION(BlueprintPure, Category="Choices|HTTP")
	int32 GetPendingDecisionCount() const { return Pending.Num(); }

private:
	UPROPERTY(Config)
	bool bHttpEnabled = true;

	UPROPERTY(Config)
	FString HttpBaseUrl = TEXT("http://127.0.0.1:8088");

	UPROPERTY(Config)
	float RequestTimeoutSeconds = 5.0f;

	UPROPERTY(Config)
	float RetryBaseDelaySeconds = 0.5f;

	UPROPERTY(Config)
	float RetryMaxDelaySeconds = 10.0f;

	UPROPERTY(Config)
	int32 MaxInFlightRequests = 8;

	struct FPendingDecision
	{
		FString Session;
		int64 Sequence = 0;
		int32 Value = 0;
		float GameTime = 0;
		FString FilePath;
		int32 Attempts = 0;
		double NextAttemptAt = 0;
		bool bPersisted = false;
		bool bBlocked = false;
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
	};

	TArray<FString> Rows;
	TMap<FString, FPendingDecision> Pending;
	FString SessionId;
	FString WriteToken;
	FString OutboxDirectory;
	int64 NextSequence = 1;
	bool bStopping = false;
	FTSTicker::FDelegateHandle RetryTicker;

	bool TickPending(float DeltaTime);
	void PumpPending();
	void SendDecision(const FString& Key);
	void ScheduleRetry(FPendingDecision& Decision, int32 StatusCode);
	bool PersistDecision(const FPendingDecision& Decision) const;
	void LoadOutbox();
	void HandleResponse(const FString& Key,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, bool bSucceeded);
};
