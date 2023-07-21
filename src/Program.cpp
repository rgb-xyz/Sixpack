#include "Program.h"
#include "Exception.h"
#include <algorithm>
#include <array>
#include <format>

SIXPACK_NAMESPACE_BEGIN

namespace {

    static constexpr std::array<Executable<Program::Scalar>::Function, 14> SCALAR_FUNCTIONS = {
        // Opcode::NOP
        [](const Executable<Program::Scalar>::Instruction*) {},
        // Opcode::ADD
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = *instruction->extraInput + *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::ADD_IMM
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = Program::Scalar(instruction->immediate) + *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SUBTRACT
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = *instruction->extraInput - *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SUBTRACT_IMM
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = Program::Scalar(instruction->immediate) - *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::MULTIPLY
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = *instruction->extraInput * *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::MULTIPLY_IMM
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = Program::Scalar(instruction->immediate) * *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::DIVIDE
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = *instruction->extraInput / *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::DIVIDE_IMM
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = Program::Scalar(instruction->immediate) / *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::POWER
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = std::pow(*instruction->extraInput, *instruction->input);
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::CALL
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = instruction->callable(*instruction->input);
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SIN
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = std::sin(*instruction->input);
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::COS
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar result = std::cos(*instruction->input);
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SINCOS
        [](const Executable<Program::Scalar>::Instruction* instruction) {
            const Program::Scalar argument = *instruction->input;
            const Program::Scalar sine     = std::sin(argument);
            const Program::Scalar cosine   = std::cos(argument);
            *instruction->output           = sine;
            *instruction->extraOutput      = cosine;
            return instruction->next(instruction + 1);
        }
    };

    static constexpr std::array<Executable<Program::Vector>::Function, 14> VECTOR_FUNCTIONS = {
        // Opcode::NOP
        [](const Executable<Program::Vector>::Instruction*) {},
        // Opcode::ADD
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = *instruction->extraInput + *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::ADD_IMM
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = Program::Vector(instruction->immediate) + *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SUBTRACT
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = *instruction->extraInput - *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SUBTRACT_IMM
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = Program::Vector(instruction->immediate) - *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::MULTIPLY
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = *instruction->extraInput * *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::MULTIPLY_IMM
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = Program::Vector(instruction->immediate) * *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::DIVIDE
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = *instruction->extraInput / *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::DIVIDE_IMM
        [](const Executable<Program::Vector>::Instruction* instruction) {
            const Program::Vector result = Program::Vector(instruction->immediate) / *instruction->input;
            *instruction->output         = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::POWER
        [](const Executable<Program::Vector>::Instruction* instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument1 = *instruction->extraInput;
            const Program::Vector argument2 = *instruction->input;
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::pow(argument1[i], argument2[i]);
            }
            *instruction->output = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::CALL
        [](const Executable<Program::Vector>::Instruction* instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = *instruction->input;
            const RealFunction    callable = instruction->callable;
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = callable(argument[i]);
            }
            *instruction->output = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SIN
        [](const Executable<Program::Vector>::Instruction* instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = *instruction->input;
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::sin(argument[i]);
            }
            *instruction->output = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::COS
        [](const Executable<Program::Vector>::Instruction* instruction) {
            Program::Vector       result; // prevents aliasing
            const Program::Vector argument = *instruction->input;
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                result[i] = std::cos(argument[i]);
            }
            *instruction->output = result;
            return instruction->next(instruction + 1);
        },
        // Opcode::SINCOS
        [](const Executable<Program::Vector>::Instruction* instruction) {
            Program::Vector       sines, cosines; // prevents aliasing
            const Program::Vector argument = *instruction->input;
            for (int i = 0; i < Program::Vector::SIZE; ++i) {
                sines[i]   = std::sin(argument[i]);
                cosines[i] = std::cos(argument[i]);
            }
            *instruction->output      = sines;
            *instruction->extraOutput = cosines;
            return instruction->next(instruction + 1);
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
        default:
            assert(false);
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

template <typename TWord>
static Executable<TWord> makeExecutable(const Program::Constants&    constants,
                                        const Program::Instructions& program,
                                        const auto&                  functions) {
    std::vector<TWord>                                   memory;
    std::vector<typename Executable<TWord>::Instruction> instructions;
    typename Executable<TWord>::Function                 startPoint;

    const auto emitCall = [&](typename Executable<TWord>::Function call) {
        if (instructions.empty()) {
            startPoint = call;
        } else {
            instructions.back().next = call;
        }
    };

    const size_t programSize = program.instructions.size();
    memory.resize(program.memoryOffset + programSize, TWord{});
    std::copy(constants.values.begin(), constants.values.end(), memory.begin() + constants.memoryOffset);
    instructions.reserve(programSize);
    for (int i = 0; i < programSize; ++i) {
        const Program::Instruction& input = program.instructions[i];
        if (input.opcode == Program::Opcode::NOP) {
            continue;
        }
        emitCall(functions[int(input.opcode)]);
        typename Executable<TWord>::Instruction& output = instructions.emplace_back();
        output.output                                   = memory.data() + program.memoryOffset + i;
        output.input                                    = memory.data() + input.operand;
        switch (input.opcode) {
        case Program::Opcode::ADD:
        case Program::Opcode::SUBTRACT:
        case Program::Opcode::MULTIPLY:
        case Program::Opcode::DIVIDE:
        case Program::Opcode::POWER:
            output.extraInput = memory.data() + input.source;
            break;
        case Program::Opcode::ADD_IMM:
        case Program::Opcode::SUBTRACT_IMM:
        case Program::Opcode::MULTIPLY_IMM:
        case Program::Opcode::DIVIDE_IMM:
        case Program::Opcode::CALL:
            output.immediate = input.immediate;
            break;
        case Program::Opcode::SIN:
        case Program::Opcode::COS:
            break;
        case Program::Opcode::SINCOS:
            output.extraOutput = memory.data() + program.memoryOffset + (i + input.target);
            break;
        default:
            assert(false);
        }
    }
    emitCall(functions[int(Program::Opcode::NOP)]);
    return Executable<TWord>(std::move(memory), std::move(instructions), startPoint);
}

Executable<Program::Scalar> Program::makeScalarExecutable() const {
    return makeExecutable<Scalar>(mConstants, mInstructions, SCALAR_FUNCTIONS);
}

Executable<Program::Vector> Program::makeVectorExecutable() const {
    return makeExecutable<Vector>(mConstants, mInstructions, VECTOR_FUNCTIONS);
}

SIXPACK_NAMESPACE_END
