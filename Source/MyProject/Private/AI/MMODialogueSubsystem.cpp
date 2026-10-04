// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/MMODialogueSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

namespace
{
	// Saglayiciya gore anahtarin okunacagi ortam degiskeninin adi.
	const TCHAR* ApiKeyEnvVar(EMMOLLMProvider Provider)
	{
		return Provider == EMMOLLMProvider::NVIDIA ? TEXT("NVIDIA_API_KEY") : TEXT("ANTHROPIC_API_KEY");
	}

	// Anahtar SADECE ortam degiskeninden okunur; koda gomulmez, .ini'ye yazilmaz.
	FString GetApiKeyFor(EMMOLLMProvider Provider)
	{
		return FPlatformMisc::GetEnvironmentVariable(ApiKeyEnvVar(Provider));
	}
}

bool UMMODialogueSubsystem::HasApiKey() const
{
	return !GetApiKeyFor(Provider).IsEmpty();
}

void UMMODialogueSubsystem::RequestNpcLine(const FString& NpcName, const FString& NpcRole,
	const FString& PlayerInfo, FMMOOnDialogueReady OnReady)
{
	// Sistem promptu = NPC'nin kimligi ve uslup kurallari. Oyun ici oldugu icin
	// kisa, ekranda tek balonluk repliklere zorluyoruz.
	const FString SystemPrompt = FString::Printf(TEXT(
		"Sen bir MMORPG oyununda '%s' adli bir NPC'sin. Rolun: %s. "
		"Oyuncuya, karakterine sadik kalarak TEK bir kisa replikle (en fazla 2 cumle) cevap ver. "
		"Turkce konus. Sistem/oyun terimlerini, madde imlerini veya XML etiketlerini kullanma; "
		"sadece NPC'nin agzindan cikacak sozu yaz."),
		*NpcName, *NpcRole);

	const FString UserPrompt = PlayerInfo.IsEmpty()
		? TEXT("Oyuncu seninle konusmaya basladi. Onu selamla.")
		: FString::Printf(TEXT("Oyuncu seninle konusuyor. Oyuncu hakkinda bilinenler: %s"), *PlayerInfo);

	SendRequest(SystemPrompt, UserPrompt, OnReady);
}

void UMMODialogueSubsystem::RequestRaw(const FString& SystemPrompt, const FString& UserPrompt,
	FMMOOnDialogueReady OnReady)
{
	SendRequest(SystemPrompt, UserPrompt, OnReady);
}

void UMMODialogueSubsystem::SendRequest(const FString& SystemPrompt, const FString& UserPrompt,
	FMMOOnDialogueReady OnReady)
{
	const FString ApiKey = GetApiKeyFor(Provider);
	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] %s tanimli degil; istek atlaniyor."), ApiKeyEnvVar(Provider));
		OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		return;
	}

	// Sunucu geneli hiz siniri: dakikada MaxRequestsPerMinute, ayni anda MaxConcurrentRequests.
	const double NowSec = FPlatformTime::Seconds();
	RecentRequestTimes.RemoveAll([NowSec](double T) { return NowSec - T > 60.0; });
	if (RecentRequestTimes.Num() >= MaxRequestsPerMinute || InFlightRequests >= MaxConcurrentRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Hiz siniri asildi (%d/dk, %d eszamanli); istek atlandi."),
			RecentRequestTimes.Num(), InFlightRequests);
		OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		return;
	}
	RecentRequestTimes.Add(NowSec);
	++InFlightRequests;

	const bool bNvidia = (Provider == EMMOLLMProvider::NVIDIA);

	// --- Istek govdesini saglayiciya gore kur ---
	FString Body;
	FString Url;
	if (bNvidia)
	{
		// NVIDIA NIM = OpenAI-uyumlu /chat/completions:
		// { "model", "max_tokens", "temperature",
		//   "messages": [ {role:system,...}, {role:user,...} ] }
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), NvidiaModel);
		Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
		Root->SetNumberField(TEXT("temperature"), Temperature);

		const TSharedRef<FJsonObject> SysMsg = MakeShared<FJsonObject>();
		SysMsg->SetStringField(TEXT("role"), TEXT("system"));
		SysMsg->SetStringField(TEXT("content"), SystemPrompt);
		const TSharedRef<FJsonObject> UsrMsg = MakeShared<FJsonObject>();
		UsrMsg->SetStringField(TEXT("role"), TEXT("user"));
		UsrMsg->SetStringField(TEXT("content"), UserPrompt);

		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(SysMsg));
		Messages.Add(MakeShared<FJsonValueObject>(UsrMsg));
		Root->SetArrayField(TEXT("messages"), Messages);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Root, Writer);
		Url = TEXT("https://integrate.api.nvidia.com/v1/chat/completions");
	}
	else
	{
		// Anthropic Messages API:
		// { "model", "max_tokens", "system", "output_config":{effort}, "messages":[{user}] }
		// NOT: thinking parametresini GONDERMIYORUZ; effort=low ile hizli+ucuz tutuyoruz.
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), Model);
		Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
		Root->SetStringField(TEXT("system"), SystemPrompt);

		const TSharedRef<FJsonObject> OutputConfig = MakeShared<FJsonObject>();
		OutputConfig->SetStringField(TEXT("effort"), Effort);
		Root->SetObjectField(TEXT("output_config"), OutputConfig);

		const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), UserPrompt);

		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
		Root->SetArrayField(TEXT("messages"), Messages);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Root, Writer);
		Url = TEXT("https://api.anthropic.com/v1/messages");
	}

	// --- HTTP istegini kur ---
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bNvidia)
	{
		// OpenAI-uyumlu kimlik: Bearer token.
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	}
	else
	{
		Request->SetHeader(TEXT("x-api-key"), ApiKey);
		Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	}

	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// Yanit callback'i. TWeakObjectPtr ile subsystem yok olduysa guvenli kal.
	TWeakObjectPtr<UMMODialogueSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnReady, bNvidia](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
		{
			// Subsystem yok olduysa (dunya kapandi vs.) hicbir sey yapma — niyet edilen omur korumasi.
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis->InFlightRequests = FMath::Max(0, WeakThis->InFlightRequests - 1);

			if (!bConnectedOk || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Baglanti/istek hatasi."));
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
				return;
			}

			const int32 Code = Resp->GetResponseCode();
			const FString RespBody = Resp->GetContentAsString();

			if (Code != 200)
			{
				// Hata govdesi kisaltilir: bazi saglayicilar iletide anahtar parcasi yankilar (SECURITY_AUDIT D-10).
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] HTTP %d: %s"), Code, *RespBody.Left(200));
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
				return;
			}

			FString Text, Error;
			const bool bOk = bNvidia ? ParseOpenAIResponse(RespBody, Text, Error)
			                         : ParseAnthropicResponse(RespBody, Text, Error);
			if (bOk)
			{
				OnReady.ExecuteIfBound(true, Text);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Yanit ayristirilamadi: %s"), *Error);
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
			}
		});

	Request->ProcessRequest();
}

