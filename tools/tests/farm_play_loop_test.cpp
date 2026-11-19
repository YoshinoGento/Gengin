#include "farm_test_edit.h"
#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmSoilSystem.h"
#include "farm/system/FarmContestJudgeSystem.h"
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmContestSeasonSystem.h"
#include <limits>
#include <cassert>
#include <cmath>
#include <iostream>
#include <array>

namespace {
void CheckPlayFlowTransitions() {
    using P = FarmPlayFlow::Phase;
    constexpr std::array phases{P::Briefing,P::Starting,P::Playing,P::Result,P::Review};
    using Input = void (*)(FarmPlayFlow&);
    constexpr std::array<Input,5> inputs{{
        [](FarmPlayFlow& f) { f.Observe(false); }, [](FarmPlayFlow& f) { f.Observe(true); },
        [](FarmPlayFlow& f) { f.Continue(); }, [](FarmPlayFlow& f) { f.BeginStart(); },
        [](FarmPlayFlow& f) { f.FinishStart(); }
    }};
    constexpr P expected[][inputs.size()] = {
        {P::Briefing,P::Result, P::Playing,P::Starting,P::Briefing},
        {P::Starting,P::Starting,P::Starting,P::Starting,P::Playing},
        {P::Playing, P::Result, P::Playing,P::Playing, P::Playing},
        {P::Briefing,P::Result, P::Review, P::Result,  P::Result},
        {P::Briefing,P::Review, P::Review, P::Review,  P::Review}
    };
    for (std::size_t i = 0; i < phases.size(); ++i) {
        FarmPlayFlow start;
        switch (phases[i]) {
        case P::Briefing: break;
        case P::Starting: start.BeginStart(); break;
        case P::Playing: start.Continue(); break;
        case P::Result: start.Observe(true); break;
        case P::Review: start.Observe(true); start.Continue(); break;
        }
        for (std::size_t j = 0; j < inputs.size(); ++j) {
            auto flow = start;
            inputs[j](flow);
            assert(flow.GetPhase() == expected[i][j] && start.GetPhase() == phases[i]);
            assert(flow.BlocksSimulation() == (flow.GetPhase() != P::Playing && flow.GetPhase() != P::Review));
            flow.RequestTitle();
            assert(flow.RequestsTitle() && flow.BlocksSimulation() && !start.RequestsTitle());
            flow.Reset();
            assert(!flow.RequestsTitle() && flow.GetPhase() == P::Briefing && flow.BlocksSimulation());
        }
    }
}
}

