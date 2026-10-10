// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Basarimin hangi sayaci/olcutu izledigi. */
enum class EMMOAchievementKind : uint8
{
	KillTotal,   // toplam oldurulen mob >= Target
	KillMob,     // TargetId arketipinden oldurulen >= Target
	ReachLevel,  // karakter seviyesi >= Target
	GoldHeld,    // cuzdanda bir an Target altin vardi (tepe degeri saklanir)
	QuestsDone,  // tamamlanan gorev sayisi >= Target
	Deaths       // olum sayisi >= Target
};

/** Tek bir basarim tanimi (kod icinde veri tablosu; yeni satir eklerken Id benzersiz olmali). */
struct FMMOAchievementDef
{
	FName Id;
	FString Name;
	FString Description;
	EMMOAchievementKind Kind = EMMOAchievementKind::KillTotal;
	FName TargetId;       // yalniz KillMob: mob arketip kimligi
	int32 Target = 1;
	int32 RewardGold = 0; // kucuk tutulur: ekonomi sink'ini bozmasin; asil odul unvan
	FString Title;        // bos = unvan yok
};

/** Tum basarimlarin statik veritabani. */
class MYPROJECT_API FMMOAchievementDatabase
{
public:
	static const TArray<FMMOAchievementDef>& GetAll();
	static const FMMOAchievementDef* Find(FName Id);
};
