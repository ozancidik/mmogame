// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOCharacterStore.h"
#include "Save/MMOSupabaseRestClient.h"

/**
 * Sonuc-turlu (asenkron) depo sonucu. DB kapali/ulasilamaz "yok" (NotFound) DEGILDIR: ayri sonuclar
 * (docs/DESIGN_SUPABASE.md 4.2, S04.00). Sadece NotFound baslangic paketi kararini acar.
 */
enum class EMMOSupabaseStoreOutcome : uint8
{
	Ok,
	NotFound,           // otoriter yokluk (DB "yok" dedi)
	AlreadyExists,
	Invalid,            // ad kurali / veri dogrulamasi
	RevisionConflict,   // eski revision: yuva "yeniden yukleme gerekli" olur (LoadEx cozer); o zamana dek SaveEx istek GONDERMEDEN bunu doner
	OwnershipLost,      // kira/epoch yitirildi (baska sunucu aldi / sure doldu)
	IdempotencyConflict,
	NotLeased,         // bu yuva icin LoadEx ile kira alinmadi
	Frozen,             // belirsiz sonuc yuzunden bagimli ilerleme donduruldu (ReconcileEx/LoadEx ile cozulur)
	Unavailable,        // DB/ag/kimlik yok: islem UYGULANMADI (ya da okuma basarisiz)
	Unknown,            // BELIRSIZ: yazim uygulanmis olabilir; yuva donar
	InternalError,
};

/**
 * Dilim 2'nin kendi ek arayuzu: IMMOCharacterStore DEGISMEZ (Dilim 3 temiz dosyalara koordinator onayi ile dokunur).
 * Geri cagrilar oyun is parcaciginda doner; hata dallari (gecersiz ad, donmus yuva) ayni karede de donebilir.
 */
class MYPROJECT_API IMMOAsyncCharacterStore
{
public:
	using FLoadExComplete = TFunction<void(EMMOSupabaseStoreOutcome, FMMOCharacterSaveData&&)>;
	using FStatusComplete = TFunction<void(EMMOSupabaseStoreOutcome)>;
	using FExistsComplete = TFunction<void(EMMOSupabaseStoreOutcome, bool bExists)>;

	virtual ~IMMOAsyncCharacterStore() = default;

	/**
	 * acquire_lease: kira al + veriyi yukle. NotFound = otoriter yokluk. DONMUS yuvada once eski epoch'la release_lease
	 * (ownership_lost/not_found = kira zaten bitmis, devam), sonra acquire (epoch artar: zaman asimina ugramis gec yazim
	 * eski epoch'la ownership_lost olur), sonra yukleme; basarili yuklemeyle yuva cozulur. Release'e ulasilamazsa Unavailable (yuva donuk kalir).
	 */
	virtual void LoadEx(const FString& SlotName, FLoadExComplete&& Completion) = 0;
	/**
	 * create_character: yoksa olusturur. Basarida (outcome == "committed") SQL olusturana epoch 1 + 120 sn kira verir:
	 * yuva kiralanmis (epoch/revision) kaydedilir, SaveEx dogrudan calisir (yine de kira SURESI icin RenewLeaseEx gerekir).
	 */
	virtual void CreateEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion) = 0;
	/** save_character: kira+epoch+expected_revision ile. Yuva basina tek ucustaki yazim; yenisi kuyruktakini birlestirir. */
	virtual void SaveEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion) = 0;
	virtual void ExistsEx(const FString& SlotName, FExistsComplete&& Completion) = 0;
	/** Donmus yuvayi get_op ile uzlastirir (save VE create). Donmamissa Ok. Bulunamazsa Unknown ve yuva DONUK kalir. */
	virtual void ReconcileEx(const FString& SlotName, FStatusComplete&& Completion) = 0;
	/**
	 * renew_lease: kirayi uzatir (periyodik cagri Dilim 4'tur). OwnershipLost/NotFound yuvayi NotLeased yapar;
	 * ulasilamazsa Unavailable ve kira bilgisi KORUNUR (sonraki yenileme denenebilir).
	 */
	virtual void RenewLeaseEx(const FString& SlotName, FStatusComplete&& Completion) = 0;
	/** release_lease: cikista kirayi birakir. Ok/OwnershipLost/NotFound yuvayi NotLeased yapar; Unavailable'da kira korunur. */
	virtual void ReleaseLeaseEx(const FString& SlotName, FStatusComplete&& Completion) = 0;
	virtual bool IsSlotFrozen(const FString& SlotName) const = 0;
	/**
	 * healthcheck: api_version ve contract_version == FMMOSupabaseRestClient::ContractVersion (2) dogrulanir. Ok = uyumlu.
	 * Invalid = surum uyusmazligi (yanlis sema/surum: yeniden denemek ise yaramaz), Unavailable = ulasilamadi/yanit sozlesmeye uymuyor.
	 * Dilim 3 NOTU: depo acilisinda (sunucu baslangici, ilk LoadEx'ten ONCE) cagrilmali, Ok degilse depo kullanilmamali.
	 * Simdi yalniz API + test; hicbir yerden cagrilmaz.
	 */
	virtual void HealthcheckEx(FStatusComplete&& Completion) = 0;
};

