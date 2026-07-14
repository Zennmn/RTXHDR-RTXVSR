#include "jobs/job_runner.h"
#include "video/fake_pipeline.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

using namespace vsr;

namespace {

TranscodeRequest valid_request() {
    TranscodeRequest request;
    request.input_path = "C:\\Videos\\in.mp4";
    request.output_path = "C:\\Videos\\out.mp4";
    request.processing.vsr.enabled = true;
    return request;
}

class SpyPipeline final : public VideoPipeline {
public:
    Result<void> run(
        const TranscodeRequest& request,
        CancellationToken&,
        ProgressCallback progress,
        WarningCallback) override {
        called = true;
        observed_request = request;

        JobProgress snapshot;
        snapshot.stage = JobStage::processing_rtx;
        snapshot.progress = 0.75;
        snapshot.frames_done = 75;
        snapshot.frames_total = 100;
        progress(snapshot);

        return Result<void>::Ok();
    }

    bool called = false;
    TranscodeRequest observed_request;
};

class CountingPipeline final : public VideoPipeline {
public:
    Result<void> run(const TranscodeRequest&, CancellationToken&, ProgressCallback, WarningCallback) override {
        ++calls;
        return Result<void>::Ok();
    }

    int calls = 0;
};

class CancelBeforeSuccessPipeline final : public VideoPipeline {
public:
    explicit CancelBeforeSuccessPipeline(std::function<void()> cancel) : cancel_(std::move(cancel)) {}

    Result<void> run(
        const TranscodeRequest&,
        CancellationToken&,
        ProgressCallback progress,
        WarningCallback) override {
        JobProgress snapshot;
        snapshot.stage = JobStage::encoding;
        snapshot.progress = 0.5;
        progress(snapshot);
        cancel_();
        return Result<void>::Ok();
    }

private:
    std::function<void()> cancel_;
};

class ConcurrentProbePipeline final : public VideoPipeline {
public:
    ConcurrentProbePipeline(std::atomic<int>& active, std::atomic<int>& max_active)
        : active_(active), max_active_(max_active) {}

    Result<void> run(
        const TranscodeRequest&,
        CancellationToken&,
        ProgressCallback progress,
        WarningCallback) override {
        const int active = active_.fetch_add(1) + 1;
        record_max(active);

        JobProgress snapshot;
        snapshot.stage = JobStage::encoding;
        snapshot.progress = 0.5;
        progress(snapshot);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        active_.fetch_sub(1);
        return Result<void>::Ok();
    }

private:
    void record_max(int value) {
        int current = max_active_.load();
        while (current < value && !max_active_.compare_exchange_weak(current, value)) {
        }
    }

    std::atomic<int>& active_;
    std::atomic<int>& max_active_;
};

class WaitForCancelPipeline final : public VideoPipeline {
public:
    std::future<void> started() { return started_.get_future(); }

    Result<void> run(
        const TranscodeRequest&,
        CancellationToken& cancellation,
        ProgressCallback,
        WarningCallback) override {
        started_.set_value();
        for (int i = 0; i < 1000; ++i) {
            if (cancellation.requested.load()) {
                observed_cancel.store(true);
                return Result<void>::Fail({"job_canceled", "Job was canceled.", ""});
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return Result<void>::Fail({"cancel_not_observed", "Cancel token was not set.", ""});
    }

    std::atomic_bool observed_cancel{false};

private:
    std::promise<void> started_;
};

class WarningPipeline final : public VideoPipeline {
public:
    Result<void> run(
        const TranscodeRequest&,
        CancellationToken&,
        ProgressCallback,
        WarningCallback warning) override {
        warning("Skipped MP4-incompatible audio stream (stream=1, codec=dts).");
        return Result<void>::Ok();
    }
};

} // namespace

TEST(JobRunner, completesFakePipelineJob) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::succeeds);
    JobRunner runner(store, std::move(pipeline));

    const auto id = store.create(valid_request()).value();

    const auto result = runner.run_one(id);

    ASSERT_TRUE(result.ok()) << result.error().message;
    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::succeeded);
    EXPECT_DOUBLE_EQ(snapshot.progress.progress, 1.0);
    EXPECT_EQ(snapshot.progress.frames_total, 100);
}

TEST(JobRunner, recordsPipelineWarningsOnSuccessfulJob) {
    JobStore store;
    auto pipeline = std::make_unique<WarningPipeline>();
    JobRunner runner(store, std::move(pipeline));
    const auto id = store.create(valid_request()).value();

    const auto result = runner.run_one(id);

    ASSERT_TRUE(result.ok()) << result.error().message;
    const auto snapshot = store.get(id).value();
    ASSERT_EQ(snapshot.warnings.size(), 1u);
    EXPECT_EQ(snapshot.warnings.front(), "Skipped MP4-incompatible audio stream (stream=1, codec=dts).");
}

TEST(JobRunner, recordsFakePipelineFailure) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::fails);
    JobRunner runner(store, std::move(pipeline));

    const auto id = store.create(valid_request()).value();

    const auto result = runner.run_one(id);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "fake_pipeline_failed");
    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.error_code, "fake_pipeline_failed");
    EXPECT_EQ(snapshot.error_message, "Fake pipeline failed.");
}

