#include <fmt/core.h>
#include "dataset.hpp"
#include "stats.hpp"
#pragma GCC diagnostic ignored "-Wreorder"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "csv.hpp"
#pragma GCC diagnostic warning "-Wreorder"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunknown-pragmas"
#include "jsf.hpp"

namespace Operon
{
    Dataset::Dataset(const std::string& file, bool hasHeader)
    {
        csv::CSVReader reader(file);
        auto ncol = reader.get_col_names().size();
        variables.resize(ncol);
        values.resize(ncol);

        // fill in variable names
        std::vector<std::string> names;
        if (hasHeader)
        {
            names = reader.get_col_names();
        }
        else
        {
            int i = 0;
            std::generate(names.begin(), names.end(), [&]() { return fmt::format("X{}", ++i); });
        }

        for (auto& row : reader)
        {
            int i = 0;
            for (auto& field : row) 
            {
                if (field.is_num())
                {
                    auto value = field.get<double>();
                    values[i++].push_back(value); 
                }
                else 
                {
                    throw std::runtime_error(fmt::format("The field {} could not be parsed as a number.", field.get()));
                }
            }
        }

        // the code below sorts the variables vector by variable name, then assigns hash values (also sorted, increasing order) and indices
        // this is done for the purpose of enabling searching with std::equal_range so we can retrieve data using name, hash value or index

        // fill in variable names 
        for (size_t i = 0; i < ncol; ++i) 
        {
            variables[i].Name = names[i];
            variables[i].Index = i;
        }

        std::sort(variables.begin(), variables.end(), [&](const Variable& a, const Variable& b) { return CompareWithSize(a.Name, b.Name); });
        // fill in variable hash values
        Random::JsfRand<64> jsf(1234);
        // generate some hash values 
        std::vector<uint64_t> hashes(ncol);
        std::generate(hashes.begin(), hashes.end(), [&]() { return jsf(); });
        std::sort(hashes.begin(), hashes.end());
        for (size_t i = 0; i < ncol; ++i)
        {
            variables[i].Hash = hashes[i];
        }
    }
}