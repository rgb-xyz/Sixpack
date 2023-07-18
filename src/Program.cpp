#include "Program.h"
#include "Exception.h"
#include <algorithm>
#include <array>
#include <format>

SIXPACK_NAMESPACE_BEGIN

namespace {

    using ScalarOpcodeFunction = void (*)(Program::Scalar*            output,
                                          const Program::Scalar*      memory,
                                          const Program::Instruction& instruction);
    using VectorOpcodeFunction = void (*)(Program::Vector*            output,
                                          const Program::Vector*      memory,
                                          const Program::Instruction& instruction);

    static constexpr std::array<ScalarOpcodeFunction, 12> SCALAR_OPCODE_FUNCTIONS = {
        // Opcode::NOP
        [](Program::Scalar*, const Program::Scalar*, const Program::Instruction&) {},
        // Opcode::ADD
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.operand1] + memory[instruction.operand2];
        },
        // Opcode::ADD_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate + memory[instruction.operand2];
        },
        // Opcode::SUBTRACT
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.operand1] - memory[instruction.operand2];
        },
        // Opcode::SUBTRACT_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate - memory[instruction.operand2];
        },
        // Opcode::MULTIPLY
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.operand1] * memory[instruction.operand2];
        },
        // Opcode::MULTIPLY_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate * memory[instruction.operand2];
        },
        // Opcode::DIVIDE
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.operand1] / memory[instruction.operand2];
        },
        // Opcode::DIVIDE_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate / memory[instruction.operand2];
        },
        // Opcode::POWER
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = std::pow(memory[instruction.operand1], memory[instruction.operand2]);
        },
        // Opcode::SINCOS
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            const Real argument              = memory[instruction.operand2];
            const Real sine                  = std::sin(argument);
            const Real cosine                = std::cos(argument);
            output[0]                        = sine;
            output[instruction.displacement] = cosine;
        },
        // Opcode::CALL
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.function(memory[instruction.operand2]);
        }
    };

    static constexpr std::array<VectorOpcodeFunction, 12> VECTOR_OPCODE_FUNCTIONS = {
        // Opcode::NOP
        [](Program::Vector*, const Program::Vector*, const Program::Instruction&) {},
        // Opcode::ADD
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument1 = memory[instruction.operand1];
            const Program::Vector& argument2 = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = argument1[i] + argument2[i];
            }
            *output = result;
        },
        // Opcode::ADD_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.immediate + argument[i];
            }
            *output = result;
        },
        // Opcode::SUBTRACT
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument1 = memory[instruction.operand1];
            const Program::Vector& argument2 = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = argument1[i] - argument2[i];
            }
            *output = result;
        },
        // Opcode::SUBTRACT_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.immediate - argument[i];
            }
            *output = result;
        },
        // Opcode::MULTIPLY
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument1 = memory[instruction.operand1];
            const Program::Vector& argument2 = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = argument1[i] * argument2[i];
            }
            *output = result;
        },
        // Opcode::MULTIPLY_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.immediate * argument[i];
            }
            *output = result;
        },
        // Opcode::DIVIDE
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument1 = memory[instruction.operand1];
            const Program::Vector& argument2 = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = argument1[i] / argument2[i];
            }
            *output = result;
        },
        // Opcode::DIVIDE_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.immediate / argument[i];
            }
            *output = result;
        },
        // Opcode::POWER
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument1 = memory[instruction.operand1];
            const Program::Vector& argument2 = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = pow(argument1[i], argument2[i]);
            }
            *output = result;
        },
        // Opcode::SINCOS
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        sines, cosines; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                sines[i]   = std::sin(argument[i]);
                cosines[i] = std::cos(argument[i]);
            }
            output[0]                        = sines;
            output[instruction.displacement] = cosines;
        },
        // Opcode::CALL
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector        result; // prevents aliasing
            const Program::Vector& argument = memory[instruction.operand2];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.function(argument[i]);
            }
            *output = result;
        }
    };

} // anonymous namespace

