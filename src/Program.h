#pragma once
#include "Common.h"
#include <memory>
#include <unordered_map>
#include <vector>

SIXPACK_NAMESPACE_BEGIN

namespace asg {
    class Term;
}

class Program {
public:
    using Scalar = Real;

    struct alignas(32) Vector {
        static constexpr int SIZE = 4;

        Vector() = default;
        constexpr Vector(const Real value)
            : mValues{ value, value, value, value } {}
        constexpr Vector(const Real v0, const Real v1, const Real v2, const Real v3)
            : mValues{ v0, v1, v2, v3 } {}
        constexpr Vector(const Vector& other)
            : mValues{ other.mValues[0], other.mValues[1], other.mValues[2], other.mValues[3] } {}

        FORCEINLINE Vector operator+(const Vector other) const {
            return { mValues[0] + other.mValues[0],
                     mValues[1] + other.mValues[1],
                     mValues[2] + other.mValues[2],
                     mValues[3] + other.mValues[3] };
        }
        FORCEINLINE Vector operator-(const Vector other) const {
            return { mValues[0] - other.mValues[0],
                     mValues[1] - other.mValues[1],
                     mValues[2] - other.mValues[2],
                     mValues[3] - other.mValues[3] };
        }
        FORCEINLINE Vector operator*(const Vector other) const {
            return { mValues[0] * other.mValues[0],
                     mValues[1] * other.mValues[1],
                     mValues[2] * other.mValues[2],
                     mValues[3] * other.mValues[3] };
        }
        FORCEINLINE Vector operator/(const Vector other) const {
            return { mValues[0] / other.mValues[0],
                     mValues[1] / other.mValues[1],
                     mValues[2] / other.mValues[2],
                     mValues[3] / other.mValues[3] };
        }

        FORCEINLINE Real  operator[](const auto index) const { return mValues[index]; }
        FORCEINLINE Real& operator[](const auto index) { return mValues[index]; }

    private:
        Real mValues[SIZE];
    };

    using Address      = uint32_t;
    using ScalarMemory = std::vector<Scalar>;
    using VectorMemory = std::vector<Vector>;

    static constexpr Address SCRATCHPAD_ADDRESS = 0;

    using Variables = std::unordered_map<String, Address>;

    struct Constants {
        Address           memoryOffset;
        std::vector<Real> values;
    };

    enum class Opcode : uint8_t {
        NOP,                          //
        ADD,                          // output <-- memory[source] + memory[operand]
        ADD_IMM,                      // output <-- immediate      + memory[operand]
        SUBTRACT,                     // output <-- memory[source] - memory[operand]
        SUBTRACT_IMM,                 // output <-- immediate      - memory[operand]
        MULTIPLY,                     // output <-- memory[source] * memory[operand]
        MULTIPLY_IMM,                 // output <-- immediate      * memory[operand]
        DIVIDE,                       // output <-- memory[source] / memory[operand]
        DIVIDE_IMM,                   // output <-- immediate      / memory[operand]
        POWER,                        // output <-- memory[source] ^ memory[operand]
        CALL,                         // output <-- function(memory[operand])
        /*** Intrinsic Functions ***/ //
        SIN,                          // output        <-- sin(memory[operand])
        COS,                          // output        <-- cos(memory[operand])
        SINCOS                        // output        <-- sin(memory[operand])
                                      // output+target <-- cos(memory[operand])
    };

    struct alignas(16) Instruction {
        Opcode  opcode;
        Address operand;
        union {
            Address      source;
            Real         immediate;
            RealFunction function;
            ptrdiff_t    target;
        };

        bool operator==(const Instruction& other) const;
    };
    static_assert(sizeof(Instruction) == 16);

    struct Instructions {
        Address                  memoryOffset;
        std::vector<Instruction> instructions;
    };

    using Comments = std::unordered_map<Address, String>;

private:
    const Variables    mInputs;
    const Variables    mOutputs;
    const Constants    mConstants;
    const Instructions mInstructions;
    const Comments     mComments;

public:
    Program(Variables&&    inputs,
            Variables&&    outputs,
            Constants&&    constants,
            Instructions&& instructions,
            Comments&&     comments);

    const Variables&    inputs() const { return mInputs; }
    const Variables&    outputs() const { return mOutputs; }
    const Constants&    constants() const { return mConstants; }
    const Instructions& instructions() const { return mInstructions; }
    const Comments&     comments() const { return mComments; }

    Address getInputAddress(StringView name) const;
    Address getOutputAddress(StringView name) const;

    ScalarMemory allocateScalarMemory() const;
    VectorMemory allocateVectorMemory() const;

    void run(ScalarMemory& memory) const;
    void run(VectorMemory& memory) const;
};

SIXPACK_NAMESPACE_END