bool UMMODialogueSubsystem::ParseOpenAIResponse(const FString& Body, FString& OutText, FString& OutError)
{
	// OpenAI/NVIDIA /chat/completions basarili yaniti:
	// { "choices": [ { "message": { "role":"assistant", "content":"..." },
	//                  "finish_reason":"stop" } ] }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON cozulemedi");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!Root->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		OutError = TEXT("choices bos");
		return false;
	}

	const TSharedPtr<FJsonObject> First = (*Choices)[0]->AsObject();
	if (!First.IsValid())
	{
		OutError = TEXT("choice[0] gecersiz");
		return false;
	}

	const TSharedPtr<FJsonObject>* MsgObj = nullptr;
	if (!First->TryGetObjectField(TEXT("message"), MsgObj) || !MsgObj->IsValid())
	{
		OutError = TEXT("message yok");
		return false;
	}

	FString Content;
	if ((*MsgObj)->TryGetStringField(TEXT("content"), Content) && !Content.TrimStartAndEnd().IsEmpty())
	{
		OutText = Content.TrimStartAndEnd();
		return true;
	}

	OutError = TEXT("content bos");
	return false;
}

bool UMMODialogueSubsystem::ParseAnthropicResponse(const FString& Body, FString& OutText, FString& OutError)
{
	// Basarili yanit sekli:
	// {
	//   "role": "assistant",
	//   "stop_reason": "end_turn" | "refusal" | "max_tokens" | ...,
	//   "content": [
	//     { "type": "thinking", "thinking": "" },   // effort>low'da olabilir; atlanir
	//     { "type": "text", "text": "NPC replik..." }
	//   ]
	// }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON cozulemedi");
		return false;
	}

	// Icerigi okumadan ONCE stop_reason'i kontrol et: guvenlik siniflandiricisi
	// istegi reddettiyse content bos/kismi olur ve [0].text okumak coker.
	FString StopReason;
	Root->TryGetStringField(TEXT("stop_reason"), StopReason);
	if (StopReason == TEXT("refusal"))
	{
		OutError = TEXT("Model istegi guvenlik nedeniyle reddetti (refusal)");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
	if (!Root->TryGetArrayField(TEXT("content"), Content) || Content->Num() == 0)
	{
		OutError = TEXT("content bos");
		return false;
	}

	// Ilk 'text' blogunu bul (thinking bloklari atlanir).
	for (const TSharedPtr<FJsonValue>& Block : *Content)
	{
		const TSharedPtr<FJsonObject> Obj = Block->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FString BlockType;
		Obj->TryGetStringField(TEXT("type"), BlockType);
		if (BlockType == TEXT("text"))
		{
			FString Text;
			if (Obj->TryGetStringField(TEXT("text"), Text) && !Text.TrimStartAndEnd().IsEmpty())
			{
				OutText = Text.TrimStartAndEnd();
				return true;
			}
		}
	}

	OutError = TEXT("text blogu bulunamadi");
	return false;
}