/**
 * Supabase (mimari C: mmo_api RPC) karakter deposu. Senkron IMMOCharacterStore yontemleri KAPALI-BASARISIZDIR
 * (false + Error log): uzak depoda senkron Load=false "kayit yok" ile karisirdi (baslangic paketi/ezme). Dogru
 * cagirani *Ex yollarina tasimak Dilim 4'tur. Varsayilan LoadAsync/SaveAsync de bu yuzden false doner.
 */
class MYPROJECT_API FMMOSupabaseCharacterStore : public IMMOCharacterStore, public IMMOAsyncCharacterStore
{
public:
	/** Holder: bu sunucu surecinin kira kimligi (ASCII, bos degil). */
	FMMOSupabaseCharacterStore(const TSharedRef<FMMOSupabaseRestClient>& InClient, const FString& InHolder);
	virtual ~FMMOSupabaseCharacterStore();

	// IMMOCharacterStore (kapali-basarisiz)
	virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) override;
	virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) override;
	virtual bool Exists(const FString& SlotName) override;
	virtual void ListSlotNames(TArray<FString>& Out) override;

	// IMMOAsyncCharacterStore
	virtual void LoadEx(const FString& SlotName, FLoadExComplete&& Completion) override;
	virtual void CreateEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion) override;
	virtual void SaveEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion) override;
	virtual void ExistsEx(const FString& SlotName, FExistsComplete&& Completion) override;
	virtual void ReconcileEx(const FString& SlotName, FStatusComplete&& Completion) override;
	virtual void RenewLeaseEx(const FString& SlotName, FStatusComplete&& Completion) override;
	virtual void ReleaseLeaseEx(const FString& SlotName, FStatusComplete&& Completion) override;
	virtual bool IsSlotFrozen(const FString& SlotName) const override;
	virtual void HealthcheckEx(FStatusComplete&& Completion) override;

	/** revision_conflict sonrasi yuva "yeniden yukleme gerekli" mi (LoadEx basarisi temizler). */
	bool IsReloadRequired(const FString& SlotName) const;

	/** Yuva icin istemcinin bildigi kira/revision (testler ve tani). Kira yoksa false. */
	bool TryGetSlotInfo(const FString& SlotName, int64& OutEpoch, int64& OutRevision) const;

	/**
	 * Yuva adi -> kanonik anahtar. "MMOSave_" onegi YALNIZ BIR KEZ soyulur; sonrasi SQL name_regex ile birebir:
	 * 1..32 karakter, [A-Za-z0-9 _-], bas/son bosluk YOK. ASCII disi/ayrilmis/uzun/bosluklu ad RED (kirpilip baska ada cevrilmez).
	 */
	static bool SlotToKey(const FString& SlotName, FString& OutKey, FString& OutDisplayName);

	/** p_holder kurali (vectors.json limits.holder_regex): ^[A-Za-z0-9_.:-]{1,96}$. Gecersiz holder ile hicbir istek gonderilmez. */
	static bool IsValidHolder(const FString& Holder);

private:
	struct FImpl;
	TSharedRef<FImpl> Impl;
};
