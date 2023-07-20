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

    static constexpr std::array<ScalarOpcodeFunction, 14> SCALAR_OPCODE_FUNCTIONS = {
        // Opcode::NOP
        [](Program::Scalar*, const Program::Scalar*, const Program::Instruction&) {},
        // Opcode::ADD
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] + memory[instruction.operand];
        },
        // Opcode::ADD_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate + memory[instruction.operand];
        },
        // Opcode::SUBTRACT
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] - memory[instruction.operand];
        },
        // Opcode::SUBTRACT_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate - memory[instruction.operand];
        },
        // Opcode::MULTIPLY
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] * memory[instruction.operand];
        },
        // Opcode::MULTIPLY_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate * memory[instruction.operand];
        },
        // Opcode::DIVIDE
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] / memory[instruction.operand];
        },
        // Opcode::DIVIDE_IMM
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.immediate / memory[instruction.operand];
        },
        // Opcode::POWER
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = std::pow(memory[instruction.source], memory[instruction.operand]);
        },
        // Opcode::CALL
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = instruction.function(memory[instruction.operand]);
        },
        // Opcode::SIN
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = std::sin(memory[instruction.operand]);
        },
        // Opcode::COS
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            *output = std::cos(memory[instruction.operand]);
        },
        // Opcode::SINCOS
        [](Program::Scalar* output, const Program::Scalar* memory, const Program::Instruction& instruction) {
            const Program::Scalar argument = memory[instruction.operand];
            output[0]                      = std::sin(argument);
            output[instruction.target]     = std::cos(argument);
        }
    };

    static constexpr std::array<VectorOpcodeFunction, 14> VECTOR_OPCODE_FUNCTIONS = {
        // Opcode::NOP
        [](Program::Vector*, const Program::Vector*, const Program::Instruction&) {},
        // Opcode::ADD
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] + memory[instruction.operand];
        },
        // Opcode::ADD_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = Program::Vector(instruction.immediate) + memory[instruction.operand];
        },
        // Opcode::SUBTRACT
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] - memory[instruction.operand];
        },
        // Opcode::SUBTRACT_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = Program::Vector(instruction.immediate) - memory[instruction.operand];
        },
        // Opcode::MULTIPLY
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] * memory[instruction.operand];
        },
        // Opcode::MULTIPLY_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = Program::Vector(instruction.immediate) * memory[instruction.operand];
        },
        // Opcode::DIVIDE
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = memory[instruction.source] / memory[instruction.operand];
        },
        // Opcode::DIVIDE_IMM
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            *output = Program::Vector(instruction.immediate) / memory[instruction.operand];
        },
        // Opcode::POWER
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument1 = memory[instruction.source];
            const Program::Vector argument2 = memory[instruction.operand];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::pow(argument1[i], argument2[i]);
            }
            *output = result;
        },
        // Opcode::CALL
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = memory[instruction.operand];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = instruction.function(argument[i]);
            }
            *output = result;
        },
        // Opcode::SIN
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = memory[instruction.operand];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::sin(argument[i]);
            }
            *output = result;
        },
        // Opcode::COS
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = memory[instruction.operand];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::cos(argument[i]);
            }
            *output = result;
        },
        // Opcode::SINCOS
        [](Program::Vector* output, const Program::Vector* memory, const Program::Instruction& instruction) {
            Program::Vector       sines, cosines; // prevents aliasing
            const Program::Vector argument = memory[instruction.operand];
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                sines[i]   = std::sin(argument[i]);
                cosines[i] = std::cos(argument[i]);
            }
            output[0]                  = sines;
            output[instruction.target] = cosines;
        }
    };

} // anonymous namespace

bool Program::Instruction::operator==(const Instruction& other) const {
    if (opcode == other.opcode) {
        // clang-format off
        switch (opcode) {
        case Opcode::NOP:          return false; // NOPs are never merged.
        case Opcode::ADD:          return operand == other.operand && source == other.source;
        case Opcode::ADD_IMM:      return operand == other.operand && immediate == other.immediate;
        case Opcode::SUBTRACT:     return operand == other.operand && source == other.source;
        case Opcode::SUBTRACT_IMM: return operand == other.operand && immediate == other.immediate;
        case Opcode::MULTIPLY:     return operand == other.operand && source == other.source;
        case Opcode::MULTIPLY_IMM: return operand == other.operand && immediate == other.immediate;
        case Opcode::DIVIDE:       return operand == other.operand && source == other.source;
        case Opcode::DIVIDE_IMM:   return operand == other.operand && immediate == other.immediate;
        case Opcode::POWER:        return operand == other.operand && source == other.source;
        case Opcode::CALL:         return operand == other.operand && function  == other.function;
        case Opcode::SIN:          return operand == other.operand;
        case Opcode::COS:          return operand == other.operand;
        case Opcode::SINCOS:       return operand == other.operand && target == other.target;
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
