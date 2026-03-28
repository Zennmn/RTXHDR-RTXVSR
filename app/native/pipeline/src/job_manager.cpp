#include "rtx/pipeline/job_manager.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rtx/core/logger.h"
#include "rtx/core/validation.h"
#include "rtx/pipeline/processing_pipeline.h"
#include "rtx/video_io/ffmpeg_probe.h"

namespace rtx {

struct JobManager::Impl {
  struct JobRecord {
    std::uint64_t id = 0;
    JobConfig config;
    JobProgress progress;
    MediaInfo input_info;
    Logger logger;
    std::deque<LogEvent> events;
    std::thread worker;
    std::atomic_bool cancel_requested = false;
    bool started = false;
    mutable std::mutex mutex;
  };

  mutable std::mutex jobs_mutex;
  std::uint64_t next_job_id = 1;
  std::unordered_map<std::uint64_t, std::shared_ptr<JobRecord>> jobs;
};

namespace {

std::uint64_t NowUnixMilliseconds() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

void UpdateProgress(JobManager::Impl::JobRecord& record,
                    JobState state,
                    const std::string& phase,
                    double total_progress,
                    double phase_progress,
                    std::uint64_t processed_frames,
                    std::uint64_t total_frames,
                    double fps,
                    double elapsed_seconds,
                    double eta_seconds,
                    const std::string& message) {
  std::scoped_lock lock(record.mutex);
  record.progress.state = state;
  record.progress.current_phase = phase;
  record.progress.total_progress = total_progress;
  record.progress.phase_progress = phase_progress;
  record.progress.processed_frames = processed_frames;
  record.progress.total_frames = total_frames;
  record.progress.fps = fps;
  record.progress.elapsed_seconds = elapsed_seconds;
  record.progress.eta_seconds = eta_seconds;
  record.progress.status_message = message;
}

void PushEvent(JobManager::Impl::JobRecord& record, EventSeverity severity, const std::string& message) {
  std::scoped_lock lock(record.mutex);
  if (record.events.size() > 512) {
    record.events.pop_front();
  }
  record.events.push_back(LogEvent{
      .severity = severity,
      .timestamp_ms = NowUnixMilliseconds(),
      .phase = record.progress.current_phase,
      .message = message,
  });
}

}  // namespace

JobManager::JobManager() : impl_(std::make_unique<Impl>()) {}

JobManager::~JobManager() {
  std::vector<std::shared_ptr<Impl::JobRecord>> jobs;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    for (auto& [_, job] : impl_->jobs) {
      jobs.push_back(job);
    }
  }

  for (auto& job : jobs) {
    job->cancel_requested = true;
    if (job->worker.joinable()) {
      job->worker.join();
    }
  }
}

Status JobManager::CreateJob(const JobConfig& config, std::uint64_t& job_id) {
  auto status = ValidateJobConfig(config);
  if (!status.ok()) {
    return status;
  }

  auto record = std::make_shared<Impl::JobRecord>();
  record->config = config;
  record->progress.state = JobState::kCreated;
  record->progress.current_phase = "已创建";
  record->progress.status_message = "任务已创建";
  record->logger.SetLevel(config.logging_level);
  record->logger.SetSink([record](const LogRecord& log_record) {
    PushEvent(*record, ToEventSeverity(log_record.level), log_record.line);
  });

  {
    std::scoped_lock lock(impl_->jobs_mutex);
    record->id = impl_->next_job_id++;
    impl_->jobs.emplace(record->id, record);
    job_id = record->id;
  }

  PushEvent(*record, EventSeverity::kInfo, "任务已创建");
  return Status::Ok();
}

