// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Soyut HTTP ulasimi (Supabase RPC istemcisi icin, docs/DESIGN_SUPABASE.md 4.1). Gercegi FHttpModule
 * (FMMOUnrealHttpTransport); testte sahte ulasim (gecikmeli yanit, zaman asimi, ag hatasi).
 *
 * SOZLESME: Send'in OnResponse'u TAMAMLANINCA tam bir kez cagrilir (zaman asimi/ag hatasi da bir yanittir,
 * Result alani ile). Oyun is parcacigindan cagrilmasi beklenir; istemci yine de baska is parcacigindan
 * gelen yaniti oyun is parcacigina tasir. Ulasim URL'yi/basliklari LOGLAMAZ (anahtar basliktadir).
 */
struct FMMOHttpHeader
{
	FString Name;
	FString Value;
};

struct FMMOHttpRequest
{
	FString Verb = TEXT("POST");
	FString Url;
	TArray<FMMOHttpHeader> Headers;
	FString Body;
	float TimeoutSeconds = 8.f;
};

enum class EMMOHttpResult : uint8
{
	Ok,           // HTTP yaniti alindi (HttpCode gecerli, 4xx/5xx de buraya girer)
	Timeout,      // zaman asimi: sunucu isi yapmis olabilir
	NetworkError, // baglanti/DNS/TLS hatasi
};

struct FMMOHttpResponse
{
	EMMOHttpResult Result = EMMOHttpResult::NetworkError;
	int32 HttpCode = 0;
	FString Body;
};

class MYPROJECT_API IMMOHttpTransport
{
public:
	using FResponseHandler = TFunction<void(FMMOHttpResponse&&)>;

	virtual ~IMMOHttpTransport() = default;

	virtual void Send(const FMMOHttpRequest& Request, FResponseHandler&& OnResponse) = 0;
	/** Saat (saniye); butce hesabi icin. Testte elle ilerletilir. */
	virtual double NowSeconds() const = 0;
	/** Gecikmeli is (yeniden deneme beklemesi); oyun is parcacaginda calisir. */
	virtual void ScheduleAfter(float DelaySeconds, TFunction<void()>&& Task) = 0;
};
