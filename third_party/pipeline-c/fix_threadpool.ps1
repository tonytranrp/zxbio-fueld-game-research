$file = 'C:\Users\Tonyt\Documents\GitHub\Pipeline-c++\include\pb\runtime\thread_pool.hpp'
$lines = Get-Content $file

# Find the line with "[[nodiscard]] std::size_t worker_count()"
$insertLine = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '\[\[nodiscard\]\] std::size_t worker_count\(\)') {
        $insertLine = $i
        break
    }
}

if ($insertLine -eq -1) {
    Write-Output "ERROR: Could not find worker_count line"
    exit 1
}

$newMethods = @'

  template <class Iterator>
  auto enqueue_range(Iterator begin, Iterator end)
      -> std::vector<std::future<std::invoke_result_t<typename std::iterator_traits<Iterator>::value_type>>> {
    using R = std::invoke_result_t<typename std::iterator_traits<Iterator>::value_type>;
    std::vector<std::future<R>> futures;
    futures.reserve(std::distance(begin, end));

    {
      std::unique_lock lock(mutex_);
      if (stop_) {
        throw std::runtime_error("enqueue_range on stopped thread_pool");
      }

      for (auto it = begin; it != end; ++it) {
        auto task = std::make_shared<std::packaged_task<R()>>(*it);
        futures.push_back(task->get_future());
        tasks_.emplace([task] { (*task)(); });
      }
    }
    cv_.notify_all();
    return futures;
  }

  [[nodiscard]] std::size_t pending_tasks() const {
    std::unique_lock lock(mutex_);
    return tasks_.size();
  }

'@

$newLines = $newMethods -split "`n"

$result = @()
for ($i = 0; $i -lt $insertLine; $i++) {
    $result += $lines[$i]
}
foreach ($line in $newLines) {
    $result += $line
}
for ($i = $insertLine; $i -lt $lines.Count; $i++) {
    $result += $lines[$i]
}

$result | Set-Content $file -Encoding UTF8
Write-Output "Done. Added enqueue_range and pending_tasks before worker_count at line $($insertLine + 1)"