bool Program::Instruction::operator==(const Instruction& other) const {
    if (opcode == other.opcode) {
        // clang-format off
        switch (opcode) {
        case Opcode::NOP:          return false; // NOPs are never merged.
        case Opcode::ADD:          return operand1  == other.operand1  && operand2 == other.operand2;
        case Opcode::ADD_IMM:      return immediate == other.immediate && operand2 == other.operand2;
        case Opcode::SUBTRACT:     return operand1  == other.operand1  && operand2 == other.operand2;
        case Opcode::SUBTRACT_IMM: return immediate == other.immediate && operand2 == other.operand2;
        case Opcode::MULTIPLY:     return operand1  == other.operand1  && operand2 == other.operand2;
        case Opcode::MULTIPLY_IMM: return immediate == other.immediate && operand2 == other.operand2;
        case Opcode::DIVIDE:       return operand1  == other.operand1  && operand2 == other.operand2;
        case Opcode::DIVIDE_IMM:   return immediate == other.immediate && operand2 == other.operand2;
        case Opcode::POWER:        return operand1  == other.operand1  && operand2 == other.operand2;
        case Opcode::CALL:         return function  == other.function  && operand2 == other.operand2;
        // clang-format on
        default: assert(false);
        }
    }
    return false;
}

Program::Program(Variables&&    inputs,
                 Variables&&    outputs,
                 Constants&&    constants,
                 Instructions&& instructions,
                 Comments&&     comments)
    : mInputs(std::move(inputs))
    , mOutputs(std::move(outputs))
    , mConstants(std::move(constants))
    , mInstructions(std::move(instructions))
    , mComments(std::move(comments)) {
    assert(mConstants.values.empty() ||
           (SCRATCHPAD_ADDRESS - mConstants.memoryOffset >= mConstants.values.size() &&
            mConstants.memoryOffset + mConstants.values.size() <= mInstructions.memoryOffset));
    assert(std::none_of(mInputs.begin(), mInputs.end(), [&](const auto& input) {
        return input.second >= mInstructions.memoryOffset ||
               (input.second >= mConstants.memoryOffset &&
                input.second < mConstants.memoryOffset + mConstants.values.size());
    }));
    assert(std::none_of(mOutputs.begin(), mOutputs.end(), [&](const auto& output) {
        return output.second == SCRATCHPAD_ADDRESS;
    }));
}

Program::Address Program::getInputAddress(StringView name) const {
    const auto inputIt = mInputs.find(String(name));
    if (inputIt == mInputs.end()) {
        throw Exception(std::format("Unknown input '{}'", name));
    }
    return inputIt->second;
}

Program::Address Program::getOutputAddress(StringView name) const {
    const auto outputIt = mOutputs.find(String(name));
    if (outputIt == mOutputs.end()) {
        throw Exception(std::format("Unknown output '{}'", name));
    }
    return outputIt->second;
}

Program::ScalarMemory Program::allocateScalarMemory() const {
    ScalarMemory memory(mInstructions.memoryOffset + mInstructions.instructions.size(), Scalar{});
    std::copy(mConstants.values.begin(), mConstants.values.end(), memory.begin() + mConstants.memoryOffset);
    return memory;
}

Program::VectorMemory Program::allocateVectorMemory() const {
    VectorMemory memory(mInstructions.memoryOffset + mInstructions.instructions.size(), Vector{});
    std::copy(mConstants.values.begin(), mConstants.values.end(), memory.begin() + mConstants.memoryOffset);
    return memory;
}

void Program::run(ScalarMemory& memory) const {
    assert(memory.size() == mInstructions.memoryOffset + mInstructions.instructions.size());
    const Scalar* inputs = memory.data();
    Scalar*       output = memory.data() + mInstructions.memoryOffset;
    for (const Instruction& instruction : mInstructions.instructions) {
        SCALAR_OPCODE_FUNCTIONS[int(instruction.opcode)](output, inputs, instruction);
        ++output;
    }
}

void Program::run(VectorMemory& memory) const {
    assert(memory.size() == mInstructions.memoryOffset + mInstructions.instructions.size());
    const Vector* inputs = memory.data();
    Vector*       output = memory.data() + mInstructions.memoryOffset;
    for (const Instruction& instruction : mInstructions.instructions) {
        VECTOR_OPCODE_FUNCTIONS[int(instruction.opcode)](output, inputs, instruction);
        ++output;
    }
}

SIXPACK_NAMESPACE_END