int main() {
    CheckPlayFlowTransitions();
    {
        for (int points = 0; points <= 300; ++points) {
            FarmEconomySystem::ContestResults records{};
            int remaining = points;
            for (std::size_t i=0; i<records.size(); ++i) {
                records[i].contestDay=FarmContestEntrySystem::kContestDays[i];
                const int score=(std::min)(remaining,100);
                records[i].qualityPoints=(std::min)(score,60);
                records[i].sizePoints=score-records[i].qualityPoints;
                remaining-=score;
            }
            const auto result=FarmContestSeasonSystem::Evaluate(30,records);
            const auto expected=points>=270 ? FarmContestRating::S : points>=240 ? FarmContestRating::A :
                points>=180 ? FarmContestRating::B : points>=120 ? FarmContestRating::C : FarmContestRating::D;
            assert(result.valid && result.finalized && result.rating==expected);
            assert(result.totalPoints==points);
            const int target=points>=270 ? points : points>=240 ? 270 : points>=180 ? 240 : points>=120 ? 180 : 120;
            assert(result.pointsToNextRating==target-points);
            assert((result.nextRating==FarmContestRating::Unrated)==(points>=270));
            assert(FarmContestSeasonSystem::Evaluate(31,records).rating==expected);
            assert(FarmContestSeasonSystem::Evaluate(29,records).rating==FarmContestRating::Unrated);
            records[0].qualityPoints=61;
            assert(FarmContestSeasonSystem::Evaluate(31,records).rating==FarmContestRating::Unrated);
        }
        FarmEconomySystem::ContestResults empty{};
        assert(FarmContestSeasonSystem::Evaluate(31,empty).rating==FarmContestRating::Unrated);
        empty[0].contestDay=10;
        assert(FarmContestSeasonSystem::Evaluate(20,empty).rating==FarmContestRating::Unrated);
        assert(FarmContestSeasonSystem::Evaluate(31,empty).rating==FarmContestRating::D);
        assert(FarmContestSeasonSystem::Evaluate(31,empty).pointsToNextRating==120);
        assert(FarmContestSeasonSystem::Evaluate(0,empty).rating==FarmContestRating::Unrated);
        std::cout << "PASS: all301 rating totals, next thresholds, no entry versus zero points, pending/invalid and expiry\n";
    }
    {
        FarmProgressionSystem progress;
        FarmEconomySystem::ContestResults records{};
        const auto during=FarmContestSeasonSystem::Evaluate(30,records);
        progress.Initialize({},FarmProgressionMode::ContestSeason);
        assert(progress.GetProgress(2147483647, 1) == 0.0f);
        assert(progress.GetProgress(2147483647, (std::numeric_limits<int>::min)()) == 0.0f);
        assert(std::abs(progress.GetProgress(0, 30) - 29.0f / 30.0f) < 0.0001f);
        assert(progress.GetProgress(0, (std::numeric_limits<int>::max)()) == 1.0f);
        assert(!progress.EvaluateClear(2147483647) && !progress.IsCleared());
        assert(!progress.EvaluateSeason(during));
        const auto expired=FarmContestSeasonSystem::Evaluate(31,records);
        assert(progress.EvaluateSeason(expired) && progress.IsCleared());
        assert(progress.GetProgress(0, 30) == 1.0f);
        assert(!progress.EvaluateSeason(expired));
        const auto saved=progress.CaptureSnapshot();
        assert(progress.SetMode(FarmProgressionMode::Trial,300,during) && !progress.IsCleared());
        assert(progress.GetProgress(270, 30) == 0.5f);
        assert(progress.EvaluateClear(progress.GetTargetMoney()));
        assert(progress.SetMode(FarmProgressionMode::ContestSeason,999999,during) && !progress.IsCleared());
        assert(progress.RestoreSnapshot(saved) && progress.IsContestSeason() && progress.IsCleared());
        auto bad=saved; bad.mode=static_cast<FarmProgressionMode>(99);
        assert(!progress.RestoreSnapshot(bad));
        assert(!progress.SetMode(bad.mode,300,during));
        assert(!progress.SetMode(FarmProgressionMode::Trial,-1,during));
        assert(!progress.SetMode(FarmProgressionMode::Trial,300,{}));
        assert(progress.IsContestSeason() && progress.IsCleared());
        records[2].contestDay=30;
        assert(progress.SetMode(FarmProgressionMode::ContestSeason,0,FarmContestSeasonSystem::Evaluate(30,records)));
        assert(progress.IsCleared());
        progress.Initialize({},progress.GetMode());
        assert(progress.IsContestSeason() && !progress.IsCleared());
        std::cout << "PASS: trial/season completion, mode changes, zero-score final submission, reset and invalid modes\n";
    }
    {
        using Season = FarmContestSeasonSystem;
        using Event = FarmContestEventStatus;
        FarmEconomySystem::ContestResults results{};
        for (int day : {1,9,10,11,19,20,21,29,30,31,2147483647}) {
            const auto summary = Season::Evaluate(day,results);
            assert(summary.valid && summary.submitted==0 && summary.totalPoints==0);
            int missed=0;
            for (std::size_t i=0; i<results.size(); ++i) {
                const int eventDay=FarmContestEntrySystem::kContestDays[i];
                assert(summary.events[i]==(day<eventDay ? Event::Upcoming : day==eventDay ? Event::Open : Event::Missed));
                if(day>eventDay) ++missed;
            }
            assert(summary.missed==missed && summary.finalized==(day>30));
        }
        for(int day : {-2147483647, -1, 0}) assert(!Season::Evaluate(day,results).valid);
        for(unsigned mask=0; mask<8; ++mask) {
            results={}; int count=0;
            for(std::size_t i=0; i<results.size(); ++i) if(mask&(1u<<i)) {
                results[i].contestDay=FarmContestEntrySystem::kContestDays[i];
                results[i].qualityPoints=60; results[i].sizePoints=40; ++count;
            }
            for(int repeat=0; repeat<100; ++repeat) {
                const auto at30=Season::Evaluate(30,results);
                assert(at30.submitted==count && at30.totalPoints==count*100);
                assert(at30.finalized==((mask&4)!=0));
                const auto expired=Season::Evaluate(31,results);
                assert(expired.finalized && expired.missed==3-count);
            }
        }
        assert(!Season::Evaluate(29,results).valid); // A future saved result cannot contribute.
        auto bad=results;
        for(int mode=0; mode<6; ++mode) {
            bad=results;
            if(mode==0) bad[0].contestDay=20;
            if(mode==1) bad[0].qualityPoints=-1;
            if(mode==2) bad[0].qualityPoints=61;
            if(mode==3) bad[0].sizePoints=-1;
            if(mode==4) bad[0].sizePoints=41;
            if(mode==5) bad[0].qualityPoints=2147483647;
            const auto invalid=Season::Evaluate(31,bad);
            assert(!invalid.valid && invalid.totalPoints==0 && !invalid.finalized);
        }
        results={};
        results[1].contestDay=20; // A genuine submitted zero remains distinct from a missed event.
        const auto zero=Season::Evaluate(21,results);
        assert(zero.submitted==1 && zero.missed==1 && zero.totalPoints==0 && !zero.finalized);
        assert(zero.events[1]==Event::Submitted);
        assert(Season::Evaluate(20,results).missed==1);
        assert(results[1].contestDay==20 && results[0].contestDay==0);
    }
    {
        using Submit = FarmContestSubmissionSystem;
        using Status = FarmContestSubmissionStatus;
        FarmEconomySystem economy; economy.Initialize();
        FarmToolActionSystem actions; actions.Initialize();
        farm::FarmGrid grid; assert(grid.Initialize(2,1));
        FarmCropQualityResult quality; quality.crop=farm::CropType::Carrot;
        quality.basePrice=170; quality.salePrice=240; quality.score=90; quality.harvestSize={1.5f,true};
        for (int day : {10,20,30}) {
            assert(economy.AddHarvest(quality,1,day-1));
            const int id=economy.GetHarvestRecord(0)->id;
            assert(economy.SetHarvestProtection(id,true,economy.GetInventoryGeneration()));
            assert(economy.SetContestReservation(id,true,economy.GetInventoryGeneration()));
            const auto generation=economy.GetInventoryGeneration();
            const int money=economy.GetMoney();
            assert(Submit::Evaluate(economy,day)==Status::Ready);
            assert(!Submit::Submit(economy,day-1,id,generation));
            assert(!Submit::Submit(economy,day,id+1,generation));
            assert(!Submit::Submit(economy,day,id,generation+1));
            assert(economy.GetInventoryGeneration()==generation && economy.GetHarvestRecordCount()==1);
            assert(grid.Initialize(2,1));
            assert(actions.RaiseSelectedTile(grid));
            assert(actions.CommitContestSubmission(economy,day,id,generation));
            assert(!actions.Undo() && !actions.Redo());
            assert(economy.GetHarvestRecordCount()==0 && economy.GetTotalCropCount()==0);
            assert(economy.GetContestReservationId()==0 && economy.GetMoney()==money);
            const auto& result=economy.GetContestResults()[day/10-1];
            assert(result.contestDay==day && result.harvest.id==id);
            assert(result.qualityPoints==54 && result.sizePoints==27);
            assert(Submit::Evaluate(economy,day)==Status::AlreadySubmitted);
            assert(!Submit::Submit(economy,day,id,generation));
        }
        auto saved=economy.CaptureSnapshot();
        assert(economy.RestoreSnapshot(saved));
        const auto season=FarmContestSeasonSystem::Evaluate(30,economy.GetContestResults());
        assert(season.valid && season.finalized && season.submitted==3 && season.totalPoints==243);
        assert(Submit::Evaluate(economy,30)==Status::AlreadySubmitted);
        for(int mode=0; mode<5; ++mode) {
            auto bad=saved;
            if(mode==0) bad.contestResults[0].contestDay=20;
            if(mode==1) bad.contestResults[0].harvest.harvestedDay=11;
            if(mode==2) bad.contestResults[0].qualityPoints=0;
            if(mode==3) bad.contestResults[1].harvest.id=bad.contestResults[0].harvest.id;
            if(mode==4) bad.contestResults[0].harvest.id=bad.nextHarvestRecordId;
            const auto generation=economy.GetInventoryGeneration();
            assert(!economy.RestoreSnapshot(bad));
            assert(economy.GetInventoryGeneration()==generation);
        }
        assert(economy.AddHarvest(quality,1,29));
        const int id=economy.GetHarvestRecord(0)->id;
        assert(economy.SetHarvestProtection(id,true,economy.GetInventoryGeneration()));
        assert(economy.SetContestReservation(id,true,economy.GetInventoryGeneration()));
        assert(!Submit::Submit(economy,30,id,economy.GetInventoryGeneration()));
        assert(economy.GetHarvestRecordCount()==1);
        economy.Initialize();
        for (const auto& result : economy.GetContestResults()) assert(result.contestDay==0);
        assert(Submit::Evaluate(economy,10)==Status::Ineligible);
        std::cout << "PASS: contest consumption, one result/event, stale requests, undo boundary and reset\n";
    }
    {
        using Entry = FarmContestEntrySystem;
        using Issue = FarmContestEntryIssue;
        FarmEconomySystem economy; economy.Initialize();
        FarmCropQualityResult quality; quality.crop=farm::CropType::Carrot;
        quality.basePrice=170; quality.salePrice=240; quality.score=90; quality.harvestSize={1.5f,true};
        assert(economy.AddHarvest(quality,1,10));
        const auto generation= economy.GetInventoryGeneration();
        assert(economy.SetHarvestProtection(1,true,generation));
        assert(economy.SetContestReservation(1,true,generation));
        auto record=*economy.GetContestReservation();
        for (int today : {-2147483647-1, -1, 0, 1, 9, 10, 11, 19, 20, 21, 29, 30, 31, 2147483647}) {
            const auto empty=Entry::Evaluate(today,nullptr);
            if(today<1) { assert(empty.issue==Issue::InvalidDay && !empty.HasContest()); continue; }
            if(today>30) { assert(empty.issue==Issue::SeasonEnded && !empty.HasContest()); continue; }
            const int end=today<=10 ? 10 : today<=20 ? 20 : 30;
            assert(empty.issue==Issue::NoReservation && empty.contestDay==end);
            assert(empty.firstHarvestDay==end-9 && empty.daysRemaining==end-today);
            for (int harvest : {-1,0,1,9,10,11,19,20,21,29,30,31,2147483647}) {
                record.harvestedDay=harvest;
                const auto result=Entry::Evaluate(today,&record);
                const auto expected=harvest<0 ? Issue::InvalidRecord : harvest==0 ? Issue::UnknownHarvestDay
                    : harvest>today ? Issue::FutureHarvestDay : harvest<end-9 ? Issue::OutsidePeriod : Issue::Eligible;
                assert(result.issue==expected && result.contestDay==end);
            }
        }
        record.harvestedDay=10; record.quantity=2;
        assert(Entry::Evaluate(10,&record).issue==Issue::InvalidRecord);
        record.quantity=1; record.saleProtected=false;
        assert(Entry::Evaluate(10,&record).issue==Issue::InvalidRecord);
        const auto before=economy.CaptureSnapshot();
        for(int repeat=0;repeat<100;++repeat) {
            assert(Entry::Evaluate(10,economy.GetContestReservation()).issue==Issue::Eligible);
            assert(Entry::Evaluate(11,economy.GetContestReservation()).issue==Issue::OutsidePeriod);
        }
        assert(economy.GetMoney()==before.money && economy.GetInventoryGeneration()==generation);
        assert(economy.GetContestReservationId()==1 && economy.GetHarvestRecordCount()==1);
        assert(economy.GetContestReservation()->harvestedDay==10 && economy.GetContestReservation()->saleProtected);
        assert(economy.RestoreSnapshot(before));
        assert(Entry::Evaluate(10,economy.GetContestReservation()).issue==Issue::Eligible);
        std::cout << "PASS: contest period boundaries, unknown/future dates, invalid records, repeat/restore read-only\n";
    }
    {
        farm::FarmGrid grid; assert(grid.Initialize(1,1));
        FarmEconomySystem economy; economy.Initialize();
        FarmToolActionSystem actions; actions.Initialize();
        farm::FarmTile ready; ready.crop=farm::CropType::Carrot; ready.state=farm::FarmTileState::Planted; ready.growth=1;
        for(int day : {0,1,10,11,20,21,30,2147483647}) {
            assert(grid.SetTile(0,ready));
            const auto result=actions.ApplyToolDetailed(grid,FarmTool::Harvest,ready.crop,economy,day);
            assert(result.Succeeded() && economy.GetHarvestRecord(0)->harvestedDay==day);
            assert(actions.Undo() && economy.GetHarvestRecordCount()==0);
            assert(actions.Redo() && economy.GetHarvestRecord(0)->harvestedDay==day);
            assert(economy.SellAll().Succeeded()); actions.ClearHistory();
        }
        assert(grid.SetTile(0,ready));
        assert(!actions.ApplyToolDetailed(grid,FarmTool::Harvest,ready.crop,economy,-1).Succeeded());
        assert(farm::IsHarvestReady(*grid.GetTile(0)) && economy.GetHarvestRecordCount()==0);
        auto quality=actions.EvaluateHarvestQuality(ready);
        assert(!economy.AddHarvest(quality,1,-1));
        assert(economy.AddHarvest(quality,1,10));
        auto saved=economy.CaptureSnapshot(); saved.harvestRecords[0].harvestedDay=-1;
        assert(!economy.RestoreSnapshot(saved) && economy.GetHarvestRecord(0)->harvestedDay==10);
        std::cout << "PASS: harvest dates, original-date Undo/Redo, sale, invalid day atomicity\n";
    }
	{
		using Judge = FarmContestJudgeSystem;
		assert(Judge::Evaluate(nullptr).issue == FarmContestJudgeIssue::NoReservation);
		FarmEconomySystem::HarvestRecord record;
		record.id=1; record.quantity=1; record.saleProtected=true;
		record.quality.crop=farm::CropType::Carrot;
		record.quality.basePrice=170; record.quality.salePrice=240;
		record.quality.nutrientKnown=true;
		for (int score : {0, 1, 50, 93, 100}) for (float size : {0.01f, 0.5f, 1.0f, 1.46f, 2.f, 8.f}) {
			record.quality.score=score; record.quality.harvestSize={size,true};
			const auto result=Judge::Evaluate(&record);
			assert(result.IsValid() && result.recordId==1 && !result.partialQuality);
			assert(result.qualityPoints==static_cast<int>(std::lround(score*0.6)));
			assert(result.sizePoints>=0 && result.sizePoints<=40 && result.totalPoints<=100);
			assert(result.totalPoints==result.qualityPoints+result.sizePoints);
			if(size<=0.5f) assert(result.sizePoints==0);
			if(size>=2.f) assert(result.sizePoints==40);
		}
		record.quality.score=93; record.quality.harvestSize={1.46f,true};
		assert(Judge::Evaluate(&record).totalPoints==82);
		for (int mode=0;mode<10;++mode) {
			auto invalid=record;
			if(mode==0) invalid.id=0;
			if(mode==1) invalid.quantity=2;
			if(mode==2) invalid.saleProtected=false;
			if(mode==3) invalid.quality.score=101;
			if(mode==4) invalid.quality.harvestSize={};
			if(mode==5) invalid.quality.harvestSize.multiplier=std::numeric_limits<float>::quiet_NaN();
			if(mode==6) invalid.quality.waterBalance=std::numeric_limits<float>::infinity();
			if(mode==7) invalid.quality.crop=farm::CropType::None;
			if(mode==8) invalid.quality.score=-1;
			if(mode==9) invalid.quality.salePrice=0;
			assert(Judge::Evaluate(&invalid).issue==FarmContestJudgeIssue::InvalidRecord);
		}
		for (int mode=0;mode<7;++mode) {
			FarmContestJudgeRules rules;
			if(mode==0) rules.qualityPoints=std::numeric_limits<int>::max();
			if(mode==1) rules.sizePoints=-1;
			if(mode==2) rules.qualityPoints=50;
			if(mode==3) rules.sizeMinimum=rules.sizeMaximum;
			if(mode==4) rules.sizeMinimum=0;
			if(mode==5) rules.sizeMaximum=std::numeric_limits<float>::infinity();
			if(mode==6) rules.sizeMinimum=std::numeric_limits<float>::quiet_NaN();
			assert(Judge::Evaluate(&record,rules).issue==FarmContestJudgeIssue::InvalidRules);
		}
		assert(Judge::Evaluate(&record,{100,0,0.5f,2.f}).totalPoints==93);
		FarmEconomySystem economy; economy.Initialize();
		assert(economy.AddHarvest(record.quality));
		const auto gen=economy.GetInventoryGeneration();
		assert(economy.SetHarvestProtection(1,true,gen) && economy.SetContestReservation(1,true,gen));
		const auto before=economy.CaptureSnapshot();
		for(int i=0;i<100;++i) assert(Judge::Evaluate(economy.GetContestReservation()).totalPoints==82);
		assert(economy.GetInventoryGeneration()==gen && economy.GetMoney()==before.money);
		assert(economy.GetHarvestRecordCount()==1 && economy.GetContestReservationId()==1);
		assert(economy.SetContestReservation(1,false,gen));
		assert(!Judge::Evaluate(economy.GetContestReservation()).IsValid());
		assert(economy.RestoreSnapshot(before));
		assert(Judge::Evaluate(economy.GetContestReservation()).totalPoints==82);
		auto next=record.quality; next.crop=farm::CropType::TestCrop; next.score=100; next.harvestSize={2.f,true};
		next.nutrientKnown=false;
		assert(economy.AddHarvest(next));
		const auto currentGen=economy.GetInventoryGeneration();
		assert(economy.SetHarvestProtection(2,true,currentGen) && economy.SetContestReservation(2,true,currentGen));
		const auto replacement=Judge::Evaluate(economy.GetContestReservation());
		assert(replacement.totalPoints==100 && replacement.crop==farm::CropType::TestCrop && replacement.partialQuality);
		assert(economy.GetHarvestRecord(0)->saleProtected && economy.GetHarvestRecordCount()==2);
		std::cout << "PASS: judging preview bounds, invalid inputs/rules, deterministic contributions, reservation replacement/cancel/restore\n";
	}
	{
		FarmEconomySystem economy; economy.Initialize();
		FarmCropQualityResult q; q.crop=farm::CropType::Carrot; q.basePrice=170; q.salePrice=240;
		q.score=90; q.harvestSize={1.5f,true};
		assert(!economy.GetContestReservation());
		assert(economy.AddHarvest(q) && economy.AddHarvest(q) && economy.AddHarvest(q,2));
		const auto gen=economy.GetInventoryGeneration();
		const int first=economy.GetHarvestRecord(0)->id, second=economy.GetHarvestRecord(1)->id;
		assert(!economy.SetContestReservation(first,true,gen));
		assert(economy.SetHarvestProtection(first,true,gen));
		assert(economy.SetHarvestProtection(second,true,gen));
		assert(economy.SetHarvestProtection(economy.GetHarvestRecord(2)->id,true,gen));
		assert(!economy.SetContestReservation(economy.GetHarvestRecord(2)->id,true,gen));
		assert(economy.SetContestReservation(first,true,gen));
		assert(!economy.SetContestReservation(first,true,gen));
		assert(!economy.SetHarvestProtection(first,false,gen));
		assert(!economy.SellCrop(q.crop).Succeeded() && !economy.SellAll().Succeeded());
		assert(economy.SetContestReservation(second,true,gen));
		assert(economy.GetContestReservationId()==second && economy.GetHarvestRecord(0)->saleProtected);
		assert(!economy.SetContestReservation(first,false,gen));
		assert(!economy.SetContestReservation(0,true,gen) && !economy.SetContestReservation(-1,false,gen));
		assert(!economy.SetContestReservation(999,true,gen));
		assert(economy.SetHarvestProtection(first,false,gen));
		assert(economy.SellAll().soldCount==1);
		assert(economy.GetContestReservationId()==second && economy.GetContestReservation()->quality.salePrice==240);
		assert(economy.GetContestReservation()->id==economy.GetHarvestRecord(0)->id);
		const auto saved=economy.CaptureSnapshot();
		assert(FarmEconomySystem::ValidateSnapshot(saved));
		assert(economy.RestoreSnapshot(saved));
		assert(!economy.SetContestReservation(second,false,gen));
		assert(economy.SetContestReservation(second,false,economy.GetInventoryGeneration()));
		assert(!economy.GetContestReservation() && economy.GetHarvestRecord(0)->saleProtected);
		for (int mode=0; mode<5; ++mode) {
			auto bad=saved;
			if(mode==0) bad.contestReservationId=-1;
			if(mode==1) bad.contestReservationId=999;
			if(mode==2) bad.harvestRecords[0].saleProtected=false;
			if(mode==3) bad.harvestRecords[0].quality.harvestSize={};
			if(mode==4) bad.contestReservationId=bad.harvestRecords[1].id;
			const auto generation=economy.GetInventoryGeneration();
			assert(!economy.RestoreSnapshot(bad) && economy.GetInventoryGeneration()==generation);
			assert(!economy.GetContestReservation());
		}
		economy.Initialize();
		auto unknown=q; unknown.harvestSize={}; assert(economy.AddHarvest(unknown));
		assert(economy.SetHarvestProtection(economy.GetHarvestRecord(0)->id,true,economy.GetInventoryGeneration()));
		assert(!economy.SetContestReservation(economy.GetHarvestRecord(0)->id,true,economy.GetInventoryGeneration()));
		assert(economy.AddHarvest(q));
		const int id=economy.GetHarvestRecord(1)->id;
		assert(economy.SetHarvestProtection(id,true,economy.GetInventoryGeneration()));
		farm::FarmGrid grid; assert(grid.Initialize(1,1) && grid.SelectTile(0));
		FarmToolActionSystem actions; actions.Initialize();
		assert(actions.RaiseSelectedTile(grid));
		assert(!actions.CommitContestReservation(economy,999,true,economy.GetInventoryGeneration()));
		assert(actions.GetHistory().CanUndo());
		assert(actions.CommitContestReservation(economy,id,true,economy.GetInventoryGeneration()));
		assert(!actions.Undo() && !economy.RemoveHarvest(q));
		assert(actions.CommitContestReservation(economy,id,false,economy.GetInventoryGeneration()));
		assert(economy.SetHarvestProtection(id,false,economy.GetInventoryGeneration()));
		assert(economy.SellAll().soldCount==1);
		economy.Initialize(); assert(economy.GetContestReservationId()==0);
		std::cout << "PASS: contest reservation selection/replacement/cancel, sale retention, stale IDs, snapshots and history\n";
	}
	{
		FarmCropQualityResult quality; quality.crop = farm::CropType::TestCrop;
		quality.basePrice = 120; quality.salePrice = 180; quality.score = 80;
		quality.harvestSize = {1.5f, true};
		for (int mask = 0; mask < 8; ++mask) for (bool all : {false, true}) {
			FarmEconomySystem economy; economy.Initialize();
			int ids[3]{};
			for (int i=0; i<3; ++i) {
				auto q = quality; if (i==2) q.crop = farm::CropType::Carrot;
				assert(economy.AddHarvest(q, i+1)); ids[i] = economy.GetHarvestRecord(i)->id;
				if (mask & (1<<i)) assert(economy.SetHarvestProtection(ids[i], true, economy.GetInventoryGeneration()));
			}
			int expected = 0, remaining = 0;
			for (int i=0; i<3; ++i) {
				if ((all || i<2) && !(mask & (1<<i))) expected += i+1;
				else remaining += i+1;
			}
			const auto preview = economy.PreviewSale(all ? farm::CropType::None : quality.crop);
			const int money = economy.GetMoney();
			const auto sold = all ? economy.SellAll() : economy.SellCrop(quality.crop);
			assert(sold.soldCount == expected && sold.earnedMoney == expected*180);
			assert(preview.soldCount == sold.soldCount && preview.earnedMoney == sold.earnedMoney);
			assert(economy.GetMoney() == money+expected*180 && economy.GetTotalCropCount() == remaining);
			for (std::size_t i=0; i<economy.GetHarvestRecordCount(); ++i) {
				const auto* r = economy.GetHarvestRecord(i);
				assert(r->saleProtected || (!all && r->quality.crop == farm::CropType::Carrot));
				assert(r->id == ids[r->quantity-1]);
			}
			assert(FarmEconomySystem::ValidateSnapshot(economy.CaptureSnapshot()));
		}
		FarmEconomySystem economy; economy.Initialize();
		assert(economy.AddHarvest(quality));
		const int id = economy.GetHarvestRecord(0)->id;
		const auto generation = economy.GetInventoryGeneration();
		assert(economy.SetHarvestProtection(id, true, generation));
		assert(!economy.SetHarvestProtection(id, true, generation));
		assert(!economy.SetHarvestProtection(-1, true, generation));
		assert(!economy.RemoveHarvest(quality));
		assert(!economy.SellAll().Succeeded() && economy.GetProtectedCropCount()==1);
		assert(economy.AddHarvest(quality.crop, 2));
		assert(economy.SellAll().soldCount==2 && economy.GetProtectedCropCount()==1);
		const auto saved = economy.CaptureSnapshot();
		assert(economy.RestoreSnapshot(saved));
		assert(!economy.SetHarvestProtection(id, false, generation));
		assert(economy.SetHarvestProtection(id, false, economy.GetInventoryGeneration()));
		assert(economy.RemoveHarvest(quality) && economy.AddHarvest(quality));
		assert(economy.GetHarvestRecord(0)->id != id);
		assert(!economy.SetHarvestProtection(id, true, economy.GetInventoryGeneration()));
		auto bad = economy.CaptureSnapshot(); bad.nextHarvestRecordId = std::numeric_limits<int>::max();
		assert(economy.RestoreSnapshot(bad) && !economy.AddHarvest(quality));
		bad = saved; bad.harvestRecords[1] = bad.harvestRecords[0]; bad.harvestRecordCount = 2;
		bad.cropCounts[0] = 2; bad.cropValues[0] = 360;
		assert(!economy.RestoreSnapshot(bad));
		bad = saved; bad.money = std::numeric_limits<int>::max();
		assert(economy.RestoreSnapshot(bad) && economy.AddHarvest(quality));
		assert(!economy.SellAll().Succeeded() && economy.GetHarvestRecordCount()==2);
		assert(economy.GetHarvestRecord(0)->saleProtected);
		economy.Initialize();
		farm::FarmGrid grid; assert(grid.Initialize(1,1));
		FarmToolActionSystem actions; actions.Initialize();
		farm::FarmTile tile; tile.crop=quality.crop; tile.state=farm::FarmTileState::Planted; tile.growth=1;
		assert(grid.SetTile(0,tile) && grid.SelectTile(0));
		assert(actions.ApplyTool(grid,FarmTool::Harvest,quality.crop,economy));
		const int undoId=economy.GetHarvestRecord(0)->id;
		assert(actions.Undo() && actions.Redo());
		const int redoId=economy.GetHarvestRecord(0)->id; assert(redoId!=undoId);
		assert(!actions.CommitHarvestProtection(economy,undoId,true,economy.GetInventoryGeneration()));
		assert(actions.GetHistory().CanUndo());
		assert(actions.CommitHarvestProtection(economy,redoId,true,economy.GetInventoryGeneration()));
		assert(!actions.Undo() && !actions.Redo() && economy.GetProtectedCropCount()==1);
		std::cout << "PASS: protection sale matrix, stable identity, stale requests, overflow, history commitment\n";
	}
	{
		FarmEconomySystem economy; economy.Initialize();
		FarmCropQualityResult a; a.crop = farm::CropType::TestCrop;
		a.basePrice = 120; a.salePrice = 180; a.score = 80; a.harvestSize = {1.5f, true};
		a.maturity = a.waterBalance = a.terrainFit = a.nutrientBalance = 1; a.nutrientKnown = true;
		auto b = a; b.crop = farm::CropType::Carrot; b.harvestSize = {1.25f, true};
		assert(economy.AddHarvest(a, 2) && economy.AddHarvest(b));
		assert(economy.GetHarvestRecordCount() == 2 && economy.GetUnrecordedCropCount() == 0);
		assert(!economy.GetHarvestRecord(2) && !economy.GetHarvestRecord(std::numeric_limits<std::size_t>::max()));
		assert(!economy.RemoveHarvest(a));
		assert(!economy.RemoveHarvest(a.crop));
		assert(economy.RemoveHarvest(b, 1, a));
		assert(economy.GetHarvestRecordCount() == 1 && economy.GetLastHarvestQuality().crop == a.crop);
		assert(economy.AddHarvest(b));
		auto fullMoney = economy.CaptureSnapshot(); fullMoney.money = std::numeric_limits<int>::max();
		assert(economy.RestoreSnapshot(fullMoney));
		assert(!economy.SellAll().Succeeded() && !economy.SellCrop(a.crop).Succeeded());
		assert(economy.GetHarvestRecordCount() == 2 && economy.GetTotalCropCount() == 3);
		fullMoney.money = 0; assert(economy.RestoreSnapshot(fullMoney));
		assert(economy.SellCrop(a.crop).soldCount == 2);
		assert(economy.GetHarvestRecordCount() == 1 && economy.GetHarvestRecord(0)->quality.crop == b.crop);
		assert(economy.GetLastHarvestQuality().crop == b.crop);
		assert(economy.AddHarvest(a.crop, 3) && economy.GetUnrecordedCropCount() == 3);
		assert(economy.RemoveHarvest(a.crop) && economy.GetUnrecordedCropCount() == 2);
		assert(economy.SellAll().soldCount == 3 && economy.GetHarvestRecordCount() == 0);
		assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
		auto legacy = economy.CaptureSnapshot(); legacy.cropCounts[0] = 1; legacy.cropValues[0] = 181;
		assert(economy.RestoreSnapshot(legacy));
		assert(!economy.RemoveHarvest(a.crop));
		assert(economy.GetCropCount(a.crop) == 1 && economy.GetCropInventoryValue(a.crop) == 181);
		assert(economy.SellAll().Succeeded());
		for (std::size_t i = 0; i < FarmEconomySystem::kMaxHarvestRecords; ++i) assert(economy.AddHarvest(a));
		assert(!economy.AddHarvest(b));
		farm::FarmGrid grid; assert(grid.Initialize(1,1));
		farm::FarmTile tile; tile.state = farm::FarmTileState::Planted; tile.crop = a.crop; tile.growth = 1;
		assert(grid.SetTile(0, tile));
		FarmToolActionSystem actions; actions.Initialize();
		assert(actions.EvaluateTool(grid, FarmTool::Harvest, a.crop, &economy).status == FarmToolActionStatus::InventoryFull);
		assert(!actions.ApplyTool(grid, FarmTool::Harvest, a.crop, economy));
		assert(grid.GetTile(0)->crop == a.crop);
		assert(economy.RemoveHarvest(a) && economy.AddHarvest(b));
		auto bad = economy.CaptureSnapshot(); ++bad.harvestRecords[0].quantity;
		assert(!economy.RestoreSnapshot(bad));
		bad = economy.CaptureSnapshot(); bad.harvestRecordCount = 129;
		assert(!economy.RestoreSnapshot(bad));
		for (float invalid : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN()}) {
			auto quality = a; quality.maturity = invalid;
			assert(!FarmEconomySystem::IsRecordedQualityValid(quality));
		}
		std::cout << "PASS: harvest records, legacy stock, LIFO removal, failed/type/all sale, capacity and snapshot guards\n";
	}
	{
		FarmCropSizeSystem size;
		farm::FarmTile tile;
		tile.crop = farm::CropType::TestCrop; tile.growth = 0.5f;
		assert(!size.Evaluate(tile).known);
		tile.careHistory.goodSeconds = 10;
		tile.careHistory.efficiencySeconds = 10;
		tile.careHistory.nutrientGrowth = 0.5f;
		tile.careHistory.nutrientSupply = 0.5f;
		assert(size.Evaluate(tile).multiplier == 2);
		auto poor = tile; poor.careHistory.efficiencySeconds = 4;
		assert(size.Evaluate(poor).multiplier < size.Evaluate(tile).multiplier);
		poor.careHistory.nutrientSupply = 0.1f;
		const auto before = size.Evaluate(poor);
		poor.moisture = 1; poor.soilNutrients = 1; poor.heightLevel = 2;
		assert(size.Evaluate(poor).multiplier == before.multiplier);
		poor.careHistory.nutrientSupply = 0;
		assert(size.Evaluate(poor).multiplier == 0.5f);
		for (float invalid : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN()}) {
			auto bad = tile; bad.growth = invalid;
			assert(!size.Evaluate(bad).known);
		}
		poor.careHistory.efficiencySeconds = std::numeric_limits<float>::infinity();
		assert(!size.Evaluate(poor).known);
		farm::FarmRules rules; rules.minimumCropSizeMultiplier = 3; rules.maximumCropSizeMultiplier = 1;
		size.Initialize(rules); assert(size.Evaluate(tile).multiplier == 3);
		rules.minimumCropSizeMultiplier = -1; rules.maximumCropSizeMultiplier = 99;
		size.Initialize(rules); assert(size.Evaluate(tile).multiplier == 2);
		std::cout << "PASS: size forecast history, care differences, invalid inputs and rule limits\n";
	}
	{
		assert(FarmCropQualitySystem::Analyze({}).focus == FarmQualityFocus::None);
		FarmCropQualityResult full;
		full.crop = farm::CropType::Carrot; full.basePrice = 170; full.salePrice = 250;
		full.maturity = full.waterBalance = full.terrainFit = full.nutrientBalance = 1;
		full.nutrientKnown = true;
		assert(FarmCropQualitySystem::Analyze(full).focus == FarmQualityFocus::Balanced);
		constexpr std::array<FarmQualityFocus,4> focus{FarmQualityFocus::Maturity, FarmQualityFocus::Water,
			FarmQualityFocus::Terrain, FarmQualityFocus::Nutrients};
		for (std::size_t i=0; i<focus.size(); ++i) {
			auto low = full;
			std::array<float*,4> axes{&low.maturity, &low.waterBalance, &low.terrainFit, &low.nutrientBalance};
			*axes[i] = 0.58f;
			const auto advice = FarmCropQualitySystem::Analyze(low);
			assert(advice.focus == focus[i] && advice.lowestPercent == 58 && !advice.partial);
			assert(low.salePrice == 250 && *axes[i] == 0.58f);
		}
		auto tied = full; tied.waterBalance = 0.584f; tied.nutrientBalance = 0.581f;
		assert(FarmCropQualitySystem::Analyze(tied).focus == FarmQualityFocus::Water);
		auto partial = full; partial.nutrientKnown = false; partial.nutrientBalance = 0;
		assert(FarmCropQualitySystem::Analyze(partial).focus == FarmQualityFocus::Unknown);
		partial.waterBalance = 0.4f;
		assert(FarmCropQualitySystem::Analyze(partial).focus == FarmQualityFocus::Water);
		assert(FarmCropQualitySystem::Analyze(partial).partial);
		for (float invalid : {-1.0f, 2.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
			auto bad = full; bad.maturity = invalid;
			assert(FarmCropQualitySystem::Analyze(bad).focus == FarmQualityFocus::Unknown);
			assert(FarmCropQualitySystem::Analyze(bad).partial);
		}
		full.waterBalance = 0.999f;
		assert(FarmCropQualitySystem::Analyze(full).focus == FarmQualityFocus::Balanced);
		std::cout << "PASS: quality hints, displayed ties, unknown/invalid axes and read-only results\n";
	}
	{
		FarmCropQualitySystem quality;
		quality.Initialize();
		for (auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot}) {
			farm::FarmTile full;
			full.state = farm::FarmTileState::Planted; full.crop = crop; full.growth = 1;
			full.soilNutrients = 1;
			auto split = full;
			FarmSoilSystem::Grow(full, 1);
			for (int i = 0; i < 100; ++i) FarmSoilSystem::Grow(split, 0.01f);
			assert(std::fabs(full.soilNutrients - split.soilNutrients) < 0.0001f);
			assert(std::fabs(full.careHistory.nutrientSupply - split.careHistory.nutrientSupply) < 0.0001f);
			auto poor = full; poor.careHistory = {}; poor.soilNutrients = 0.1f;
			FarmSoilSystem::Grow(poor, 1);
			assert(poor.soilNutrients == 0 && quality.Evaluate(poor).score < quality.Evaluate(full).score);
			assert(quality.Evaluate(poor).salePrice < quality.Evaluate(full).salePrice);
			assert(!FarmSoilSystem::ApplyCompost(poor));
		}
		assert(FarmSoilSystem::Satisfaction(0.5f, farm::CropType::TestCrop) >
			FarmSoilSystem::Satisfaction(0.5f, farm::CropType::Carrot));
		assert(FarmSoilSystem::Profile(static_cast<farm::CropType>(99)) == nullptr);
		farm::FarmGrid grid; assert(grid.Initialize(1, 1));
		FarmToolActionSystem tools; tools.Initialize();
		assert(!tools.CompostSelectedTile(grid));
		auto soil = *grid.GetSelectedTile(); soil.state = farm::FarmTileState::Tilled;
		assert(grid.SetTile(0, soil));
		assert(tools.CompostSelectedTile(grid));
		assert(grid.GetSelectedTile()->soilNutrients == 1);
		assert(!tools.CompostSelectedTile(grid));
		assert(tools.Undo() && grid.GetSelectedTile()->soilNutrients == soil.soilNutrients);
		assert(tools.Redo() && grid.GetSelectedTile()->soilNutrients == 1);
		soil.soilNutrients = std::numeric_limits<float>::quiet_NaN();
		assert(!grid.SetTile(0, soil));
		std::cout << "PASS: soil crop profiles, timestep independence, shortage quality and compost undo/redo\n";
	}
	{
		FarmCropQualitySystem quality;
		quality.Initialize();
		farm::FarmTile wellManaged;
		wellManaged.state = farm::FarmTileState::Planted;
		wellManaged.crop = farm::CropType::TestCrop;
		wellManaged.growth = 1.0f;
		wellManaged.moisture = 0.0f;
		wellManaged.careHistory.goodSeconds = 8.0f;
		wellManaged.careHistory.efficiencySeconds = 8.0f;
		farm::FarmTile poorlyManaged = wellManaged;
		poorlyManaged.careHistory = {};
		poorlyManaged.careHistory.drySeconds = 8.0f;
		poorlyManaged.careHistory.efficiencySeconds = 3.2f;
		const auto goodQuality = quality.Evaluate(wellManaged);
		const auto poorQuality = quality.Evaluate(poorlyManaged);
		assert(goodQuality.IsValid() && poorQuality.IsValid());
		assert(std::fabs(goodQuality.waterBalance - 1.0f) < 0.0001f);
		assert(std::fabs(poorQuality.waterBalance - 0.4f) < 0.0001f);
		assert(goodQuality.score > poorQuality.score &&
			goodQuality.salePrice > poorQuality.salePrice);
	}
	{
		farm::FarmGrid grid;
		FarmEconomySystem economy;
		FarmToolActionSystem actions;
		FarmGrowthSystem growth;
		assert(grid.Initialize(1, 1));
		economy.Initialize();
		actions.Initialize();
		growth.Initialize();
		const auto crop = farm::CropType::TestCrop;
		assert(actions.ApplyTool(grid, FarmTool::Hoe, crop, economy));
		assert(economy.BuySeed(crop).Succeeded());
		for (int dose = 0; dose < 5; ++dose) {
			assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
		}
		assert(actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
		assert(EditFarmTile(grid, grid.GetSelectedIndex(), [&](auto& candidate) { candidate.moisture = 0.0f; }));
		assert(growth.Update(grid, 0.25f, 1.0f));
		assert(EditFarmTile(grid, grid.GetSelectedIndex(), [&](auto& candidate) { candidate.moisture = 0.10f; }));
		assert(growth.Update(grid, 0.25f, 1.0f));
		assert(EditFarmTile(grid, grid.GetSelectedIndex(), [&](auto& candidate) { candidate.moisture = 0.50f; }));
		assert(growth.Update(grid, 0.25f, 1.0f));
		assert(EditFarmTile(grid, grid.GetSelectedIndex(), [&](auto& candidate) { candidate.moisture = 0.90f; }));
		assert(growth.Update(grid, 0.25f, 1.0f));
		const farm::FarmTile* growingTile = grid.GetSelectedTile();
		assert(growingTile != nullptr);
		assert(std::fabs(growingTile->careHistory.drySeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.lowSeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.goodSeconds - 0.25f) < 0.0001f);
		assert(std::fabs(growingTile->careHistory.excessSeconds - 0.25f) < 0.0001f);
		assert(growingTile->careHistory.efficiencySeconds > 0.0f &&
			growingTile->careHistory.efficiencySeconds <= 1.0f);
		farm::FarmTile readyTile = *growingTile;
		readyTile.growth = 1.0f;
		assert(grid.SetTile(0, readyTile));
		const auto expectedHistory = readyTile.careHistory;
		const auto expectedSize = actions.EvaluateHarvestQuality(readyTile).harvestSize;
		assert(expectedSize.known);
		assert(readyTile.soilNutrients < 0.60f && expectedHistory.nutrientGrowth > 0);
		assert(actions.ApplyTool(grid, FarmTool::Harvest, crop, economy));
		assert(economy.GetLastHarvestQuality().harvestSize.multiplier == expectedSize.multiplier);
		assert(grid.GetSelectedTile()->careHistory.IsEmpty());
		assert(actions.Undo());
		assert(economy.GetHarvestRecordCount() == 0);
		assert(!economy.GetLastHarvestQuality().harvestSize.known);
		assert(grid.GetSelectedTile()->soilNutrients == readyTile.soilNutrients);
		assert(grid.GetSelectedTile()->careHistory.nutrientSupply == expectedHistory.nutrientSupply);
		assert(grid.GetSelectedTile()->state == farm::FarmTileState::Planted);
		assert(std::fabs(grid.GetSelectedTile()->careHistory.goodSeconds -
			expectedHistory.goodSeconds) < 0.0001f);
		assert(actions.Redo());
		assert(economy.GetHarvestRecordCount() == 1);
		assert(economy.GetHarvestRecord(0)->quality.harvestSize.multiplier == expectedSize.multiplier);
		assert(grid.GetSelectedTile()->careHistory.IsEmpty());
		assert(economy.GetLastHarvestQuality().harvestSize.multiplier == expectedSize.multiplier);
		assert(economy.SellAll().Succeeded());
		assert(economy.GetLastHarvestQuality().harvestSize.multiplier == expectedSize.multiplier);
	}
    {
        FarmEconomySystem economy;
        farm::FarmRules rules;
        rules.initialMoney = rules.testCropSeedPrice - 1;
        economy.Initialize(rules);
        assert(!economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == rules.initialMoney && economy.GetTotalSeedCount() == 0);
        rules.initialMoney = rules.testCropSeedPrice;
        economy.Initialize(rules);
        assert(economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == 0 && economy.GetTotalSeedCount() == 1);
        assert(!economy.BuySeed(farm::CropType::TestCrop).Succeeded());
        assert(economy.GetMoney() == 0 && economy.GetTotalSeedCount() == 1);
    }
	std::cout << "PASS: crop care history quality and harvest undo/redo\n";
    FarmPlayFlow flow;
    using Phase = FarmPlayFlow::Phase;
    assert(flow.BlocksSimulation());
    flow.Observe(false);
    assert(flow.GetPhase() == Phase::Briefing);
    flow.Continue(); flow.Continue();
    assert(flow.GetPhase() == Phase::Playing && !flow.BlocksSimulation());
    flow.Observe(true);
    assert(flow.GetPhase() == Phase::Result && flow.BlocksSimulation());
    flow.Continue();
    for (int i = 0; i < 10; ++i) flow.Observe(true);
    assert(flow.GetPhase() == Phase::Review && !flow.BlocksSimulation());
    flow.Observe(false);
    assert(flow.GetPhase() == Phase::Briefing);
    flow.Reset(); flow.Observe(true);
    assert(flow.GetPhase() == Phase::Result); // A loaded cleared farm is not a new challenge.

    for (const float speed : {1.0f, 2.0f, 4.0f}) {
        farm::FarmGrid grid;
        FarmEconomySystem economy;
        FarmToolActionSystem actions;
        FarmGrowthSystem growth;
        FarmProgressionSystem progression;
        assert(grid.Initialize(5, 4));
        economy.Initialize(); actions.Initialize(); growth.Initialize(); progression.Initialize();
        flow.Reset(); flow.Continue();
        const auto crop = farm::CropType::TestCrop;
        assert(actions.EvaluateTool(grid, FarmTool::Hoe, crop, &economy).Succeeded());
        assert(actions.ApplyTool(grid, FarmTool::Hoe, crop, economy));
        assert(!actions.EvaluateTool(grid, FarmTool::Hoe, crop, &economy).Succeeded());
        assert(actions.EvaluateTool(grid, FarmTool::Seed, crop, &economy).status == FarmToolActionStatus::NoSeed);
        assert(!actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
        assert(!economy.SellAll().Succeeded());
        int harvests = 0;
        while (!progression.IsCleared() && harvests < 20) {
            const int moneyBeforePurchase = economy.GetMoney();
            const int seedsBeforePurchase = economy.GetSeedCount(crop);
            assert(economy.BuySeed(crop).Succeeded());
            assert(economy.GetMoney() == moneyBeforePurchase - farm::FarmRules{}.testCropSeedPrice);
            assert(economy.GetSeedCount(crop) == seedsBeforePurchase + 1);
            assert(grid.GetSelectedTile()->state == farm::FarmTileState::Tilled);
            assert(actions.EvaluateTool(grid, FarmTool::Seed, crop, &economy).Succeeded());
            while (grid.GetSelectedTile()->moisture < 0.5f)
                assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
            assert(actions.ApplyTool(grid, FarmTool::Seed, crop, economy));
            assert(actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).status == FarmToolActionStatus::NotReady);
            assert(economy.GetSeedCount(crop) == 0);
            for (int step = 0; step < 2000 && !farm::IsHarvestReady(*grid.GetSelectedTile()); ++step) {
                if (grid.GetSelectedTile()->moisture < 0.4f)
                    assert(actions.ApplyTool(grid, FarmTool::Water, crop, economy));
                assert(growth.Update(grid, 0.05f, speed));
            }
            assert(farm::IsHarvestReady(*grid.GetSelectedTile()));
            assert(actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).Succeeded());
            assert(!actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).harvestedTile);
            const auto harvested = actions.ApplyToolDetailed(grid, FarmTool::Harvest, crop, economy);
            assert(harvested.Succeeded() && harvested.harvestedTile);
            assert(harvested.harvestedTile->crop == crop && farm::IsHarvestReady(*harvested.harvestedTile));
            assert(!actions.ApplyToolDetailed(grid, FarmTool::Harvest, crop, economy).harvestedTile);
            assert(!actions.EvaluateTool(grid, FarmTool::Harvest, crop, &economy).Succeeded());
            assert(!actions.ApplyTool(grid, FarmTool::Harvest, crop, economy));
            assert(economy.GetTotalCropCount() == 1);
            const int before = economy.GetMoney();
            const auto sale = economy.SellAll();
            assert(sale.Succeeded() && sale.soldCount == 1);
            assert(economy.GetMoney() == before + sale.earnedMoney);
            assert(economy.GetTotalCropCount() == 0 && !economy.SellAll().Succeeded());
            actions.ClearHistory(); // Same sale boundary as the Scene; no undo across committed sale.
            const bool justCleared = progression.EvaluateClear(economy.GetMoney());
            flow.Observe(progression.IsCleared());
            if (justCleared) {
                assert(!progression.EvaluateClear(economy.GetMoney()));
                assert(flow.GetPhase() == Phase::Result);
            }
            ++harvests;
        }
        assert(progression.IsCleared() && economy.GetMoney() >= progression.GetTargetMoney());
        flow.Continue(); flow.Observe(true);
        assert(flow.GetPhase() == Phase::Review);
        actions.ClearHistory();
        assert(grid.Initialize(5, 4));
        economy.Initialize(); progression.Initialize(); flow.Reset();
        assert(economy.GetMoney() == farm::FarmRules{}.initialMoney);
        assert(economy.GetTotalCropCount() == 0 && economy.GetTotalSeedCount() == 0);
        assert(!progression.IsCleared() && flow.BlocksSimulation());
        std::cout << "PASS: buy/grow/harvest/sell/clear/restart x" << speed << ", harvests=" << harvests << '\n';
    }
}
