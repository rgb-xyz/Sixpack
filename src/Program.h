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

    struct Vector {
        static constexpr int SIZE = 4;

        Vector() = default;
        constexpr Vector(const Real value) {
            for (int i = 0; i < SIZE; ++i) {
                mValues[i] = value;
            }
        }
        constexpr Vector(const Vector& other) {
            for (int i = 0; i < SIZE; ++i) {
                mValues[i] = other.mValues[i];
            }
        }

        constexpr Real  operator[](int i) const { return mValues[i]; }
        constexpr Real& operator[](int i) { return mValues[i]; }

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
        NOP,          //
        ADD,          // output <-- memory[operand1] + memory[operand2]
        ADD_IMM,      // output <-- immediate        + memory[operand2]
        SUBTRACT,     // output <-- memory[operand1] - memory[operand2]
        SUBTRACT_IMM, // output <-- immediate        - memory[operand2]
        MULTIPLY,     // output <-- memory[operand1] * memory[operand2]
        MULTIPLY_IMM, // output <-- immediate        * memory[operand2]
        DIVIDE,       // output <-- memory[operand1] / memory[operand2]
        DIVIDE_IMM,   // output <-- immediate        / memory[operand2]
        POWER,        // output <-- memory[operand1] ^ memory[operand2]
        SINCOS,       // output <-- sin(memory[operand2]) ; output+displacement <-- cos(memory[operand2])
        CALL          // output <-- function(memory[operand2])
    };

    struct alignas(16) Instruction {
        union {
            Address      operand1;
            Real         immediate;
            RealFunction function;
            ptrdiff_t    displacement;
        };
        Address operand2;
        Opcode  opcode;

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
