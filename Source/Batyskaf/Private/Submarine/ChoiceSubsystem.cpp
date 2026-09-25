#include "Submarine/ChoiceSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogChoiceHttp, Log, All);

void UChoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Rows.Empty();
	FFileHelper::SaveStringToFile(TEXT(""), *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GameData.csv")));

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	NextSequence = 1;
	bStopping = false;
	const FString OverrideUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("BATYSKAF_CHOICE_URL"));
	if (!OverrideUrl.IsEmpty()) { HttpBaseUrl = OverrideUrl; }
	HttpBaseUrl.TrimStartAndEndInline();
	while (HttpBaseUrl.EndsWith(TEXT("/"))) { HttpBaseUrl.LeftChopInline(1); }
	WriteToken = FPlatformMisc::GetEnvironmentVariable(TEXT("BATYSKAF_CHOICE_WRITE_TOKEN"));
	RequestTimeoutSeconds = FMath::Max(0.1f, RequestTimeoutSeconds);
	RetryBaseDelaySeconds = FMath::Max(0.1f, RetryBaseDelaySeconds);
	RetryMaxDelaySeconds = FMath::Max(RetryBaseDelaySeconds, RetryMaxDelaySeconds);
	MaxInFlightRequests = FMath::Clamp(MaxInFlightRequests, 1, 64);
	OutboxDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChoiceHttp/Outbox"));
	if (bHttpEnabled)
	{
		IFileManager::Get().MakeDirectory(*OutboxDirectory, true);
		LoadOutbox();
		RetryTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UChoiceSubsystem::TickPending), 0.1f);
		UE_LOG(LogChoiceHttp, Display, TEXT("Choice session %s; endpoint %s; recovered %d pending decisions"),
			*SessionId, *HttpBaseUrl, Pending.Num());
		PumpPending();
	}
}

void UChoiceSubsystem::Deinitialize()
{
	bStopping = true;
	FTSTicker::GetCoreTicker().RemoveTicker(RetryTicker);
	for (auto& Pair : Pending)
	{
		if (Pair.Value.Request)
		{
			Pair.Value.Request->OnProcessRequestComplete().Unbind();
			Pair.Value.Request->CancelRequest();
		}
		if (!Pair.Value.bPersisted) { PersistDecision(Pair.Value); }
	}
	Pending.Empty(); // Unacknowledged records remain on disk for the next launch.
	Super::Deinitialize();
}

void UChoiceSubsystem::ShowCurrentValues()
{
	for (const FString& Row : Rows) { UE_LOG(LogChoiceHttp, Display, TEXT("%s"), *Row); }
}

void UChoiceSubsystem::AddRow(float Time, int32 Value)
{
	if ((Value != 0 && Value != 1) || !FMath::IsFinite(Time))
	{
		UE_LOG(LogChoiceHttp, Error, TEXT("Rejected choice: Value must be 0/1 and Time must be finite"));
		return;
	}
	Rows.Add(FString::Printf(TEXT("%f,%d"), Time, Value));
	if (!bHttpEnabled || bStopping) { return; }

	FPendingDecision Decision;
	Decision.Session = SessionId;
	Decision.Sequence = NextSequence++;
	Decision.Value = Value;
	Decision.GameTime = Time;
	const FString Key = FString::Printf(TEXT("%s_%lld"), *SessionId, Decision.Sequence);
	Decision.FilePath = FPaths::Combine(OutboxDirectory, Key + TEXT(".json"));
	Decision.bPersisted = PersistDecision(Decision);
	Pending.Add(Key, MoveTemp(Decision));
	PumpPending(); // No batching timer and no wait for the preceding request's ACK.
}

