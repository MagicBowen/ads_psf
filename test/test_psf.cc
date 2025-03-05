#include "catch2/catch.hpp"

#include "ads_psf/processor_dsl.h"
#include "ads_psf/data_context.h"
#include "ads_psf/data_parallel_id.h"

#include "ads_psf/processors/sequential_processor.h"
#include "ads_psf/processors/parallel_processor.h"
#include "ads_psf/processors/race_processor.h"
#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/processors/data_parallel_processor.h"
#include "ads_psf/processors/data_race_processor.h"
#include "ads_psf/trackers/console_tracker.h"
#include "ads_psf/trackers/timing_tracker.h"
#include "ads_psf/executors/std_async_executor.h"

#include <iostream>
#include <random>
#include <thread>

using namespace ads_psf;

namespace {
    // DataContext 中跟踪算法执行过程的数据
    struct TestTracker {
        void Track(const std::string& key, const std::any& value) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            trackedData_.emplace_back(key, value);
        }
    
        bool HasKey(const std::string& key) const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return std::any_of(trackedData_.begin(), trackedData_.end(), [&](const auto& pair) {
                return pair.first == key;
            });
        }
    
        template<typename T>
        bool HasValue(const std::string& key, const T& value) const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return std::any_of(trackedData_.begin(), trackedData_.end(), [&](const auto& pair) {
                return pair.first == key && std::any_cast<T>(pair.second) == value;
            });
        }
    
        bool KeyAt(const std::string& key, std::size_t pos) const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = std::find_if(trackedData_.begin(), trackedData_.end(), [&](const auto& pair) {
                return pair.first == key;
            });
            return it != trackedData_.end() && std::distance(trackedData_.begin(), it) == pos;
        }
    
        std::size_t Size() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return trackedData_.size();
        }
    
        template<typename T>
        void Print() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            std::for_each(trackedData_.begin(), trackedData_.end(), [](const auto& pair) {
                std::cout << pair.first << ": " << std::any_cast<T>(pair.second) << "\n";
            });
        }
    
    private:
        std::vector<std::pair<std::string, std::any>> trackedData_;
        mutable std::shared_mutex mutex_;
    };

    // DataContext 中的数据结构
    struct MyDatas : std::vector<int> {
        using std::vector<int>::vector;
    };
    
    // 模拟业务算法
    struct MockAlgo {
        MockAlgo(const std::string& name, 
            std::chrono::milliseconds sleepTime = std::chrono::milliseconds(0))
        : name_{name}, sleepTime_{sleepTime} 
        {}
    
        void Init() {
        }
    
        void Execute(DataContext& context) {
            std::this_thread::sleep_for(sleepTime_);
            
            auto tracker = context.Fetch<TestTracker>();
            if (tracker) {
                tracker->Track(name_, 0);
            }
        }
    
    private:
        std::chrono::milliseconds sleepTime_;
        std::string name_;
    };
    
    #define DEFINE_MOCK_ALGO(name, sleepTime) \
    struct name : MockAlgo { \
        name() : MockAlgo(#name, std::chrono::milliseconds(sleepTime)) {} \
    }
    
    // 不同的业务模拟算法类，休眠不同时间
    DEFINE_MOCK_ALGO(MockAlgo1, 100);
    DEFINE_MOCK_ALGO(MockAlgo2, 200);
    DEFINE_MOCK_ALGO(MockAlgo3, 300);
    DEFINE_MOCK_ALGO(MockAlgo4, 400);
    DEFINE_MOCK_ALGO(MockAlgo5, 500);
    DEFINE_MOCK_ALGO(MockAlgo6, 600);
    DEFINE_MOCK_ALGO(MockAlgo7, 700);
    DEFINE_MOCK_ALGO(MockAlgo8, 800);
    
    // 模拟业务数据并发执行的算法类
    struct DataAccessAlgo {
        DataAccessAlgo() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distr(1, 1000);
            sleepMs_ = distr(gen);
        }

        void Init() {
        }

    protected:
        uint32_t sleepMs_{0};
    };

    // 模拟业务数据并发读取的算法类
    struct DataReadAlgo : DataAccessAlgo {
        void Execute(DataContext& context) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs_));
    
            int instanceId = data_parallel::ID<MyDatas>;
            auto& myDatas = *context.Fetch<MyDatas>();
    
            auto tracker = context.Fetch<TestTracker>();
            if (tracker) {
                tracker->Track("DataReadAlgo", myDatas[instanceId]);
            }
        }
    };
    
    // 模拟业务数据并发修改的算法类
    struct DataWriteAlgo : DataAccessAlgo {
        void Init() {
        }
    
        void Execute(DataContext& context) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs_));
            
            int instanceId = data_parallel::ID<MyDatas>;
            auto& myDatas = *context.Fetch<MyDatas>();
            
            auto tracker = context.Fetch<TestTracker>();
            if (tracker) {
                tracker->Track("DataWriteAlgo", myDatas[instanceId]);
            }
    
            myDatas[instanceId] *= 2;
        }
    };
}

