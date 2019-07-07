#ifndef SYMBOL_H
#define SYMBOL_H

#include <cstdint>
#include <functional>

#include <fmt/format.h>

namespace Operon {
    enum NodeType : uint16_t {
        // terminal nodes
        Add      = 1u << 0,
        Mul      = 1u << 1,
        Sub      = 1u << 2,
        Div      = 1u << 3,
        Log      = 1u << 4,
        Exp      = 1u << 5,
        Sin      = 1u << 6,
        Cos      = 1u << 7,
        Tan      = 1u << 8,
        Sqrt     = 1u << 9,
        Cbrt     = 1u << 10,
        Constant = 1u << 11,
        Variable = 1u << 12
    };

    namespace {
        const std::unordered_map<uint16_t, std::string> nodeNames = {
            { NodeType::Add,      "Add"      },
            { NodeType::Mul,      "Mul"      },
            { NodeType::Sub,      "Sub"      },
            { NodeType::Div,      "Div"      },
            { NodeType::Log,      "Log"      },
            { NodeType::Exp,      "Exp"      },
            { NodeType::Log,      "Log"      },
            { NodeType::Sin,      "Sin"      },
            { NodeType::Cos,      "Cos"      },
            { NodeType::Tan,      "Tan"      },
            { NodeType::Sqrt,     "Sqrt"     },
            { NodeType::Cbrt,     "Cbrt"     },
            { NodeType::Constant, "Constant" },
            { NodeType::Variable, "Variable" }
        };
    }

    struct Node {
        NodeType Type; 

        bool     IsLeaf;
        bool     IsCommutative;
        bool     IsEnabled;

        uint16_t Arity; // 0-65535
        uint16_t Length;  // 0-65535

        size_t   Parent; // index of parent node
        uint64_t HashValue;
        uint64_t CalculatedHashValue; // for arithmetic terminal nodes whose hash value depends on their children

        double   Value; // value for constants or weighting factor for variables

        Node() = delete;
        explicit Node(NodeType type) noexcept : Node(type, static_cast<uint64_t>(type)) { }
        explicit Node(NodeType type, uint64_t hashValue) noexcept : Type(type), HashValue(hashValue), CalculatedHashValue(hashValue) 
        {
            IsCommutative = type < NodeType::Sub; // Add, Mul are commutative

            Arity = 0;
            if (Type < NodeType::Log) // Add, Mul
            {
                Arity = 2;
            }
            else if (Type < NodeType::Constant) // Log, Exp, Sin, Inv, Sqrt, Cbrt
            {
                Arity = 1;
            }
            Length = Arity;

            IsLeaf = Arity == 0;
            IsEnabled = true;

            Value = IsConstant() ? 1. : 0.;
        }
    
        const std::string& Name() const noexcept { return nodeNames.find(Type)->second; }

        // comparison operators
        inline bool operator==(const Node& rhs) const noexcept
        {
            return CalculatedHashValue == rhs.CalculatedHashValue;
        }

        inline bool operator!=(const Node& rhs) const noexcept
        {
            return !((*this) == rhs);
        }

        inline bool operator<(const Node& rhs) const noexcept
        {
            return HashValue == rhs.HashValue ? CalculatedHashValue < rhs.CalculatedHashValue : HashValue < rhs.HashValue;
        }

        inline bool operator<=(const Node& rhs) const noexcept
        {
            return ((*this) == rhs || (*this) < rhs);
        }

        inline bool operator>(const Node& rhs) const noexcept
        {
            return !((*this) <= rhs);
        }

        inline bool operator>=(const Node& rhs) const noexcept
        {
            return !((*this) < rhs);
        }

        template<NodeType T>
        inline bool Is() const { return T == Type; }

        inline bool IsConstant()       const { return Is<NodeType::Constant>(); }
        inline bool IsVariable()       const { return Is<NodeType::Variable>(); }
        inline bool IsAddition()       const { return Is<NodeType::Add>();      }
        inline bool IsSubtraction()    const { return Is<NodeType::Sub>();      }
        inline bool IsMultiplication() const { return Is<NodeType::Mul>();      }
        inline bool IsDivision()       const { return Is<NodeType::Div>();      }
        inline bool IsExp()            const { return Is<NodeType::Exp>();      }
        inline bool IsLog()            const { return Is<NodeType::Log>();      }
        inline bool IsSin()            const { return Is<NodeType::Sin>();      }
        inline bool IsCos()            const { return Is<NodeType::Cos>();      }
        inline bool IsTan()            const { return Is<NodeType::Tan>();      }
        inline bool IsSquareRoot()     const { return Is<NodeType::Sqrt>();     }
        inline bool IsCubeRoot()       const { return Is<NodeType::Cbrt>();     }
    };
}

namespace fmt
{
    template<>
        struct formatter<Operon::Node>
        {
            template <typename ParseContext>
                constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

            template <typename FormatContext>
                auto format(const Operon::Node &s, FormatContext &ctx)
                {
                    return format_to(ctx.begin(), "Name: {}, Hash: {}, Value: {}, Arity: {}, Length: {}", Operon::nodeNames.find(s.Type)->second, s.CalculatedHashValue, s.Value, s.Arity, s.Length);
                }
        };
}
#endif // SYMBOL_H
