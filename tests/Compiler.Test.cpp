#include "Compiler.h"
#include "Exception.h"
#include "Program.h"
#include "Utilities.h"
#include <cmath>
#include <format>
#include <iostream>
#include <numbers>
using namespace sixpack;

static constexpr StringView SOURCE = R"SOURCE(### Kerr Metric ###
#
# Inputs
input  t
input  r
input  phi
input  theta

# Parameters
param  M     = 1                       # mass
param  J     = 0.8                     # angular momentum
       a     = J/M                     # spin parameter
       r_s   = 2*M                     # Schwarzschild radius
       DELTA = r^2 - 2*M*r + a^2       # discriminant
       SIGMA = r^2 + a^2*cos(theta)^2

# Outputs
output g_00 = -(1-r_s*r/SIGMA)
output g_01 = 0
output g_02 = 0
output g_03 = -[r_s*r*a*sin(theta)^2]/SIGMA
output g_10 = 0
output g_11 = SIGMA/DELTA
output g_12 = 0
output g_13 = 0
output g_20 = 0
output g_21 = 0
output g_22 = SIGMA
output g_23 = 0
output g_30 = -a*[2*M*r]/[a^2*cos(theta)^2 + r^2]*sin(theta)^2    # same as "g_03" but written differently
output g_31 = 0
output g_32 = 0
output g_33 = (r^2 + a^2 + [r_s*r*a^2]/SIGMA*sin(theta)^2)*sin(theta)^2
)SOURCE";

static constexpr int    PHI_STEPS   = 7200;  // [0..2*pi)
static constexpr int    THETA_STEPS = 3601;  // [0..pi]
static constexpr double DIFF_STEP   = 0.001; // step for the calculation of the metric differentials

static void printSection(StringView title) {
    std::cout << std::endl << "-- " << title << " " << std::string(120 - title.size(), '-') << std::endl;
}

static void test() {
    Compiler compiler;
    compiler.addFunction("sin", &std::sin);
    compiler.addFunction("cos", &std::cos);

    std::cout << SOURCE;
    compiler.addSourceScript(SOURCE);

    Program program = compiler.compile();
    printSection("Compiled Program");
    dumpProgram(program, std::cout);

    const Program::Address rAddress     = program.getInputAddress("r");
    const Program::Address phiAddress   = program.getInputAddress("phi");
    const Program::Address thetaAddress = program.getInputAddress("theta");
    Program::Address       resultAddress[4][4];
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            resultAddress[j][i] = program.getOutputAddress(std::format("g_{}{}", j, i));
        }
    }

    printSection("Test Bench");
    std::atomic_int activeTasks;

    const auto run = [&](const double r) {
        Executable<Program::Vector>   executable = program.makeVectorExecutable();
        std::vector<Program::Vector>& memory     = executable.memory();
        Program::Vector               result[4][4];
        memory[rAddress] = { r, r + DIFF_STEP, r, r };
        for (int pi = 0; pi < PHI_STEPS; ++pi) {
            const double phi   = double(pi) * 2.0 * std::numbers::pi_v<double> / double(PHI_STEPS);
            memory[phiAddress] = { phi, phi, phi + DIFF_STEP, phi };
            for (int ti = 0; ti < THETA_STEPS; ++ti) {
                const double theta   = double(ti) * std::numbers::pi_v<double> / double(THETA_STEPS - 1);
                memory[thetaAddress] = { theta, theta, theta, theta + DIFF_STEP };
                executable.run();
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        result[j][i] = memory[resultAddress[j][i]];
                    }
                }
            }
        }
        if (--activeTasks == 0) {
            // Print the very last calculated tensor (to prevent optimizing out the result).
            std::cout << std::format("Last result [r = {:f}, phi -> 360deg, theta -> 180deg]:", r)
                      << std::endl;
            std::cout << "g" << std::endl;
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    std::cout << " " << std::format("{:10f}", result[j][i][0]);
                }
                std::cout << std::endl;
            }
            std::cout << "dg/dr" << std::endl;
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    std::cout << " "
                              << std::format("{:10f}", (result[j][i][1] - result[j][i][0]) / DIFF_STEP);
                }
                std::cout << std::endl;
            }
            std::cout << "dg/dphi" << std::endl;
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    std::cout << " "
                              << std::format("{:10f}", (result[j][i][2] - result[j][i][0]) / DIFF_STEP);
                }
                std::cout << std::endl;
            }
            std::cout << "dg/dtheta" << std::endl;
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    std::cout << " "
                              << std::format("{:10f}", (result[j][i][3] - result[j][i][0]) / DIFF_STEP);
                }
                std::cout << std::endl;
            }
        }
    };

    const unsigned rSteps = 1 + 4 * std::thread::hardware_concurrency();
    const auto     start  = std::chrono::steady_clock::now();
    {
        std::vector<std::thread> threads; // poor man's thread pool
        threads.reserve(rSteps);
        activeTasks = rSteps + 1; // +1 to prevent the actual printout
        for (unsigned ri = 0; ri < rSteps; ++ri) {
            const double r = double(ri) * 10.0 / double(rSteps - 1);
            threads.emplace_back(run, r);
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
    }
    const auto   stop    = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(stop - start).count();
    const auto   tensorCount = int64_t(Program::Vector::SIZE) * rSteps * PHI_STEPS * THETA_STEPS;
    String       speed       = std::format("{}", int64_t(double(tensorCount) / seconds));
    for (StringPosition i = speed.size() - 3; i > 0 && i < speed.size(); i -= 3) {
        speed.insert(i, "'");
    }
    std::cout << std::endl << "Evaluation speed: ~" << speed << " tensors per second." << std::endl;
}

int main() {
    try {
        test();
        return 0;
    } catch (const Exception& exception) {
        printSection("ERROR");
        std::cerr << std::format("Unhandled exception: {}.", exception.message()) << std::endl;
        return 1;
    }
}