TEST_CASE("Processor Test") {

    auto scheduler = SCHEDULE(
        SEQUENCE(
            PROCESS(MockAlgo1),
            PROCESS(MockAlgo2),
            PROCESS(MockAlgo3)
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACK(ConsoleTracker), 
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() == 3);
    REQUIRE(tracker->KeyAt("MockAlgo1", 0));
    REQUIRE(tracker->KeyAt("MockAlgo2", 1));
    REQUIRE(tracker->KeyAt("MockAlgo3", 2));
}

TEST_CASE("Processor Ref Algo Test") {
    MockAlgo1 algo1;
    MockAlgo2 algo2;
    MockAlgo3 algo3;

    auto scheduler = SCHEDULE(
        SEQUENCE(
            PROCESS_REF(algo1),
            PROCESS_REF(algo2),
            PROCESS_REF(algo3)
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACK(ConsoleTracker), 
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() == 3);
    REQUIRE(tracker->KeyAt("MockAlgo1", 0));
    REQUIRE(tracker->KeyAt("MockAlgo2", 1));
    REQUIRE(tracker->KeyAt("MockAlgo3", 2));
}


TEST_CASE("Processor composite Test") {
    auto processor = SEQUENCE(
        PROCESS(MockAlgo1),
        PARALLEL(
            PROCESS(MockAlgo2),
            PROCESS(MockAlgo3),
            PARALLEL(
                PROCESS(MockAlgo4),
                RACE(
                    PROCESS(MockAlgo5),
                    SEQUENCE(
                        PROCESS(MockAlgo6),
                        PROCESS(MockAlgo7)
                    )
                )
            )
        ),
        PROCESS(MockAlgo8)
    );

    auto scheduler = SCHEDULE(
        processor, 
        EXECUTOR(StdAsyncExecutor),
        TRACK(ConsoleTracker), 
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() == 7);
    REQUIRE(tracker->KeyAt("MockAlgo1", 0));
    REQUIRE(tracker->KeyAt("MockAlgo2", 1));
    REQUIRE(tracker->KeyAt("MockAlgo3", 2));
    REQUIRE(tracker->KeyAt("MockAlgo8", 6));

    REQUIRE(tracker->HasKey("MockAlgo4"));
    REQUIRE(tracker->HasKey("MockAlgo5"));
    REQUIRE(tracker->HasKey("MockAlgo6"));
}

TEST_CASE("DataParallelProcessor basic Test") {    
    auto scheduler = SCHEDULE(
        DATA_PARALLEL(MyDatas, 5, PROCESS(DataReadAlgo)),
        EXECUTOR(StdAsyncExecutor),
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();
    dataCtx.Create<MyDatas>(1, 2, 3, 4, 5);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() == 5);
    REQUIRE(tracker->HasValue("DataReadAlgo", 1));
    REQUIRE(tracker->HasValue("DataReadAlgo", 2));
    REQUIRE(tracker->HasValue("DataReadAlgo", 3));
    REQUIRE(tracker->HasValue("DataReadAlgo", 4));
    REQUIRE(tracker->HasValue("DataReadAlgo", 5));
}

TEST_CASE("DataParallelProcessor complex Test") {
    auto scheduler = SCHEDULE(
        SEQUENCE(
            PROCESS(MockAlgo1),
            DATA_PARALLEL(MyDatas, 3, 
                SEQUENCE(
                    PROCESS(MockAlgo2),
                    PROCESS(DataWriteAlgo),
                    PROCESS(DataReadAlgo)
                )
            ),
            PROCESS(MockAlgo3)
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();
    dataCtx.Create<MyDatas>(1, 2, 3);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() == 11);
    REQUIRE(tracker->KeyAt("MockAlgo1", 0));
    REQUIRE(tracker->KeyAt("MockAlgo3", 10));

    REQUIRE(tracker->HasKey("MockAlgo2"));

    REQUIRE(tracker->HasValue("DataWriteAlgo", 1));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 2));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 3));
   
    REQUIRE(tracker->HasValue("DataReadAlgo", 2));
    REQUIRE(tracker->HasValue("DataReadAlgo", 4));
    REQUIRE(tracker->HasValue("DataReadAlgo", 6));
}

TEST_CASE("DataRaceProcessor basic Test") {
    auto scheduler = SCHEDULE(
        DATA_RACE(MyDatas, 5, 
            SEQUENCE(
                PROCESS(DataWriteAlgo),
                PROCESS(DataReadAlgo)
            )
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACK(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<TestTracker>();
    dataCtx.Create<MyDatas>(1, 2, 3, 4, 5);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() > 5);
    REQUIRE(tracker->Size() <= 10);

    REQUIRE(tracker->HasValue("DataWriteAlgo", 1));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 2));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 3));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 4));
    REQUIRE(tracker->HasValue("DataWriteAlgo", 5));

    REQUIRE(tracker->HasKey("DataReadAlgo"));
}

TEST_CASE("DataRaceProcessor complex Test") {
    auto scheduler = SCHEDULE(
        SEQUENCE(
            PROCESS(MockAlgo1),
            DATA_RACE(MyDatas, 3, 
                SEQUENCE(
                    PROCESS(MockAlgo2),
                    PROCESS(DataWriteAlgo),
                    PROCESS(DataReadAlgo)
                )
            ),
            PROCESS(MockAlgo3)
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACK(TimingTracker)
    );

    DataContext dataCtx;
    dataCtx.Create<TestTracker>();
    dataCtx.Create<MyDatas>(1, 2, 3);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto tracker = dataCtx.Fetch<TestTracker>();
    REQUIRE(tracker->Size() > 6);
    REQUIRE(tracker->Size() <= 11);

    REQUIRE(tracker->KeyAt("MockAlgo1", 0));
    REQUIRE(tracker->KeyAt("MockAlgo3", tracker->Size() - 1));

    REQUIRE(tracker->HasKey("MockAlgo2")); 
    REQUIRE(tracker->HasKey("DataWriteAlgo")); 
    REQUIRE(tracker->HasKey("DataReadAlgo")); 
}