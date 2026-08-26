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
	// Anahtar SADECE ortam degiskeninden okunur; koda gomulmez, .ini'ye yazilmaz.
	FString GetAnthropicApiKey()
	{
		return FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
	}
}

bool UMMODialogueSubsystem::HasApiKey() const
{
	return !GetAnthropicApiKey().IsEmpty();
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
	const FString ApiKey = GetAnthropicApiKey();
	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] ANTHROPIC_API_KEY tanimli degil; istek atlaniyor."));
		OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		return;
	}

	// --- Istek govdesini kur (Messages API) ---
	// {
	//   "model": "...", "max_tokens": N,
	//   "system": "<sistem promptu>",
	//   "output_config": { "effort": "low" },      // dusuk gecikme
	//   "messages": [ { "role": "user", "content": "<oyuncu>" } ]
	// }
	// NOT: thinking parametresini GONDERMIYORUZ. Opus 5'te dusunme varsayilan
	// olarak ACIK ve adaptif; kapatmak yerine effort=low ile hizli+ucuz tutuyoruz.
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

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	// --- HTTP istegini kur ---
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Request->SetVerb(TEXT("POST"));

	// Zorunlu header'lar: kimlik (x-api-key) + API surumu + icerik tipi.
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-api-key"), ApiKey);
	Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));

	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// Yanit callback'i. TWeakObjectPtr ile subsystem yok olduysa guvenli kal.
	TWeakObjectPtr<UMMODialogueSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnReady](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
		{
			// Subsystem yok olduysa (dunya kapandi vs.) hicbir sey yapma — niyet edilen omur korumasi.
			if (!WeakThis.IsValid())
			{
				return;
			}

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
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] HTTP %d: %s"), Code, *RespBody);
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
				return;
			}

			FString Text, Error;
			if (ParseResponse(RespBody, Text, Error))
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

bool UMMODialogueSubsystem::ParseResponse(const FString& Body, FString& OutText, FString& OutError)
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
