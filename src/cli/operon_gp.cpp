/* This file is part of:
 * Operon - Large Scale Genetic Programming Framework
 *
 * Licensed under the ISC License <https://opensource.org/licenses/ISC> 
 * Copyright (C) 2019 Bogdan Burlacu 
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE. 
 */

#include <cstdlib>

#include <cxxopts.hpp>
#include <fmt/core.h>

#include <tbb/task_scheduler_init.h>

#include "algorithms/gp.hpp"

#include "core/common.hpp"
#include "core/metrics.hpp"
#include "operators/crossover.hpp"
#include "operators/evaluator.hpp"
#include "operators/creator.hpp"
#include "operators/mutation.hpp"
#include "operators/recombinator.hpp"
#include "operators/selection.hpp"
#include "stat/linearscaler.hpp"

#include "util.hpp"

using namespace Operon;

int main(int argc, char* argv[])
{
    cxxopts::Options opts("operon_cli", "C++ large-scale genetic programming");

    opts.add_options()
        ("dataset", "Dataset file name (csv) (required)", cxxopts::value<std::string>())
        ("train", "Training range specified as start:end (required)", cxxopts::value<std::string>())
        ("test", "Test range specified as start:end", cxxopts::value<std::string>())
        ("target", "Name of the target variable (required)", cxxopts::value<std::string>())
        ("population-size", "Population size", cxxopts::value<size_t>()->default_value("1000"))
        ("seed", "Random number seed", cxxopts::value<operon::rand_t::result_type>()->default_value("0"))
        ("generations", "Number of generations", cxxopts::value<size_t>()->default_value("1000"))
        ("evaluations", "Evaluation budget", cxxopts::value<size_t>()->default_value("1000000"))
        ("iterations", "Local optimization iterations", cxxopts::value<size_t>()->default_value("50"))
        ("selection-pressure", "Selection pressure", cxxopts::value<size_t>()->default_value("100"))
        ("maxlength", "Maximum length", cxxopts::value<size_t>()->default_value("50"))
        ("maxdepth", "Maximum depth", cxxopts::value<size_t>()->default_value("12"))
        ("crossover-probability", "The probability to apply crossover", cxxopts::value<operon::scalar_t>()->default_value("1.0"))
        ("mutation-probability", "The probability to apply mutation", cxxopts::value<operon::scalar_t>()->default_value("0.25"))
        ("enable-symbols", "Comma-separated list of enabled symbols (add, sub, mul, div, exp, log, sin, cos, tan, sqrt, cbrt)", cxxopts::value<std::string>())
        ("disable-symbols", "Comma-separated list of disabled symbols (add, sub, mul, div, exp, log, sin, cos, tan, sqrt, cbrt)", cxxopts::value<std::string>())
        ("show-grammar", "Show grammar (primitive set) used by the algorithm")
        ("threads", "Number of threads to use for parallelism", cxxopts::value<size_t>()->default_value("0"))
        ("debug", "Debug mode (more information displayed)")("help", "Print help");

    auto result = opts.parse(argc, argv);
    if (result.arguments().empty() || result.count("help") > 0) {
        fmt::print("{}\n", opts.help());
        exit(EXIT_SUCCESS);
    }

    // parse and set default values
    GeneticAlgorithmConfig config;
    config.Generations = result["generations"].as<size_t>();
    config.PopulationSize = result["population-size"].as<size_t>();
    config.Evaluations = result["evaluations"].as<size_t>();
    config.Iterations = result["iterations"].as<size_t>();
    config.CrossoverProbability = result["crossover-probability"].as<operon::scalar_t>();
    config.MutationProbability = result["mutation-probability"].as<operon::scalar_t>();
    auto maxLength = result["maxlength"].as<size_t>();
    auto maxDepth = result["maxdepth"].as<size_t>();

    // parse remaining config options
    Range trainingRange;
    Range testRange;
    std::optional<Dataset> optionalDataset;
    std::string fileName; // data file name
    std::string target;
    bool showGrammar = false;
    auto threads = tbb::task_scheduler_init::default_num_threads();
    operon::rand_t::result_type seed = std::random_device {}();
    GrammarConfig grammarConfig = Grammar::Arithmetic;

    try {
        for (auto kv : result.arguments()) {
            auto& key = kv.key();
            auto& value = kv.value();
            if (key == "dataset") {
                fileName = value;
                optionalDataset = std::make_optional(Dataset(fileName, true));
            }
            if (key == "train") {
                trainingRange = ParseRange(value);
            }
            if (key == "test") {
                testRange = ParseRange(value);
            }
            if (key == "target") {
                target = value;
            }
            if (key == "population-size") {
                config.PopulationSize = kv.as<size_t>();
            }
            if (key == "seed") {
                seed = kv.as<operon::rand_t::result_type>();
            }
            if (key == "selection-pressure") {
                config.MaxSelectionPressure = kv.as<size_t>();
            }
            if (key == "generations") {
                config.Generations = kv.as<size_t>();
            }
            if (key == "evaluations") {
                config.Evaluations = kv.as<size_t>();
            }
            if (key == "iterations") {
                config.Iterations = kv.as<size_t>();
            }
            if (key == "maxlength") {
                maxLength = kv.as<size_t>();
            }
            if (key == "maxdepth") {
                maxDepth = kv.as<size_t>();
            }
            if (key == "enable-symbols") {
                auto mask = ParseGrammarConfig(value);
                grammarConfig |= mask;
            }
            if (key == "disable-symbols") {
                auto mask = ~ParseGrammarConfig(value);
                grammarConfig &= mask;
            }
            if (key == "threads") {
                threads = kv.as<size_t>();
            }
            //if (key == "debug")
            //{
            //    debug = true;
            //}
            if (key == "show-grammar") {
                showGrammar = true;
            }
        }

        if (showGrammar) {
            Grammar tmpGrammar;
            tmpGrammar.SetConfig(grammarConfig);
            for (auto i = 0u; i < NodeTypes::Count; ++i) {
                auto type = static_cast<NodeType>(1u << i);
                auto n = Node(type);
                if (tmpGrammar.IsEnabled(type))
                {
                    fmt::print("{}\t{}\n", n.Name(), tmpGrammar.GetFrequency(type));
                }
            }
            return 0;
        }

        const auto& dataset = optionalDataset.value();

        if (result.count("dataset") == 0) {
            fmt::print(stderr, "{}\n{}\n", "Error: no dataset given.", opts.help());
            exit(EXIT_FAILURE);
        }
        if (result.count("target") == 0) {
            fmt::print(stderr, "{}\n{}\n", "Error: no target variable given.", opts.help());
            exit(EXIT_FAILURE);
        }

        if (result.count("train") == 0) {
            trainingRange = { 0, 2 * dataset.Rows() / 3 }; // by default use 66% of the data as training
        }
        // validate training range
        if (trainingRange.Start() >= dataset.Rows() || trainingRange.End() > dataset.Rows()) {
            fmt::print(stderr, "The training range {}:{} exceeds the available data range ({} rows)\n", trainingRange.Start(), trainingRange.End(), dataset.Rows());
            exit(EXIT_FAILURE);
        }
        if (trainingRange.Start() > trainingRange.End()) {
            fmt::print(stderr, "Invalid training range {}:{}\n", trainingRange.Start(), trainingRange.End());
            exit(EXIT_FAILURE);
        }
        if (result.count("test") == 0) {
            // if no test range is specified, we try to infer a reasonable range based on the trainingRange
            if (trainingRange.Start() > 0) {
                testRange = { 0, trainingRange.Start() };
            } else if (trainingRange.End() < dataset.Rows()) {
                testRange = { trainingRange.End(), dataset.Rows() };
            } else {
                testRange = { 0, 0 };
            }
        }
        operon::rand_t random(seed);

        auto variables = dataset.Variables();
        auto problem = Problem(dataset, variables, target, trainingRange, testRange);
        problem.GetGrammar().SetConfig(grammarConfig);
        auto inputs = problem.InputVariables();

        tbb::task_scheduler_init init(threads);

        using Ind = Individual<1>;
        using Evaluator = RSquaredEvaluator<Ind>;
        //using Evaluator = NormalizedMeanSquaredErrorEvaluator<Ind>;
        Evaluator evaluator(problem);
        evaluator.LocalOptimizationIterations(config.Iterations);
        evaluator.Budget(config.Evaluations);

        const gsl::index idx { 0 };
        TournamentSelector<Individual<1>, idx, Evaluator::Maximization> selector(5);
        //ProportionalSelector<Ind, idx, Evaluator::Maximization> selector;
        //RandomSelector<Ind, idx, Evaluator::Maximization> selector;

        //auto creator  = FullTreeCreator(5, maxLength);
        std::uniform_int_distribution<size_t> sizeDistribution(1, maxLength / 4);
        //std::normal_distribution<operon::scalar_t> sizeDistribution(25, 7);
        auto creator  = BalancedTreeCreator { sizeDistribution, maxDepth, maxLength };
        //auto creator = RampedHalfAndHalfCreator { maxDepth, maxLength };
        auto crossover = SubtreeCrossover { 0.9, maxDepth, maxLength };
        auto mutator = MultiMutation {};
        auto onePoint = OnePointMutation {};
        auto changeVar = ChangeVariableMutation { inputs };
        mutator.Add(onePoint, 1.0);
        mutator.Add(changeVar, 1.0);
        BasicRecombinator recombinator(evaluator, selector, crossover, mutator);
        //BroodRecombinator recombinator(evaluator, selector, crossover, mutator);
        //recombinator.BroodSize(10);
        //recombinator.BroodTournamentSize(10);
        //OffspringSelectionRecombinator recombinator { evaluator, selector, crossover, mutator };
        //recombinator.MaxSelectionPressure(config.MaxSelectionPressure);

        auto t0 = std::chrono::high_resolution_clock::now();
        GeneticProgrammingAlgorithm gp { problem, config, creator, recombinator };

        auto targetValues = problem.TargetValues();
        auto trainingRange = problem.TrainingRange();
        auto testRange = problem.TestRange();
        auto targetTrain = targetValues.subspan(trainingRange.Start(), trainingRange.Size());
        auto targetTest = targetValues.subspan(testRange.Start(), testRange.Size());

        // some boilerplate for reporting results
        auto getBest = [&](const gsl::span<const Ind> pop) {
            auto [minElem, maxElem] = std::minmax_element(pop.begin(), pop.end(), [&](const auto& lhs, const auto& rhs) { return lhs.Fitness[idx] < rhs.Fitness[idx]; });
            return Evaluator::Maximization ? *maxElem : *minElem;
        };

//        char sizes[] = " KMGT"; 
//        auto bytesToSize = [&](size_t bytes) -> std::string {
//            auto p = static_cast<size_t>(std::floor(std::log(bytes) / std::log(1024)));
//            auto i = static_cast<size_t>(std::round(bytes / std::pow(1024, p)));
//            return fmt::format("{} {}b", bytes / std::pow(1024, i), sizes[i]);
//        };

        auto report = [&]() {
            auto pop            = gp.Parents();
            auto best           = getBest(pop);
            auto estimatedTrain = Evaluate<operon::scalar_t>(best.Genotype, problem.GetDataset(), trainingRange);
            auto estimatedTest  = Evaluate<operon::scalar_t>(best.Genotype, problem.GetDataset(), testRange);

            // scale values
            auto [a, b] = LinearScalingCalculator::Calculate(estimatedTrain.begin(), estimatedTrain.end(), targetTrain.begin());
            std::transform(estimatedTrain.begin(), estimatedTrain.end(), estimatedTrain.begin(), [a = a, b = b](operon::scalar_t v) { return b * v + a; });
            std::transform(estimatedTest.begin(), estimatedTest.end(), estimatedTest.begin(), [a = a, b = b](operon::scalar_t v) { return b * v + a; });
            
            auto r2Train    = RSquared(estimatedTrain, targetTrain);
            auto r2Test     = RSquared(estimatedTest, targetTest);

            auto nmseTrain  = NormalizedMeanSquaredError(estimatedTrain, targetTrain);
            auto nmseTest   = NormalizedMeanSquaredError(estimatedTest, targetTest);

            auto avgLength  = std::transform_reduce(std::execution::par_unseq, pop.begin(), pop.end(), 0.0, std::plus<size_t> {}, [](const auto& ind) { return ind.Genotype.Length(); }) / pop.size();
            auto avgQuality = std::transform_reduce(std::execution::par_unseq, pop.begin(), pop.end(), 0.0, std::plus<operon::scalar_t> {}, [=](const auto& ind) { return ind[idx]; }) / pop.size();

            auto t1         = std::chrono::high_resolution_clock::now();

            auto elapsed    = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() / 1000.0;

            // calculate memory consumption
            size_t totalMemory = 0u;
            totalMemory += std::transform_reduce(std::execution::par_unseq, 
                    pop.begin(), 
                    pop.end(), 
                    0U, 
                    std::plus<operon::scalar_t> {}, [](const auto& ind) { return sizeof(ind) + sizeof(ind.Genotype) + sizeof(Node) * ind.Genotype.Nodes().capacity(); });

            fmt::print("{:.4f}\t{}\t", elapsed, gp.Generation() + 1);
            fmt::print("{:.4f}\t{:.4f}\t{:.4f}\t{:.4f}\t", r2Train, r2Test, nmseTrain, nmseTest);
            fmt::print("{:.4f}\t{:.1f}\t{}\t{}\t{}\n", avgQuality, avgLength, evaluator.FitnessEvaluations(), evaluator.LocalEvaluations(), evaluator.TotalEvaluations());
            //fmt::print("{}\n", bytesToSize(totalMemory));
        };

        gp.Run(random, report);
    } catch (std::exception& e) {
        fmt::print("{}\n", e.what());
        std::exit(EXIT_FAILURE);
    }

    return 0;
}