void UChoiceSubsystem::SaveCSV()
{
	const FString Content = FString::Join(Rows, TEXT("\n"));
	FFileHelper::SaveStringToFile(Content, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GameData.csv")));
}

bool UChoiceSubsystem::PersistDecision(const FPendingDecision& Decision) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("endpoint"), HttpBaseUrl);
	Json->SetStringField(TEXT("session_id"), Decision.Session);
	Json->SetStringField(TEXT("sequence"), LexToString(Decision.Sequence));
	Json->SetNumberField(TEXT("value"), Decision.Value);
	Json->SetNumberField(TEXT("game_time"), Decision.GameTime);
	FString Content;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Content));
	const FString TempPath = Decision.FilePath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Content, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !IFileManager::Get().Move(*Decision.FilePath, *TempPath, true))
	{
		UE_LOG(LogChoiceHttp, Error, TEXT("Cannot persist decision %s/%lld; keeping it in memory and retrying"),
			*Decision.Session, Decision.Sequence);
		return false;
	}
	return true;
}

void UChoiceSubsystem::LoadOutbox()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(OutboxDirectory, TEXT("*.json")), true, false);
	TArray<FString> TempFiles;
	IFileManager::Get().FindFiles(TempFiles, *FPaths::Combine(OutboxDirectory, TEXT("*.json.tmp")), true, false);
	Files.Append(TempFiles);
	for (const FString& File : Files)
	{
		FPendingDecision Decision;
		Decision.FilePath = FPaths::Combine(OutboxDirectory, File);
		FString Content, Endpoint, Sequence;
		double Value = -1, GameTime = 0;
		TSharedPtr<FJsonObject> Json;
		FGuid ParsedSession;
		if (!FFileHelper::LoadFileToString(Content, *Decision.FilePath)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content), Json) || !Json.IsValid()
			|| !Json->TryGetStringField(TEXT("endpoint"), Endpoint)
			|| !Json->TryGetStringField(TEXT("session_id"), Decision.Session) || !FGuid::Parse(Decision.Session, ParsedSession)
			|| !Json->TryGetStringField(TEXT("sequence"), Sequence) || !LexTryParseString(Decision.Sequence, *Sequence)
			|| Decision.Sequence < 1 || !Json->TryGetNumberField(TEXT("value"), Value) || (Value != 0 && Value != 1)
			|| !Json->TryGetNumberField(TEXT("game_time"), GameTime) || !FMath::IsFinite(GameTime)
			|| !FMath::IsFinite(static_cast<float>(GameTime)))
		{
			UE_LOG(LogChoiceHttp, Error, TEXT("Invalid outbox record retained for inspection: %s"), *File);
			continue;
		}
		if (Endpoint != HttpBaseUrl)
		{
			UE_LOG(LogChoiceHttp, Warning, TEXT("Retaining %s: it belongs to a different endpoint"), *File);
			continue;
		}
		Decision.Value = static_cast<int32>(Value);
		Decision.GameTime = static_cast<float>(GameTime);
		if (Decision.FilePath.EndsWith(TEXT(".tmp")))
		{
			const FString FinalPath = Decision.FilePath.LeftChop(4);
			if (!IFileManager::Get().Move(*FinalPath, *Decision.FilePath, true)) { continue; }
			Decision.FilePath = FinalPath;
		}
		Decision.bPersisted = true;
		const FString Key = FString::Printf(TEXT("%s_%lld"), *Decision.Session, Decision.Sequence);
		Pending.Add(Key, MoveTemp(Decision));
	}
}

bool UChoiceSubsystem::TickPending(float DeltaTime)
{
	PumpPending();
	return !bStopping;
}

