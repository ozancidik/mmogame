// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOUnrealHttpTransport.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void FMMOUnrealHttpTransport::Send(const FMMOHttpRequest& Request, FResponseHandler&& OnResponse)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(Request.Url);
	Http->SetVerb(Request.Verb);
	// Basliklar oldugu gibi gider; "User-Agent" verilmisse UE'nin varsayilan UA'sini ezer (WinHttp: ADD|REPLACE).
	for (const FMMOHttpHeader& Header : Request.Headers)
	{
		Http->SetHeader(Header.Name, Header.Value);
	}
	if (!Request.Body.IsEmpty())
	{
		Http->SetContentAsString(Request.Body);
	}
	Http->SetTimeout(Request.TimeoutSeconds);

	FResponseHandler Handler = MoveTemp(OnResponse);
	Http->OnProcessRequestComplete().BindLambda(
		[Handler](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			FMMOHttpResponse Out;
			if (bConnected && Resp.IsValid())
			{
				Out.Result = EMMOHttpResult::Ok;
				Out.HttpCode = Resp->GetResponseCode();
				Out.Body = Resp->GetContentAsString();
			}
			else
			{
				const bool bTimedOut = Req.IsValid() && Req->GetFailureReason() == EHttpFailureReason::TimedOut;
				Out.Result = bTimedOut ? EMMOHttpResult::Timeout : EMMOHttpResult::NetworkError;
			}
			if (Handler)
			{
				Handler(MoveTemp(Out));
			}
		});

	if (!Http->ProcessRequest())
	{
		// Baslatilamadi: yaniti yine async ilet (cagiranin yeniden giris varsayimi bozulmasin).
		ScheduleAfter(0.f, [Handler]()
		{
			FMMOHttpResponse Out;
			Out.Result = EMMOHttpResult::NetworkError;
			if (Handler)
			{
				Handler(MoveTemp(Out));
			}
		});
	}
}

double FMMOUnrealHttpTransport::NowSeconds() const
{
	return FPlatformTime::Seconds();
}

void FMMOUnrealHttpTransport::ScheduleAfter(float DelaySeconds, TFunction<void()>&& Task)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Work = MoveTemp(Task)](float) -> bool
		{
			if (Work)
			{
				Work();
			}
			return false;
		}), DelaySeconds);
}