TEST(JobRunner, passesOriginalProcessingAndOutputSettingsToPipeline) {
    JobStore store;
    auto pipeline = std::make_unique<SpyPipeline>();
    auto* spy = pipeline.get();
    JobRunner runner(store, std::move(pipeline));
    auto request = valid_request();
    request.processing.vsr.quality = 4;
    request.processing.vsr.scale = 3.0;
    request.processing.hdr.enabled = true;
    request.processing.hdr.contrast = 125;
    request.processing.hdr.saturation = 110;
    request.processing.hdr.middle_gray = 50;
    request.processing.hdr.max_luminance = 1500;
    request.output.video_codec = "hevc";
    request.output.audio_mode = "none";
    request.output.subtitle_mode = "none";
    const auto id = store.create(request).value();

    const auto result = runner.run_one(id);

    ASSERT_TRUE(result.ok()) << result.error().message;
    ASSERT_TRUE(spy->called);
    EXPECT_EQ(spy->observed_request.input_path, request.input_path);
    EXPECT_EQ(spy->observed_request.output_path, request.output_path);
    EXPECT_EQ(spy->observed_request.processing.vsr.quality, 4);
    EXPECT_DOUBLE_EQ(spy->observed_request.processing.vsr.scale, 3.0);
    EXPECT_TRUE(spy->observed_request.processing.hdr.enabled);
    EXPECT_EQ(spy->observed_request.processing.hdr.contrast, 125);
    EXPECT_EQ(spy->observed_request.processing.hdr.saturation, 110);
    EXPECT_EQ(spy->observed_request.processing.hdr.middle_gray, 50);
    EXPECT_EQ(spy->observed_request.processing.hdr.max_luminance, 1500);
    EXPECT_EQ(spy->observed_request.output.video_codec, "hevc");
    EXPECT_EQ(spy->observed_request.output.audio_mode, "none");
    EXPECT_EQ(spy->observed_request.output.subtitle_mode, "none");
}

TEST(JobRunner, returnsMissingJobError) {
    JobStore store;
    auto pipeline = std::make_unique<FakePipeline>(FakePipelineMode::succeeds);
    JobRunner runner(store, std::move(pipeline));

    const auto result = runner.run_one("missing-job");

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "job_not_found");
}

TEST(JobRunner, marksExistingJobFailedWhenPipelineUnavailable) {
    JobStore store;
    JobRunner runner(store, nullptr);
    const auto id = store.create(valid_request()).value();

    const auto result = runner.run_one(id);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "pipeline_unavailable");
    const auto snapshot = store.get(id).value();
    EXPECT_EQ(snapshot.state, JobState::failed);
    EXPECT_EQ(snapshot.error_code, "pipeline_unavailable");
}

TEST(JobRunner, preCanceledQueuedJobDoesNotRunPipelineAndBecomesCanceled) {
    JobStore store;
    auto pipeline = std::make_unique<CountingPipeline>();
    auto* counting = pipeline.get();
    JobRunner runner(store, std::move(pipeline));
    const auto id = store.create(valid_request()).value();
    ASSERT_TRUE(store.request_cancel(id).ok());

    const auto result = runner.run_one(id);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "job_canceled");
    EXPECT_EQ(counting->calls, 0);
    EXPECT_EQ(store.get(id).value().state, JobState::canceled);
}

TEST(JobRunner, cancellationRequestedBeforePipelineSuccessWinsFinalState) {
    JobStore store;
    const auto id = store.create(valid_request()).value();
    auto pipeline = std::make_unique<CancelBeforeSuccessPipeline>([&store, id]() {
        store.request_cancel(id);
    });
    JobRunner runner(store, std::move(pipeline));

    const auto result = runner.run_one(id);

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "job_canceled");
    EXPECT_EQ(store.get(id).value().state, JobState::canceled);
}

TEST(JobRunner, requestCancelSetsInFlightPipelineToken) {
    JobStore store;
    auto pipeline = std::make_unique<WaitForCancelPipeline>();
    auto* wait_for_cancel = pipeline.get();
    auto started = wait_for_cancel->started();
    JobRunner runner(store, std::move(pipeline));
    const auto id = store.create(valid_request()).value();
    std::promise<Result<void>> result_promise;
    auto result_future = result_promise.get_future();

    std::thread worker([&]() {
        result_promise.set_value(runner.run_one(id));
    });

    ASSERT_EQ(started.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    const auto canceled = runner.request_cancel(id);
    worker.join();
    const auto result = result_future.get();

    ASSERT_TRUE(canceled.ok()) << canceled.error().message;
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "job_canceled");
    EXPECT_TRUE(wait_for_cancel->observed_cancel.load());
    EXPECT_EQ(store.get(id).value().state, JobState::canceled);
}

TEST(JobRunner, serializesConcurrentRunOneCallsAcrossSharedPipeline) {
    JobStore store;
    std::atomic<int> active{0};
    std::atomic<int> max_active{0};
    auto pipeline = std::make_unique<ConcurrentProbePipeline>(active, max_active);
    JobRunner runner(store, std::move(pipeline));
    const auto first_id = store.create(valid_request()).value();
    const auto second_id = store.create(valid_request()).value();
    std::atomic_bool start{false};
    std::atomic_bool first_ok{false};
    std::atomic_bool second_ok{false};

    std::thread first([&]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        first_ok.store(runner.run_one(first_id).ok());
    });
    std::thread second([&]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        second_ok.store(runner.run_one(second_id).ok());
    });

    start.store(true);
    first.join();
    second.join();

    EXPECT_TRUE(first_ok.load());
    EXPECT_TRUE(second_ok.load());
    EXPECT_EQ(max_active.load(), 1);
    EXPECT_EQ(store.get(first_id).value().state, JobState::succeeded);
    EXPECT_EQ(store.get(second_id).value().state, JobState::succeeded);
}
