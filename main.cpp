import std;
import BS_thread_pool;
import benchmark;

template <typename T>
concept bidirectional_sized_range = std::ranges::bidirectional_range<T> && std::ranges::sized_range<T>;

void print_omitted_range(bidirectional_sized_range auto &&rng, std::size_t num_head_tails = 5){
    if (rng.size() <= 2 * num_head_tails){
        std::println("{}", rng);
    }
    else{
        std::println("{} ... {}",
            rng | std::views::take(num_head_tails),
            rng | std::views::reverse | std::views::take(num_head_tails) | std::views::reverse
        );
    }
}

template <typename Gen>
auto random_vector(std::size_t size, Gen &&gen) -> std::vector<std::invoke_result_t<Gen>>{
    return std::views::iota(0UZ, size)
          | std::views::transform([&](auto) { return gen(); })
          | std::ranges::to<std::vector<std::invoke_result_t<Gen>>>();
}

int main(int argc, char **argv) {
    // Parse arguments.
    // ./prefix_sum [num_elements] [thread_count]

    if (argc < 3){
        std::cerr << "Invalid argument count.\n";
        return 1;
    }

    std::size_t size;
    if (std::string_view arg { argv[1] };
        std::from_chars(arg.data(), arg.data() + arg.size(), size).ec != std::errc{})
    {
        std::cerr << "Invalid size.";
        return 2;
    }
    else if (!std::has_single_bit(size)){
        std::cerr << "Size must be a power of 2.";
        return 3;
    }

    BS::concurrency_t num_threads;
    if (std::string_view arg { argv[2] };
        std::from_chars(arg.data(), arg.data() + arg.size(), num_threads).ec != std::errc{})
    {
        std::cerr << "Invalid thread count.";
        return 4;
    }

    // Generate random ints with given size.
    std::random_device rd;
    std::mt19937 gen { rd() };
    std::uniform_int_distribution<std::uint64_t> dis { 0, 100 };

    const auto nums = random_vector(size, [&] { return dis(gen); });
    std::print("Input: ");
    print_omitted_range(nums);

    // Sequential calculation with std::exclusive_scan.
    const auto [seq, seq_benchmark] = benchmark_with_result<std::milli>([&]{
        std::vector<std::uint64_t> sequential_exclusive_scan;
        sequential_exclusive_scan.reserve(size);
        std::exclusive_scan(nums.cbegin(), nums.cend(), std::back_inserter(sequential_exclusive_scan), static_cast<std::uint64_t>(0));

        return sequential_exclusive_scan;
    });
    std::print("Seq: ");
    print_omitted_range(seq);

    // Parallel calculation: Blelloch 1990. https://stackoverflow.com/questions/10053629/parallel-prefix-sum-fastest-implementation/12874227#12874227
    const auto [par, par_benchmark] = benchmark_with_result<std::milli>([&]{
        std::vector<std::uint64_t> parallel_exclusive_scan { nums };
        BS::thread_pool thread_pool { num_threads };

        // Phase 1: Up-Sweep.
        for (std::size_t step = 2; step < size; step *= 2){
            thread_pool.push_loop(size / step, [&](std::size_t idx_start, std::size_t idx_end){
                for (std::size_t i = idx_start; i < idx_end; ++i){
                    const std::size_t k = step * i;
                    parallel_exclusive_scan[k + step - 1] += parallel_exclusive_scan[k + step / 2 - 1];
                }
            });
            thread_pool.wait_for_tasks();
        }

        // Phase 2: Down-Sweep.
        parallel_exclusive_scan[size - 1] = 0;
        for (std::size_t step = std::bit_floor(size); step >= 2; step /= 2){
            thread_pool.push_loop(size / step, [&](std::size_t idx_start, std::size_t idx_end) {
                for (std::size_t i = idx_start; i < idx_end; ++i){
                    const std::size_t k = step * i;
                    const std::uint64_t t = std::exchange(parallel_exclusive_scan[k + step / 2 - 1],
                                                  parallel_exclusive_scan[k + step - 1]);
                    parallel_exclusive_scan[k + step - 1] += t;
                }
            });
            thread_pool.wait_for_tasks();
        }

        return parallel_exclusive_scan;
    });
    std::print("Par: ");
    print_omitted_range(par);

    // Show benchmark results.
    std::println("Seq: {:.0f}±{:.0f} ms ({} repetition).", seq_benchmark.mean, seq_benchmark.std, seq_benchmark.n);
    std::println("Par: {:.0f}±{:.0f} ms ({} repetition).", par_benchmark.mean, par_benchmark.std, par_benchmark.n);
}
