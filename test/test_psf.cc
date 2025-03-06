#include "catch2/catch.hpp"
#include "ads_psf/processor_dsl.h"
#include "ads_psf/scheduler_dsl.h"
#include "ads_psf/data_context.h"
#include "ads_psf/data_parallel_id.h"
#include <iostream>
#include <random>
#include <thread>

using namespace ads_psf;

namespace {
    // DataContext 中跟踪算法执行过程的数据
    struct AlgoChecker {
        void Record(const std::string& algo, const std::any& data) {
            std::lock_guard lock{mutex_};
            algoDatas_.emplace_back(algo, data);
        }
    
        bool HasAlgo(const std::string& algo) const {
            std::lock_guard lock{mutex_};
            return std::any_of(algoDatas_.begin(), algoDatas_.end(), [&](const auto& pair) {
                return pair.first == algo;
            });
        }
    
        template<typename T>
        bool HasAlgoData(const std::string& algo, const T& data) const {
            std::lock_guard lock{mutex_};
            return std::any_of(algoDatas_.begin(), algoDatas_.end(), [&](const auto& pair) {
                return pair.first == algo && std::any_cast<T>(pair.second) == data;
            });
        }
    
        bool IsAlgoAt(const std::string& algo, std::size_t pos) const {
            std::lock_guard lock{mutex_};
            auto it = std::find_if(algoDatas_.begin(), algoDatas_.end(), [&](const auto& pair) {
                return pair.first == algo;
            });
            return it != algoDatas_.end() && std::distance(algoDatas_.begin(), it) == pos;
        }
    
        std::size_t Size() const {
            std::lock_guard lock{mutex_};
            return algoDatas_.size();
        }
    
        template<typename T>
        void Print() const {
            std::lock_guard lock{mutex_};
            std::for_each(algoDatas_.begin(), algoDatas_.end(), [](const auto& pair) {
                std::cout << pair.first << ": " << std::any_cast<T>(pair.second) << "\n";
            });
        }
    
    private:
        std::vector<std::pair<std::string, std::any>> algoDatas_;
        mutable std::mutex mutex_;
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
            
            auto checker = context.Fetch<AlgoChecker>();
            if (checker) {
                checker->Record(name_, 0);
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
    
            auto checker = context.Fetch<AlgoChecker>();
            if (checker) {
                checker->Record("DataReadAlgo", myDatas[instanceId]);
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
            
            auto checker = context.Fetch<AlgoChecker>();
            if (checker) {
                checker->Record("DataWriteAlgo", myDatas[instanceId]);
            }
    
            myDatas[instanceId] *= 2;
        }
    };
}

TEST_CASE("Processor Test") {
    auto processor = SEQUENCE(
        PROCESS(MockAlgo1),
        PROCESS(MockAlgo2),
        PROCESS(MockAlgo3)
    );

    auto scheduler = SCHEDULER(
        PROCESSOR(processor),
        EXECUTOR(StdAsyncExecutor),
        TRACKER(ConsoleTracker), 
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() == 3);
    REQUIRE(checker->IsAlgoAt("MockAlgo1", 0));
    REQUIRE(checker->IsAlgoAt("MockAlgo2", 1));
    REQUIRE(checker->IsAlgoAt("MockAlgo3", 2));
}

TEST_CASE("Processor Ref Algo Test") {
    MockAlgo1 algo1;
    MockAlgo2 algo2;
    MockAlgo3 algo3;

    auto processor = SEQUENCE(
        PROCESS_REF(algo1),
        PROCESS_REF(algo2),
        PROCESS_REF(algo3)
    );

    auto scheduler = SCHEDULER(
        PROCESSOR(processor),
        EXECUTOR(StdAsyncExecutor),
        TRACKER(ConsoleTracker), 
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() == 3);
    REQUIRE(checker->IsAlgoAt("MockAlgo1", 0));
    REQUIRE(checker->IsAlgoAt("MockAlgo2", 1));
    REQUIRE(checker->IsAlgoAt("MockAlgo3", 2));
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

    auto scheduler = SCHEDULER(
        PROCESSOR(processor), 
        EXECUTOR(StdAsyncExecutor),
        TRACKER(ConsoleTracker), 
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() == 7);
    REQUIRE(checker->IsAlgoAt("MockAlgo1", 0));
    REQUIRE(checker->IsAlgoAt("MockAlgo2", 1));
    REQUIRE(checker->IsAlgoAt("MockAlgo3", 2));
    REQUIRE(checker->IsAlgoAt("MockAlgo8", 6));

    REQUIRE(checker->HasAlgo("MockAlgo4"));
    REQUIRE(checker->HasAlgo("MockAlgo5"));
    REQUIRE(checker->HasAlgo("MockAlgo6"));
}

TEST_CASE("DataParallelProcessor basic Test") {    
    auto scheduler = SCHEDULER(
        DATA_PARALLEL(MyDatas, 5, PROCESS(DataReadAlgo)),
        EXECUTOR(StdAsyncExecutor),
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();
    dataCtx.Create<MyDatas>(1, 2, 3, 4, 5);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() == 5);
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 1));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 2));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 3));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 4));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 5));
}

TEST_CASE("DataParallelProcessor complex Test") {
    auto scheduler = SCHEDULER(
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
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();
    dataCtx.Create<MyDatas>(1, 2, 3);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() == 11);
    REQUIRE(checker->IsAlgoAt("MockAlgo1", 0));
    REQUIRE(checker->IsAlgoAt("MockAlgo3", 10));

    REQUIRE(checker->HasAlgo("MockAlgo2"));

    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 1));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 2));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 3));
   
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 2));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 4));
    REQUIRE(checker->HasAlgoData("DataReadAlgo", 6));
}

TEST_CASE("DataRaceProcessor basic Test") {
    auto scheduler = SCHEDULER(
        DATA_RACE(MyDatas, 5, 
            SEQUENCE(
                PROCESS(DataWriteAlgo),
                PROCESS(DataReadAlgo)
            )
        ),
        EXECUTOR(StdAsyncExecutor),
        TRACKER(TimingTracker)
    );
    
    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();
    dataCtx.Create<MyDatas>(1, 2, 3, 4, 5);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() > 5);
    REQUIRE(checker->Size() <= 10);

    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 1));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 2));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 3));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 4));
    REQUIRE(checker->HasAlgoData("DataWriteAlgo", 5));

    REQUIRE(checker->HasAlgo("DataReadAlgo"));
}

TEST_CASE("DataRaceProcessor complex Test") {
    auto scheduler = SCHEDULER(
        PROCESSOR(
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
        TRACKER(ConsoleTracker),
        TRACKER(TimingTracker)
    );

    DataContext dataCtx;
    dataCtx.Create<AlgoChecker>();
    dataCtx.Create<MyDatas>(1, 2, 3);

    auto status = scheduler->Run(dataCtx);
    REQUIRE(status == ProcessStatus::OK);

    scheduler->Dump();

    auto checker = dataCtx.Fetch<AlgoChecker>();
    REQUIRE(checker->Size() > 6);
    REQUIRE(checker->Size() <= 11);

    REQUIRE(checker->IsAlgoAt("MockAlgo1", 0));
    REQUIRE(checker->IsAlgoAt("MockAlgo3", checker->Size() - 1));

    REQUIRE(checker->HasAlgo("MockAlgo2")); 
    REQUIRE(checker->HasAlgo("DataWriteAlgo")); 
    REQUIRE(checker->HasAlgo("DataReadAlgo")); 
}