Status JobManager::StartJob(std::uint64_t job_id,
                            const RuntimePaths& runtime_paths,
                            const SystemProbe& system_probe) {
  std::shared_ptr<Impl::JobRecord> record;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    const auto it = impl_->jobs.find(job_id);
    if (it == impl_->jobs.end()) {
      return Status::NotFound("找不到指定的作业。");
    }
    record = it->second;
  }

  {
    std::scoped_lock lock(record->mutex);
    if (record->started) {
      return Status::InvalidArgument("该作业已经启动。");
    }
    record->started = true;
    record->progress.state = JobState::kQueued;
    record->progress.current_phase = "已排队";
    record->progress.status_message = "任务已加入队列";
  }

  record->worker = std::thread([record, runtime_paths, system_probe]() {
    UpdateProgress(*record, JobState::kAnalyzing, "媒体探测", 0.01, 0.1, 0, 0, 0.0, 0.0, -1.0,
                   "正在分析输入文件");

    FfmpegProbe probe(record->logger);
    auto status = probe.Probe(record->config.input_path, record->input_info);
    if (!status.ok()) {
      UpdateProgress(*record, JobState::kFailed, "Probe", 0.0, 0.0, 0, 0, 0.0, 0.0, -1.0,
                     status.message());
      PushEvent(*record, EventSeverity::kError, status.message());
      return;
    }

    ProcessingPipeline pipeline(runtime_paths, record->logger);
    ProcessingCallbacks callbacks;
    callbacks.is_cancelled = [record]() { return record->cancel_requested.load(); };
    callbacks.on_progress =
        [record](JobState state, const std::string& phase, double total_progress,
                 double phase_progress, std::uint64_t processed_frames,
                 std::uint64_t total_frames, double fps, double elapsed_seconds, double eta_seconds,
                 const std::string& message) {
          UpdateProgress(*record, state, phase, total_progress, phase_progress, processed_frames,
                         total_frames, fps, elapsed_seconds, eta_seconds, message);
        };

    status = pipeline.Run(record->config, system_probe, record->input_info, callbacks);
    if (!status.ok()) {
      const auto final_state =
          status.code() == StatusCode::kCancelled ? JobState::kCancelled : JobState::kFailed;
      UpdateProgress(*record, final_state, "已结束", record->progress.total_progress,
                     record->progress.phase_progress, record->progress.processed_frames,
                     record->progress.total_frames, record->progress.fps,
                     record->progress.elapsed_seconds, record->progress.eta_seconds,
                     status.message());
      PushEvent(*record,
                status.code() == StatusCode::kCancelled ? EventSeverity::kWarn
                                                        : EventSeverity::kError,
                status.message());
      return;
    }

    PushEvent(*record, EventSeverity::kInfo, "任务已成功完成");
  });

  return Status::Ok();
}

Status JobManager::QueryJobProgress(std::uint64_t job_id, JobProgress& progress) const {
  std::shared_ptr<Impl::JobRecord> record;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    const auto it = impl_->jobs.find(job_id);
    if (it == impl_->jobs.end()) {
      return Status::NotFound("找不到指定的作业。");
    }
    record = it->second;
  }

  std::scoped_lock lock(record->mutex);
  progress = record->progress;
  return Status::Ok();
}

Status JobManager::CancelJob(std::uint64_t job_id) {
  std::shared_ptr<Impl::JobRecord> record;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    const auto it = impl_->jobs.find(job_id);
    if (it == impl_->jobs.end()) {
      return Status::NotFound("找不到指定的作业。");
    }
    record = it->second;
  }

  record->cancel_requested = true;
  PushEvent(*record, EventSeverity::kWarn, "已请求取消任务");
  return Status::Ok();
}

Status JobManager::DestroyJob(std::uint64_t job_id) {
  std::shared_ptr<Impl::JobRecord> record;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    const auto it = impl_->jobs.find(job_id);
    if (it == impl_->jobs.end()) {
      return Status::NotFound("找不到指定的作业。");
    }
    record = it->second;
    impl_->jobs.erase(it);
  }

  record->cancel_requested = true;
  if (record->worker.joinable()) {
    record->worker.join();
  }
  return Status::Ok();
}

Status JobManager::DrainEvents(std::uint64_t job_id,
                               std::size_t max_events,
                               std::vector<LogEvent>& events) {
  std::shared_ptr<Impl::JobRecord> record;
  {
    std::scoped_lock lock(impl_->jobs_mutex);
    const auto it = impl_->jobs.find(job_id);
    if (it == impl_->jobs.end()) {
      return Status::NotFound("找不到指定的作业。");
    }
    record = it->second;
  }

  std::scoped_lock lock(record->mutex);
  const auto count = std::min(max_events, record->events.size());
  for (std::size_t index = 0; index < count; ++index) {
    events.push_back(record->events.front());
    record->events.pop_front();
  }
  return Status::Ok();
}

}  // namespace rtx
