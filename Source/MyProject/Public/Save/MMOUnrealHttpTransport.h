// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOHttpTransport.h"

/** IMMOHttpTransport'un FHttpModule gerceklemesi. URL/baslik/govde LOGLANMAZ. */
class MYPROJECT_API FMMOUnrealHttpTransport : public IMMOHttpTransport
{
public:
	virtual void Send(const FMMOHttpRequest& Request, FResponseHandler&& OnResponse) override;
	virtual double NowSeconds() const override;
	virtual void ScheduleAfter(float DelaySeconds, TFunction<void()>&& Task) override;
};
