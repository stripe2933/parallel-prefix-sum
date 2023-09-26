//
// Created by gomkyung2 on 2023/09/26.
//

module;

import std;

export module benchmark;

export template <std::floating_point T, typename Ratio>
struct NumberStatistics{
    const std::size_t n;
    const T mean, std;

    template <typename Floats>
        requires std::ranges::forward_range<Floats> && std::ranges::sized_range<Floats> &&
                 std::is_convertible_v<std::ranges::range_value_t<Floats>, T>
    static constexpr NumberStatistics from(Floats &&measured){
        const std::size_t size = measured.size();

        // Variance = E[X^2] - E[X]^2
        const T mean = std::reduce(measured.begin(), measured.end()) / size;
        const T x2_mean = std::transform_reduce(measured.begin(), measured.end(), 0.f, std::plus<>{}, [](T x) { return x*x; }) / size;
        const T std = std::sqrt(x2_mean - mean * mean);

        return { size, mean, std };
    }
};

template <typename T>
void doNotOptimizeAway(T& val) {
#    if defined(__clang__)
    // NOLINTNEXTLINE(hicpp-no-assembler)
    asm volatile("" : "+r,m"(val) : : "memory");
#    else
    // NOLINTNEXTLINE(hicpp-no-assembler)
    asm volatile("" : "+m,r"(val) : : "memory");
#    endif
}

/**
 * @brief Benchmark the execution time of a function.
 * @tparam Ratio The ratio of the unit of the elapsed time.
 * @tparam Fn The type of the function to be benchmarked.
 * @param fn The function to be benchmarked.
 * @param rep The number of repetitions of the benchmark. More repetitions will result in more accurate statistics.
 * @return The statistics of the elapsed times in \p float , contains mean and standard deviation with given time unit ratio.
 */
export template <typename Ratio, typename Fn>
NumberStatistics<float, Ratio> benchmark(Fn &&fn, std::size_t rep = 10){
    std::vector<float> measured;
    for (std::size_t it = 0; it < rep; ++it){
        // Measure execution time.
        const auto start = std::chrono::high_resolution_clock::now();
        if constexpr (std::is_void_v<std::invoke_result_t<Fn>>){
            fn();
        }
        else{
            auto result = fn();
            doNotOptimizeAway(result);
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float, Ratio> elapsed = end - start;

        // Push to the records.
        measured.push_back(elapsed.count());
    }

    // Return elapsed time statistics.
    return NumberStatistics<float, Ratio>::from(measured);
}

/**
 * @brief Benchmark the execution time of a function, and return the last execution result.
 * @tparam Ratio The ratio of the unit of the elapsed time.
 * @tparam Fn The type of the function to be benchmarked.
 * @param fn The function to be benchmarked. The function must return a value.
 * @param rep The number of repetitions of the benchmark. More repetitions will result in more accurate statistics.
 * @return The pair of last execution result and statistics of the elapsed times in \p float , contains mean and
 * standard deviation with given time unit ratio.
 */
export template <typename Ratio, typename Fn>
    requires (not std::is_void_v<std::invoke_result_t<Fn>>)
std::pair<std::invoke_result_t<Fn>, NumberStatistics<float, Ratio>> benchmark_with_result(Fn &&fn, std::size_t rep = 10){
    std::vector<float> measured;
    for (std::size_t it = 0; it < rep; ++it){
        // Measure execution time.
        const auto start = std::chrono::high_resolution_clock::now();
        auto result = fn();
        doNotOptimizeAway(result);
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float, Ratio> elapsed = end - start;

        // Push to the records.
        measured.push_back(elapsed.count());

        if (it == rep - 1){
            // Return elapsed time statistics.
            return { std::move(result), NumberStatistics<float, Ratio>::from(measured) };
        }
    }

    std::unreachable();
}