void UChoiceSubsystem::PumpPending()
{
	if (bStopping) { return; }
	int32 InFlight = 0;
	TArray<FString> Ready;
	const double Now = FPlatformTime::Seconds();
	for (const auto& Pair : Pending)
	{
		if (Pair.Value.Request) { ++InFlight; }
		else if (!Pair.Value.bBlocked && Pair.Value.NextAttemptAt <= Now) { Ready.Add(Pair.Key); }
	}
	Ready.Sort([this](const FString& A, const FString& B)
	{
		const auto& Left = Pending.FindChecked(A);
		const auto& Right = Pending.FindChecked(B);
		return Left.Session == Right.Session ? Left.Sequence < Right.Sequence : Left.Session < Right.Session;
	});
	for (const FString& Key : Ready)
	{
		if (InFlight >= MaxInFlightRequests) { break; }
		SendDecision(Key);
		if (const auto* Decision = Pending.Find(Key); Decision && Decision->Request) { ++InFlight; }
	}
}

void UChoiceSubsystem::SendDecision(const FString& Key)
{
	FPendingDecision& Decision = Pending.FindChecked(Key);
	++Decision.Attempts;
	if (!Decision.bPersisted)
	{
		Decision.bPersisted = PersistDecision(Decision);
		if (!Decision.bPersisted) { ScheduleRetry(Decision, 0); return; }
	}
	const auto Request = FHttpModule::Get().CreateRequest();
	Decision.Request = Request;
	Request->SetURL(FString::Printf(TEXT("%s/api/sessions/%s/decisions/%lld"),
		*HttpBaseUrl, *Decision.Session, Decision.Sequence));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Game-Time"), FString::Printf(TEXT("%.9g"), Decision.GameTime));
	if (!WriteToken.IsEmpty()) { Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + WriteToken); }
	Request->SetContentAsString(Decision.Value == 0 ? TEXT("0") : TEXT("1"));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Key](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
		{ HandleResponse(Key, Response, bSucceeded); });
	if (!Request->ProcessRequest())
	{
		if (auto* Current = Pending.Find(Key); Current && Current->Request == Request)
		{
			Request->OnProcessRequestComplete().Unbind();
			Current->Request.Reset();
			ScheduleRetry(*Current, 0);
		}
	}
}

void UChoiceSubsystem::HandleResponse(const FString& Key, FHttpResponsePtr Response, bool bSucceeded)
{
	FPendingDecision* Decision = Pending.Find(Key);
	if (bStopping || !Decision) { return; }
	Decision->Request.Reset();
	const int32 Status = Response ? Response->GetResponseCode() : 0;
	if (bSucceeded && Response && (Status == 200 || Status == 201))
	{
		TSharedPtr<FJsonObject> Ack;
		FString AckSession, AckSequence;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Ack)
			&& Ack.IsValid() && Ack->TryGetStringField(TEXT("session_id"), AckSession)
			&& Ack->TryGetStringField(TEXT("sequence"), AckSequence)
			&& AckSession == Decision->Session && AckSequence == LexToString(Decision->Sequence)
			&& IFileManager::Get().Delete(*Decision->FilePath, false, false, true))
		{
			UE_LOG(LogChoiceHttp, Verbose, TEXT("Server stored decision %s"), *Key);
			Pending.Remove(Key);
			return;
		}
	}
	if (Status >= 400 && Status < 500 && Status != 408 && Status != 429)
	{
		Decision->bBlocked = true;
		UE_LOG(LogChoiceHttp, Error, TEXT("Decision %s blocked by HTTP %d. Retained on disk; fix configuration/server and restart the game."), *Key, Status);
		return;
	}
	ScheduleRetry(*Decision, Status);
}

void UChoiceSubsystem::ScheduleRetry(FPendingDecision& Decision, int32 StatusCode)
{
	const float Delay = FMath::Min(RetryMaxDelaySeconds,
		RetryBaseDelaySeconds * FMath::Pow(2.0f, static_cast<float>(FMath::Min(Decision.Attempts - 1, 8))));
	Decision.NextAttemptAt = FPlatformTime::Seconds() + Delay;
	UE_LOG(LogChoiceHttp, Warning, TEXT("Choice %s/%lld not acknowledged (HTTP %d); retry in %.1fs"),
		*Decision.Session, Decision.Sequence, StatusCode, Delay);
}
