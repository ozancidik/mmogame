// Copyright Epic Games, Inc. All Rights Reserved.
// OTOMATIK URETILDI: Scripts/GenQuestDialogue.py (kaynak: docs/Lore.md). Elle duzenleme.

#include "Quests/MMOQuestDialogue.h"

namespace
{
	struct FEntry { const TCHAR* Accept; const TCHAR* TurnIn; };

	const TMap<FName, FEntry>& BuildTable()
	{
		static TMap<FName, FEntry> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}
		Table.Add(FName(TEXT("Q_AbyssBastion")), { TEXT("Naibin onunde son bir siper var, evlat: yaylanin dogusuna dizilmis koruyucular. On ikisini devir; ardinda yalniz o golge kalacak."), TEXT("Siper yikildi, dogunun yolu acik. Al su Buyuk Can Iksirleri'ni ve soluklan; Naibin karsisina yorgun cikma.") });
		Table.Add(FName(TEXT("Q_AbyssDescent")), { TEXT("Kuzeybatida gezginlerin yuvasi var; ucurumdan oraya tirmanip yaylaya oradan yayiliyorlar. On ikisini dagit. Asagiya inen yolu goreceksin - bakma, yalniz dagit."), TEXT("Yuva dagildi. Gozun hala o yola takiliyorsa su Buyuk Mana Iksirleri'ni ic de kendine gel.") });
		Table.Add(FName(TEXT("Q_AbyssEdge")), { TEXT("Gezginler kiyidan yukari tirmaniyor; yaylanin bize kalan yarisini da istiyorlar. On ikisini geri it - kiyida adimini agirlastirmalarina izin verme."), TEXT("Kiyi yeniden bizim. Bu Buyuk Mana Iksirleri senin; dibi gorunmeyen yerde gucun tukenmesin.") });
		Table.Add(FName(TEXT("Q_AbyssGuardians")), { TEXT("Yaylanin obur ucunda koruyucular bekler; ucuruma inen yolu tutarlar. Besini devir. Kolay devrilmezler, son nefeslerinde ofkelenirler - geri cekilme."), TEXT("Koruyucular dustu. Ucurum Kaski'ni tak, evlat; asagidan gelen sesi duymamak icin degil, duyup da aldirmamak icin.") });
		Table.Add(FName(TEXT("Q_AbyssLine")), { TEXT("Koruyucularin ardinda bir hat daha var; altisini daha devir. Bu son hat, evlat. Ardinda bir naib bekliyor."), TEXT("Yol acik. Al su Ucurum Zirhi'ni. Yillar once sana tasiyabileceginden agirini vermem demistim; sozumu tuttum mu, artik bilemiyorum.") });
		Table.Add(FName(TEXT("Q_AbyssPatrol")), { TEXT("Kenar temiz dedik, ama gezginler devriye gibi doner durur; biri gidince oburu gelir. Onunun yolunu kes, evlat, kenari biraz daha biz tutalim."), TEXT("Devriye bozuldu. Al su iksirleri; kenarda yaralanan, gozunu asagiya dikmeye baslar.") });
		Table.Add(FName(TEXT("Q_AbyssRegent")), { TEXT("Naib, sahibinin yerine hukum surendir, evlat. Yaylanin dogusunda oturan o golge kimin adina konusuyorsa, fisiltiyi da onun adina yayiyor. Devir onu; dokunusu bedenini agirlastirir, ofkesi sona yaklastikca buyur. Ve donmeyi unutma."), TEXT("Duyuyor musun? Hicbir sey. Fisilti sustu, evlat... simdilik. Al su Ucurum Kilici'ni; naib dustu ama taht bos kalmaz. O gun geldiginde kilicin elinde olsun.") });
		Table.Add(FName(TEXT("Q_AbyssStriders")), { TEXT("Yillarca fisiltinin ormandan geldigini sandik, evlat. Ormanin kokleri, buzun kapisi, yazin yarigi - hepsi ayni bosluga aciliyormus. Ucurumun kenarinda gezginler dolasiyor; besini durdur. Dokunuslari adimini agirlastirir, kenarda agir adim tehlikelidir."), TEXT("Kenar temiz. Ucurum Botu'nu giy; dibi gorunmeyen yerde saglam basmak her seydir.") });
		Table.Add(FName(TEXT("Q_AbyssVigil")), { TEXT("Hat kirildi ama koruyucular yaylada nobetini surduruyor; sahipleri donecekmis gibi. Onunu bekle ve devir. Sabirli ol, evlat; nobetciyi ancak nobetci yener."), TEXT("Nobet dustu. Al su Buyuk Can Iksirleri'ni; Naibe giden yolda hepsine ihtiyacin olacak.") });
		Table.Add(FName(TEXT("Q_AshBinders")), { TEXT("Kulu buza, buzu kule baglayan baglayicilar var. Atesle kisi ayni ipe dizen bir el... Gumus Dal'in yaziti yine dogru cikti. Besini devir; vuruslari adimini agirlastirir, son nefeslerinde ofkelenirler."), TEXT("Ip gevsedi. Kul Kaski'ni tak; dumanin icinde basini dik, gozunu acik tut.") });
		Table.Add(FName(TEXT("Q_AshLine")), { TEXT("Baglayicilar karin ustune bir hat ormus; tek dugum kalsa yeniden orerler. Alti baglayici daha devir ki hat cozulsun."), TEXT("Hat cozuldu. Al su Kul Zirhi'ni; atesten de soguktan da gecmis birine yakisir.") });
		Table.Add(FName(TEXT("Q_AshOrcs")), { TEXT("Kul ovasinda orklar yeni bir kabile kurmus; derileri kavrulmus, gozleri dumanli. Kaldirdiklari kul adimini agirlastirir. Altisini dagit, yoksa vadiden tasacaklar."), TEXT("Ova temizlendi. Adamantin Kask'i tak; kulun icinde bile basini dik tut.") });
		Table.Add(FName(TEXT("Q_AshTyrant")), { TEXT("Hattin ucunda bir tiran oturuyor: Magma Tirani'nin kulunden dogmus, atesi sonmus ama hirsi sonmemis. Bir kez devirdigin seyi ikinci kez devirmek daha zordur, evlat - artik seni taniyor. Dokunusu yakar, cani azaldikca ofkelenir."), TEXT("Kul yagmuru durdu. Al su Kul Kilici'ni. Kuzeyde sis cekiliyor; yaylanin ortasinda bir bosluk var - fisilti oradan geliyor.") });
		Table.Add(FName(TEXT("Q_AshVanguard")), { TEXT("Kis Diyari'nin guneyinde kar griye donmus, evlat; gokten kul yagiyor. Alev Vadisi'nde sondurdugun ates olmemis, kule donup kuzeye yurumus. Onculerinden besini durdur; hizlidirlar, yaniklari derine isler."), TEXT("Onculer dustu ama kul hala yagiyor. Al su Kul Botu'nu; kor kapli karda bile ayagin saglam bassin.") });
		Table.Add(FName(TEXT("Q_CursedKnights")), { TEXT("O harabeler bir zamanlar bir sovalye tabyasiydi; Uzun Kis'ta son adamina dek dayandilar. Simdi fisilti onlari yeniden nobete kaldirmis. Besini yeminlerinden azat et; son nefeslerinde ofkelenirler, geri cekilme."), TEXT("Yeminleri bitti, huzurlari basladi. Al su Mithril Bot'u; buzda yolun kaymasin.") });
		Table.Add(FName(TEXT("Q_EmberHounds")), { TEXT("Buz Lordu dustugu gun dogu daglarindan duman yukseldi; bir yerde kis cekilince baska bir yerde ates uyandi. Alev Vadisi'nin gecidini kor tazilari tutuyor. Besini avla; isiriklari yakar, izleri kolay silinmez."), TEXT("Gecit acik. Al su sifa siselerini; bundan sonra her nefes sicak gelecek.") });
		Table.Add(FName(TEXT("Q_FrostLich")), { TEXT("Iste buzun kalbi. Tahtinda oturan lord Uzun Kis'in ta kendisi mi, ondan dogan bir sey mi - bilmiyorum; bildigim, o nefes aldikca buzulun erimeyecegi. Git, devir onu; dokunusu seni dondurmaya, ofkesi ezmeye calisacak."), TEXT("Ruzgar dindi, evlat. Iki nesildir ilk kez. Al bu Mithril Kilic'i; hak ettin.") });
		Table.Add(FName(TEXT("Q_FrostWraiths")), { TEXT("Ork Reisi dustu, evlat, ama kuzeyden esen ruzgar hala olum kokuyor. Ayazdis'in sisinde Uzun Kis'in oluleri dolasiyor; bes Buz Hayaleti'ni dagit ki yol acilsin. Dokunuslarina dikkat et - adimini agirlastirirlar."), TEXT("Sis inceldi. Simdi buzun altindaki taslari gorebiliyorsun, degil mi?") });
		Table.Add(FName(TEXT("Q_FrostboundLine")), { TEXT("Dusen nobetcilerin yerine yenileri gecti; hat yirtilmadi, yalnizca inceldi. Alti nobetci daha devir ki Buz Kapisi'na giden yol acilsin. Hat dedigin, son halkasi kirilinca hat olmaktan cikar."), TEXT("Hat kirildi. Buzbagli Zirh artik senin; onu giyen yeminli degildir, ama yeminlilerden de geri kalmaz.") });
		Table.Add(FName(TEXT("Q_FrostboundScouts")), { TEXT("Hukumdar dustu, firtina dindi; ama dinen firtina, ardinda saklanani gosterir, evlat. Diyarin kenarinda buzdan bir sinir var ve onu yeminli gozculer kolluyor. Besini sustur; hizlidirlar, dokunuslari adimini agirlastirir."), TEXT("Gozculer sustu; artik sinir seni gormeden sen onu goreceksin. Al su Buzbagli Bot'u, buzda iz birakma.") });
		Table.Add(FName(TEXT("Q_FrostboundSentinels")), { TEXT("Gozculerin ardinda nobetciler bekler; yavas yururler ama geri adim atmayi bilmezler. Besini devir. Canlari azaldikca yeminlerine sarilirlar - en tehlikeli anlari odur."), TEXT("Yemin agirdir, evlat; tasiyani da agirlastirir. Bu Buzbagli Kask'i tak; aklin sogukta bile berrak kalsin.") });
		Table.Add(FName(TEXT("Q_FrostboundWarden")), { TEXT("Kapinin onunde son bir bekci var. Bil ki o fisiltiya degil, kapiya sadiktir; belki de bu diyarin en durust askeridir. Yine de yolumuz onun ardindan geciyor. Devir onu - ama dustugunde kapiya kulak ver."), TEXT("Dustu... ve kapi titredi, degil mi? Ben de duydum. Al su Buzbagli Kilic'i, evlat; korkarim yakinda guneyde lazim olacak.") });
		Table.Add(FName(TEXT("Q_InfernalWraiths")), { TEXT("Buzulda gordugun hayaletlerin atesten ikizleri vadinin dibinde dolasiyor. Onlar dondurmaz, yakar; yandikca da ofkelenirler. Besini kov ki dibe inen yol acilsin."), TEXT("Dip bosaldi. Adamantin Zirh artik senin; vadinin dibinden donen, onu tasimayi hak etmistir.") });
		Table.Add(FName(TEXT("Q_MagmaGolems")), { TEXT("Karakaya'nin tas bekcisini hatirliyor musun? Vadinin lav golunu de onun atesten akrabalari bekliyor. Besini kir; yavas gelirler ama canlari azaldikca ofkeleri kabarir."), TEXT("Gol sessiz. Adamantin Bot'u giy; lavin kiyisinda bile saglam basarsin.") });
		Table.Add(FName(TEXT("Q_MagmaTyrant")), { TEXT("Vadinin hukumdari Magma Tirani. Buz Lordu sogugun tahtindaysa o da atesin tahtinda oturur - ikisi ayni fisiltinin iki yuzu. Onu devir; ama bil ki yanigi derine isler, ofkesi daglari sarsar."), TEXT("Dag sustu, evlat. Al su Adamantin Kilic'i. Fisilti hala bir yerlerde konusuyor; ama artik seni taniyor - ve senden korkuyor.") });
		Table.Add(FName(TEXT("Q_OrcBerserkers")), { TEXT("Reislerini kaybeden orklar dagilmadi; en gozu donmusleri kuzey kampina cekildi. Aci onlari yildirmaz, cildirtir. Besini kir; o kamp ayakta kaldikca buzulda kimse guvende degil."), TEXT("Kamp dagildi. Bu Mithril Zirh artik senin; soguga da baltaya da gogus gerer.") });
		Table.Add(FName(TEXT("Q_RiftDepths")), { TEXT("Bekcilerin ardinda yarik derinlesiyor; mor isik her gece biraz daha asagidan geliyor. On bekci devir. Derinligi kapatamayiz belki, ama yolunu daraltabiliriz."), TEXT("Isik sonuklesti. Bu Buyuk Mana Iksirleri'ni sakla; derine bakan, gucunu cabuk tuketir.") });
		Table.Add(FName(TEXT("Q_RiftHerald")), { TEXT("Yarigi acan el bir habercinin eli. Haberciler savasi degil sozu getirir; ama getirdikleri soz kimi zaman savastan beterdir. Devir onu; dokunusu zehirler, cani azaldikca ofkelenir. Son sozunu iyi dinle."), TEXT("'Kul yuruyor,' dedi, oyle mi... Al su Yarik Kilici'ni, evlat. Yolun yine kuzeye; Kis seni ikinci kez bekliyor.") });
		Table.Add(FName(TEXT("Q_RiftKeepers")), { TEXT("Yarigin agzinda bekciler duruyor; kapatmak icin degil, acik tutmak icin. Besini devir. Agir gelirler, canlari azaldikca ofkeleri tasar."), TEXT("Agiz daraldi. Yarik Kaski'ni tak; o mor isiga uzun bakanin akli karisir.") });
		Table.Add(FName(TEXT("Q_RiftPurge")), { TEXT("Nobet dagildi ama yarigin agzi bos kalmadi, evlat; arayicilar yeniden toplaniyor. Onunu seyrelt ki habercinin yolunu acarken sirtin guvende olsun."), TEXT("Agiz seyreldi. Al su Buyuk Can Iksirleri'ni; zehrin goturdugunu bunlar geri getirir.") });
		Table.Add(FName(TEXT("Q_RiftSeekers")), { TEXT("Teraziyi hatirla, evlat: kuzeyde kapi gevsedi, guneyde toprak yarildi. Yaz'in dis yamaclarinda bir yarik acilmis, icinden arayicilar cikiyor. Kuzey Yolu'ndan geri in ve besini durdur; isiriklari zehirler."), TEXT("Ne aradiklarini bilmiyoruz; bulamadiklarini bilmek simdilik yeter. Al su Yarik Botu'nu; bu topraklar artik eskisi kadar saglam basmiyor.") });
		Table.Add(FName(TEXT("Q_RiftWatch")), { TEXT("Bir bekci dusunce yerine bir baskasi geciyor; nobet bozulmadikca yarigi acan el ortaya cikmaz. Alti bekci daha devir."), TEXT("Nobet dagildi; simdi yarigin icinden bir ses duyuluyor. Yarik Zirhi'ni giy - o ses seninle konusmak isteyecek.") });
		Table.Add(FName(TEXT("Q_ShadowAssassin")), { TEXT("Bir golge dolasiyor patikalarda; kervanlar sabaha sessiz variyor, kimse bagirmaya firsat bulamiyor. O golgeyi bul ve dagit - yoksa bir sonraki sessizlik senin olur."), TEXT("") });
		Table.Add(FName(TEXT("Q_SpiderQueen")), { TEXT("Ormanin golgesi agirlasti, evlat; agaclarin arasi ipek ordu. O agin ortasinda bir ana bekler - onu yakmadan, orman bir daha nefes almayacak."), TEXT("") });
		Table.Add(FName(TEXT("Q_StoneGolem")), { TEXT("Karakaya bir zamanlar kendi bekcisini tastan yontmustu. Simdi o bekci yanlis bir sozle uyandi ve gecide vuruyor. Onu tekrar uykuya yatir; ama unutma, ofkesi buyudukce darbesi agirlasir."), TEXT("") });
		Table.Add(FName(TEXT("Q_VenomSpiders")), { TEXT("Buzun altinda sicak oyuklar var, evlat; sicaklik hayati cagirir, iyisini de kotusunu de. Orumcek Ana'nin soyu oraya yuva kurmus. Altisini ez, zehirleri kanina islemeden."), TEXT("Yuva sustu. Su Mithril Kask'i tak; aklin basinda, basin omzunda dursun.") });
		Table.Add(FName(TEXT("Q_Warlocks")), { TEXT("Karakaya'da orklari toplayan el, bir baltanin degil bir buyucunun eli. Solgun Fisilti'ya kanip insanligini degisenleri sustur; sozlerini keser kesmez, ordu da bassiz kalir."), TEXT("") });
		return Table;
	}
}

FString FMMOQuestDialogue::GetAcceptLine(FName QuestId)
{
	const FEntry* E = BuildTable().Find(QuestId);
	return E ? FString(E->Accept) : FString();
}

FString FMMOQuestDialogue::GetTurnInLine(FName QuestId)
{
	const FEntry* E = BuildTable().Find(QuestId);
	return E ? FString(E->TurnIn) : FString();
}
