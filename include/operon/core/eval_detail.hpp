/* This file is part of:
 * Operon - Large Scale Genetic Programming Framework
 *
 * Licensed under the ISC License <https://opensource.org/licenses/ISC> 
 * Copyright (C) 2020 Bogdan Burlacu 
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

#ifndef OPERON_EVAL_DETAIL
#define OPERON_EVAL_DETAIL

#include "node.hpp"
#include <Eigen/Dense>

namespace Operon {
constexpr size_t BATCHSIZE = 64;

namespace detail {
    // addition up to 5 arguments
    template <typename T, Operon::NodeType N = NodeType::Add>
    struct op {
        using Arg = Eigen::Ref<typename Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>::ColXpr, Eigen::Unaligned, Eigen::Stride<BATCHSIZE, 1>>;

        inline void apply(Arg ret, Arg arg1) { ret = arg1; }

        template <typename... Args>
        inline void apply(Arg ret, Arg arg1, Args... args) { ret = arg1 + (args + ...); }

        inline void accumulate(Arg ret, Arg arg1) { ret += arg1; }

        template <typename... Args>
        inline void accumulate(Arg ret, Arg arg1, Args... args) { ret += arg1 + (args + ...); }
    };

    template <typename T>
    struct op<T, Operon::NodeType::Sub> {
        using Arg = Eigen::Ref<typename Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>::ColXpr, Eigen::Unaligned, Eigen::Stride<BATCHSIZE, 1>>;

        inline void apply(Arg ret, Arg arg1) { ret = -arg1; }

        template <typename... Args>
        inline void apply(Arg ret, Arg arg1, Args... args) { ret = arg1 - (args + ...); }

        inline void accumulate(Arg ret, Arg arg1) { ret -= arg1; }

        template <typename... Args>
        inline void accumulate(Arg ret, Arg arg1, Args... args) { ret -= arg1 + (args + ...); }
    };

    template <typename T>
    struct op<T, Operon::NodeType::Mul> {
        using Arg = Eigen::Ref<typename Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>::ColXpr, Eigen::Unaligned, Eigen::Stride<BATCHSIZE, 1>>;

        inline void apply(Arg ret, Arg arg1) { ret = arg1; }

        template <typename... Args>
        inline void apply(Arg ret, Arg arg1, Args... args) { ret = arg1 * (args * ...); }

        inline void accumulate(Arg ret, Arg arg1) { ret *= arg1; }

        template <typename... Args>
        inline void accumulate(Arg ret, Arg arg1, Args... args) { ret *= arg1 * (args * ...); }
    };

    template <typename T>
    struct op<T, Operon::NodeType::Div> {
        using Arg = Eigen::Ref<typename Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>::ColXpr, Eigen::Unaligned, Eigen::Stride<BATCHSIZE, 1>>;

        template <bool acc = false>
        inline void apply(Arg ret, Arg arg1) { ret = arg1.inverse(); }

        template <typename... Args>
        inline void apply(Arg ret, Arg arg1, Args... args) { ret = arg1 / (args * ...); }

        inline void accumulate(Arg ret, Arg arg1) { ret /= arg1; }

        template <typename... Args>
        inline void accumulate(Arg ret, Arg arg1, Args... args) { ret /= arg1 * (args * ...); }
    };

    // dispatching mechanism
    // compared to the simple/naive way of evaluating n-ary symbols, this method has the following advantages:
    // 1) improved performance: the naive method accumulates into the result for each argument, leading to unnecessary assignments 
    // 2) improving floating-point precision by minimizing the number of intermediate steps.
    //    if arity > 5, one accumulation is performed every 5 args
    template <typename T, Operon::NodeType N>
    inline void dispatch_op(Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>& m, Operon::Vector<Node> const& nodes, size_t parentIndex)
    {
        op<T, N> op;

        int arity = nodes[parentIndex].Arity;
        auto nextArg = [&](size_t i) { return i - (nodes[i].Length + 1); };
        auto result = m.col(parentIndex);

        auto arg1 = parentIndex - 1;
        bool continued = false;

        while (arity > 0) {
            switch (arity) {
            case 1: {
                continued
                    ? op.accumulate(result, m.col(arg1))
                    : op.apply(result, m.col(arg1));
                arity = 0;
                break;
            }
            case 2: {
                auto arg2 = nextArg(arg1);
                continued
                    ? op.accumulate(result, m.col(arg1), m.col(arg2))
                    : op.apply(result, m.col(arg1), m.col(arg2));
                arity = 0;
                break;
            }
            case 3: {
                auto arg2 = nextArg(arg1), arg3 = nextArg(arg2);
                continued
                    ? op.accumulate(result, m.col(arg1), m.col(arg2), m.col(arg3))
                    : op.apply(result, m.col(arg1), m.col(arg2), m.col(arg3));
                arity = 0;
                break;
            }
            case 4: {
                auto arg2 = nextArg(arg1), arg3 = nextArg(arg2), arg4 = nextArg(arg3);
                continued
                    ? op.accumulate(result, m.col(arg1), m.col(arg2), m.col(arg3), m.col(arg4))
                    : op.apply(result, m.col(arg1), m.col(arg2), m.col(arg3), m.col(arg4));
                arity = 0;
                break;
            }
            default: {
                auto arg2 = nextArg(arg1), arg3 = nextArg(arg2), arg4 = nextArg(arg3), arg5 = nextArg(arg4);
                continued
                    ? op.accumulate(result, m.col(arg1), m.col(arg2), m.col(arg3), m.col(arg4), m.col(arg5))
                    : op.apply(result, m.col(arg1), m.col(arg2), m.col(arg3), m.col(arg4), m.col(arg5));
                arity -= 5;
                arg1 = nextArg(arg5);
                break;
            }
            }
            continued = true;
        }
    }

    template <typename T, Operon::NodeType N>
    inline void dispatch_op_simple_binary(Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>& m, Operon::Vector<Node> const& nodes, size_t parentIndex)
    {
        op<T, N> op;
        auto r = m.col(parentIndex);
        size_t i = parentIndex - 1;
        size_t arity = nodes[parentIndex].Arity;

        if (arity == 1) {
            op.apply(r, m.col(i));
        } else {
            auto j = i - (nodes[i].Length + 1);
            op.apply(r, m.col(i), m.col(j));
        }
    }

    template <typename T, Operon::NodeType N>
    inline void dispatch_op_simple_nary(Eigen::DenseBase<Eigen::Array<T, BATCHSIZE, Eigen::Dynamic, Eigen::ColMajor>>& m, Operon::Vector<Node> const& nodes, size_t parentIndex)
    {
        op<T, N> op;
        auto r = m.col(parentIndex);
        size_t arity = nodes[parentIndex].Arity;

        auto i = parentIndex - 1;

        if (arity == 1) {
            op.apply(r, m.col(i));
        } else {
            r = m.col(i);

            for (size_t k = 1; k < arity; ++k) {
                i -= nodes[i].Length + 1;
                op.accumulate(r, m.col(i));
            }
        }
    }

} // namespace detail
} // namespace Operon

#endif
